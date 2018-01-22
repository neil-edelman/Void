/** Copyright 2000, 2014 Neil Edelman, distributed under the terms of the GNU
 General Public License, see copying.txt

 This is an idempotent class dealing with the interface to OpenGL.

 @title		Window
 @author	Neil
 @version	2017-12 Added font.
 @since		2015-05 */

#include <stdio.h>	/* *printf */
#include <time.h>   /* for errors: can't rely on external libraries */
#include <stdlib.h> /* exit */
#include <stdarg.h>	/* va_* */
#include "../general/OrthoMath.h" /* Vec2i */
#include "Glew.h"
#include "Window.h"

static const unsigned max_gl_fails = 64;
static const double forget_errors = 10.0; /* s */
static const int warn_texture_size = 1024;

static struct Window {
	int is_started;
	struct Vec2i size, position;
	int is_full;
	time_t last_error;
	unsigned no_errors;
	unsigned console_start;
	char console[4][80];
} window;

static const size_t console_size = sizeof ((struct Window *)0)->console[0];

/** Gets the window started. There is no destructor.
 @param title The title of the window (can be null.)
 @param argc, argv: {main} program arguments; passed to glutInit().
 @return Whether the graphics library is ready and the window is started. */
int Window(const char *title, int argc, char **argv) {
	GLint max_tex;
	/* {is_started} serves as null. Initialise the {window}. This can only be
	 done once. */
	if(window.is_started) return 1;
	window.size.x = 600, window.size.y = 400;
	/* We keep track of how many errors per time; too many, we exit. */
	if(time(&window.last_error) == (time_t)-1)
		return fprintf(stderr, "Window: time function not working.\n"), 0;
	/* Glut. */
	fprintf(stderr, "Window: GLUT initialising.\n");
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE);
	glutInitWindowSize(window.size.x, window.size.y);
	glutCreateWindow(title ? title : "Untitled");
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_tex);
	fprintf(stderr, "Window: GLSL: vendor %s; version %s; renderer %s; shading "
		"language version %s; combined texture image units %d; maximum texture "
		"size %d.\n", glGetString(GL_VENDOR), glGetString(GL_VERSION),
		glGetString(GL_RENDERER), glGetString(GL_SHADING_LANGUAGE_VERSION),
		GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, max_tex);
	if(max_tex < warn_texture_size)
		fprintf(stderr, "Window: maximum texture size is probably too small, "
		"%d/%d.\n", max_tex, warn_texture_size);
	/*glutMouseFunc(&mouse);
	 glutIdleFunc(0); */
	/* Load OpenGl2+ after the window. */
	if(!Glew()) return 0;
	window.is_started = 1;
	return 1;
}

/** @return Whether the window is active. */
int WindowStarted(void) {
	return window.is_started;
}

/** Begins animation with the hooks that have been placed in the window. Does
 not return! */
void WindowGo(void) {
	if(!window.is_started) return;
	glutMainLoop();
}

/** Checks for all OpenGL errors and prints them to stderr.
 @param function: The calling function, for prepending to stderr.
 @return Returns true if any errors, (not very useful.) */
int WindowIsGlError(const char *function) {
	GLenum err;
	int is_some_error = 0;
	if(!window.is_started) return 0;
	while((err = glGetError()) != GL_NO_ERROR) {
		time_t now;
		time(&now);
		/* Reset errors after a time. */
		if(difftime(now, window.last_error) > forget_errors)
			window.no_errors = 0, time(&window.last_error);
		fprintf(stderr, "WindowIsGlError: OpenGL error caught in %s: %s.\n",
			function, gluErrorString(err));
		if(++window.no_errors > max_gl_fails) {
			fprintf(stderr, "Window:IsGLError: too many errors! :[\n");
			exit(EXIT_FAILURE);
		}
		is_some_error = 1;
	}
	return is_some_error;
}

/** Toggle fullscreen. */
void WindowToggleFullScreen(void) {
	if(!window.is_started) return;
	if(!window.is_full) {
		fprintf(stderr, "WindowToggleFullScreen: entering fullscreen.\n");
		window.size.x     = glutGet(GLUT_WINDOW_WIDTH);
		window.size.y     = glutGet(GLUT_WINDOW_HEIGHT);
		window.position.x = glutGet(GLUT_WINDOW_X);
		window.position.y = glutGet(GLUT_WINDOW_Y);
		glutFullScreen(), window.is_full = 1;
		glutSetCursor(GLUT_CURSOR_NONE);
	} else {
		fprintf(stderr, "WindowToggleFullScreen: exiting fullscreen.\n");
		glutReshapeWindow(window.size.x, window.size.y), window.is_full = 0;
		glutPositionWindow(window.position.x, window.position.y);
		glutSetCursor(GLUT_CURSOR_INHERIT);
	}
	WindowIsGlError("WindowToggleFullScreen");
}

/** Printf for the window. */
void WindowPrint(const char *fmt, ...) {
	va_list ap;
	if(!fmt || !window.is_started) return;
	/* Print the chars into the buffer. */
	va_start(ap, fmt);
	vsnprintf(window.console[window.console_start], console_size, fmt, ap);
	va_end(ap);
	fprintf(stderr, "WindowPrint: \"%s\" (%lu)\n",
		window.console[window.console_start], (long unsigned)console_size);
	++window.console_start, window.console_start &= 3;
}

/** @fixme Called by display every frame. */
void WindowRasteriseText(void) {
	char *a;
	if(!window.is_started) return;

	glPushAttrib(GL_TRANSFORM_BIT | GL_VIEWPORT_BIT);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glTranslatef(-1.0f, 0.9f, 0.0f);
	glViewport(0, 0, window.size.x, window.size.y);

	glColor4f(0.0f, 1.0f, 0.0f, 0.7f);
	glRasterPos2i(0, 0);
	/* WTF!!!! it's drawing it in the bottom?? fuck I can't take this anymore */
	/* draw overlay */
	/*glMatrixMode(GL_MODELVIEW);
	 glPushMatrix();
	 glTranslatef(-(float)open->screen.max.x * 1.f, -(float)open->screen.max.y * 1.f, 0);*/
	for(a = window.console[0]; *a; a++)
		glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *a);
	glColor4f(1, 1, 1, 1);

	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glPopAttrib();

	WindowIsGlError("rasterise_text");
}
