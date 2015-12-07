/** Copyright 2015 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt */

#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* fprintf */

/* include these functions */
#define TRIM
#define GROW
#define STRCOPY
#define CAMEL_TO_SNAKE_CASE
#include "Functions.h"

#include "Error.h"
#include "Reader.h"
#include "Type.h"
#include "Record.h"

/* asprintf, index, strsep, snprintf undefined */

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
/* bsearch fn's */
static int string_record_comp(const char **key_ptr, const struct Record *elem);
static int record_comp(const struct Record *key, const struct Record *elem);
static char *field_to_name(const struct Field *const field);
static void sort(void);

struct Field {
	char        name[1024];      /* eg, likes */
	char        type_name[1024]; /* eg, Alignment */
	struct Type *type;           /* eg, 0 */
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
static const unsigned max_record_fields = sizeof((struct Record *)0)->fields / sizeof(struct Field);
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
	/*spam->fprintf(stderr, "~Records: freeing Records, #%p.\n", (void *)records);*/
	free(records);
	records = 0;
	no_records = records_capacity = 0;
}

char *RecordGetName(const struct Record *const r) {
	if(!r) return 0;
	return (char *)r->name;
}

struct Type *RecordGetKeyType(const struct Record *const r) {
	if(!r) return 0;
	return r->key.type;
}

int RecordCompare(const struct Record *r1, const struct Record *r2) {
	if(!r1 || !r2) return 0;
	return record_comp(r1, r2);
}

/** Convert it to .h struct. */
void RecordOutput(void) {
	struct Record *record;
	struct Type *type;
	char *name;
	int i;
	unsigned j;

	if(!is_sorted) sort();

	for(i = 0; i < no_records; i++) printf("struct %s;\n", records[i].name);
	printf("\n");

	/* this is variable, loaded from all the .type */
	for(i = 0; i < no_records; i++) {
		record = &records[i];
		printf("struct %s {\n", record->name);
		printf("\t%s%s; /* key */\n", field_to_name(&record->key), record->key.name);
		for(j = 0; j < record->no_fields; j++) {
			printf("\t%s%s;\n", field_to_name(&record->fields[j]), record->fields[j].name);
		}
		printf("};\n\n"/*, camel_to_snake_case(record->name)*/);
	}

	/* search prototypes */
	printf("/* search protocols */\n");
	for(i = 0; i < no_records; i++) {
		record = &records[i];
		name = record->name;
		type = record->key.type; /* RecordGetKeyType(record) */
		printf("struct %s *%sSearch(%s%s);\n", name, name, TypeGetTypeName(type), camel_to_snake_case(name));
	}
}

/** Allocates and reads a record. */
int RecordLoadInstance(const struct Record *const record, char *data[MAX_FIELDS], struct Reader *read) {
	char **datum_ptr, *szvalue, *str;
	const char *szrecord;
	unsigned i;

	if(!record || !data || !read) return 0;

	if(!is_sorted) sort();

	if(!TypeLoader(record->key.type, &data[0], read)) return 0;
	for(i = 0; i < record->no_fields; i++) {
		/* data is i+1 because the key is 0 */
		datum_ptr = &data[i + 1];
		if(!TypeLoader(record->fields[i].type, datum_ptr, read)) return 0;
		/* is a fk; append type by replacing string with multi-string;
		 viz, two strings packed in one; it's kindof a hack! now the data has
		 a type and a foreign key; enough to resolve, but very dangerous */
		if(!record->fields[i].type) {
			szrecord = record->fields[i].type_name;
			szvalue  = *datum_ptr; /* replace... */
			asprintf(datum_ptr, "%s %s", szrecord, szvalue); /* \0 screws up */
			/* wtf is index(const char *, size_t)? write! */
			*(str = index(*datum_ptr, ' ')) = '\0'; /* so we need this */
			free(szvalue); /* replaced it */
			/* print */
			/*szvalue = *datum_ptr + strlen(szrecord) + 1;
			fprintf(stderr, "***< %s.%s = %s : { %s, %s } >!!!!\n", szrecord, record->fields[i].name, szvalue, *datum_ptr, szvalue);*/
		}
	}

	return -1;
}

