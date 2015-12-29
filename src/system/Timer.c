/* Copyright 2000, 2014 Neil Edelman, distributed under the terms of the GNU
 General Public License, see copying.txt */

#include "../Print.h"
#include "Glew.h"
#include "Timer.h"
#include "../game/Game.h"

/** This is an idempotent class dealing with the interface to OpenGL.
 @author	Neil
 @version	3.3, 2015-12
 @since		3.0, 2014 */

static const int frametime_ms = 20; /* 50 fps -- fixme: variable */
static const int persistance  = (int)(0.9 * 1024); /* fixed point :10 */

static int is_running;

static unsigned mean_frametime = 20;

static unsigned last_time;
static unsigned paused_time;

/* private */

static void update(int value);

/** This starts the Timer. */
void TimerRun(void) {
	const unsigned time = glutGet(GLUT_ELAPSED_TIME);

	if(is_running) return;

	paused_time += time - last_time;
	last_time   =  time;
	is_running  =  -1;
	Debug("Timer: starting timer with %ums paused, %ums programme, %ums game.\n", paused_time, last_time, TimerGetGameTime());

	glutTimerFunc(frametime_ms, &update, 0);

}

/** This stops the Timer. */
void TimerPause(void) {
	if(!is_running) return;
	is_running = 0;
	Debug("Timer: stopped timer with last %ums.\n", last_time);
}

/** @return		Boolean value. */
int TimerIsRunning(void) {
	return is_running;
}

/** Last time an update was called. */
unsigned TimerGetGameTime(void) {
	return last_time - paused_time;
}

/** The value is a positive number.
 @returns	Moving average in milliseconds. */
unsigned TimerGetMean(void) {
	return mean_frametime > 0 ? mean_frametime : 1;
}

/** Callback for glutTimerFunc.
 @param zero	0 */
static void update(int zero) {
	const unsigned time = glutGet(GLUT_ELAPSED_TIME);
	const unsigned dt   = time - last_time;

	if(!is_running) return;

	last_time = time;
	mean_frametime = (mean_frametime*persistance + dt*(1024-persistance)) >> 10;
	glutTimerFunc(frametime_ms, &update, 0);
	GameUpdate(dt);
	glutPostRedisplay();
}
