/* Copyright 2000, 2013 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt */

#include <stdio.h>  /* stderr vfprintf */
#include <stdarg.h> /* va_* */
#include "Print.h"

/** This is to save typing; pedantic and debug map are equivalent to fprintf,
 but they can be turned off right here instead of grepping all files. The
 defualt is to print debug but not pedantic. You can control these by defining
 PRINT_NDEBUG and PRINT_PEDANTIC.
 @author	Neil
 @version	3.3, 2015-12
 @since		3.3, 2015-12 */

/** For warnings/errors that should always be printed to stderr. */
void Warn(const char *format, ...) {
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}

/** For info that should always be printed to stdout. */
void Info(const char *format, ...) {
	va_list args;
	va_start(args, format);
	vfprintf(stdout, format, args);
	va_end(args);
}

#ifdef PRINT_DEBUG
/** Debug level message to stderr. */
void Debug(const char *format, ...) {
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}
#endif

#ifdef PRINT_PEDANTIC
/** Dubug level pedantic to stderr. */
void Pedantic(const char *format, ...) {
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}
#endif
