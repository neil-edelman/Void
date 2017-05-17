/** Copyright 2016 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt.

 This provides access to random functions. There is no object, just static.
 These functions use rand(), so if you want it to return different values, you
 can use srand().

 @title		Random
 @author	Neil
 @version	3.3; 2016-01
 @since		3.3; 2016-01 */

#include <stdlib.h> /* rand */
#include "Random.h"

/* public */

/** Uniform integer.
 @param sigma
 @return		Random int about [-max, max]. */
int RandomUniformInt(const int max) {
	if(max <= 0) return 0;
	return rand() / (RAND_MAX / max + 1);
}

/** Uniform float.
 @param sigma
 @return		Random int about [-max, max]. */
float RandomUniformFloat(const float max) {
	return (float)(((double)rand() / (1.0 + RAND_MAX) - 0.5) * 2.0 * (double)max);
}
