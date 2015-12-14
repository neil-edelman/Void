/* Copyright 2013 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt */

#include <stdio.h>  /* fprintf */
#include "Glew.h"

/** OpenGL not-on-a-mac is a little harder since OpenGL > 1.0 functions have to
 be quiried from the library in a platform-specific way; I use glew to make it
 easier (ostensibly.)
 <p>
 Unless GLEW is defined, this does exactly nothing. If GLEW is defined, it
 loads the OpenGL2+ from the library using
 <a href = "http://glew.sourceforge.net/">GLEW</a>.
 <p>
 Include Glew.h for OpenGL functions.
 @author	Neil
 @version	3.1, 2013
 @since		3.1, 2013 */

static int is_started;

/** Load OpenGL2+ from the library.
 @return	True if success. */
int Glew(void) {
#ifdef GLEW
	GLenum err;
#endif

	if(is_started) return -1;

#ifdef GLEW
	if((err = glewInit()) != GLEW_OK) {
		fprintf(stderr, "Glew: %s\n", glewGetErrorString(err));
		return 0;
	}
	if(!glewIsSupported("GL_VERSION_2_0") /* !GLEW_VERSION_2_0 ? */) {
		fprintf(stderr, "Glew: OpenGL 2.0+ shaders are not supported. :[\n");
		return 0;
	}
	fprintf(stderr, "Glew: GLEW %s extension loading library ready for OpenGL 2.0+.\n", glewGetString(GLEW_VERSION));
#endif

	is_started = -1;

	return -1;
}
