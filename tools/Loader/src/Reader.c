/** Copyright 2015 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt */

#include <stdlib.h> /* malloc FILE */
#include <stdio.h>  /* fprintf */
#include <string.h> /* strncpy */
#include <errno.h>  /* errno */
#include "Reader.h"

/* This is an attempt to copy some functionality of JavaSE8's LineNumberReader.

 "A buffered character-input stream that keeps track of line numbers. This class
 defines methods setLineNumber(int) and getLineNumber() for setting and getting
 the current line number respectively.
 
 By default, line numbering begins at 0. This number increments at every line
 terminator as the data is read, and can be changed with a call to
 setLineNumber(int). Note however, that setLineNumber(int) does not actually
 change the current position in the stream; it only changes the value that will
 be returned by getLineNumber().
 
 A line is considered to be terminated by any one of a line feed ('\n'), a
 carriage return ('\r'), or a carriage return followed immediately by a
 linefeed."
 
 @author	Neil
 @version	1.0, 2015-08
 @since		1.0, 2015-08 */

struct Reader {
	char     fn[256];
	FILE     *fp;
	unsigned number;
};
const static int max_fn = sizeof((struct Reader *)0)->fn / sizeof(char);

static char read[1024];

/* public */

/** Constructor.
 @return	An object or a null pointer, if the object couldn't be created.
			Call perror to see what was the error. */
struct Reader *Reader(const char *fn) {
	struct Reader *reader;

	if(!fn) return 0;
	if(!(reader = malloc(sizeof(struct Reader)))) {
		perror(fn);
		Reader_(&reader);
		return 0;
	}
	strncpy(reader->fn, fn, max_fn - 1);
	reader->fn[max_fn - 1] = '\0';
	reader->fp             = 0;
	reader->number         = 0;
	/*fprintf(stderr, "Reader: new, #%p.\n", (void *)reader);*/
	if(!(reader->fp = fopen(fn, "r"))) {
		perror(fn);
		Reader_(&reader);
		return 0;
	}

	return reader;
}

/** Destructor.
 @param reader_ptr	A reference to the object that is to be deleted. */
void Reader_(struct Reader **reader_ptr) {
	struct Reader *reader;
	
	if(!reader_ptr || !(reader = *reader_ptr)) return;
	/*fprintf(stderr, "~Reader: erase, #%p.\n", (void *)reader);*/
	if(reader->fp && fclose(reader->fp)) perror(reader->fn);
	free(reader);
	*reader_ptr = reader = 0;
}

/** Set the current line number.
@param lineNumber	An int specifying the line number. */
void ReaderSetLineNumber(struct Reader *r, int lineNumber) {
	if(!r) return;
	r->number = lineNumber;
}

/** Get the current line number.
 @return	The current line number. */
unsigned ReaderGetLineNumber(const struct Reader *r) {
	if(!r) return 0;
	return r->number;
}

/** Read a line of text. Whenever a line terminator is read the current line
 number is incremented.
@return	"A String containing the contents of the line, not including any line
		termination characters, or null if the end of the stream has been
		reached." Not exactly. */
char *ReaderReadLine(struct Reader *r) {
	char *last;

	if(!r) return 0;
	if(!fgets(read, sizeof(read) / sizeof(char), r->fp)) {
		if(errno) perror(r->fn);
		return 0;
	}
	last = &read[strlen(read) - 1];
	if(*last == '\n') {
		*last = '\0';
		r->number++;
	} else {
		/* fixme: maybe we should have it user-set what to do in this case? */
		fprintf(stderr, "Reader \"%s\" line %u: warning, gone over maximum %u characters.\n", ReaderGetFilename(r), ReaderGetLineNumber(r), (unsigned)(sizeof(read) / sizeof(char)) - 1);
	}

	return read;
}

/** @return The internal (limits charachters, so may be truncated) filename. */
const char *ReaderGetFilename(const struct Reader *r) {
	if(!r) return 0;
	return r->fn;
}
