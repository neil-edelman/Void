/** Commonly used.

 @author	Neil
 @version	1.0, 2015-08
 @since		1.0, 2015-08 */

#ifdef SUFFIX

#include <string.h>

static int suffix(char *const str, const char *suf);

/** @return True is the suffix of the string {str} is {suf}. */
static int suffix(char *const str, const char *suf) {
	char *sub = str;
	const size_t len = strlen(suf);
	while((sub = strstr(sub, suf))) if(*(sub += len) == '\0') return -1;
	return 0;
}

#endif

#ifdef TRIM

#include <ctype.h>
#include <string.h>

static char *trim(char *const str);

/** @return The trimmed string. The original is trimmed from the end. */
static char *trim(char *const str) {
	char *start, *end;
	
	if(!str) return 0;
	/* trim the end */
	for(end = str + strlen(str) - 1; str <= end && isspace(*end); end--);
	*(++end) = '\0';
	/* trim the start */
	for(start = str; start < end && isspace(*start); start++);
	return start;
}

#endif

#ifdef GROW

static void grow(int f[2]);

/** Fibonacci growing thing. Make sure the numbers are monotonic. */
static void grow(int f[2]) {
	f[0] ^= f[1];
	f[1] ^= f[0];
	f[0] ^= f[1];
	f[1] += f[0];
}

#endif

#ifdef STRCOPY

#include <string.h>

static void strcopy(char *s1, const char *s2, const size_t n);

/** Stdlib is ridiculous. This is sort-of strlcpy.
 <p>
 Fixme: still goes until the end.
 @param s1	The destination.
 @param s2	The source.
 @param n	The maximum size; when it gets to {@code n}, it stores a null.
 @return	The string. */
static void strcopy(char *s1, const char *s2, const size_t n) {
	strncpy(s1, s2, n - 1);
	s1[n - 1] = '\0';
}

#endif

#ifdef CAMEL_TO_SNAKE_CASE

#include <ctype.h>

static char *camel_to_snake_case(const char *const str);

/** Returns a locally allocated buffer that is overwriten each call.
 Trucates the string to fit in the buffer. Null returns empty. */
static char *camel_to_snake_case(const char *const str) {
	static char buffer[1024];
	char *c, *b = &buffer[0];

	for(c = (char *)str; c; c++) {
		if(!*c || (char *)(b - buffer) >= (char *)(sizeof(buffer) / sizeof(char)) - 1 - 2) break;
		if(islower(*c)) {
			*(b++) = *c;
		} else if(isupper(*c)) {
			if(b != buffer) *(b++) = '_';
			*(b++) = tolower(*c);
		} else if(isdigit(*c) && c > str) {
			*(b++) = *c;
		} else {
			*(b++) = '_';
		}
	}
	*b = '\0';
	return buffer;
}

#endif

#ifdef TO_NAME

#include <string.h>

/** Conservatively filters out all things that shouldn't be there. Guesses what
 filenames should be. Also is a valid (maybe) variable name.
 <p>
 Returns a locally allocated buffer that is overwriten each call.
 <p>
 If the string is too long to fit or the string is null, returns null.
 <p>
 The space of strings is much more dense then the space after you run this on
 it; cavet: collisions are very possible. */
static char *to_name(const char *const fn) {
	static char buffer[1024];
	const size_t bufsize = sizeof(buffer) / sizeof(char);
	size_t len;
	char *a;

	if(!fn) return 0;
	if((len = strlen(fn)) + 1 >= bufsize || len == 0) return 0;
	strcpy(buffer, fn);
	for(a = buffer; *a; a++) {
		if(   (*a < 'A' || *a > 'Z')
		   && (*a < 'a' || *a > 'z')
		   && (*a < '0' || *a > '9' || a == buffer)) *a = '_';
	}
	return buffer;
}

#endif

#ifdef CLIP

/** Clips c to [min, max]. */
static int clip(const int c, const int min, const int max) {
	return c <= min ? min : c >= max ? max : c;
}

#endif

#ifdef STRSEPARATE

/** strsep is not ANSI-C89, so we make our own, just in case (*cough*Windows*.)
 @param pstr	Pointer to string; the string is modified, and pstr is advanced.
 				Must not be null, altough the string may be.
 @param delim	Delimiters; this function does not collapse mutiple delimiters
				into one; it could return a blank string. Must not be null.
 @return		A string that has [^delim] or null. */
static char *strseparate(char **const pstr, const char *delim) {
	char *token, *d, *s;
	
	if(!(token = *pstr)) return 0;
	for(s = token; *s; s++) {
		for(d = (char *)delim; *d; d++) {
			if(*s != *d) continue;
			/* token */
			*s    = '\0';
			*pstr = s + 1;
			return token;
		}
	}
	/* last token */
	*pstr = 0;
	return token;
}

#endif
