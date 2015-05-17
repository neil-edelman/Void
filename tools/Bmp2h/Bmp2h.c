/* Copyright 2014 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt

 This is a converter from bmp files to C includes

 @since 2013
 @author Neil */

#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* fprintf */
#include <string.h> /* strrchr */
#include "Bitmap.h"

/* constants */
static const char *programme   = "Bmp2h";
static const char *year        = "2014";
static const int versionMajor  = 1;
static const int versionMinor  = 1;

/* private */
static void usage(const char *argvz);

/** private (entry point) */
int main(int argc, char **argv) {
	unsigned char *data;
	char *a, *name;
	int w, width, h, height, d, depth;
	struct Bitmap *bmp;

	/* well formed */
	if(argc != 2 || *argv[1] == '-') {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	/* load bmp */
	if(!(bmp = Bitmap(argv[1]))) return EXIT_FAILURE;

	/* get c name from bmp name */
	if((name = strrchr(argv[1], '/'))) {
		name++;
	} else {
		name = argv[1];
	}
	for(a = name; *a; a++) {
		if((*a < 'A' || *a > 'Z')
		   && (*a < 'a' || *a > 'z')
		   && (*a < '0' || *a > '9')) *a = '_';
	}

	width  = BitmapGetWidth(bmp);
	height = BitmapGetHeight(bmp);
	depth  = BitmapGetDepth(bmp);
	data   = BitmapGetData(bmp);
	printf("static const char *%s_name = \"%s\";\n", name, name);
	printf("static const int %s_width  = %d;\n", name, width);
	printf("static const int %s_height = %d;\n", name, height);
	printf("static const int %s_depth  = %d;\n", name, depth);
	printf("static const unsigned char %s_bmp[] = {\n", name);
	for(h = 0; h < height; h++) {
		printf("\t/* row %d */\n", h);
		for(w = 0; w < width; w++) {
			printf("\t");
			for(d = 0; d < depth; d++) {
				printf("0x%02x", (unsigned int)*data);
				data++;
				if((h != height - 1) || (w != width - 1) || (d != depth - 1)) {
					printf(",");
				}
			}
			printf("\n");
		}
		if((h != height - 1)) printf("\n");
	}
	printf("};\n");
	Bitmap_(&bmp);
	return EXIT_SUCCESS;
}

/* private */

static void usage(const char *argvz) {
	fprintf(stderr, "Usage: %s <bmp>\n", argvz);
	fprintf(stderr, "Version %d.%d.\n\n", versionMajor, versionMinor);
	fprintf(stderr, "%s Copyright %s Neil Edelman\n", programme, year);
	fprintf(stderr, "This program comes with ABSOLUTELY NO WARRANTY.\n");
	fprintf(stderr, "This is free software, and you are welcome to redistribute it\n");
	fprintf(stderr, "under certain conditions; see copying.txt.\n\n");
}
