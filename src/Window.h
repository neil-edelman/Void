/* {FreeGLUT} is a simple add-on to OpenGL that does other stuff. It includes
 all {OpenGL} files. */

#ifndef WINDOW_H /* <-- idempotent */
#define WINDOW_H

/* https://sourceforge.net/p/predef/wiki/OperatingSystems/ */
#if defined(__APPLE__) && defined(__MACH__) /* <-- macx */

/* We are on MacOSX? Supports OpenGL 2.0 intrinsically as well as the original
 GLUT, but has non-std include directory for no reason. */
#include <GLUT/glut.h>

#else /* macx --><-- !macx */

#ifdef _MSC_VER /* <-- msvc; rely on other comilers to suppress warnings in
system libraries on their own? */
#pragma warning(push, 0)
#endif /* msvc --> */

#define GLUT_DISABLE_ATEXIT_HACK
#define FREEGLUT_STATIC /* (of course) */
/* \url{http://freeglut.sourceforge.net/} add include directories {\include}. */
#include <GL/freeglut.h>

#ifdef _MSC_VER /* <-- msvc */
#pragma warning(pop)
#endif /* msvc --> */

#endif /* !macx */

/* There are several GLUT functions that take this callback form. */
typedef void (*WindowIntAcceptor)(int);

int WindowIsGlError(const char *function);
void WindowToggleFullScreen(void);

#endif /* idempotent --> */
