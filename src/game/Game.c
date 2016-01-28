/* Copyright 2000, 2013 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt */

/* Used to set up the Game and get it running.
 
 @author	Neil
 @version	3.3, 2015-12
 @since		1.0, 1999 */

#include <stdlib.h> /* malloc free rand (fixme) */
#include <math.h>   /* M_PI */
#include <string.h> /* strcmp for bsearch */
#include <stdio.h>  /* printf */
#include "../../bin/Auto.h"
#include "../Print.h"
#include "Sprite.h"
#include "Far.h"
#include "Event.h"
#include "Zone.h"
#include "Light.h"
#include "../general/Map.h"
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
	struct Sprite *player; /* camera moves with this */
	int ms_turning, ms_acceleration, ms_shoot; /* input per frame, ms */
	/* defined in Lore.h (hopefully!) */
	const struct AutoTypeOfObject *asteroid;
	const struct AutoShipClass *nautilus, *scorpion;
	const struct AutoSpaceZone *start;
} game;

static const float asteroid_max_speed = 0.03f;

/* positions larger then this value will be looped around;
 used in Sprite and Zone */
const float de_sitter = 8192.0f;

/* private */
static void quit(void);
static void pause(void);
static void fps(void);
static void gametime(void);
/*static float rnd(const float limit);*/
/*static void add_sprites(void);*/
/*static void poll_sprites(void);*/
static void position(void);
static void con(const char *const a);
static void bi(char *, char *);

/* public */

/** constructor */
int Game(void) {
	struct Sprite *s;

	if(is_started) return -1;

	/* register gameplay keys -- motion keys are polled in {@see GameUpdate} */
	KeyRegister(27,   &quit);
	KeyRegister('p',  &pause);
	KeyRegister(k_f1, &WindowToggleFullScreen);
	KeyRegister('f',  &fps);
	KeyRegister('z',  &position);
	KeyRegister('t',  &gametime);
	KeyRegister('l',  &LightList);
	/*if(KeyPress('q'))  printf("%dJ / %dJ\n", ShipGetHit(game.player), ShipGetMaxHit(game.player));
	if(KeyPress('f'))  printf("Foo!\n");
	if(KeyPress('a'))  SpritePrint("Game::update");*/

	/* game elements */
	if(!(game.asteroid = AutoTypeOfObjectSearch("asteroid"))
	   || !(game.nautilus = AutoShipClassSearch("Nautilus"))
	   || !(game.scorpion = AutoShipClassSearch("Scorpion"))
	   || !(game.start    = AutoSpaceZoneSearch("Earth"))) {
		Debug("Game: couldn't find required game elements.\n");
		return 0;
	};

	/* set drawing elements */
	DrawSetShield("Bar.png");

	Zone(game.start);
	Event('a', 3000, 1000, FN_CONSUMER, &con, "cool");
	Event('b', 2000, 1000, FN_RUNNABLE, &position);
	s = Sprite(SP_SHIP, 0, 0, 0.0, game.nautilus, B_HUMAN);
	SpriteSetNotify(s, &game.player);

	Debug("Game: on.\n");
	is_started = -1;

	return -1;
}

/** destructor: fixme: what if there's a second game */
void Game_(void) {

	if(!is_started) return;

	/*int i;
	struct Ship *ship;
	struct Game *game;
	if(!gptr || !(game = *gptr)) return;
	for(i = 0, ship = game->ship; i < game->noShips; i++, ship++) {
		Sprite_(&ship->sprite);
	}
	Pilot_(&game->pilot);
	Resources_(&game->resources);
	Debug("~Game: deleting #%p.\n", (void *)game);
	free(game);
	*gptr = game = 0;*/
	Debug("~Game: over.\n");
	is_started = 0;
}

/** updates the gameplay */
void GameUpdate(const int dt_ms) {

	if(!is_started) return;

	/* update game time (fixme: time is accessed multiple ways, confusing) */
	/*game.t_s = t_ms * 0.001f;*/ /* fixme: may cause overflow; wrap-around will cause crazyness if you run it for 49.8 days (I assume; it will just
		wrap around and not do anthing crazy? because, if so 24.9 days) */
	/*game.dt_s = dt_ms * 0.001f;*/
	/* fixme: where is this even used? it should be taken out; it's numerically
	 unstable */

	/* in-game */
	game.ms_turning      = KeyTime(k_left) - KeyTime(k_right);
	game.ms_acceleration = KeyTime(k_up)   - KeyTime(k_down);
	game.ms_shoot        = KeyTime(32);
	if(game.ms_acceleration < 0) game.ms_acceleration = 0;

	/* apply to player */
	if((game.player)) {
		float x, y;

		SpriteInput(game.player, game.ms_turning, game.ms_acceleration, dt_ms);
		if(game.ms_shoot) SpriteShoot(game.player);
		SpriteGetPosition(game.player, &x, &y);
		DrawSetCamera(x, y);
	}

	/* O(n) + O(n + m); collision detect + move sprites; a lot of work */
	SpriteUpdate(dt_ms);

	/* check events */
	EventDispatch();
}

struct Sprite *GameGetPlayer(void) {
	return game.player;
}

/* private */

static void quit(void) {
	Debug("Quit key pressed.\n");
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
	Info("%.1f fps, %ums average frame-time; %ums game-time.\n", 1000.0 / mean, mean, TimerGetGameTime());
}

static void gametime(void) {
	Info("%u ms game time.\n", TimerGetGameTime());
}

/** "Random" -- used for initialising. FIXME: this will be used a lot! have
 Rando.c include all sorts of random fuctions.
 @param limit
 @return		A uniformly distributed random variable in the range
				[-limit, limit]. */
/*static float rnd(const float limit) {
	return limit * (2.0f * rand() / RAND_MAX - 1.0f);
}*/

/*static void add_sprites(void) {
	struct Debris *asteroid;
	struct Ship *bad;
	const int no = SpriteNo(), cap = SpriteGetCapacity() >> 5, rocks = 350, aliens = 10;
	int i;
	
	if(no + rocks + aliens >= cap) {
		Debug("Game: reached the limit.\n");
		return;
	}
	Debug("Game: adding more sprites.\n");
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
	float x, y, t;
	if(!game.player) {
		Info("You are scattered across space.\n");
		return;
	}
	SpriteGetPosition(game.player, &x, &y);
	t = SpriteGetTheta(game.player);
	Info("Position(%.1f,%.1f:%.1f)\n", x, y, t);
}

static void con(const char *const a) {
	Info("con: %s\n", a);
}

static void bi(char *a, char *b) {
	if(--a[0] < 'A') a[0] = 'z';
	printf("!!!! %s : %s\n", a, b);
	Event('z', 1000, 100, FN_BICONSUMER, &bi, a, b);
}
