/* Copyright 2000, 2014 Neil Edelman, distributed under the terms of the GNU
 General Public License, see copying.txt */

#include "../Print.h"
#include "Glew.h"
#include "Timer.h"
#include "Window.h"
#include "../game/Game.h"

/** This is an idempotent class dealing with the interface to OpenGL.
 @author	Neil
 @version	3.2, 2015-05
 @since		2.0, 2014 (<- what?)*/

static const int framelength_ms = 20; /* 50 fps -- fixme: variable */
static const int persistance    = (int)(0.9 * 1024); /* fixed point :10 */

static int is_started;

#ifdef KEEP_TRACK_OF_TIMES
#define TIMER_LOG_FRAMES 2
#endif

struct Timer {
	int step;  /* ms */
	int time;
	int mean;
} timer;

static void update(int value);

/** This starts the Timer or modifies the Timer.
 <p>
 Fixme: I don't know how this will repond when you've not called glutInit();
 don't do it. "Requesting state for an invalid GLUT state name returns
 negative one."? Scetchy.
 @param step	The resolution of the timer. */
int Timer(const int step) {

	timer.step  = step;
	if(is_started) return -1;

	if(!WindowStarted()) {
		Debug("Timer: window not started.\n");
		return 0;
	}

	timer.time  = glutGet(GLUT_ELAPSED_TIME);
	timer.mean  = framelength_ms;
	is_started = -1;
	Debug("Timer: created timer with %ums.\n", timer.time);

	glutTimerFunc(timer.step, &update, timer.time);

	return -1;
}

/** This stops the Timer. */
void Timer_(void) {
	if(!is_started) return;
	is_started = 0;
	Debug("~Timer: erased timer.\n");
}

/** Last time an update was called. */
int TimerLastTime(void) {
	return timer.time;
}

/** The value is a positive number.
 @returns	Moving average in milliseconds. */
int TimerMean(void) {
	return timer.mean > 0 ? timer.mean : 1;
}

/** @returns	Framelength in milliseconds. */
int TimerGetFramelength(void) {
	return framelength_ms;
}

/** Callback for glutTimerFunc.
 @param old_time	The previous time. */
static void update(int old_time) {
	int dt;

	if(!is_started) return;

	/* all times are in ms */
	timer.time  = glutGet(GLUT_ELAPSED_TIME);
	dt = timer.time - old_time;
	timer.mean = (timer.mean*persistance + dt*(1024-persistance)) >> 10;
	glutTimerFunc(timer.step, &update, timer.time);
	GameUpdate(timer.time, dt);
	/* now void -> if(!GameUpdate(timer.time, dt)) printf("Should really be shutting down.\n");*/
	/*Debug("Timer::update: %ums %ums.\n", timer.time, dt);*/
	glutPostRedisplay();
}
