/* GLEW is set ourselves in the Makefile; Glew will include win.h in Windows,
 which is has a bajillon warnings. */
#ifdef GLEW
#pragma warning(push, 0)

/* GLEW allows OpenGL2+. */

#define GL_GLEXT_PROTOTYPES /* *duh* */
#define GLEW_STATIC         /* (of course) */
/* http://glew.sourceforge.net/
 add include directories eg ...\glew-1.9.0\include */
#include <GL/glew.h>

/* GLUT is a simple add-on to OpenGL that does other stuff. */

/* I don't exactly know what this does, but it seems to do nothing */
#define GLUT_DISABLE_ATEXIT_HACK
#define FREEGLUT_STATIC          /* (of course) */
/* http://freeglut.sourceforge.net/
 add include directories eg ...\freeglut-2.8.1\include */
#include <GL/glut.h>

#pragma warning(pop)
#else
/* Mac-like, supports OpenGL 2.0 intrinsically? */

#include <GLUT/glut.h> /* GLUT, done */

#endif
