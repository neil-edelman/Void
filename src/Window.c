/** Copyright 2000, 2013 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt.

 Entry-point. Deals with providing an interface to the libraries.

 Uses the {OpenGL} API for graphics, https://www.opengl.org/. On some machines,
 you have to query the library for {OpenGL2+}. If {GLEW} is defined by the
 environment, it uses this, \url{http://glew.sourceforge.net/}. (Necessary for
 Windows, Linux.)

 {FreeGLUT}, \url{http://freeglut.sourceforge.net/}, is used for the
 cross-platform window-manager. However, this is built-in
 1998-{GLUT}-compatible for Mac: {FreeGLUT} uses {XQuartz} or {X11}, which is
 absolutely unacceptable.

 @title		Window
 @author	Neil
 @version	2018-01
 @since		2015-05
 @fixme		Windowing: Tl? GTK+?
 @fixme		{SDL}: opening a OS-specific window without the stupid terminal;
 synch; had window is hidden control; had terrible polling input functions. */

#include <stdlib.h> /* EXIT_ */
#include <stdio.h>  /* fprintf */
#include <time.h>   /* srand clock */
#include <string.h> /* strcmp */
#include <assert.h>
#include "Ortho.h" /* Vec2i */
#include "Unused.h"
#include "system/Draw.h"
#include "system/Timer.h"
#include "system/Key.h"
#include "system/Glew.h"
#include "general/Events.h"
#include "game/Sprites.h"
#include "game/Fars.h"
#include "game/Game.h"
#include "Window.h"

/* constants */
static const char *programme   = "Void";
static const char *year        = "2015";

static const unsigned max_gl_fails = 64;
static const double forget_errors = 10.0; /* s */
static const int warn_texture_size = 1024;

static struct Window {
	/* <-- glut */
	struct Vec2i size, position;
	int is_full;
	/* glut --> */
	time_t last_error;
	unsigned no_errors;
} window;

/** Help screen. */
static void usage(void) {
	fprintf(stderr, "Usage: %s\n"
		"To win, blow up everything that's not you.\n"
		"Player one controls: left, right, up, down, space\n"
		"Fullscreen: F1.\n"
		"Exit: Escape.\n\n"
		"%s Copyright %s Neil Edelman\n"
		"This program comes with ABSOLUTELY NO WARRANTY.\n"
		"This is free software, and you are welcome to redistribute it\n" 
		"under certain conditions; see copying.txt.\n\n",
		programme, programme, year);
	fprintf(stderr, "Image credit: NASA; JPL; ESA; Caltech; UCLA; MPS; DLR;\n"
		"IDA; Johns Hopkins University APL; Carnegie Institution of\n"
		"Washington; DSS Consortium; SDSS.\n\n"
		"lodepng: Copyright (c) 2005-2015 Lode Vandevenne\n"
		"http://lodev.org/lodepng/\n\n"
		"nanojpeg: by Martin J. Fiedler\n"
		"http://keyj.emphy.de/nanojpeg/\n\n"
		"OpenGL: by SGI\n"
		"https://www.opengl.org/\n\n"
		"GLEW: by Milan Ikits and Marcelo Magallon\n"
		"http://glew.sourceforge.net/\n\n"
		"GLUT: by Mark Kilgard and FreeGLUT: by Pawel W. Olszta\n"
		"http://freeglut.sourceforge.net/\n\n");
}

/** Gets the window started.
 @param title The title of the window (can be null.)
 @param argc, argv: {main} program arguments; passed to glutInit().
 @return Whether the graphics library is ready and the window is started. */
static int Window(const char *title, int argc, char **argv) {
	GLint max_tex;
	assert(title);
	/* We keep track of how many errors per time; too many, we exit. */
	if(time(&window.last_error) == (time_t)-1)
		return fprintf(stderr, "Window: time function not working.\n"), 0;
	/* <-- glut */
	fprintf(stderr, "Window: GLUT initialising.\n");
	window.size.x = 600, window.size.y = 400;
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE);
	glutInitWindowSize(window.size.x, window.size.y);
	glutCreateWindow(title);
	/* glut --> fixme: figure out how to do SDL_GL_SetSwapInterval(1) */
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_tex);
	fprintf(stderr, "Window: GLSL: vendor %s; version %s; renderer %s; shading "
		"language version %s; combined texture image units %d; maximum texture "
		"size %d.\n", glGetString(GL_VENDOR), glGetString(GL_VERSION),
		glGetString(GL_RENDERER), glGetString(GL_SHADING_LANGUAGE_VERSION),
		GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, max_tex);
	if(max_tex < warn_texture_size)
		fprintf(stderr, "Window: maximum texture size is probably too small, "
		"%d/%d.\n", max_tex, warn_texture_size);
	return 1;
}

/** This is legacy code from 1998 when there was no way to get out of the main
 loop. */
static void atexit_hack(void) {
	TimerPause(), Game_(), Draw_(), Fars_(), Sprites_(), Events_();
}

/** Entry point.
 @param argc the number of arguments starting with the programme name
 @param argv the arguments
 @return     either EXIT_SUCCESS or EXIT_FAILURE */
int main(int argc, char **argv) {
	const char *e = 0;
	/* @fixme More options (ie, load game, etc.) */
	if(argc > 1) return usage(), EXIT_SUCCESS;
	/* Direct sdtout, stderr, to log files instead of to a awkward separate
	 terminal window. Nope. */
	/*freopen("stdout.txt", "a+", stdout);
	freopen("stderr.txt", "a+", stderr);*/
#ifdef FREEGLUT /* <-- free */
	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE,GLUT_ACTION_GLUTMAINLOOP_RETURNS);
#else /* free --><-- !free */
	if(atexit(&atexit_hack)) perror("atexit");
#endif /* !free --> */
	/* Entropy increase. */
	srand((unsigned)clock());
	do { /* try */
		/* Window has to be first. */
		if(!Window(programme, argc, argv)) { e = "window"; break; }
		if(!Glew()) { e = "glew"; break; }
		Key();
		if(!Events()) { e = "events"; break; }
		if(!Sprites()) { e = "sprites"; break; }
		if(!Fars()) { e = "fars"; break; }
		if(!Draw()) { e = "draw"; break; }
		if(!Game()) { e = "game"; break; }
		/* <-- glut */
		glutMainLoop();
		/* glut --> */
	} while(0); if(e) { /* catch */
		fprintf(stderr, "Error with the %s.\n", e);
	} { /* finally */
		atexit_hack();
	}
	return e ? EXIT_FAILURE : EXIT_SUCCESS;
}

/** Checks for all OpenGL errors and prints them to stderr.
 @param function: The calling function, for prepending to stderr.
 @return Returns true if any errors, (not very useful.) */
int WindowIsGlError(const char *function) {
	GLenum err;
	int is_some_error = 0;
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
	/* <-- glut */
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
	/* glut --> */
	WindowIsGlError("WindowToggleFullScreen");
}
