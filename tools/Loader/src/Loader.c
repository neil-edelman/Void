/* Copyright 2015 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt */

#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* fprintf */
#include <string.h> /* strncpy, strrchr */
#include <ctype.h>  /* toupper */
#include <unistd.h> /* chdir (POSIX, not ANSI) */
#include <dirent.h> /* opendir readdir closedir */

/* include code to load images for dimensions */
#include "../../../src/format/Bitmap.h"
#include "../../../src/format/lodepng.h"
#include "../../../src/format/nanojpeg.h"

#define SUFFIX
#define STRCOPY
#define GROW
#define TO_NAME
#include "Functions.h"

#include "Error.h"
#include "Reader.h"
#include "Record.h"
#include "Lore.h"
#include "Loader.h"

/** This is a crude static database that preprocesses all the media files and
 turns them into C header files for inclusion in Void.
 <p>
 {\code strsep} is not ansi, you may need to rewrite it with strtok.

 @author	Neil
 @version	1.0, 2015-08
 @since		1.0, 2015-08 */

struct ImageName;

/* private prototypes */
static void main_(void);
static int read_directory(const char *const directory, int (*exec)(const char *const), const char *const extension);
static void usage(const char *argvz);
static int new_image(const char *const fn);
static void sort(void);
static int include_images(void);
static int print_images(const char *const dir);
static int string_image_comp(const char **key_ptr, const struct ImageName *elem);

/* variables */
/*static char *image_names[1024];
static const int image_names_size = sizeof(image_names) / sizeof(char *);*/
static struct ImageName {
	char name[1024];
} *image_names;
static const int max_image_names_name = sizeof((struct ImageName *)0)->name / sizeof(char);
static int no_image_names, image_names_capacity;
static int is_sorted = -1;

/* constants */
static const char *programme   = "Loader";
static const char *year        = "2015";
static const int versionMajor  = 1;
static const int versionMinor  = 0;

static const char *ext_type    = ".type";
static const char *ext_lore    = ".lore";

static const char *ext_png_h   = ".png";
static const char *ext_jpeg_h  = ".jpeg";
static const char *ext_bmp_h   = ".bmp";

/** If you add an image, the position probably won't be valid, so to this after
 you've added all the images.
 @return	The {@code str} entry of the image table or -1 if it couldn't be
			found. */
int LoaderImagePosition(const char *const str) {
	struct ImageName *in;
	if(!is_sorted) sort();
	if(!(in = bsearch(&str, image_names, no_image_names,
					  sizeof(struct ImageName),
					  (int (*)(const void *, const void *))&string_image_comp)))
		return -1;
	/* printf("*********%s: %s\n", str, in->name); */
	return in - image_names;
}

