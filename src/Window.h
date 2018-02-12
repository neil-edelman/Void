/* {FreeGLUT} is a simple add-on to OpenGL that does other stuff. It includes
 all {OpenGL} files. */

#ifndef WINDOW_H /* <-- idempotent */
#define WINDOW_H

/* https://sourceforge.net/p/predef/wiki/OperatingSystems/ */
#if defined(__APPLE__) && defined(__MACH__) /* <-- macx */

/* Supports OpenGL 2.0 intrinsically as well as the original GLUT, but has
 non-std include directory for some reason. */
#include <GLUT/glut.h>

#else /* macx --><-- !macx */

#pragma warning(push, 0)
/* I don't exactly know what this does, but it seems to do nothing. */
#define GLUT_DISABLE_ATEXIT_HACK
#define FREEGLUT_STATIC /* (of course) */
/* \url{http://freeglut.sourceforge.net/} add include directories {\include}. */
#include <GL/freeglut.h>
#pragma warning(pop)

#endif /* !macx */

/* There are several GLUT functions that take this callback form. */
typedef void (*WindowGlutFunction)(int);

int WindowIsGlError(const char *function);
void WindowToggleFullScreen(void);

#endif /* idempotent --> */