void RecordEraseInstance(const struct Record *const record, char *data[MAX_FIELDS]) {
	unsigned i;

	if(!record || !data) return;

	if(!is_sorted) sort();

	/*spam->fprintf(stderr, "~Lore: freeing lore data.\n");*/
	for(i = 0; i <= record->no_fields; i++) {
		if(!TypeIsLoaded(!i ? record->key.type : record->fields[i - 1].type)) continue;
		if(!data[i]) return; /* this is caused by an error; the other fields
							  won't be valid */
		/*spam->fprintf(stderr, "Freeing data, #%p.\n", data[i]);*/
		free(data[i]);
	}
}

void RecordPrintInstance(const struct Record *record, const char *const data[MAX_FIELDS]) {
	unsigned i;

	if(!record || !data) return;

	if(!is_sorted) sort();

	printf("\t{ ");
	TypePrintData(record->key.type, &data[0]);
	for(i = 0; i < record->no_fields; i++) {
		printf(", ");
		TypePrintData(record->fields[i].type, &data[i + 1]);
	}
	printf(" }");

}

struct Record *RecordSearch(const char *str) {
	if(!is_sorted) sort();
	return bsearch(&str, records, no_records, sizeof(struct Record),
	               (int (*)(const void *, const void *))&string_record_comp);
}

/** For every record, writes search f'ns; c file. Call {@see TypePrintCompares}
 first, since this is referencing them.
 <p>
 This will crash if you set records to have foriegn keys; only basic types for
 keys please.
 <p>
 The definition of the things that {@see RecordOutput} prints out. */
void RecordPrintSearches(void) {
	struct Record *record;
	struct Type *key_type;
	int i;
	char *name, *snake, *key, *key_type_name;

	if(!is_sorted) sort();

	/* stdlib.h is included at the top in Loader.c */
	printf("/* f'ns that we (might) need to compare basic types */\n\n");
	/* fixme: only do it if it's actually used */
	printf("static int cmp_float(const float f1, const float f2) {\n");
	printf("\tconst float comp = f1 - f2;\n");
	printf("\tif(comp < 0.0f) return -1;\n");
	printf("\tif(comp > 0.0f) return 1;\n");
	printf("\treturn 0;\n");
	printf("}\n\n");
	printf("static int cmp_int(const int i1, const int i2) {\n");
	printf("\treturn i1 - i2;\n");
	printf("}\n\n");
	printf("/* comparison f'ns and public *Search() for compound Types; *Search can return\nzero if it didn't find it */\n\n");
	for(i = 0; i < no_records; i++) {
		record = &records[i];
		name          = record->name;
		snake         = camel_to_snake_case(name);
		key           = record->key.name;
		key_type      = record->key.type;
		key_type_name = TypeGetTypeName(record->key.type);
		printf("int %s_comp(%s*key_ptr, const struct %s *elem) {\n", snake, key_type_name, name);
		printf("\t%sk = *key_ptr;\n", key_type_name);
		printf("\t%se = elem->%s;\n\n", key_type_name, key);
		printf("\treturn %s(k, e);\n", TypeGetComparatorName(key_type));
		printf("}\n\n");
		printf("struct %s *%sSearch(%skey) {\n", name, name, TypeGetTypeName(key_type));
		printf("\treturn bsearch(&key, %s, max_%s, sizeof(struct %s), (int (*)(const void *, const void *))&%s_comp);\n", snake, snake, name, snake);
		printf("}\n\n");
	}
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
	/*spam->fprintf(stderr, "New record type \"%s.\"\n", record->name);*/
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
		snprintf(foreign, sizeof(foreign) / sizeof(char), "const struct %s *", str);
		type_name = foreign;
	}
	return type_name;
}

/** This is important. */
static void sort(void) {
	/*spam->fprintf(stderr, "Sorting Records.\n");*/
	qsort(records, no_records, sizeof(struct Record),
		  (int (*)(const void *, const void *))&record_comp);
	is_sorted = -1;
}
