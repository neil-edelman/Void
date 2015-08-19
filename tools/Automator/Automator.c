/* Copyright 2015 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt */

#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* fprintf */
#include <string.h> /* strings */
#include <dirent.h>   /* opendir readdir closedir */
/*#include <sys/stat.h>*/ /* fstat */

/** Automates calling all the graphics resources.

 @author	Neil
 @version	3.2, 2015-08
 @since		3.2, 2015-08 */

/* constants */
static const char *programme   = "Automator";
static const char *year        = "2015";
static const int versionMajor  = 1;
static const int versionMinor  = 0;

/*static const char *path  = "../../bin/"; <- okay, that is interesting */
static const char *bmp_h = "_bmp.h";
static const char *tsv_h = "_tsv.h";

/* private */
static void usage(const char *argvz);

/** private (entry point) */
int main(int argc, char **argv) {
	/*struct stat   st;*/
	struct dirent *de;
	DIR           *dir;
	char          *fn, *str, *clip;

	/* well formed */
	if(argc != 2 || *argv[1] == '-') {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	printf("/** Automated code from Automator; should be in bin/ and included in Resources.c */\n\n");
	printf("struct Image;\n\n");
	/* we'll let the
	 printf("#include \"../src/general/Map.h\"\n");
	printf("#include \"../src/general/Image.h\"\n\n");*/

	if(!(dir = opendir(argv[1]))) { perror(argv[1]); return EXIT_FAILURE; }
	while((de = readdir(dir))) {
		if(!(fn = de->d_name) || !(str = strstr(fn, bmp_h))) continue;
		str += strlen(bmp_h);
		if(*str != '\0') continue;
		/*if(stat(de->d_name, &st)) { perror(de->d_name); continue; }*/

		printf("#include \"%s\"\n", /*path,*/ fn);
	}
	if(closedir(dir)) { perror(argv[1]); }

	/* this is inefficient */
	printf("\n");

	if(!(dir = opendir(argv[1]))) { perror(argv[1]); return EXIT_FAILURE; }
	while((de = readdir(dir))) {
		if(!(fn = de->d_name) || !(str = strstr(fn, tsv_h))) continue;
		str += strlen(bmp_h);
		if(*str != '\0') continue;
		printf("#include \"%s\"\n", fn);
	}
	if(closedir(dir)) { perror(argv[1]); }

	printf("\nint AutomatorImages(struct Map *images) {\n");
	printf("\tstruct Image *img;\n\n");

	if(!(dir = opendir(argv[1]))) { perror(argv[1]); return EXIT_FAILURE; }
	while((de = readdir(dir))) {
		if(!(fn = de->d_name) || !(str = strstr(fn, bmp_h))) continue;
		clip = str;
		str += strlen(bmp_h);
		if(*str != '\0') continue;
		*clip = '\0';

		printf("\t/* %s */\n", fn);
		printf("\tif(!(img = Image(%s_bmp_width, %s_bmp_height, %s_bmp_depth, %s_bmp_bmp))\n", fn, fn, fn, fn);
		printf("\t   || !MapPut(images, %s_bmp_name, img)) {\n", fn);
		printf("\t\tImage_(&img);\n");
		printf("\t\treturn 0;\n");
		printf("\t}\n\n");
	}
	if(closedir(dir)) { perror(argv[1]); }

	printf("\treturn -1;\n");
	printf("}\n");

	return EXIT_SUCCESS;
}

/* private */

static void usage(const char *argvz) {
	fprintf(stderr, "Usage: %s <dir>\n", argvz);
	fprintf(stderr, "Version %d.%d.\n\n", versionMajor, versionMinor);
	fprintf(stderr, "For *_bmp.h, creates loading code for Void.\n");
	fprintf(stderr, "%s Copyright %s Neil Edelman\n", programme, year);
	fprintf(stderr, "This program comes with ABSOLUTELY NO WARRANTY.\n");
	fprintf(stderr, "This is free software, and you are welcome to redistribute it\n");
	fprintf(stderr, "under certain conditions; see copying.txt.\n\n");
}
