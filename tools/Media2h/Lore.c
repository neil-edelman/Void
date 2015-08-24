/** Copyright 2015 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt */

#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* fprintf */
#define TRIM
#define GROW
#define STRCOPY
#include "Functions.h"
#include "Error.h"
#include "Reader.h"
#include "Record.h"
#include "Type.h"
#include "Lore.h"

/* data/item/info/list/index/axiom/term/clue/cue/lore/tale/myth[os]/word/fact/
 saga/tale/yarn/writ/file/word/story; ".data" would be more appropriate, but
 that implies that it's a binary file. ".file" is cute.
 <p>
 ".lore" files contain typed data. "lore" contains all the data from every
 resource. You have to load the types for it to be meaningful.

 @author	Neil
 @version	3.2, 2015-08
 @since		3.2, 2015-08 */

/* private prototypes */
static struct Lore *new_lore(void);
static int load_lore(struct Reader *r);

/* one datum and it's type; potentally many Lores per file */
static struct Lore {
	struct Record *record;
	char          *data[MAX_FIELDS]; /* will have to be freed */
} *lores;
static const int max_lore_data = sizeof((struct Lore *)0)->data / sizeof(char);
static int no_lores, lores_capacity;

/* the info is on top of every file */
static struct Info {
	char filename[128];
	char title[128];
	char author[128];
} infos[1024]; /* upper limit on the number of .lore files that have author
				info; fixme: dynamically alloc */
static const int max_info_filename = sizeof((struct Info *)0)->filename / sizeof(char);
static const int max_info_title = sizeof((struct Info *)0)->title / sizeof(char);
static const int max_info_author = sizeof((struct Info *)0)->author / sizeof(char);
static const int max_infos = sizeof(infos) / sizeof(struct Info);
static int no_infos;

/* data */

extern const char *delimiters;
extern struct Record *records;

static int is_sorted = -1;

/** Loads the meta-data on the types from a file.
 @return	Wheather it was successful. */
int Lore(const char *const fn) {
	struct Reader *r;
	struct Info *info;
	char *line;
	int no_content = 0;

	/* get the pointer to a new info */
	info = &infos[no_infos];
	strcopy(info->filename, fn, max_info_filename);
	if(!(r = Reader(fn))) return 0;
	/* lore data: first the header */
	while((line = ReaderReadLine(r))) {
		line = trim(line);
		if(*line == '#') continue;
		if(no_content == 0) {
			strcopy(info->title, line, max_info_title);
			no_content = 1;
		} else if(no_content == 1) {
			strcopy(info->author, line, max_info_author);
			no_content = 2;
		} else {
			/* just ignore all the stuff after the tilde
			 if(!(word = strsep(&line, delimiters)) || word[0] != '~')
			 || (word[1] != '\0' && word[1] != '#')
			 || ((word = strsep(&line, delimiters)) && word[0] != '#')) { */
			if(*line != '~') {
				fprintf(stderr, "\"%s\" line %u: syntax error.\n", ReaderGetFilename(r), ReaderGetLineNumber(r));
				return 0;
			}
			no_content = 0;
			break;
		}
	}
	while(load_lore(r));
	/* print an error */
	if(ErrorIsError()) {
		fprintf(stderr, "\"%s\" line %u: %s.\n", ReaderGetFilename(r), ReaderGetLineNumber(r), ErrorString());
		Reader_(&r);
		return 0;
	}
	Reader_(&r);
	/* augment info count */
	if(no_infos < max_infos) {
		no_infos++;
	} else {
		fprintf(stderr, "Lore warning: %u author information blocks; maximum number.\n", no_infos);
	}

	return -1;
}

/** Does a global erase. FIXME!! */
void Lore_(void) {
	int i;

	if(!lores) return;
	for(i = 0; i < no_lores; i++) {
		if(!lores[i].record) break;
		RecordEraseInstance(lores[i].record, lores[i].data);
	}
	fprintf(stderr, "~Lore: freeing lores, #%p.\n", (void *)lores);
	free(lores);
	no_lores = lores_capacity = 0;
}

/** Convert it to .h struct. */
void LoreOutput(void) {
	struct Info *info;
	struct Lore *lore;
	int i;

	/*if(!is_sorted) sort();*/

	printf("/**\n");
	for(i = 0; i < no_infos; i++) {
		info = &infos[i];
		printf(" %s: \"%s\" by %s\n", info->filename, info->title, info->author); /* version, date? */
	}
	printf("*/\n\n");
	printf("/* loaded %u lores */\n\n", no_lores);
	for(i = 0; i < no_lores; i++) {
		lore = &lores[i];
		printf("struct %s\n", RecordGetName(lore->record));
		/* fixme: etc */
	}
}

/** Ensures that we have space for the new lore.
 @return	The new lore or null. */
static struct Lore *new_lore(void) {
	static int fibonacci[2] = { 5, 8 };
	struct Lore *lore, *l;

	if(no_lores >= lores_capacity) {
		grow(fibonacci);
		if(!(l = realloc(lores, sizeof(struct Lore) * fibonacci[0]))) {
			perror("lore");
			return 0;
		}
		lores_capacity = fibonacci[0];
		lores = l;
		fprintf(stderr, "Lore: grew size to %u.\n", fibonacci[0]);
	}
	lore = &lores[no_lores++];
	lore->record = 0;
	is_sorted = 0;
	return lore;
}

/** Converts each individal resource to .h.
 @return	Success. Sets {@code error} on failure. */
static int load_lore(struct Reader *r) {
	struct Record *record;
	struct Lore *lore;
	int is_loaded = 0;
	char *line, *word, *next_word;

	/* line: get the title */
	while((line = ReaderReadLine(r))) {
		/*fprintf(stderr, "Line \"%s\"\n", line);*/
		/* comment */
		line = trim(line);
		if(*line == '#') continue;
		/* break off the first word */
		if(!(word = strsep(&line, delimiters))) continue;
		/* ending */
		if(*word == '~') { is_loaded = -1; break; }
		/* this is expected to be a type; only one word */
		if((next_word = strsep(&line, delimiters)) && *next_word != '#') { Error(E_SYNTAX); return 0; }
		/*fprintf(stderr, "sep: \"%s\", \"%s\"\n", word, line);*/
		if(!(record = RecordSearch(word))) { Error(E_NOT_RECORD); return 0; }
		fprintf(stderr, "Loading as %s.\n", RecordGetName(record));
		if(!(lore = new_lore())) { Error(E_MEMORY); return 0; }
		lore->record = record;
		if(!RecordLoadInstance(record, lore->data, r)) { Error(E_RECORD); return 0; }
	}

	return is_loaded;
}

/** This is important; fixme: call sort() on Records, if 0, then sort them by
 type (not implemented; first turn strtoint()) */
/*static void sort(void) {
	fprintf(stderr, "Sorting Records.\n");
	qsort(records, no_records, sizeof(struct Record),
		  (int (*)(const void *, const void *))&record_comp);
	is_sorted = -1;
}*/
