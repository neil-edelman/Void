#include "../Window.h" /* GLUT */

void TimerRun(const WindowIntAcceptor logic);
void TimerPause(void);
int TimerIsRunning(void);
unsigned TimerGetGameTime(void);
int TimerIsGameTime(const unsigned t);
unsigned TimerGetFrame(void);
unsigned TimerGetTime(void);
