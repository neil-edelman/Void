/** Copyright 2000, 2013 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt.

 Entry-point. Deals with providing an interface to the libraries.

 Uses OpenGL for graphics; it's never that easy, of course. OpenGL comes in
 many different versions. Loading functions have to be queried from the library
 in a platform-specific way for all versions since {OpenGL1.0}. If GLEW is
 defined, (it should be, except MacOSX which provides native support), it loads
 the OpenGL2+ from the library using GLEW, \url{http://glew.sourceforge.net/}.
 
 @title		Window
 @author	Neil
 @version	2018-01
 @since		2015-05
 @fixme		Windowing: Tl? GTK+? */

#include <stdlib.h> /* exit */
#include <stdio.h>  /* fprintf */
#include <time.h>   /* srand(clock()) */
#include <string.h> /* strcmp */
#include <assert.h>
#include "WindowGlew.h"
#include "WindowKey.h"
#include "system/Draw.h"
#include "system/Timer.h"
#include "general/Events.h"
#include "game/Sprites.h"
#include "game/Fars.h"
#include "game/Game.h"
#include "general/OrthoMath.h" /* Vec2i */

#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif

/* constants */
static const char *programme   = "Void";
static const char *year        = "2015";

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
		"GLEW: by Milan Ikits and Marcelo Magallon\n"
		"http://glew.sourceforge.net/\n\n"
		"GLUT: by Mark Kilgard and FreeGLUT: by Pawel W. Olszta\n"
		"http://freeglut.sourceforge.net/\n\n"
		"SDL2:\nhttps://www.libsdl.org/\n\n");
}

/** Load {OpenGL2+} from the library under {-D GLEW}.
 @return True if success. */
static int glew(void) {
#ifdef GLEW
	GLenum err;
	if((err = glewInit()) != GLEW_OK)
		return fprintf(stderr, "Glew: %s", glewGetErrorString(err)), 0;
	if(!glewIsSupported("GL_VERSION_2_0") /* !GLEW_VERSION_2_0 ? */)
		return fprintf(stderr,
		"Glew: OpenGL 2.0+ shaders are not supported.\n"), 0;
	fprintf(stderr, "Glew: GLEW %s extension loading library ready for "
		"OpenGL2+.\n", glewGetString(GLEW_VERSION));
#endif
	return 1;
}

/** Destructor (called with atexit().)
 @allow */
static void main_(void) {
	TimerPause();
	Game_();
	Draw_();
	Fars_();
	Sprites_();
	Events_();
}

/** Entry point.
 @param argc the number of arguments starting with the programme name
 @param argv the arguments
 @return     either EXIT_SUCCESS or EXIT_FAILURE */
int main(int argc, char **argv) {
	/* @fixme More options (ie, load game, etc.) */
	if(argc > 1) return usage(), EXIT_SUCCESS;
	/* we generally don't have return because glutMainLoop() never does */
	if(atexit(&main_)) perror("atexit");
	/* entropy increase */
	srand((unsigned)clock());
	/* start up subsystems; window has to be first; timer ms */
	if(!Window(programme, argc, argv)
		/* Load OpenGl2+ after the window. */
		|| !glew()
		|| !Key()
		|| !Events()
		|| !Sprites()
		|| !Fars()
		|| !Draw()
		|| !Game()) return EXIT_FAILURE;
	/* hand over control to the grahics library */
	TimerRun();
	WindowGo();
	/* never get here */
	return EXIT_FAILURE;
}

#ifdef GLUT_API_VERSION /* <-- GLUT */

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
	assert(title);
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
	glutCreateWindow(title);
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
	window.is_started = 1;
	return 1;
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

#if 0
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
#endif


static struct Key {
	int state;
	int down;
	int integral;
	int time;
	void (*handler)(void);
} keys[KEY_MAX];

/* private prototypes */
static enum Keys glut_to_keys(const int k);
static void key_down(unsigned char k, int x, int y);
static void key_up(unsigned char k, int x, int y);
static void key_down_special(int k, int x, int y);
static void key_up_special(int k, int x, int y);

/** Attach the static keys to the Window depending on whether the Timer is
 active (poll) or not (direct to functions.)
 @return Success? */
