/* Copyright 2015 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt */

#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* fprintf */
#include <string.h> /* strncpy, strrchr */
#include <ctype.h>  /* toupper */
#include <unistd.h> /* chdir (POSIX, not ANSI) */
#include <dirent.h> /* opendir readdir closedir */
#include "Reader.h"

/** This is a crude static database that preprocesses all the media files and
 turns them into C header files for inclusion in Void.
 <p>
 {\code strsep} is not ansi, you may need to rewrite it with strtok.

 @author	Neil
 @version	3.2, 2015-08
 @since		3.2, 2015-08 */

/* constants */
static const char *programme   = "Media2h";
static const char *year        = "2015";
static const int versionMajor  = 1;
static const int versionMinor  = 0;

/* private */
static void usage(const char *argvz);

enum ErrorCode {
	E_NO_ERROR,
	E_NO_DATA,
	E_MAX_RECORDS,
	E_MAX_FIELDS,
	E_SYNTAX,
	E_KEYWORD
} error = E_NO_ERROR;
static const struct Error {
	enum ErrorCode code;
	char *string;
} error_str[] = {
	{ E_NO_ERROR,    "no error" },
	{ E_NO_DATA,     "no data" },
	{ E_MAX_RECORDS, "surpassed maximum records" },
	{ E_MAX_FIELDS,  "surpassed maximum fields" },
	{ E_SYNTAX,      "syntax error" },
	{ E_KEYWORD,     "already a keyword" }
};

/* these are the types of fields */
enum TypeCode {
	FOREIGN,
	AUTOINCREMENT,
	SPACER,
	INT,
	FLOAT,
	STRING,
	TEXT,
	IMAGE
};
static const struct Type {
	enum TypeCode code;
	char          *name;
	void *        (*constructor)(FILE *fp);
} types[] = {
	{ FOREIGN,       "foreign",       0/*&load_fk*/ },
	{ AUTOINCREMENT, "autoincrement", 0/*&load_ai*/ },
	{ SPACER,        "-",             0/*&load_spacer*/ },
	{ INT,           "int",           0/*&load_int*/ },
	{ FLOAT,         "float",         0/*&load_float*/ },
	{ STRING,        "string",        0/*&load_string*/ },
	{ TEXT,          "text",          0/*&load_text*/ },
	{ IMAGE,         "image",         0/*&load_image*/ }
};
static const int max_types = sizeof(types) / sizeof(struct Type);

/* field */
struct Field {
	enum TypeCode type;
	char          name[1024];
};
static const int max_field_name = sizeof((struct Field *)0)->name / sizeof(char);

/* a record is made up of fields */
static struct Record {
	char         name[1024];
	struct Field key;
	unsigned     no_fields;
	struct Field fields[64];
} records[64];
static const int max_record_name = sizeof((struct Record *)0)->name / sizeof(char);
static const int max_record_fields = sizeof((struct Record *)0)->fields / sizeof(struct Field);
static const int max_records = sizeof(records) / sizeof(struct Record);
static int no_records;

/* constants */
static const int autoincrement_start = 1;
static const char *delimiters = " \t\n\r";
static const char *media_file = "Media";

/* private prototypes */
int load_resources(const char *fn);
int load_resource(struct Reader *r);
enum TypeCode string_to_type(const char *str);

