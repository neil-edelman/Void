/* Copyright 2013 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt */

/* OpenGL not-on-a-mac is a little harder since OpenGL > 1.0 functions have to
 be quiried from the library in a platform-specific way; I use glew to make it
 easier */

#include <stdio.h>  /* fprintf */
#include "OpenGlew.h"

/* public */

/** constructor */
int OpenGlew(void) {

#ifdef GLEW

	GLenum err;
	if((err = glewInit()) != GLEW_OK) {
		fprintf(stderr, "OpenGlew: %s\n", glewGetErrorString(err));
		return 0;
	}
	if(!glewIsSupported("GL_VERSION_2_0") /* !GLEW_VERSION_2_0 ? */) {
		printf("OpenGlew: OpenGL 2.0 shaders are not supported.\n");
		return 0;
	}
	fprintf(stdout, "OpenGlew: GLEW %s extension loading library ready for OpenGL 2.0.\n", glewGetString(GLEW_VERSION));

#endif

	return -1;
}