int Key(void) {
	glutKeyboardFunc(&key_down);
	glutKeyboardUpFunc(&key_up);
	glutSpecialFunc(&key_down_special);
	glutSpecialUpFunc(&key_up_special); 
	fprintf(stderr, "Key: handlers set-up.\n");
	return -1;
}

/** Registers a function to call asynchronously on press. */
void KeyRegister(const unsigned k, void (*const handler)(void)) {
	if(k >= KEY_MAX) return;
	keys[k].handler = handler;
}

/** Polls how long the key has been pressed, without repeat rate. Destructive.
 @param key		The key.
 @return		Number of ms. */
int KeyTime(const int key) {
	int time;
	struct Key *k;
	
	if(key < 0 || key >= KEY_MAX) return 0;
	k = &keys[key];
	if(k->state) {
		int ct  = glutGet(GLUT_ELAPSED_TIME);/*TimerLastTime();<-too granular?*/
		time    = ct - k->down + k->integral;
		k->down = ct;
	} else {
		time    = k->integral;
	}
	k->integral = 0;
	return time;
}

/** GLUT_ to internal keys.
 @param k	Special key in OpenGl.
 @return	Keys. */
static enum Keys glut_to_keys(const int k) {
	switch(k) {
		case GLUT_KEY_LEFT:   return k_left;
		case GLUT_KEY_UP:     return k_up;
		case GLUT_KEY_RIGHT:  return k_right;
		case GLUT_KEY_DOWN:   return k_down;
		case GLUT_KEY_PAGE_UP:return k_pgup;
		case GLUT_KEY_PAGE_DOWN: return k_pgdn;
		case GLUT_KEY_HOME:   return k_home;
		case GLUT_KEY_END:    return k_end;
		case GLUT_KEY_INSERT: return k_ins;
		case GLUT_KEY_F1:     return k_f1;
		case GLUT_KEY_F2:     return k_f2;
		case GLUT_KEY_F3:     return k_f3;
		case GLUT_KEY_F4:     return k_f4;
		case GLUT_KEY_F5:     return k_f5;
		case GLUT_KEY_F6:     return k_f6;
		case GLUT_KEY_F7:     return k_f7;
		case GLUT_KEY_F8:     return k_f8;
		case GLUT_KEY_F9:     return k_f9;
		case GLUT_KEY_F10:    return k_f10;
		case GLUT_KEY_F11:    return k_f11;
		case GLUT_KEY_F12:    return k_f12;
		default: return k_unknown;
	}
}

/** Callback for {glutKeyboardFunc}. */
static void key_down(unsigned char k, int x, int y) {
	struct Key *key = &keys[k];
	if(key->state) return;
	key->state = -1;
	key->down  = glutGet(GLUT_ELAPSED_TIME);
	if(key->handler) key->handler();
	/* fprintf(stderr, "key_down: key %d hit at %d ms.\n", k, key->down);*/
	UNUSED(x), UNUSED(y);
}

/** Callback for {glutKeyboardUpFunc}. */
static void key_up(unsigned char k, int x, int y) {
	struct Key *key = &keys[k];
	if(!key->state) return;
	key->state = 0;
	key->integral += glutGet(GLUT_ELAPSED_TIME) - key->down;
	/* fprintf(stderr, "key_up: key %d pressed %d ms at end of frame.\n",k,key->integral);*/
	UNUSED(x), UNUSED(y);
}

/** Callback for {glutSpecialFunc}. */
static void key_down_special(int k, int x, int y) {
	struct Key *key = &keys[glut_to_keys(k)];
	if(key->state) return;
	key->state  = -1;
	key->down = glutGet(GLUT_ELAPSED_TIME);
	if(key->handler) key->handler();
	/* fprintf(stderr, "key_down_special: key %d hit at %d ms.\n", k, key->down);*/
	UNUSED(x), UNUSED(y);
}

/** Callback for {glutSpecialUpFunc}. */
static void key_up_special(int k, int x, int y) {
	struct Key *key = &keys[glut_to_keys(k)];
	if(!key->state) return;
	key->state = 0;
	key->integral += glutGet(GLUT_ELAPSED_TIME) - key->down;
	/* fprintf(stderr, "key_up_special: key %d pressed %d ms at end of frame.\n", k,
			 key->integral);*/
	UNUSED(x), UNUSED(y);
}

#endif
