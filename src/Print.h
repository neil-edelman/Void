#include <stdio.h>  /* *printf */
#include <stdarg.h> /* va_* */

#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif

/* defaut is to print debug messages; turn it off by defining PRINT_NDEBUG */
#ifndef PRINT_NDEBUG
#define PRINT_DEBUG
#endif

/* default is not to print pedantic messages; turn them on PRINT_PEDANTIC */

/** Debug level message to stderr. */
static void debug(const char *const fmt, ...) {
#ifdef PRINT_DEBUG
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
#else
	UNUSED(fmt);
#endif
}

/** Dubug level pedantic to stderr. */
static void pedantic(const char *const fmt, ...) {
#ifdef PRINT_PEDANTIC
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
#else
	UNUSED(fmt);
#endif
}

/** For warnings/errors that should always be printed to stderr. */
static void warn(const char *const fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

/** For info that should always be printed to stdout. */
static void info(const char *const fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);
}

static void print_unused_coda(void);
static void print_unused(void) {
	debug(0);
	pedantic(0);
	warn(0);
	info(0);
	print_unused_coda();
}
static void print_unused_coda(void) {
	print_unused();
}
