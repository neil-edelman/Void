/** Copyright 2000, 2014 Neil Edelman, distributed under the terms of the GNU
 General Public License, see copying.txt.

 This is an idempotent class dealing with the interface to OpenGL. Time is
 represented with {unsigned} that loops around.

 @title		Timer
 @author	Neil
 @version	3.3, 2015-12
 @since		3.0, 2014 */

#include <stdio.h>	/* fprintf */
#include <limits.h> /* MAX_INT, MIN_INT */
#include "../Unused.h"
#include "Timer.h"

/* 50 fps. @fixme Sync to refresh, why is it so hard? */
static const int frametime_ms = 20;
/* Smooths out the frame-rate reporting, fixed point :10. */
static const int persistance  = (int)(0.9 * 1024);

static struct Timer {
	unsigned last, paused, game, mean_frame;
	int is_running;
	WindowGlutFunction logic;
} timer;

static unsigned ms_time(void) {
	/* <-- glut */
	return glutGet(GLUT_ELAPSED_TIME);
	/* glut --> */
}

/* private */

/** Callback for {glutTimerFunc}.
 @param zero: Unused. */
static void update(int zero) {
	const unsigned time = ms_time();
	const unsigned dt   = time - timer.last;
	if(!timer.is_running) return;
	timer.last = time;
	timer.game = timer.last - timer.paused;
	timer.mean_frame
		= (timer.mean_frame * persistance + dt * (1024 - persistance)) >> 10;
	glutTimerFunc(frametime_ms, &update, 0);
	timer.logic(dt);
	glutPostRedisplay();
	UNUSED(zero);
}

/** This starts the Timer. */
void TimerRun(const WindowGlutFunction logic) {
	const unsigned time = ms_time();
	if(timer.is_running || !logic) return;
	timer.paused    += time - timer.last;
	timer.game       = timer.last;
	timer.last       = time;
	timer.is_running = 1;
	timer.logic      = logic;
	fprintf(stderr, "Timer: starting timer with %ums paused, %ums programme, "
		"%ums game. Aiming for %ums frametime.\n", timer.paused, timer.last,
		TimerGetGameTime(), frametime_ms);
	/* <-- glut */
	glutTimerFunc(frametime_ms, &update, 0);
	/* glut --> */
}

/** This stops the Timer. */
void TimerPause(void) {
	if(!timer.is_running) return;
	timer.is_running = 0;
	fprintf(stderr, "TimerPause: stopped timer with last %ums.\n", timer.last);
}

/** @return Whether the timer is running. */
int TimerIsRunning(void) { return timer.is_running; }

/** @return Last time an update was called in {ms}, taking into account the
 pauses. */
unsigned TimerGetGameTime(void) { return timer.game; }

/** @return If the game time is greater or equal {t}. Only game time is
 represented as a circular list; 'greater than' is not really defined, so as
 long as it's {(t - INT_MIN, t]} it's considered 'in.' This defines a limit of
 an interval of 24.85 days. */
int TimerIsGameTime(const unsigned t) {
	const int p1 = (t <= timer.game);
	const int p2 = ((timer.game ^ INT_MIN) < t);
	const int p3 = (timer.game <= INT_MAX);
	/* This is literally the worst case for optimising; p3 is generally true. */
	return (p1 && p2) || ((p2 || p1) && p3);
}

/** @return Moving average in milliseconds. */
unsigned TimerGetFrame(void) {
	return timer.mean_frame > 0 ? timer.mean_frame:1;
}

/** @return The asynchronous time since the the programme was started in
 {ms}. */
unsigned TimerGetTime(void) {
	return ms_time();
}
