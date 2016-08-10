/* Copyright 2016 Neil Edelman, distributed under the terms of the MIT License;
 see readme.txt */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>  /* fprintf */

/** This is a replacement for strsep.

 @author	Neil
 @version	1.0; 2016-07
 @since		1.0; 2016-07 */

#if 1
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
#else
/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * Get next token from string *stringp, where tokens are possibly-empty
 * strings separated by characters from delim.
 *
 * Writes NULs into the string at *stringp to end tokens.
 * delim need not remain constant from call to call.
 * On return, *stringp points past the last NUL written (if there might
 * be further tokens), or is NULL (if there are definitely no more tokens).
 *
 * If *stringp is NULL, strsep returns NULL.
 */
static char *strseparate(char **pstr, const char *delim) {
	char *s;
	const char *d;
	int sc, dc;
	char *token;
	
	if(!(token = *pstr)) return 0;
	for(s = token; ; ) {
		sc = *s++;
		d = delim;
		do {
			if((dc = *d++) != sc) continue;
			if(!sc) {
				*pstr = 0;/*s = 0;*/
			} else {
				s[-1] = '\0';
				*pstr = s;
			}
			return token;
		} while(dc);
	}
}
#endif

/** Entry point.
 @param argc	The number of arguments, starting with the programme name.
 @param argv	The arguments.
 @return		Either EXIT_SUCCESS or EXIT_FAILURE. */
int main(void) {
	char str[1024], *s, *string;
	snprintf(str, sizeof(str), "the rain in spain falls mainly on the plains");
	string = str;
	while((s = strseparate(&string, " "))) { printf("%s\n", s); }
	return EXIT_SUCCESS;
}
