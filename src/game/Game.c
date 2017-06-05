/** Copyright 2000, 2013 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt.

 Used to set up the Game and get it running.

 @title		Game
 @author	Neil
 @version	3.3, 2015-12
 @since		1.0, 1999 */

#include <stdlib.h> /* malloc free rand (fixme) */
#include <math.h>   /* M_PI (fixme: no, GNU) */
#include <string.h> /* strcmp for bsearch */
#include <stdio.h>  /* printf */
#include "../../build/Auto.h"
#include "../Print.h"
#include "Sprite.h"
#include "Far.h"
#include "Event.h"
#include "Zone.h"
#include "Light.h"
#include "../system/Key.h"
#include "../system/Window.h"
#include "../system/Draw.h"
#include "../system/Timer.h" /* only for reporting framerate */
#include "Game.h"

/* from Auto */
extern const struct AutoObjectInSpace auto_object_in_space[];
extern const int max_auto_object_in_space;
extern const struct AutoGate auto_gate[];
extern const int max_auto_gate;

static int is_started;

/* public struct */
struct Game {
	struct Ship *player; /* camera moves with this */
	int ms_turning, ms_acceleration, ms_shoot; /* input per frame, ms */
	/* defined in Lore.h (hopefully!) */
	const struct AutoDebris *asteroid;
	const struct AutoShipClass *nautilus, *scorpion;
	const struct AutoSpaceZone *start;
} game;

static const float asteroid_max_speed = 0.03f;

/* positions larger then this value will be looped around;
 used in Sprite and Zone; if you change, you should also change waypoints in
 Sprite */
/*const float de_sitter = 8192.0f; !!! */

/* private */
static void quit(void);
static void pause(void);
static void fps(void);
static void gametime(void);
/*static float rnd(const float limit);*/
/*static void add_sprites(void);*/
/*static void poll_sprites(void);*/
static void position(void);

/* public */

/** constructor */
int Game(void) {

	if(is_started) return -1;

	/* register gameplay keys -- motion keys are polled in {@see GameUpdate} */
	KeyRegister(27,   &quit);
	KeyRegister('p',  &pause);
	KeyRegister(k_f1, &WindowToggleFullScreen);
	KeyRegister('f',  &fps);
	KeyRegister('a',  &position);
	KeyRegister('t',  &gametime);
	KeyRegister('l',  &LightList);
	/*KeyRegister('s',  &SpriteList);*/
	/*if(KeyPress('q'))  printf("%dJ / %dJ\n", ShipGetHit(game.player), ShipGetMaxHit(game.player));
	if(KeyPress('f'))  printf("Foo!\n");
	if(KeyPress('a'))  SpritePrint("Game::update");*/

	/* game elements */
	if(!(game.asteroid = AutoDebrisSearch("Asteroid"))
		|| !(game.nautilus = AutoShipClassSearch("Fox"))
		|| !(game.scorpion = AutoShipClassSearch("Blob"))
		|| !(game.start    = AutoSpaceZoneSearch("Earth"))) {
		debug("Game: couldn't find required game elements.\n");
		return 0;
	};

	/* set drawing elements */
	DrawSetShield("Bar.png");

	Zone(game.start);
	Event(0, 2000, 1000, FN_RUNNABLE, &position);
	game.player = Ship(game.nautilus, 0, SB_HUMAN);

	debug("Game: on.\n");
	is_started = -1;

	return -1;
}

/** destructor: fixme: what if there's a second game */
void Game_(void) {

	if(!is_started) return;

	debug("~Game: over.\n");
	is_started = 0;
}

/** updates the gameplay */
void GameUpdate(const int dt_ms) {

	if(!is_started) return;

	/* in-game */
	game.ms_turning      = KeyTime(k_left) - KeyTime(k_right);
	game.ms_acceleration = KeyTime(k_up)   - KeyTime(k_down);
	game.ms_shoot        = KeyTime(32);
	if(game.ms_acceleration < 0) game.ms_acceleration = 0;

	/* apply to player */
	if((game.player)) {
		struct Ortho2f x;
		ShipInput(game.player, game.ms_turning, game.ms_acceleration, dt_ms);
		if(game.ms_shoot) ShipShoot(game.player);
		ShipGetPosition(game.player, &x);
		DrawSetCamera(x.x, x.y);
	}

	/* O(n) + O(n + m); collision detect + move sprites; a lot of work */
	SpriteUpdate(dt_ms);

	/* check events */
	EventDispatch();
}

struct Ship *GameGetPlayer(void) {
	return game.player;
}

/* private */

static void quit(void) {
	debug("quit: key pressed.\n");
	exit(EXIT_SUCCESS); /* meh */
}

static void pause(void) {
	if(TimerIsRunning()) {
		TimerPause();
	} else {
		TimerRun();
	}
}

static void fps(void) {
	unsigned mean = TimerGetMean();
	info("%.1f fps, %ums average frame-time; %ums game-time.\n", 1000.0 / mean, mean, TimerGetGameTime());
}

static void gametime(void) {
	info("%u ms game time.\n", TimerGetGameTime());
}

/*static void add_sprites(void) {
	struct Debris *asteroid;
	struct Ship *bad;
	const int no = SpriteNo(), cap = SpriteGetCapacity() >> 5, rocks = 350, aliens = 10;
	int i;
	
	if(no + rocks + aliens >= cap) {
		debug("Game: reached the limit.\n");
		return;
	}
	debug("Game: adding more sprites.\n");
	for(i = 0; i < rocks; i++) {
		asteroid = Debris(game.asteroid->image, 10.0f);
		DebrisSetOrientation(asteroid, rnd(de_sitter), rnd(de_sitter), rnd((float)M_PI), rnd(50.0f), rnd(50.0f), rnd(1.0f));
	}
	for(i = 0; i < aliens; i++) {
		bad = Ship(0, game.scorpion, B_STUPID);
		ShipSetOrientation(bad, rnd(de_sitter), rnd(de_sitter), rnd((float)M_PI));
	}
	Event(5000, 500, FN_RUNNABLE, &add_sprites);
}*/

static void position(void) {
	if(!game.player) {
		info("You are scattered across space.\n");
		return;
	}
	SpriteOut((struct Sprite *)game.player);
}
