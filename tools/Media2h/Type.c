/** Copyright 2015 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt */

#include <stdlib.h> /* bsearch */
#include <stdio.h>  /* fprintf */
#include <string.h> /* strcmp */
#define TRIM
#include "Functions.h"
#include "Error.h"
#include "Reader.h"
#include "Type.h"

/* Not to be confused with .type records.

 @author	Neil
 @version	3.2, 2015-08
 @since		3.2, 2015-08 */

/* private prototypes */
static int (*get_loader(const struct Type *const type))(char **, struct Reader *const);
static int string_type_comp(const char **key_ptr, const struct Type *elem);
/* reading fn's */
static int load_word(char **data_ptr, struct Reader *const r);
static int load_string(char **data_ptr, struct Reader *const r);
static int load_paragraph(char **data_ptr, struct Reader *const r);	
/* writing fn's */
/* fixme */

extern const char *delimiters;

/* these are the types of fields -- must be in order by the name;
 make sure you update type_foreign on changing anything */
static const struct Type {
	char          *name;
	char          *type_name;
	int           (*loader)(char **, struct Reader *const);
} types[] = {
	{ "-",             "void *",         0 },
	{ "autoincrement", "int ",           0 },
	{ "float",         "float ",         &load_word },
	/*{ "foreign",       "void *",         &load_word },*/
	{ "image",         "struct Image *", &load_string },
	{ "int",           "int ",           &load_word },
	{ "string",        "char *",         &load_string },
	{ "text",          "char *",         &load_paragraph }
};
static const int max_types = sizeof(types) / sizeof(struct Type);
/*static const struct Type *type_foreign = types[3];*/

char *TypeGetName(const struct Type *const type) {
	if(!type) return "null"; /* maybe? shouldn't really happen */
	return type->name;
}

char *TypeGetTypeName(const struct Type *const type) {
	if(!type) return "void *"; /* maybe? shouldn't really happen */
	return type->type_name;
}

/** @return		Wheater the type has a loader. Foriegn keys have loaders, but
				autoincrements don't, and don't have data assoiated to
				data. */
int TypeIsLoaded(const struct Type *const type) {
	return type && (get_loader(type) == 0) ? 0 : -1;
}

/** This calls the loader of the type on data and read.
 @return	Success; on returning success, the data is allocated;
			see {@see free}. */
int TypeLoader(const struct Type *const type, char **datum_ptr, struct Reader *r) {
	int (*loader)(char **, struct Reader *const);
	if(!datum_ptr || !r) return 0;
	return (loader = get_loader(type)) && !loader(datum_ptr, r) ? 0 : -1;
}

/** Keyword search.
 @return	Type matching the keyword or null if it is not a keyword. */
struct Type *TypeFromString(const char *str) {
	return bsearch(&str, types, max_types, sizeof(struct Type),
				   (int (*)(const void *, const void *))&string_type_comp);
}

/* private */

int (*get_loader(const struct Type *const type))(char **, struct Reader *const) {
	/* if type is null, assume it's a foreign key and set load_word; this isn't
	 to say that null will not be returned; eg, - or autoincrement. */
	return type ? type->loader : &load_word;
}

/** comparable types */
static int string_type_comp(const char **key_ptr, const struct Type *elem) {
	/*fprintf(stderr, "key: %s; elem: %s\n", *key_ptr, elem->name);*/
	return strcmp(*key_ptr, elem->name);
}

/* reading fn's */

static int load_word(char **data_ptr, struct Reader *const r) {
	char *line = ReaderReadLine(r), *word, *next_word;
	line = trim(line);
	if(!(word = strsep(&line, delimiters))
	   || ((next_word = strsep(&line, delimiters))
		   && (*next_word != '#'))) return 0;
	if(!(*data_ptr = strdup(word))) {
		perror("strdup");
		Error(E_MEMORY);
		return 0;
	}
	/*strcopy(lore->string, word, max_lore_string);*/
	fprintf(stderr, "load_word: %s\n", word);
	return -1;
}

static int load_string(char **data_ptr, struct Reader *const r) {
	char *line = ReaderReadLine(r);
	line = trim(line); /* fixme: no? */
	if(!(*data_ptr = strdup(line))) {
		perror("strdup");
		Error(E_MEMORY);
		return 0;
	}
	/*strcopy(lore->string, line, max_lore_string);*/
	fprintf(stderr, "load_string: %s\n", line);
	return -1;
}

static int load_paragraph(char **data_ptr, struct Reader *const r) {
	fprintf(stderr, "loading a paragraph is not done yet\n");
	return 0;
}

/* have an int? */

/* writing fn's */


