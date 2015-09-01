/** Copyright 2015 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt */

#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* fprintf */
#include "Error.h"

/* Error reporting for resources.

 @author	Neil
 @version	3.2, 2015-08
 @since		3.2, 2015-08 */

static enum Error error = E_NO_ERROR;

/* must be corresponding to ErrorCode */
static const char *error_str[] = {
	"no error",
	"out of memory",
	"no data",
	"surpassed maximum records",
	"surpassed maximum fields",
	"syntax error",
	"already a keyword",
	"not a record",
	"maximum resouces allowed exceeded",
	"error loading record"
};

void Error(enum Error e) {
	error = e;
}

char *ErrorString(void) {
	return (char *)error_str[error];
}

int ErrorIsError(void) {
	return error == E_NO_ERROR ? 0 : -1;
}