/** Entry point. */
int main(int argc, char **argv) {
	char *types_dir = 0, *lores_dir = 0; /* can have '/' or not! */

	/* check that the user specified dir or two */
	if(argc <= 1 || argc >= 4 || *argv[1] == '-') {
		usage(argv[0]);
		return EXIT_FAILURE;
	}
	types_dir = argv[1];
	if(argc == 3) lores_dir = argv[2];

	/* load the types, whatever comes next */
	if(!read_directory(types_dir, &Record, ext_type)) {
		main_();
		return EXIT_FAILURE;
	}

	/* convert to .h if we have one arg */
	if(!lores_dir) {
		printf("/** auto-generated from %s%s by %s %d.%d %s */\n\n",
			   types_dir, types_dir[strlen(argv[1]) - 1] != '/' ? "/" : "",
			   programme, versionMajor, versionMinor, year);
		printf("#include <stddef.h> /* size_t */\n\n");
		printf("enum ImageFormat { IF_UNKNOWN, IF_PNG, IF_JPEG, IF_BMP };\n\n");
		printf("/* image is a base datatype; it's not in c; we need this */\n");
		printf("struct Image {\n");
		printf("\tconst char *name;\n");
		printf("\tconst enum ImageFormat data_format;\n");
		printf("\tconst size_t           data_size;\n");
		printf("\tconst unsigned char    *data;\n");
		printf("\tconst unsigned         width;\n");
		printf("\tconst unsigned         height;\n");
		printf("\tconst unsigned         depth;\n");
		printf("\tunsigned               texture;\n");
		printf("};\n\n");

		/* debug thing */
		printf("void image_print(const struct Image *image);\n\n");

		printf("/* these are datatypes that are loaded from %s%s */\n\n", types_dir, types_dir[strlen(argv[1]) - 1] != '/' ? "/" : "");
		/*printf("void ImageSetTexture(struct Image *image, const int tex) { if(!image) return; image->texture = tex; }\n\n");*/
		RecordOutput();
		main_();
		return EXIT_SUCCESS;
	}

	/* now we need to load the resources (.lore) because we have lores_dir */
	if(!read_directory(lores_dir, &Lore, ext_lore))
		{ main_(); return EXIT_FAILURE; }

	/* build up a list of the raw image files */
	if(   !read_directory(lores_dir, &new_image, ext_png_h)
	   || !read_directory(lores_dir, &new_image, ext_jpeg_h)
	   || !read_directory(lores_dir, &new_image, ext_bmp_h))
		{ main_(); return EXIT_FAILURE; }

	printf("/** auto-generated from %s%s by %s %d.%d %s */\n\n",
	       lores_dir, lores_dir[strlen(argv[2]) - 1] != '/' ? "/" : "",
	       programme, versionMajor, versionMinor, year);

	printf("#include <stdio.h> /* fprintf */\n");
	printf("#include \"Lore.h\"; /* or whatever you ./Loader dir/ > Lore.h */\n\n");

	/* debug! */
	printf("struct Image;\n\n");
	printf("/** Prints out the red channel in text format for debugging purposes; please\n");
	printf("don't call it on large images!\n");
	printf(" @param img\t\tThe image.\n");
	printf(" @param fp\t\tFile pointer where you want the image to go; eg, stdout. */\n");
	printf("void image_print(const struct Image *image) {\n");
	printf("\tint x, y;\n\n");
	printf("\tif(!image) { fprintf(stderr, \"0\\n\"); return; }\n");
	printf("\tfor(y = 0; y < image->height; y++) {\n");
	printf("\t\tfor(x = 0; x < image->width; x++) {\n");
	printf("\t\t\t/* red */\n");
	printf("\t\t\tfprintf(stderr, \"%%1.1d\", image->data[(y * image->width + x) * image->depth] >> 5);\n");
	printf("\t\t}\n");
	printf("\t\tfprintf(stderr, \"\\n\");\n");
	printf("\t}\n");
	printf("}\n\n");

	/* #include all images; it assumes that they've been converted by File2h */
	include_images();

	/* include info about the images */
	if(!print_images(lores_dir)) { main_(); return EXIT_FAILURE; }

	/* finally, the data */
	LoreOutput();

#if 0 /* fixme: later, when it's perfected */
	/* oh, let's put this here, too? */
	printf("#include <stdio.h> /* fprintf */\n\n");
	printf("/** creates a new raw RBG[A] decompressed image which you can pass to OpenGl,\n");
	printf(" or returns zero. Free with {@code free()}. */");
	printf("unsigned char *decode(const struct Image *image) {\n");
	printf("\tsw");
	switch(image->format) {
		case IF_PNG:
			{
				unsigned error;
				if((error = lodepng_decode32(&data, &width, &height, image->data, sizeof image->data))) {
					fprintf(stderr, "lodepng error %u: %s\n", error, lodepng_error_text(error));
					return 0;
				}
			}
			break;
		case IF_JPEG:
			enum { E_NO_ERR = 0, E_PERROR, E_READ, E_DECODE } err = E_NO_ERR;
			FILE          *fp = 0;
			size_t        size;
			unsigned char *buffer = 0;
			do { /* try */
				/* nanojpeg doesn't do io */
				if(!(fp = fopen(pn, "r"))) { err = E_PERROR; break; }
				fseek(fp, 0, SEEK_END);
				size = ftell(fp);
				fseek(fp, 0, SEEK_SET);
				rewind(fp);
				if(!(buffer = malloc(size))) { err = E_PERROR; break; }
				if(fread(buffer, sizeof(char), size, fp) != size)
				{ err = E_READ; break; }
				/* decode */
				if(njDecode(buffer, size) || !njIsColor())
				{ err = E_DECODE; break; }
				width  = njGetWidth();
				height = njGetHeight();
				depth  = 3; /* colour always, but no alpha */
			} while(0); { /* finally */
				njDone();
				free(buffer);
				fclose(fp);
			} if(err) { /* catch */
				if(err == E_PERROR) perror(pn);
				else fprintf(stderr, "%s: decoding failed.\n", pn);
				return 0;
			}
		case IF_BMP:
			struct Bitmap *bmp;
			if(!(bmp = BitmapFile(pn))) return 0;
			width  = BitmapGetWidth(bmp);
			height = BitmapGetHeight(bmp);
			depth  = BitmapGetDepth(bmp);
			Bitmap_(&bmp);
		case IF_UNKNOWN:
		default:
			fprintf(stderr, "Unknown image format.\n");
			return 0;
	}
	
	printf("\t{ \"%s\", %s, %s, %u, %u, %u, 0 }%s", fn, type, to_name(fn), width, height, depth, i != no_image_names - 1 ? ",\n" : "\n");

	
	
	printf("}\n");
	printf("/* fixme: fix vertical chirality */\n");
#endif

	main_();
	return EXIT_SUCCESS;

}

