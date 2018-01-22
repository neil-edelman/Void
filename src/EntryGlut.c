/** Copyright 2000, 2013 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt.

 Entry point for command-line, unix-like operating systems (ie, all of them.)

 @title		EntryGlut
 @author	Neil
 @version	3.2, 2015-05
 @since		3.2, 2015-05
 @fixme		Tl? */

#include <stdlib.h> /* exit */
#include <stdio.h>  /* fprintf */
#include <time.h>   /* srand(clock()) */
#include <string.h> /* strcmp */
#include "EntryGlut.h"
#include "system/Window.h"
#include "system/Draw.h"
#include "system/Timer.h"
#include "system/Key.h"
#include "general/Events.h"
#include "game/Sprites.h"
#include "game/Fars.h"
#include "game/Game.h"

/* constants */
static const char *programme   = "Void";
static const char *year        = "2015";
static const int versionMajor  = 3;
static const int versionMinor  = 3;

static void main_(void);
static void usage(void);

/** Entry point.
 @param argc the number of arguments starting with the programme name
 @param argv the arguments
 @return     either EXIT_SUCCESS or EXIT_FAILURE */
int main(int argc, char **argv) {
	/* fixme: more options! (ie, load game, etc) */
	if(argc > 1) {
		usage();
		return EXIT_SUCCESS;
	}
	/* we generally don't have return because glutMainLoop() never does */
	if(atexit(&main_)) perror("atexit");
	/* entropy increase */
	srand((unsigned)clock());
	/* start up subsystems; window has to be first; timer ms */
	if(!Window(programme, argc, argv)
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

/** Destructor (called with atexit().)
 @allow */
static void main_(void) {
	TimerPause();
	Game_();
	Draw_();
	Fars_();
	Sprites_();
	Events_();
	/* There is no Key_() or Window_(). */
}

/** Help screen. */
static void usage(void) {
	fprintf(stderr, "Usage: %s\n"
		"To win, blow up everything that's not you.\n"
		"Player one controls: left, right, up, down, space\n"
		"Fullscreen: F1.\n"
		"Exit: Escape.\n\n"
		"Version %d.%d.\n\n"
		"%s Copyright %s Neil Edelman\n"
		"This program comes with ABSOLUTELY NO WARRANTY.\n"
		"This is free software, and you are welcome to redistribute it\n" 
		"under certain conditions; see copying.txt.\n\n",
		programme, versionMajor, versionMinor, programme, year);
	fprintf(stderr, "Image credit: NASA; JPL; ESA; Caltech; UCLA; MPS; DLR;\n"
		"IDA; Johns Hopkins University APL; Carnegie Institution of\n"
		"Washington; DSS Consortium; SDSS.\n\n"
		"lodepng: Copyright (c) 2005-2015 Lode Vandevenne\n"
		"http://lodev.org/lodepng/\n\n"
		"nanojpeg: by Martin J. Fiedler\n"
		"http://keyj.emphy.de/nanojpeg/\n\n");
}
