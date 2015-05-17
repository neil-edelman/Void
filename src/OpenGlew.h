/* we need to go about using glew */
#ifdef GLEW
#define GL_GLEXT_PROTOTYPES /* *duh* */
#define GLEW_STATIC         /* (of course) */
/* http://glew.sourceforge.net/
 add include directories eg ...\glew-1.9.0\include */
#include <GL/glew.h>

#define GLUT_DISABLE_ATEXIT_HACK /* I don't exactly know what this does */
#define FREEGLUT_STATIC          /* (of course) */
/* http://freeglut.sourceforge.net/
 add include directories eg ...\freeglut-2.8.1\include */
#include <GL/glut.h>

#else
/* supports OpenGL 2.0 intrinsically (hopefully) */

#include <GLUT/glut.h> /* GLUT, done */

#endif

int OpenGlew(void);
