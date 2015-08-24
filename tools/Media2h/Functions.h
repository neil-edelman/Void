
#ifdef SUFFIX

#include <string.h>

/** @return		True is the suffix of the string {@code str} is {@code suf}. */
static int suffix(char *const str, const char *suf) {
	char *sub = str;
	const int len = strlen(suf);
	while((sub = strstr(sub, suf))) if(*(sub += len) == '\0') return -1;
	return 0;
}

#endif

#ifdef TRIM

#include <string.h>
#include <ctype.h>

/** @return		The trimmed string. The original is trimmed from the end. */
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

/** Fibonacci growing thing. Make sure the numbers are monotonic. */
static void grow(int f[2]) {
	f[0] ^= f[1];
	f[1] ^= f[0];
	f[0] ^= f[1];
	f[1] += f[0];
}

#endif

#ifdef STRCOPY

/** Stdlib is ridiculous. This is sort-of strlcpy.
 @param s1	The destination.
 @param s2	The source.
 @param n	The maximum size; when it gets to {@code n}, it stores a null.
 @return	The string. */
static void strcopy(char *s1, const char *s2, const size_t n) {
	strncpy(s1, s2, n - 1);
	s1[n - 1] = '\0';
}

#endif
