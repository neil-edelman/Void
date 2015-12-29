/* Copyright 2000, 2014 Neil Edelman, distributed under the terms of the GNU
 General Public License, see copying.txt */

#include "../Print.h"
#include "Glew.h"
#include "Timer.h"
#include "Window.h"
#include "../game/Game.h"

/** This is an idempotent class dealing with the interface to OpenGL.
 @author	Neil
 @version	3.3, 2015-12
 @since		3.0, 2014 */

static const int framelength_ms = 20; /* 50 fps -- fixme: variable */
static const int persistance    = (int)(0.9 * 1024); /* fixed point :10 */

static int is_started;

static struct Timer {
	int step;  /* ms */
	int time;
	int mean;
} timer;

static void update(int value);

/** This starts the Timer or modifies the Timer. */
int Timer(void) {

	timer.step  = framelength_ms;
	if(is_started) return -1;

	if(!WindowStarted()) {
		Debug("Timer: window not started.\n");
		return 0;
	}

	timer.time = glutGet(GLUT_ELAPSED_TIME);
	timer.mean = framelength_ms;
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
int TimerGetMean(void) {
	return timer.mean > 0 ? timer.mean : 1;
}

/** @return		Boolean value. */
int TimerIsRunning(void) {
	return is_started;
}

/** Callback for glutTimerFunc.
 @param old_time	The previous time. */
static void update(int old_time) {
	int dt;

	if(!is_started) return;

	/* all times are in ms */
	timer.time  = glutGet(GLUT_ELAPSED_TIME);
	dt = timer.time - old_time;
	/* smoothed function */
	timer.mean = (timer.mean*persistance + dt*(1024-persistance)) >> 10;
	glutTimerFunc(timer.step, &update, timer.time);
	GameUpdate(timer.time, dt);
	glutPostRedisplay();
}