static void main_(void) {
	Lore_();
	Record_();
	free(image_names);
}

/* Reads all files that have {@code extension} in {@code directory} and does
 {@code exec} on the filenames; if {@code exec} returns false, than the whole
 process stops and this will return false.
 @return	True on success. */
static int read_directory(const char *const directory, int (*exec)(const char *const), const char *const extension) {
	static char   pn[1024];
	const int     max_pn = sizeof(pn) / sizeof(char);
	DIR           *dir;
	struct dirent *de;

	if(!directory || !exec || !extension) return 0;

	if(!(dir = opendir(directory))) { perror(directory); return 0; }
	while((de = readdir(dir))) {
		if(!suffix(de->d_name, extension)) continue;
		if(snprintf(pn, sizeof(pn) / sizeof(char), "%s%s%s", directory,
					directory[strlen(directory) - 1] != '/' ? "/" : "",
					de->d_name) + 1 > max_pn) {
			fprintf(stderr, "%s: path name buffer can't hold %u characters.\n", pn, max_pn);
			continue;
		}
		/*fprintf(stderr, "Matched \"%s.\"\n", fn);*/
		if(!exec(pn)) return 0;
	}
	if(closedir(dir)) { perror(directory); }
	return -1;
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

static int new_image(const char *const fn) {
	static int fibonacci[2] = { 5, 8 };
	char *stripped = strrchr(fn, '/');
	if(!stripped) stripped = (char *)fn;
	else stripped++;

	/* copy and put it in table */
	if(no_image_names >= image_names_capacity) {
		struct ImageName *i_n;
		grow(fibonacci);
		if(!(i_n = realloc(image_names, sizeof(struct ImageName) * fibonacci[0]))) {
			perror("image names");
			return 0;
		}
		image_names_capacity = fibonacci[0];
		image_names = i_n;
		fprintf(stderr, "Image names: grew size to %u.\n", fibonacci[0]);
	}
	strcopy(image_names[no_image_names].name, stripped, max_image_names_name);
	no_image_names++;
	is_sorted = 0;

	return -1;
}

static void sort(void) {
	/*spam->fprintf(stderr, "Sorting image names.\n");*/
	qsort(image_names, no_image_names, sizeof(struct ImageName),
		  (int (*)(const void *, const void *))&strcmp);
	is_sorted = -1;
}

static int include_images(void) {
	int i;

	if(!is_sorted) sort();
	printf("/* external dependencies -- use File2h to automate */\n");
	for(i = 0; i < no_image_names; i++) {
		printf("#include \"%s.h\"\n", to_name(image_names[i].name));
	}
	printf("\n");
	return -1;
}

static int print_images(const char *const directory) {
	size_t size = 0;
	static char pn[1024];
	const int   max_pn = sizeof(pn) / sizeof(char);
	int i;
	char type[8], *str, *fn;
	unsigned char *data;
	unsigned width = 0, height = 0, depth = 0;

	if(!is_sorted) sort();

	printf("/*must set texture->const*/ struct Image images[] = {\n");
	for(i = 0; i < no_image_names; i++) {
		fn = image_names[i].name;

		/* build into path */
		if(snprintf(pn, sizeof(pn) / sizeof(char), "%s%s%s", directory,
					directory[strlen(directory) - 1] != '/' ? "/" : "",
					fn) + 1 > max_pn) {
			fprintf(stderr, "%s: path name buffer can't hold %u characters.\n", fn, max_pn);
			return 0;
		}

		/* guess the format based on it's extension;
		 assumes .type; hijack it and turn it into IF_TYPE */
		if(!(str = strrchr(fn, '.'))) {
			fprintf(stderr, "%s: couldn't determine type.\n", fn);
			return 0;
		}
		snprintf(type, sizeof(type) / sizeof(char), "IF_%s", ++str);
		for(str = &type[3]; *str; str++) *str = toupper(*str);

		/* fixme: ick, no, compare then have an enum */
		if(!strcmp("IF_PNG", type)) {
			FILE *fp;
			unsigned error;

			/* get file size */
			if(!(fp = fopen(pn, "r"))
			   || fseek(fp, 0, SEEK_END)
			   || (size = ftell(fp)) == -1
			   || fclose(fp)) { perror(fn); fclose(fp); return 0; }
			/* re-open it in lodepng to get the other info */
			if((error = lodepng_decode32_file(&data, &width, &height, pn))) {
				fprintf(stderr, "Loader: lodepng error %u on %s: %s\n", error, pn, lodepng_error_text(error));
				return 0;
			}
			free(data); /* lol, we just need the dimensions */
			depth = 4; /* png files get automatically converted to 32 bits --
						decode32 -- I would have to change the code for lodepng
						to do 3-bit, but why when you have jpeg? */
		} else if(!strcmp("IF_JPEG", type)) {
			enum { E_NO_ERR = 0, E_PERROR, E_READ, E_DECODE } err = E_NO_ERR;
			FILE          *fp = 0;
			unsigned char *buffer = 0;
			do { /* try */
				/* nanojpeg doesn't do io */
				if(!(fp = fopen(pn, "r"))) { err = E_PERROR; break; }
				fseek(fp, 0, SEEK_END);
				size = ftell(fp);
				fseek(fp, 0, SEEK_SET);
				rewind(fp);
				if(!(buffer = malloc(size))) { err = E_PERROR; break; }
				if(fread(buffer, sizeof(char), size, fp) != size)
					{ err = E_READ; break; }
				/* decode */
				if(njDecode(buffer, size) || !njIsColor())
					{ err = E_DECODE; break; }
				width  = njGetWidth();
				height = njGetHeight();
				depth  = 3; /* colour always, but no alpha */
			} while(0); { /* finally */
				njDone();
				free(buffer);
				fclose(fp);
			} if(err) { /* catch */
				if(err == E_PERROR) perror(pn);
				else fprintf(stderr, "%s: decoding failed.\n", pn);
				return 0;
			}
		} else if(!strcmp("IF_BMP", type)) {
			FILE *fp;
			struct Bitmap *bmp;
			/* get file size */
			if(!(fp = fopen(pn, "r"))
			   || fseek(fp, 0, SEEK_END)
			   || (size = ftell(fp)) == -1
			   || fclose(fp)) { perror(fn); fclose(fp); return 0; }
			/* re-open */
			if(!(bmp = BitmapFile(pn))) return 0;
			width  = BitmapGetWidth(bmp);
			height = BitmapGetHeight(bmp);
			depth  = BitmapGetDepth(bmp);
			Bitmap_(&bmp);
		} else {
			fprintf(stderr, "%s: unrecognised image format.\n", pn);
			return 0;
		}

		printf("\t{ \"%s\", %s, %u, %s, %u, %u, %u, 0 }%s", fn, type, (unsigned)size, to_name(fn), width, height, depth, i != no_image_names - 1 ? ",\n" : "\n");
	}
	printf("};\n");
	printf("const int max_images = sizeof images / sizeof(struct Image);\n\n");

	return -1;
}

/* bsearch fn's */

/** comparable record types */
static int string_image_comp(const char **key_ptr, const struct ImageName *elem) {
	/*fprintf(stderr, "key: %s; elem %s\n", *key_ptr, elem->name);*/
	return strcmp(*key_ptr, elem->name);
}
