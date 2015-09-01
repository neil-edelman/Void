/* Copyright 2015 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt

 This is a converter from (binary) files to C includes.
 <p>
 Eg, "File2h file.png > file.h," converts the file.png into a C,
 static const file_png that you can access by including file.h.

 @author	Neil
 @version	1.0, 2015-08
 @since		1.0, 2015-08 */

#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* fprintf */
#include <string.h> /* strrchr */

/* constants */
static const char *programme   = "File2h";
static const char *year        = "2015";
static const int versionMajor  = 1;
static const int versionMinor  = 0;

static const int no_per_line = 16/*12*/;

/* private */
static void usage(const char *argvz);

/** private (entry point) */
int main(int argc, char **argv) {
	FILE *fp;
	unsigned char read[1024], *byte;
	char *name, *a;
	size_t read_size = sizeof(read) / sizeof(unsigned char), no;
	int ret = EXIT_SUCCESS;
	int no_line = 0;
	int is_first = -1;

	/* well formed */
	if(argc != 2 || *argv[1] == '-') {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	/* get c name from file name */
	if((name = strrchr(argv[1], '/'))) {
		name++;
	} else {
		name = argv[1];
	}
	if(!(name = strdup(name))) {
		perror(argv[1]);
		return EXIT_FAILURE;
	};
	for(a = name; *a; a++) {
		if((*a < 'A' || *a > 'Z')
		   && (*a < 'a' || *a > 'z')
		   && ((*a < '0' || *a > '9') && a != name)) *a = '_';
	}

	/* open the file */
	if(!(fp = fopen(argv[1], "r"))) {
		perror(argv[1]);
		free(name);
		return EXIT_FAILURE;
	}

	printf("static const unsigned char %s[] = {\n", name);

	free(name);

	/*printf("\t");*/
	while((no = fread(read, sizeof(unsigned char), read_size, fp))) {
		for(byte = read; byte < read + no; byte++) {
			if(is_first) {
				is_first = 0;
			} else {
				printf(",");
			}
			if(no_line >= no_per_line) {
				printf("\n"/*"\n\t"*/);
				no_line = 0;
			}
			printf("0x%02x", (unsigned int)*byte);
			no_line++;
		}
		if(no < read_size) break;
	}
	if(ferror(fp)) {
		perror(argv[1]);
		ret = EXIT_FAILURE;
	}
	printf("\n};\n");

	if(fclose(fp)) perror(argv[1]);

	return EXIT_SUCCESS;
}

/* private */

static void usage(const char *argvz) {
	fprintf(stderr, "Usage: %s <file>\n", argvz);
	fprintf(stderr, "Version %d.%d.\n\n", versionMajor, versionMinor);
	fprintf(stderr, "This is a converter from (binary) files to C includes.\n\n");
	fprintf(stderr, "Eg, \"File2h file.png > file.h,\" converts the file.png into a C,\n");
	fprintf(stderr, "static const file_png that you can access by including file.h.\n\n");
	fprintf(stderr, "%s Copyright %s Neil Edelman\n", programme, year);
	fprintf(stderr, "This program comes with ABSOLUTELY NO WARRANTY.\n");
	fprintf(stderr, "This is free software, and you are welcome to redistribute it\n");
	fprintf(stderr, "under certain conditions; see copying.txt.\n\n");
}
