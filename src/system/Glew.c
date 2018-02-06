/** 20xx Neil Edelman, distributed under the terms of the GNU General
 Public License 3, see copying.txt, or
 \url{ https://opensource.org/licenses/GPL-3.0 }.
 20xx Neil Edelman, distributed under the terms of the MIT License;
 see readme.txt, or \url{ https://opensource.org/licenses/MIT }.

 This is a standard C file.

 @file		Text
 @author	Neil
 @std		C89/90
 @version	1.0; 20xx-xx
 @since		1.0; 20xx-xx
 @param
 @fixme
 @deprecated */

#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* fprintf */
#include "Cee.h"

/** Load {OpenGL2+} from the library under {-D GLEW}.
 @return True if success. */
static int Glew(void) {
#ifdef GLEW
	GLenum err;
	if((err = glewInit()) != GLEW_OK)
		return fprintf(stderr, "Glew: %s", glewGetErrorString(err)), 0;
	if(!glewIsSupported("GL_VERSION_2_0") /* !GLEW_VERSION_2_0 ? */)
		return fprintf(stderr,
					   "Glew: OpenGL 2.0+ shaders are not supported.\n"), 0;
	fprintf(stderr, "Glew: GLEW %s extension loading library ready for "
			"OpenGL2+.\n", glewGetString(GLEW_VERSION));
#endif
	return 1;
}
