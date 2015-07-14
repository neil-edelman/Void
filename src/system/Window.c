/* Copyright 2000, 2014 Neil Edelman, distributed under the terms of the GNU
 General Public License, see copying.txt */

#include <stdio.h>  /* fprintf */
#include "Glew.h"
#include "Window.h"

/** This is an idempotent class dealing with the interface to OpenGL.
 @author	Neil
 @version	3.0, 05-2015
 @since		3.0, 05-2015 */

/* if is started, we don't and can't start it again */
static int is_started;

/** Gets the window started.
 @param title	The title of the window.
 @param argc
 @param argv	main() program arguments; passed to glutInit().
 @return		Whether the graphics library is ready. */
int Window(const char *title, int argc, char **argv) {

	if(is_started) return -1;

	/* load OpenGl2+ */
	if(!Glew()) return 0;

	/* glut */
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE);
	glutInitWindowSize(600, 400);
	glutCreateWindow(title ? title : "Untitled");
	fprintf(stderr,
			"Window: started; GLSL stats: vendor %s; version %s; renderer %s; shading language version %s; combined texture image units %d.\n",
			glGetString(GL_VENDOR), glGetString(GL_VERSION),
			glGetString(GL_RENDERER), glGetString(GL_SHADING_LANGUAGE_VERSION),
			GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS);
	/*glutMouseFunc(&mouse);
	 glutIdleFunc(0); */

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
	GLenum err;
	int ohoh = 0;
	while((err = glGetError()) != GL_NO_ERROR) {
		fprintf(stderr, "Draw::isGLError(caught in %s:) OpenGL error: %s.\n", function, gluErrorString(err));
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
	} else {
		fprintf(stderr, "Exiting fullscreen.\n");
		full = 0;
		glutReshapeWindow(x_size, y_size);
		glutPositionWindow(x_pos, y_pos);
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