/** Entry point. */
int main(int argc, char **argv) {
	struct Record *record;
	int i, j;

	/* check that the user specified file and give meaningful names to args */
	if(argc != 2 || *argv[1] == '-') {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	printf("/** auto-generated from %s/ by %s %d.%d */\n\n",
		   argv[1], programme, versionMajor, versionMinor);

	fprintf(stderr, "Changing directory to <%s>.\n", argv[1]);
	if(chdir(argv[1])) { perror(argv[1]); return EXIT_FAILURE; }
	if(!load_resources(media_file)) {
	}
	for(i = 0; i < no_records; i++) {
		record = &records[i];
		printf("struct %s {\n", record->name);
		printf("\t%s %s; /* key */\n", types[record->key.type].name, record->key.name);
		for(j = 0; j < record->no_fields; j++) {
			printf("\t%s %s;\n", types[record->fields[j].type].name, record->fields[j].name);
		}
		printf("}\n\n");
	}
	/*opendir(".");*/

	return EXIT_SUCCESS;
}

/** Loads the meta-data on the types. */
int load_resources(const char *fn) {
	struct Reader *r;

	if(!(r = Reader(fn))) return 0;
	while(load_resource(r));
	if(error) {
		fprintf(stderr, "\"%s\" line %u: %s.\n", ReaderGetFilename(r), ReaderGetLineNumber(r), error_str[error].string);
	}
	Reader_(&r);

	return error ? 0 : -1;
}

/** Loads each individal meta-data resource.
 @return	Success. Sets {@code error} on failure. */
int load_resource(struct Reader *r) {
	struct Record *record;
	struct Field  *field;
	int no_content = 0, is_loaded = 0;
	char *line, *word[3] = { "", "", "" };

	if(no_records >= max_records) { error = E_MAX_RECORDS; return 0; }
	record = &records[no_records];
	record->name[0] = '\0';
	record->key.type = FOREIGN;
	record->key.name[0] = '\0';
	record->no_fields = 0;
	/* line: get the title */
	while((line = ReaderReadLine(r))) {
		fprintf(stderr, "Line \"%s\"\n", line);
		/* fixme: trim */
		/* comment */
		if(*line == '#') continue;
		/* break it up */
		if(!(word[0] = strsep(&line, delimiters))) continue;
		if(!(word[1] = strsep(&line, delimiters)) || (*word[1] == '#')) {
			word[1] = 0;
		} else if(!(word[2] = strsep(&line, delimiters)) || !(*word[2] == '#')) {
			word[2] = 0;
		}
		/* ending */
		if(*word[0] == '~') {
			if(no_content < 2) { error = E_SYNTAX; return 0; }
			is_loaded = -1;
			break;
		}
		/* yay, we got though */
		if(no_content == 0) {
			if(word[1]) { error = E_SYNTAX; return 0; }
			if(string_to_type(word[0]) != FOREIGN) {
				error = E_KEYWORD; return 0;
			}
			strncpy(record->name, word[0], max_record_name - 1);
			record->name[max_record_name - 1] = '\0';
			fprintf(stderr, "New type: %s.\n", record->name);
		} else {
			enum TypeCode type = string_to_type(word[0]);
			if(record->no_fields >= max_record_fields) {
				error = E_MAX_FIELDS; return 0;
			}
			if(!word[1] || word[2]) {
				error = E_SYNTAX; return 0;
			}
			field = (no_content == 1) ?
			        &record->key : &record->fields[record->no_fields++];
			strncpy(field->name, word[1], max_field_name - 1);
			field->name[max_field_name - 1] = '\0';
			field->type = type;
		}
		no_content++;
	}
	if(is_loaded) no_records++;
	return is_loaded;
}

/** Matches string to type. */
enum TypeCode string_to_type(const char *str) {
	int t;
	/* fixme */
	for(t = 0; t < max_types; t++) if(!strcmp(types[t].name, str)) break;
	if(t >= max_types) return FOREIGN;
	return types[t].code;
}

#if 0
/** private (entry point)
 @param resouces dir
 @param which file is turned into *.h
 @bug the file name should be < 1024 */
int main(int argc, char **argv) {
	/* may need to change the bounds */
	char name[1024], classname[1024], *a, *base, *res_fn, *tsv_fn;
	char read[1024], *line, *word;
	int len, field, first, ai;
	int no_tsv_fields = 0;
	int tsv_field[64];
	const int max_tsv_fields = sizeof(tsv_field) / sizeof(int);
	FILE *fp;

	/* check that the user specified file and give meaningful names to args */
	if(argc != 3 || *argv[1] == '-' || *argv[2] == '-') {
		usage(argv[0]);
		return EXIT_FAILURE;
	}
	res_fn = argv[1];
	tsv_fn = argv[2];

	/* extract the user-specifed filename; change into a c varname, name */
	if((base = strrchr(tsv_fn, '/'))) {
		base++;
	} else {
		base = tsv_fn;
	}
	strncpy(name, base, sizeof(name) / sizeof(char));
	name[sizeof(name) / sizeof(char) - 1] = '\0';
	for(a = name; *a; a++) {
		if(*a == '.') { /* we don't want any extensions */
			*a = '\0';
			break;
		} else if((*a < 'A' || *a > 'Z')
		   && (*a < 'a' || *a > 'z')
		   && (*a < '0' || *a > '9')) { /* no crazy chars */
			*a = '_';
		}
	}
	strncpy(classname, name, sizeof(classname) / sizeof(char));
	name[sizeof(classname) / sizeof(char) - 1] = '\0';
	for(a = classname; ; ) {
		*a = toupper(*a);
		if(!(a = strchr(a, '_'))) break;
		memmove(a, a + 1, strlen(a + 1) + 1);
	}

	/* open the resources file! */
	if(!(fp = fopen(res_fn, "r"))) {
		perror(res_fn);
		return EXIT_FAILURE;
	}
	printf("/** auto-generated from %s by %s %d.%d\n",
		   base, programme, versionMajor, versionMinor);
	/* line */
	while(fgets(read, sizeof(read) / sizeof(char), fp)) {
		if(!(len = strlen(line = read))) break;
		/* word */
		if(!(word = strsep(&line, delimiters))) {
			continue; /* blank line? */
		} else {
			/* go back to line unless the first word is name */
			if(*word == '#') continue;
			/*printf("cmp(<%s>, <%s>)?\n", word, name);*/
			if(strcmp(word, name)) continue;
		}
		/* we have a hit in the first field; fill up the rest */
		fprintf(stdout, "Type definition <%s>:\n", word);
		while((word = strsep(&line, delimiters)) && *word) {
			/*fprintf(stderr, "read <%s>\n", word);*/
			/* lol; I guess it doesn't have to be fast */
			for(field = 0; field < no_fields; field++) {
				/*printf(" cmp(<%s>, <%s>)?\n", word, fields[field]);*/
				if(!strcmp(word, fields[field])) break;
			}
			/* assume it's a foreign key since it doesn't match */
			if(field >= no_fields) field = FOREIGN;
			tsv_field[no_tsv_fields] = field;
			no_tsv_fields++;
			if(no_tsv_fields >= max_tsv_fields) {
				fprintf(stderr, "Warning: field truncated at <%s>; %d maximum.\n", word, max_tsv_fields);
				break;
			}
			fprintf(stdout, "field %s\n", fields[field]);
		}
		printf("end. */\n\n");
		break;
	}
	fclose(fp);

	/* print output */
	printf("static struct %s %s[] = {\n", classname, name);
	if(!(fp = fopen(tsv_fn, "r"))) {
		perror(tsv_fn);
	} else {
		ai = auto_increment_start;
		while(fgets(read, sizeof(read) / sizeof(char), fp)) {
			line = read;
			if(*line == '#') continue; /* lazy */
			if(ai != auto_increment_start) printf(",\n");
			printf("\t{ ");
			if(!(len = strlen(read))) break;
			first = -1;
			for(field = 0; field < no_tsv_fields; field++) {
				if(first) {
					first = 0;
				} else {
					printf(", ");
				}
				/* fixme: type validation */
				switch(tsv_field[field]) {
					case FOREIGN:
						if(!(word = strsep(&line, delimiters))) word = "null";
						printf("\"%s\", 0 /* fk */", word);
						break;
					case AUTOINCREMENT:
						printf("%d", ai);
						break;
					case SPACER:
						printf("0"); /* value will be computed at run-time */
						break;
					case INT:
						if(!(word = strsep(&line, delimiters))) word = "0";
						printf("%s", word);
						break;
					case STRING:
						if(!(word = strsep(&line, delimiters))) word = "null";
						printf("\"%s\"", word);
						break;
					case FLOAT:
						if(!(word = strsep(&line, delimiters))) word = "0.0";
						printf("\"%s\"", word);
						break;
					case IMAGE:
						if(!(word = strsep(&line, delimiters))) word = "null";
						printf("\"%s\", 0 /* image */", word);
						break;
				}
			}
			printf(" }");
			ai++;
		}
	}
	printf("\n};\n\n");
	printf("static const int no_%s = sizeof(%s) / sizeof(struct %s);\n", name, name, classname);

	return EXIT_SUCCESS;
}
#endif

static void usage(const char *argvz) {
	fprintf(stderr, "Usage: %s <resource directory>\n", argvz);
	fprintf(stderr, "This is a crude static database that preprocesses all the media files and\n");
	fprintf(stderr, "turns them into C header files for inclusion in Void.\n");
	fprintf(stderr, "Version %d.%d.\n\n", versionMajor, versionMinor);
	fprintf(stderr, "%s Copyright %s Neil Edelman\n", programme, year);
	fprintf(stderr, "This program comes with ABSOLUTELY NO WARRANTY.\n");
	fprintf(stderr, "This is free software, and you are welcome to redistribute it\n");
	fprintf(stderr, "under certain conditions; see copying.txt.\n\n");
}
