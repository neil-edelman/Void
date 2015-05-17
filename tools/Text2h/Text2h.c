/* Copyright 2014 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt

 converts a text file (without " and any other tricks) into a C header

 @since 2014
 @author Neil */

#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* fprintf */
#include <string.h> /* strncpy, strrchr */

/* constants */
static const char *programme   = "Text2h";
static const char *year        = "2014";
static const int versionMajor  = 1;
static const int versionMinor  = 1;

/* private */
static void usage(const char *argvz);

/** private (entry point)
 @param which file is turned into *.h
 @bug do not include \", it will break
 @bug the file name should be < 1024 */
int main(int argc, char **argv) {
	char name[1024], *a, *base;
	char fn[1024], read[1024];
	int len;
	FILE *fp;
	
	/* check that the user specified file */
	if(argc != 2 || *argv[1] == '-') {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	/* extract the user-specifed filename; change into a c varname */
	if((base = strrchr(argv[1], '/'))) {
		base++;
	} else {
		base = argv[1];
	}
	strncpy(name, base, sizeof(name) / sizeof(char));
	name[sizeof(name) / sizeof(char) - 1] = '\0';
	for(a = name; *a; a++) {
		if((*a < 'A' || *a > 'Z')
		   && (*a < 'a' || *a > 'z')
		   && (*a < '0' || *a > '9')) *a = '_';
	}

	/* print output */
	printf("/** auto-generated from %s by %s %d.%d */\n\n",
		   base, programme, versionMajor, versionMinor);
	/*printf("static const char *%s_name = \"%s\";\n", name, name);*/
	printf("static const char *%s      =\n\"", name);
	if(!(fp = fopen(argv[1], "r"))) {
		perror(fn);
	} else {
		while(fgets(read, sizeof(read) / sizeof(char), fp)) {
			if(!(len = strlen(read))) break;
			if(read[len - 1] == '\n') {
				read[len - 1] = '\0';
				printf("%s\\n\"\n\"", read);
			} else {
				printf("%s", read);
			}
		}
	}
	printf("\";\n");
	
	return EXIT_SUCCESS;
}

/* private */

static void usage(const char *argvz) {
	fprintf(stderr, "Usage: %s <text>\n", argvz);
	fprintf(stderr, "Converts <text> into a C .h and outputs it.\nDo not include files with \"; it isn't that complex.\n");
	fprintf(stderr, "Version %d.%d.\n\n", versionMajor, versionMinor);
	fprintf(stderr, "%s Copyright %s Neil Edelman\n", programme, year);
	fprintf(stderr, "This program comes with ABSOLUTELY NO WARRANTY.\n");
	fprintf(stderr, "This is free software, and you are welcome to redistribute it\n");
	fprintf(stderr, "under certain conditions; see copying.txt.\n\n");
}
