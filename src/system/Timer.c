/* Copyright 2000, 2014 Neil Edelman, distributed under the terms of the GNU
 General Public License, see copying.txt */

#include <stdio.h>  /* fprintf */
#include "Glew.h"
#include "Timer.h"
#include "Window.h"
#include "../game/Game.h"

/** This is an idempotent class dealing with the interface to OpenGL.
 @author	Neil
 @version	3.2, 2015-05
 @since		2.0, 2014 (<- what?)*/

static const int framelength_ms = 20; /* 50 fps -- fixme: variable */
static const int persistance    = 0.9 * 1024;

static int is_started;

#ifdef KEEP_TRACK_OF_TIMES
#define TIMER_LOG_FRAMES 2
#endif

struct Timer {
	int step;  /* ms */
	int time;
	int mean;
#ifdef KEEP_TRACK_OF_TIMES
	int frame;
	int frame_time[1 << TIMER_LOG_FRAMES];
#endif
} timer;
#ifdef KEEP_TRACK_OF_TIMES
const static int max_frame_time = sizeof(((struct Timer *)0)->frame_time) / sizeof(int);
#endif

static void update(int value);

/** This starts the Timer or modifies the Timer.
 <p>
 Fixme: I don't know how this will repond when you've not called glutInit();
 don't do it. "Requesting state for an invalid GLUT state name returns
 negative one."? Scetchy.
 @param step	The resolution of the timer. */
int Timer(const int step) {
#ifdef KEEP_TRACK_OF_TIMES
	int i;
#endif

	timer.step  = step;
	if(is_started) return -1;

	if(!WindowStarted()) {
		fprintf(stderr, "Timer: window not started.\n");
		return 0;
	}

	timer.time  = glutGet(GLUT_ELAPSED_TIME);
	timer.mean  = framelength_ms;
#ifdef KEEP_TRACK_OF_TIMES
	timer.frame = 0;
	for(i = 0; i < max_frame_time; i++) timer.frame_time[i] = timer.time;
#endif
	is_started = -1;
	fprintf(stderr, "Timer: created timer with %ums.\n", timer.time);

	glutTimerFunc(timer.step, &update, timer.time);

	return -1;
}

/** This stops the Timer. */
void Timer_(void) {
	if(!is_started) return;
	is_started = 0;
	fprintf(stderr, "~Timer: erased timer.\n");
}

/** Last time an update was called. */
int TimerLastTime(void) {
	return timer.time;
}

/** @returns	Moving average in milliseconds. */
int TimerMean(void) {
	return timer.mean;
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
#ifdef KEEP_TRACK_OF_TIMES
	timer.frame = (timer.frame + 1) & (max_frame_time - 1);
	timer.frame_time[timer.frame] = dt;
#else
	timer.mean = (timer.mean*persistance + dt*(1024-persistance)) >> 10;
#endif
	glutTimerFunc(timer.step, &update, timer.time);
	GameUpdate(timer.time, dt);
	/* now void -> if(!GameUpdate(timer.time, dt)) printf("Should really be shutting down.\n");*/
	/*fprintf(stderr, "Timer::update: %ums %ums.\n", timer.time, dt);*/
	glutPostRedisplay();
}
