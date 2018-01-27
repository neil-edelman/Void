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

/* typedef Uint32 (SDLCALL * SDL_TimerCallback) (Uint32 interval, void *param); */

#include <stdlib.h> /* exit */
#include <stdio.h>  /* fprintf */
#include <time.h>   /* srand(clock()) */
#include <string.h> /* strcmp */
#include <assert.h>

/* GLEW is set ourselves in the Makefile; GLEW allows OpenGL2+ on machines
 where you need to query the library. Glew will include {win.h} in Windows,
 which has a bajillon warnings that are not the slightest bit useful. Before
 {WindowGl.h}. */
#ifdef GLEW /* <-- glew */
#pragma warning(push, 0)
#define GL_GLEXT_PROTOTYPES /* *duh* */
#define GLEW_STATIC         /* (of course) */
/* http://glew.sourceforge.net/
 add include directories eg ...\glew-1.9.0\include */
#include <GL/glew.h>
#pragma warning(pop)
#endif /* glew --> */

#include "WindowGl.h"
#include "WindowKey.h"
#include "Window.h"
#include "system/Draw.h"
#include "system/Timer.h"
#include "general/Events.h"
#include "general/OrthoMath.h" /* Vec2i */
#include "game/Sprites.h"
#include "game/Fars.h"
#include "game/Game.h"

#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif

/* constants */
static const char *programme   = "Void";
static const char *year        = "2015";

static struct Window {
#ifdef GLUT /* <-- glut */
	struct Vec2i size, position;
#elif defined(SDL) /* glut --><-- sdl */
	SDL_Window *sdl_window;
#else /* sdl --><-- nothing */
#error Define GLUT or SDL.
#endif /* nothing --> */
	int is_full;
	time_t last_error;
	unsigned no_errors;
} window;

static const unsigned max_gl_fails = 64;
static const double forget_errors = 10.0; /* s */
static const int warn_texture_size = 1024;

/** Gets the window started. There is no destructor.
 @param title The title of the window (can be null.)
 @param argc, argv: {main} program arguments; passed to glutInit().
 @return Whether the graphics library is ready and the window is started. */
static int Window(const char *title, int argc, char **argv) {
	GLint max_tex;
	assert(title);
	/* We keep track of how many errors per time; too many, we exit. */
	if(time(&window.last_error) == (time_t)-1)
		return fprintf(stderr, "Window: time function not working.\n"), 0;
#ifdef GLUT /* <-- glut */
	fprintf(stderr, "Window: GLUT initialising.\n");
	window.size.x = 600, window.size.y = 400;
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE);
	glutInitWindowSize(window.size.x, window.size.y);
	glutCreateWindow(title);
#elif defined(SDL) /* glut --><-- sdl */
	UNUSED(argc && argv);
	{
		SDL_version linked;
		SDL_GetVersion(&linked);
		fprintf(stderr, "Window: SDL version %d.%d.%d.\n",
			linked.major, linked.minor, linked.patch);
	}
	SDL_SetMainReady(); /* Doesn't do anything on SDL2? why is it there? */
	if(SDL_Init(SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_VIDEO /*@fixme?*/))
		return fprintf(stderr, "SDL failed: %s.\n", SDL_GetError()), 0;
#else /* sdl --><-- nothing */
#error Define GLUT or SDL.
#endif /* nothing --> */
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
	return 1;
}

/** Begins animation with the hooks that have been placed in the window. Does
 not return if GLEW. */
static void WindowGo(void) {
#ifdef GLUT /* <-- glut */
	glutMainLoop();
#elif defined(SDL) /* glut --><-- sdl */
#else /* sdl --><-- nothing */
#error Define GLUT or SDL.
#endif /* nothing --> */
}

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
#ifdef SDL
	SDL_Quit();
#endif
}

/** Entry point.
 @param argc the number of arguments starting with the programme name
 @param argv the arguments
 @return     either EXIT_SUCCESS or EXIT_FAILURE */
int main(int argc, char **argv) {
	/* @fixme More options (ie, load game, etc.) */
	if(argc > 1) return usage(), EXIT_SUCCESS;
#ifdef GLUT /* <-- glut */
	/* we generally don't have return because glutMainLoop() never does */
	if(atexit(&main_)) perror("atexit");
#endif /* glut --> */
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
	/* hand over control to the graphics library */
	TimerRun();
	WindowGo();
	/* If GLUT, never get here. */
	main_();
	return EXIT_FAILURE;
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
#ifdef GLUT /* <-- glut */
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
#elif defined(SDL) /* glut --><-- sdl */
	assert(window.sdl_window);
	if(SDL_SetWindowFullscreen(window.sdl_window,
		window.is_full ? SDL_WINDOW_FULLSCREEN : 0))
		{ fprintf(stderr, "SDL Error: %s.\n", SDL_GetError()); return; }
	window.is_full = ~window.is_full;
#else /* sdl --><-- nothing */
#error Define GLUT or SDL.
#endif /* nothing --> */
	WindowIsGlError("WindowToggleFullScreen");
}
