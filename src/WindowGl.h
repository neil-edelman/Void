/* Originally, FreeGLUT was used; it was awesome. Unfortunately, it didn't do
 quite what we wanted. SDL is a more extensive library and has replaced it. */

#if !defined(SDL) == !defined(GLUT)
#error Define one of GLUT or SDL.
#endif

/* GLUT is a simple add-on to OpenGL that does other stuff. */
#ifdef GLUT /* <-- glut */

/* GLUT is included on some computers already (eg, MacOSX.) Otherwise, define
 FREEGLUT. */
#ifdef FREEGLUT /* <-- free */

#pragma warning(push, 0)
/* I don't exactly know what this does, but it seems to do nothing
#define GLUT_DISABLE_ATEXIT_HACK */
#define FREEGLUT_STATIC /* (of course) */
/* http://freeglut.sourceforge.net/
 add include directories eg ...\freeglut-2.8.1\include */
#include <GL/glut.h>
#pragma warning(pop)

#else /* free --><-- !free */

/* Mac-like? We guess. Supports OpenGL 2.0 intrinsically as well as GLUT, we
 just need one include. */
#include <GLUT/glut.h>

#endif /* !free --> */

#else /* glut --><-- !glut */

/* SDL. */
/* Duh. */
#define SDL_MAIN_HANDLED
/* We'll take care of this, since the prototypes with SDL are not correct. */
#define NO_SDL_GLEXT
#ifdef GLEW /* <-- glew */
#include <SDL.h>
#include <SDL_opengl.h>
#else /* glew --><-- mac? */
#include <OpenGL/gl.h>
#include <SDL2/SDL.h> /* SDL_* */
/* #include <SDL2/SDL_opengl.h> what is this? */
#endif /* mac? --> */

#endif /* !glut --> */
