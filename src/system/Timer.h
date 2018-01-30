#ifdef GLUT /* <-- glut */
#define TimerGetMs() (glutGet(GLUT_ELAPSED_TIME))
#elif defined(SDL) /* glut --><-- sdl */
#define TimerGetMs() (SDL_GetTicks())
#else /* sdl --><-- nothing */
#error Define GLUT or SDL.
#endif /* nothing --> */

#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif

#ifdef SDL /* <-- awkward */
unsigned TimerUpdate(void);
#endif /* awkward --> */
void TimerRun(void);
void TimerPause(void);
int TimerIsRunning(void);
unsigned TimerGetGameTime(void);
unsigned TimerGetMean(void);
int TimerIsTime(const unsigned t);
