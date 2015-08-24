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
#include "Type.h"
#include "Record.h"

/* .type files contain Records.

 @author	Neil
 @version	3.2, 2015-08
 @since		3.2, 2015-08 */

struct Type;
struct Field;
struct Lore;

/* private prototypes */
static struct Record *new_record(void);
static int load_record(struct Reader *r);
/*static int load_field_instance(const struct Field *const field, void **datum_ptr, struct Reader *read);*/
/* bsearch fn's */
static int string_record_comp(const char **key_ptr, const struct Record *elem);
static int record_comp(const struct Record *key, const struct Record *elem);
static char *field_to_name(const struct Field *const field);
static void sort(void);

struct Field {
	char        name[1024];
	char        type_name[1024];
	struct Type *type;
};
static const int max_field_name = sizeof((struct Field *)0)->name / sizeof(char);
static const int max_field_type = sizeof((struct Field *)0)->type_name / sizeof(char);

/* a record is made up of fields */
static struct Record {
	char         name[1024];
	struct Field key;
	unsigned     no_fields, it_fields;
	struct Field fields[MAX_FIELDS];
} *records;
static const int max_record_name = sizeof((struct Record *)0)->name / sizeof(char);
static const int max_record_fields = sizeof((struct Record *)0)->fields / sizeof(struct Field);
static int no_records, records_capacity;

const        char *delimiters = " \t\n\r";

static       int  is_sorted   = -1;

/** Loads the meta-data on the types from a file.
 @return	Wheather it was successful. */
int Record(const char *const fn) {
	struct Reader *r;

	if(!(r = Reader(fn))) return 0;
	while(load_record(r));
	if(ErrorIsError()) {
		fprintf(stderr, "\"%s\" line %u: %s.\n", ReaderGetFilename(r), ReaderGetLineNumber(r), ErrorString());
	}
	Reader_(&r);

	return ErrorIsError() ? 0 : -1;
}

/** Does a global erase. */
void Record_(void) {
	if(!records) return;
	fprintf(stderr, "~Records: freeing Records, #%p.\n", (void *)records);
	free(records);
	records = 0;
	no_records = records_capacity = 0;
}

char *RecordGetName(const struct Record *const r) {
	if(!r) return 0;
	return (char *)r->name;
}

#if 0
/** May overwrite between calls. */
char *RecordFieldType(const struct Field *const field) {
/*	printf("#%p\n", (void *)field);
	return "foo";*/
	if(!field) return "void *";
	if(!field->type) {
		/*static char buffer[1024];

		if(snprintf(buffer, sizeof(buffer) / sizeof(char), "struct %s *", field->type_name) > sizeof(buffer) / sizeof(char) - 1) {
			fprintf(stderr, "Record::FieldType: truncated line.\n");
		}
		return buffer;*/
		return "fk ";
	}
	return "type ";/*TypeTypeName(field->type);*/
}
#endif

/** Convert it to .h struct. */
void RecordOutput(void) {
	struct Record *record;
	int i, j;

	if(!is_sorted) sort();

	for(i = 0; i < no_records; i++) {
		record = &records[i];
		printf("struct %s {\n", record->name);
		printf("\t%s%s; /* key */\n", field_to_name(&record->key), record->key.name);
		for(j = 0; j < record->no_fields; j++) {
			printf("\t%s%s;\n", field_to_name(&record->fields[j]), record->fields[j].name);
		}
		printf("};\n\n");
	}
}

/** Allocates and reads a record. */
int RecordLoadInstance(const struct Record *const record, char *data[MAX_FIELDS], struct Reader *read) {
	int i;

	if(!record || !data || !read) return 0;

	/*if(!load_field_instance(&record->key, &data[0], read)) return 0;*/
	if(!TypeLoader(record->key.type, &data[0], read)) return 0;
	for(i = 0; i < record->no_fields; i++) {
		/*if(!load_field_instance(&record->fields[i], &data[i + 1], read)) return 0;*/
		if(!TypeLoader(record->fields[i].type, &data[i + 1], read)) return 0;
	}

	return -1;
}

void RecordEraseInstance(const struct Record *const record, char *data[MAX_FIELDS]) {
	int i;

	if(!record || !data) return;

	fprintf(stderr, "~Lore: freeing lore data.\n");
	for(i = 0; i <= record->no_fields; i++) {
		if(!TypeIsLoaded(!i ? record->key.type : record->fields[i - 1].type)) continue;
		if(!data[i]) return; /* this is caused by an error; the other fields
							  won't be valid */
		fprintf(stderr, "Freeing data, #%p.\n", data[i]);
		free(data[i]);
	}
}

/** Iterator; first get key, then fields. */
/*int RecordNextField(struct Record *r, struct Field **field_ptr) {
	if(!r || !field_ptr) return 0;
	printf("Record::nextField: %u/%u.\n", r->it_fields, r->no_fields + 1);
	if(r->it_fields == 0) {
		*field_ptr = &r->key;
		printf(" (key %s)\n", TypeName(r->key.type));
		r->it_fields++;
	} else if(r->it_fields < r->no_fields + 1) {
		printf(" (%s)\n", TypeName(r->fields[r->it_fields - 1].type));
		*field_ptr = &r->fields[r->it_fields++ - 1];
	} else {
		r->it_fields = 0;
		return 0;
	}
	return -1;
}*/

