/** Copyright 2013 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt.

 OpenGL not-on-a-mac is a little harder since OpenGL > 1.0 functions have to
 be quiried from the library in a platform-specific way; I use glew to make it
 easier (ostensibly.)

 Unless GLEW is defined, this does exactly nothing. If GLEW is defined, it
 loads the OpenGL2+ from the library using GLEW,
 \url{http://glew.sourceforge.net/}.

 Include Glew.h for OpenGL functions.

 @title		Glew
 @author	Neil
 @version	3.1, 2013
 @since		3.1, 2013 */

#include "../Print.h"
#include "Glew.h"

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
		warn("Glew: %s", glewGetErrorString(err));
		return 0;
	}
	if(!glewIsSupported("GL_VERSION_2_0") /* !GLEW_VERSION_2_0 ? */) {
		warn("Glew: OpenGL 2.0+ shaders are not supported.\n");
		return 0;
	}
	debug("Glew: GLEW %s extension loading library ready for OpenGL2+.\n",
		glewGetString(GLEW_VERSION));
#endif

	is_started = -1;

	return -1;
}
