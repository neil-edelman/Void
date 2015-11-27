/* Copyright 2000, 2013 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt */

#include <stdlib.h> /* exit */
#include <stdio.h>  /* fprintf */
#include <time.h>   /* srand(clock()) */
#include <string.h> /* strcmp */
#include "EntryPosix.h"
#include "system/Window.h"
#include "system/Draw.h"
#include "system/Timer.h"
#include "system/Key.h"
#include "game/Game.h"

/** Entry point for command-line, unix-like operating systems (ie, all of them.)
 <p>
 Fixme: have separate entry points for MacOS and Windows that are user friendly.
 @author	Neil
 @version	3.2, 2015-05
 @since		3.2, 2015-05 */

/* constants */
static const char *programme   = "Void";
static const char *year        = "2015";
static const int versionMajor  = 3;
static const int versionMinor  = 2;

struct Entry {
	int i;
} entry;

static void main_(void);
static void usage(void);

/** Debug level message. */
int Debug(void) { return -1; }

/** Dubug level pedantic. */
int Pedantic(void) { return 0; }

/** entry point
 @param argc the number of arguments starting with the programme name
 @param argv the arguments
 @return     either EXIT_SUCCESS or EXIT_FAILURE */
int main(int argc, char **argv) {
	/*struct Bitmap *bmp;*/
	const int framelenght_ms = TimerGetFramelength();
	int number_of_objects = 1648;

	/* fixme: more options! (ie, load game, etc) */
	if(argc > 1) {
		usage();
		return EXIT_SUCCESS;
	}
	fprintf(stderr, "main: base number of objects: %d.\n", number_of_objects);

	/* we generally don't have return because glutMainLoop() never does */
	if(atexit(&main_)) perror("atexit");

	/* start up subsystems; window has to be first; timer ms */
	if(!Window(programme, argc, argv)
		|| !Key()
		|| !Draw()
	    || !Game()
		|| !Timer(framelenght_ms)) return EXIT_FAILURE;
	/* entropy increase */
	srand((unsigned)clock());

	/* hand over control to the grahics library */
	WindowGo();

	/* never get here */
	return EXIT_FAILURE;
}

/** Destructor (called with atexit().) */
static void main_(void) {
	Timer_();
	Game_();
	Draw_();
}

/** Help screen. */
static void usage(void) {
	fprintf(stderr, "Usage: %s\n", programme);
	fprintf(stderr, "To win, blow up everything that's not you.\n");
	fprintf(stderr, "Player one controls: left, right, up, down, space\n");
	/* fprintf(stderr, "Player two controls: a, d, w, s, space.\n"); */
	fprintf(stderr, "Fullscreen: F1.\n");
	fprintf(stderr, "Exit: Escape.\n\n");
	fprintf(stderr, "Version %d.%d.\n\n", versionMajor, versionMinor);
	fprintf(stderr, "%s Copyright %s Neil Edelman\n", programme, year);
	fprintf(stderr, "This program comes with ABSOLUTELY NO WARRANTY.\n");
	fprintf(stderr, "This is free software, and you are welcome to redistribute it\n");
	fprintf(stderr, "under certain conditions; see copying.txt.\n\n");
	fprintf(stderr, "Image credit: NASA; JPL; ESA; Caltech; UCLA; MPS; DLR; IDA; Johns\nHopkins University APL; Carnegie Institution of Washington; DSS\nConsortium; SDSS.\n\n");
	fprintf(stderr, "lodepng: Copyright (c) 2005-2015 Lode Vandevenne\n");
	fprintf(stderr, "http://lodev.org/lodepng/\n\n");
	fprintf(stderr, "nanojpeg: by Martin J. Fiedler\n");
	fprintf(stderr, "http://keyj.emphy.de/nanojpeg/\n\n");
}
