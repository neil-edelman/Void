/** 2018 Neil Edelman, distributed under the terms of the GNU General
 Public License 3, see copying.txt, or
 \url{ https://opensource.org/licenses/GPL-3.0 }.

 Very simple keyboard routines. {SDL} polls the keyboard, STILL! fuck that.

 @title		Key
 @author	Neil
 @std		C89/90
 @version	2018-01
 @since		2018-01 */

#include <stdio.h> /* fprintf */
#include "../WindowGl.h"
#include "Timer.h"
#include "Key.h"

static struct Key {
	int state;
	int down;
	int integral;
	int time;
	void (*handler)(void);
} keys[KEY_MAX];

#ifdef GLUT /* <-- glut */
/* private prototypes */
static enum Keys glut_to_keys(const int k);
static void key_down(unsigned char k, int x, int y);
static void key_up(unsigned char k, int x, int y);
static void key_down_special(int k, int x, int y);
static void key_up_special(int k, int x, int y);
#endif /* glut --> */

/** Attach the static keys to the Window depending on whether the Timer is
 active (poll) or not (direct to functions.)
 @return Success? */
void Key(void) {
#ifdef GLUT /* <-- glut */
	glutKeyboardFunc(&key_down);
	glutKeyboardUpFunc(&key_up);
	glutSpecialFunc(&key_down_special);
	glutSpecialUpFunc(&key_up_special); 
	fprintf(stderr, "Key: handlers set-up.\n");
#endif /* glut --> */
}

/** Registers a function to call asynchronously on press. */
void KeyRegister(const unsigned k, void (*const handler)(void)) {
	if(k >= KEY_MAX) return;
	keys[k].handler = handler;
}

/** Polls how long the key has been pressed, without repeat rate. Destructive.
 @param key		The key.
 @return		Number of ms. */
int KeyTime(const int key) {
	int time;
	struct Key *k;
	if(key < 0 || key >= KEY_MAX) return 0;
	k = &keys[key];
	if(k->state) {
		int ct  = TimerGetMs();
		time    = ct - k->down + k->integral;
		k->down = ct;
	} else {
		time    = k->integral;
	}
	k->integral = 0;
	return time;
}

#ifdef GLUT /* <-- glut */

/** GLUT_ to internal keys.
 @param k	Special key in OpenGl.
 @return	Keys. */
static enum Keys glut_to_keys(const int k) {
	switch(k) {
		case GLUT_KEY_LEFT:   return k_left;
		case GLUT_KEY_UP:     return k_up;
		case GLUT_KEY_RIGHT:  return k_right;
		case GLUT_KEY_DOWN:   return k_down;
		case GLUT_KEY_PAGE_UP:return k_pgup;
		case GLUT_KEY_PAGE_DOWN: return k_pgdn;
		case GLUT_KEY_HOME:   return k_home;
		case GLUT_KEY_END:    return k_end;
		case GLUT_KEY_INSERT: return k_ins;
		case GLUT_KEY_F1:     return k_f1;
		case GLUT_KEY_F2:     return k_f2;
		case GLUT_KEY_F3:     return k_f3;
		case GLUT_KEY_F4:     return k_f4;
		case GLUT_KEY_F5:     return k_f5;
		case GLUT_KEY_F6:     return k_f6;
		case GLUT_KEY_F7:     return k_f7;
		case GLUT_KEY_F8:     return k_f8;
		case GLUT_KEY_F9:     return k_f9;
		case GLUT_KEY_F10:    return k_f10;
		case GLUT_KEY_F11:    return k_f11;
		case GLUT_KEY_F12:    return k_f12;
		default: return k_unknown;
	}
}

/** Callback for {glutKeyboardFunc}. */
static void key_down(unsigned char k, int x, int y) {
	struct Key *key = &keys[k];
	if(key->state) return;
	key->state = -1;
	key->down  = TimerGetMs();
	if(key->handler) key->handler();
	/* fprintf(stderr, "key_down: key %d hit at %d ms.\n", k, key->down);*/
	UNUSED(x), UNUSED(y);
}

/** Callback for {glutKeyboardUpFunc}. */
static void key_up(unsigned char k, int x, int y) {
	struct Key *key = &keys[k];
	if(!key->state) return;
	key->state = 0;
	key->integral += TimerGetMs() - key->down;
	/* fprintf(stderr, "key_up: key %d pressed %d ms at end of frame.\n",k,key->integral);*/
	UNUSED(x), UNUSED(y);
}

/** Callback for {glutSpecialFunc}. */
static void key_down_special(int k, int x, int y) {
	struct Key *key = &keys[glut_to_keys(k)];
	if(key->state) return;
	key->state  = -1;
	key->down = TimerGetMs();
	if(key->handler) key->handler();
	/* fprintf(stderr, "key_down_special: key %d hit at %d ms.\n", k, key->down);*/
	UNUSED(x), UNUSED(y);
}

/** Callback for {glutSpecialUpFunc}. */
static void key_up_special(int k, int x, int y) {
	struct Key *key = &keys[glut_to_keys(k)];
	if(!key->state) return;
	key->state = 0;
	key->integral += TimerGetMs() - key->down;
	/* fprintf(stderr, "key_up_special: key %d pressed %d ms at end of frame.\n", k,
	 key->integral);*/
	UNUSED(x), UNUSED(y);
}

#endif /* glut --> */
