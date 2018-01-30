/** Copyright 2000, 2014 Neil Edelman, distributed under the terms of the GNU
 General Public License, see copying.txt.

 This is an idempotent class dealing with the interface to OpenGL. Time is
 represented with {unsigned} that loops around.

 @title		Timer
 @author	Neil
 @version	3.3, 2015-12
 @since		3.0, 2014 */

#include <limits.h> /* MAX_INT, MIN_INT */
#include "../WindowGl.h"
#include "Timer.h"
#include "../game/Game.h"

static const int frametime_ms = 20; /* 50 fps -- fixme: variable */
static const int persistance  = (int)(0.9 * 1024); /* fixed point :10 */

static struct Timer {
	unsigned last, paused, game, mean_frame;
	int is_running;
} timer;

static unsigned ms_time(void) {
#ifdef GLUT /* <-- glut */
	return glutGet(GLUT_ELAPSED_TIME);
#elif defined(SDL) /* glut --><-- sdl */
	return (unsigned)SDL_GetTicks();
#else /* sdl --><-- nothing */
#error Define GLUT or SDL.
#endif /* nothing --> */	
}

/* private */

#ifdef GLUT /* <-- glut */
/** Callback for glutTimerFunc.
 @param zero: 0 */
static void update(int zero) {
	const unsigned time = ms_time();
	const unsigned dt   = time - timer.last;
	if(!timer.is_running) return;
	timer.last = time;
	timer.game = timer.last - timer.paused;
	timer.mean_frame
		= (timer.mean_frame * persistance + dt * (1024 - persistance)) >> 10;
	glutTimerFunc(frametime_ms, &update, 0);
	GameUpdate(dt);
	glutPostRedisplay();
	UNUSED(zero);
}
#endif /* glut --> */

#ifdef SDL /* <-- awkward */
/** In SDL, you have the inefficient central event loop.
 @return The frame time. */
unsigned TimerUpdate(void) {
	const unsigned time = ms_time();
	const unsigned dt   = time - timer.last;
	if(!timer.is_running) return 0;
	timer.last = time;
	timer.game = timer.last - timer.paused;
	timer.mean_frame
		= (timer.mean_frame * persistance + dt * (1024 - persistance)) >> 10;
	return dt;
}
#endif /* awkward --> */

/** This starts the Timer. */
void TimerRun(void) {
	const unsigned time = ms_time();
	if(timer.is_running) return;
	timer.paused    += time - timer.last;
	timer.game       = timer.last;
	timer.last       = time;
	timer.is_running = 1;
	fprintf(stderr, "Timer: starting timer with %ums paused, %ums programme, "
		"%ums game. Aiming for %ums frametime.\n", timer.paused, timer.last,
		TimerGetGameTime(), frametime_ms);
#ifdef GLUT /* <-- glut */
	glutTimerFunc(frametime_ms, &update, 0);
#endif /* glut --> */

}

/** This stops the Timer. */
void TimerPause(void) {
	if(!timer.is_running) return;
	timer.is_running = 0;
	fprintf(stderr, "TimerPause: stopped timer with last %ums.\n", timer.last);
}

/** @return Whether the timer is running. */
int TimerIsRunning(void) { return timer.is_running; }

/** @return Last time an update was called in milliseconds. */
unsigned TimerGetGameTime(void) { return timer.game; }

/** @return Moving average in milliseconds. */
unsigned TimerGetMean(void) { return timer.mean_frame > 0 ? timer.mean_frame:1;}

/** @return If the game time is greater or equal {t}. Only game time is
 represented as a circular list; 'greater than' is not really defined, so as
 long as it's {(t - INT_MIN, t]} it's considered 'in.' This defines a limit of
 an interval of 24.85 days. */
int TimerIsTime(const unsigned t) {
	const int p1 = (t <= timer.game);
	const int p2 = ((timer.game ^ INT_MIN) < t);
	const int p3 = (timer.game <= INT_MAX);
	/* this is literally the worst case for optimising; p3 is generally true */
	return (p1 && p2) || ((p2 || p1) && p3);
}
