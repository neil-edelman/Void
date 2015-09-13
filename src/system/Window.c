/* Copyright 2000, 2014 Neil Edelman, distributed under the terms of the GNU
 General Public License, see copying.txt */

#include <stdio.h>  /* fprintf */
#include <time.h>   /* for errors: can't rely on external libraries */
#include <stdlib.h> /* exit */
#include "Glew.h"
#include "Window.h"

/** This is an idempotent class dealing with the interface to OpenGL.
 @author	Neil
 @version	3.0, 2015-05
 @since		3.0, 2015-05 */

static const int    no_fails = 64;
static const double forget_s = 20.0;
static const int    warn_texture_size = 1024;

/* if is started, we don't and can't start it again */
static int    is_started;
static time_t last_error;

/** Gets the window started.
 @param title	The title of the window.
 @param argc
 @param argv	main() program arguments; passed to glutInit().
 @return		Whether the graphics library is ready. */
int Window(const char *title, int argc, char **argv) {
	GLint max_tex;

	if(is_started) return -1;

	/* we keep track of how many errors per time; too many, we exit */
	time(&last_error);

	/* glut */
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE);
	glutInitWindowSize(600, 400);
	glutCreateWindow(title ? title : "Untitled");
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_tex);
	fprintf(stderr,
			"Window: started; GLSL stats: vendor %s; version %s; renderer %s; shading language version %s; combined texture image units %d; maximum texture size %d.\n",
			glGetString(GL_VENDOR), glGetString(GL_VERSION),
			glGetString(GL_RENDERER), glGetString(GL_SHADING_LANGUAGE_VERSION),
			GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, max_tex);
	if(max_tex < warn_texture_size)
		fprintf(stderr, "Window: maximum texture size is too small, %d/%d; warning! bad! (supposed to be 8 or larger.)\n", max_tex, warn_texture_size);
	/*glutMouseFunc(&mouse);
	 glutIdleFunc(0); */

	/* load OpenGl2+ (AFTER THE WINDOW!) */
	if(!Glew()) return 0;

	is_started = -1;
	return -1;
}

/** @return		is_started accessor. */
int WindowStarted(void) {
	return is_started;
}

/** Begins animation with the hooks that have been placed in the window. Does
 not return! */
void WindowGo(void) {
	if(!is_started) return;
	glutMainLoop();
}

/** Checks for all OpenGL errors and prints them to stderr.
 @param function	The calling function, for prepending to stderr.
 @return			Returns true if any errors (not very useful.) */
int WindowIsGlError(const char *function) {
	static int no_errs;
	GLenum err;
	int ohoh = 0;
	while((err = glGetError()) != GL_NO_ERROR) {
		time_t now;
		time(&now);
		/* reset the kill errors */
		if(difftime(now, last_error) > forget_s) {
			no_errs = 0;
			time(&last_error);
		}
		fprintf(stderr, "Window::isGLError(caught in %s:) OpenGL error: %s.\n", function, gluErrorString(err));
		if(++no_errs > no_fails) {
			fprintf(stderr, "Window::isGLError: too many errors! :[\n");
			exit(EXIT_FAILURE);
		}
		ohoh = -1;
	}
	return ohoh;
}

/** toggle fullscreen */
void WindowToggleFullScreen(void) {
	static int x_size = 600, y_size = 400, x_pos = 0, y_pos = 0;
	static int full = 0;

	if(!full) {
		fprintf(stderr, "Entering fullscreen.\n");
		full = -1;
		x_size = glutGet(GLUT_WINDOW_WIDTH);
		y_size = glutGet(GLUT_WINDOW_HEIGHT);
		x_pos  = glutGet(GLUT_WINDOW_X);
		y_pos  = glutGet(GLUT_WINDOW_Y);
		glutFullScreen();
		glutSetCursor(GLUT_CURSOR_NONE);
	} else {
		fprintf(stderr, "Exiting fullscreen.\n");
		full = 0;
		glutReshapeWindow(x_size, y_size);
		glutPositionWindow(x_pos, y_pos);
		glutSetCursor(GLUT_CURSOR_INHERIT);
	}

	WindowIsGlError("Window::toggleFullScreen");
}

#if 0

/** subclass Console */

static void Console(struct Console *console) {
	console->buffer[0] = 'f';
	console->buffer[1] = 'o';
	console->buffer[2] = 'o';
	console->buffer[3] = '\0';
}

/** printf for the window */
void OpenPrint(const char *fmt, ...) {
	va_list ap;
	
	if(!fmt || !open) return;
	/* print the chars into the buffer */
	va_start(ap, fmt);
	vsnprintf(open->console.buffer, console_buffer_size, fmt, ap);
	va_end(ap);
	printf("Open::Console: \"%s\" (%d.)\n", open->console.buffer, console_buffer_size);
}

/** called by display every frame */
static void rasterise_text(void) {
	char *a;
	
	glPushAttrib(GL_TRANSFORM_BIT | GL_VIEWPORT_BIT);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glTranslatef(-1, 0.9, 0);
	/*glViewport(0, 0, open->screen.dim.x, open->screen.dim.y);*/
	glColor4f(0, 1, 0, 0.7);
	glRasterPos2i(0, 0);
	
	/* WTF!!!! it's drawing it in the bottom?? fuck I can't take this anymore */
	
	/* draw overlay */
	/*glMatrixMode(GL_MODELVIEW);
	 glPushMatrix();
	 glTranslatef(-(float)open->screen.max.x * 1.f, -(float)open->screen.max.y * 1.f, 0);*/
	for(a = open->console.buffer; *a; a++) {
		switch(*a) {
			case '\n':
				glRasterPos2i(0, -1);
				break;
			default:
				glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *a);
		}
	}
	glColor4f(1, 1, 1, 1);
	/*glPopMatrix();*/
	
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glPopAttrib();
	
	isGLError("Open::Console::rasterise_text");
}
#endif
