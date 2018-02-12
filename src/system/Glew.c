/** 2018 Neil Edelman, distributed under the terms of the GNU General
 Public License 3, see copying.txt, or
 \url{ https://opensource.org/licenses/GPL-3.0 }.

 {GLEW} is set ourselves in the Makefile; {GLEW} allows {OpenGL2+} on
 machines where you need to query the library.

 @title		Glew
 @author	Neil
 @std		C89/90
 @version	2018-01 */

#include <stdio.h>  /* fprintf */
/* Inexplicably, Glew will include {win.h} in Windows, which has a bajillon
 warnings that are not the slightest bit useful; assuming you are using {MSVC},
 this silences them. Before {Window.h}. */
#ifdef GLEW /* <-- glew */
#pragma warning(push, 0)
#define GL_GLEXT_PROTOTYPES
#define GLEW_STATIC
/* http://glew.sourceforge.net/ add include directories \include */
#include <GL/glew.h>
#pragma warning(pop)
#endif /* glew --> */
#include "Glew.h"

/** Load {OpenGL2+} from the library under {-D GLEW}.
 @return True if success. */
int Glew(void) {
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
