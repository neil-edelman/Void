/* Copyright 2015 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt */

#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* fprintf */
#include <string.h> /* strncpy, strrchr */
#include <ctype.h>  /* toupper */
#include <unistd.h> /* chdir (POSIX, not ANSI) */
#include <dirent.h> /* opendir readdir closedir */
#define SUFFIX
#include "Functions.h"
#include "Error.h"
#include "Reader.h"
#include "Record.h"
#include "Lore.h"

/** This is a crude static database that preprocesses all the media files and
 turns them into C header files for inclusion in Void.
 <p>
 {\code strsep} is not ansi, you may need to rewrite it with strtok.

 @author	Neil
 @version	3.2, 2015-08
 @since		3.2, 2015-08 */

/* private prototypes */
static void main_(void);
static void usage(const char *argvz);

/* constants */
static const char *programme   = "Media2h";
static const char *year        = "2015";
static const int versionMajor  = 1;
static const int versionMinor  = 0;

static const char *ext_type    = ".type";
static const char *ext_lore    = ".lore";

/** Entry point. */
int main(int argc, char **argv) {
	struct dirent *de;
	DIR           *dir;
	char          fn[1024];
	const int     max_fn = sizeof(fn) / sizeof(char) - 1;
	int           is_output_type;

	/* check that the user specified file and give meaningful names to args */
	if(argc == 2 && *argv[1] != '-') {
		is_output_type = -1;
	} else if(argc == 3) {
		is_output_type = 0;
	} else {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	/* load the record types in directory argv[1] */
	if(!(dir = opendir(argv[1]))) { perror(argv[1]); return EXIT_FAILURE; }
	while((de = readdir(dir))) {
		if(!suffix(de->d_name, ext_type)) continue;
		if(snprintf(fn, sizeof(fn) / sizeof(char), "%s/%s", argv[1], de->d_name) > max_fn) {
			fprintf(stderr, "%s: file name buffer maxed out at %u characters.\n", fn, max_fn);
			continue;
		}
		fprintf(stderr, "Loading record types from \"%s.\"\n", fn);
		if(!Record(fn)) { main_(); return EXIT_FAILURE; }
	}
	if(closedir(dir)) { perror(argv[1]); }

	/* now that we've loaded it, convert to .h if we don't have any more args */
	if(is_output_type) {
		printf("/** auto-generated from %s/ by %s %d.%d %s */\n\n",
			   argv[1], programme, versionMajor, versionMinor, year);
		RecordOutput();
		main_();
		return EXIT_SUCCESS;
	}

	/* deeper; we've loaded the types (.type), now we need to load the
	 resources (.lore) */
	if(!(dir = opendir(argv[1]))) { perror(argv[1]); return EXIT_FAILURE; }
	while((de = readdir(dir))) {
		if(!suffix(de->d_name, ext_lore)) continue;
		if(snprintf(fn, sizeof(fn) / sizeof(char), "%s/%s", argv[1], de->d_name) > max_fn) {
			fprintf(stderr, "%s: file name buffer maxed out at %u characters.\n", fn, max_fn);
			continue;
		}
		fprintf(stderr, "Loading lores from \"%s.\"\n", fn);
		if(!Lore(fn)) { main_(); return EXIT_FAILURE; }
	}
	if(closedir(dir)) { perror(argv[1]); }

	printf("/** auto-generated from %s/ by %s %d.%d %s */\n\n",
		   argv[1], programme, versionMajor, versionMinor, year);
	LoreOutput();
	main_();

	return EXIT_SUCCESS;
}

static void main_(void) {
	Lore_();
	Record_();
}

static void usage(const char *argvz) {
	fprintf(stderr, "Usage: %s <types directory> [<resources directory>]\n", argvz);
	fprintf(stderr, "This is a crude static database that preprocesses all the media files and\n");
	fprintf(stderr, "turns them into C header files for inclusion in Void. If <resources\ndirectory> is specified, it outputs resources, otherwise, types.\n");
	fprintf(stderr, "Version %d.%d.\n\n", versionMajor, versionMinor);
	fprintf(stderr, "%s Copyright %s Neil Edelman\n", programme, year);
	fprintf(stderr, "This program comes with ABSOLUTELY NO WARRANTY.\n");
	fprintf(stderr, "This is free software, and you are welcome to redistribute it\n");
	fprintf(stderr, "under certain conditions; see copying.txt.\n\n");
}