struct Record *RecordSearch(const char *str) {
	if(!is_sorted) sort();
	return bsearch(&str, records, no_records, sizeof(struct Record),
	               (int (*)(const void *, const void *))&string_record_comp);
}

/* private */

/** Ensures that we have space for the new record.
 @return	The new record or null. */
static struct Record *new_record(void) {
	static int fibonacci[2] = { 5, 8 };
	struct Record *record, *r;

	if(no_records >= records_capacity) {
		grow(fibonacci);
		if(!(r = realloc(records, sizeof(struct Record) * fibonacci[0]))) {
			perror("records");
			return 0;
		}
		records_capacity = fibonacci[0];
		records = r;
		fprintf(stderr, "Record: grew size to %u.\n", fibonacci[0]);
	}
	record = &records[no_records++];
	record->name[0]          = '\0';
	record->key.name[0]      = '\0';
	record->key.type_name[0] = '\0';
	record->key.type         = T_UNKNOWN;
	record->no_fields        = 0;
	record->it_fields        = 0;
	is_sorted = 0;
	return record;
}

/** Loads each individal Record resource.
 @return	Success or sets {@see Error} on failure. */
static int load_record(struct Reader *r) {
	struct Record *record = 0;
	struct Field  *field;
	int           no_content = 0, i;
	int           is_loaded = 0;
	char          *line, *word[3] = { "", "", "" };

	/* line */
	while((line = ReaderReadLine(r))) {
		line = trim(line);
		/*fprintf(stderr, "Line \"%s\"\n", line);*/
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
			if(no_content < 2) { Error(E_SYNTAX); return 0; }
			is_loaded = -1;
			break;
		}
		/* yay, we got though */
		if(no_content == 0) {
			if(word[1]) { Error(E_SYNTAX); return 0; }
			/* no keywords as user-defined datatypes */
			if(TypeFromString(word[0])) { Error(E_KEYWORD); return 0; }
			if(!(record = new_record())) { Error(E_MEMORY); return 0; }
			strcopy(record->name, word[0], max_record_name);
			/*fprintf(stderr, "New type: %s.\n", record->name);*/
		} else {
			/* synatax check */
			if(record->no_fields >= max_record_fields) {
				Error(E_MAX_FIELDS); return 0;
			}
			if(!word[1] || word[2]) {
				Error(E_SYNTAX); return 0;
			}

			/* field extraction */
			field = (no_content == 1) ?
			             &record->key : &record->fields[record->no_fields++];

			/* filling in the field */
			strcopy(field->type_name, word[0], max_field_type);

			field->type = TypeFromString(word[0]);

			strcopy(field->name, word[1], max_field_name);
		}
		no_content++;
	}
	if(!is_loaded) return 0;
	fprintf(stderr, "New record type \"%s.\"\n", record->name);
	/* make sure that this type does not cause a naming conflict;
	 fixme: it's going to be sorted anyway, might as well sort! */
	for(i = 0; i < no_records - 1; i++) {
		if(!strcmp(record->name, records[i].name)) {
			fprintf(stderr, "Warning: record \"%s\" already exists; deleting.\n", record->name);
			no_records--; /* just delete this record, like that */
		}
	}
	return -1;
}

#if 0
/** Loads a field into datum from read (.lore.) */
static int load_field_instance(const struct Field *const field, void **datum_ptr, struct Reader *read) {
	char *str;

	if(!(str = ReaderReadLine(read))) return 0;
	printf("field: %s%s\n", field_to_name(field), str);

	return -1;
}
#endif

/* bsearch fn's */

/** comparable record types */
static int string_record_comp(const char **key_ptr, const struct Record *elem) {
	/*fprintf(stderr, "key: %s; elem %s\n", *key_ptr, elem->name);*/
	return strcmp(*key_ptr, elem->name);
}

/** comparable record types */
static int record_comp(const struct Record *key, const struct Record *elem) {
	/*fprintf(stderr, "key: %s; elem %s\n", key->name, elem->name);*/
	return strcmp(key->name, elem->name);
}

/** Matches string to type. */
static char *field_to_name(const struct Field *const field) {
	static char foreign[1024];
	struct Type *type;
	const char *str = field->type_name;
	char *type_name;
	
	if((type = TypeFromString(str))) {
		type_name = TypeGetTypeName(type);
	} else {
		snprintf(foreign, sizeof(foreign) / sizeof(char), "struct %s *", str);
		type_name = foreign;
	}
	return type_name;
}

/** This is important. */
static void sort(void) {
	fprintf(stderr, "Sorting Records.\n");
	qsort(records, no_records, sizeof(struct Record),
		  (int (*)(const void *, const void *))&record_comp);
	is_sorted = -1;
}
