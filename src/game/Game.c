/* Copyright 2000, 2013 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt */

#include <stdlib.h> /* malloc free rand (fixme) */
#include <math.h>   /* M_PI */
#include <string.h> /* strcmp for bsearch */
#include <stdio.h>  /* printf */
#include "../Print.h"
#include "Ship.h"
#include "Game.h"
#include "Sprite.h"
#include "Far.h"
#include "Debris.h"
#include "Wmd.h"
#include "Ethereal.h"
#include "Light.h"
#include "Event.h"
#include "Zone.h"
#include "../general/Map.h"
#include "../system/Key.h"
#include "../system/Window.h"
#include "../system/Draw.h"
#include "../system/Timer.h" /* only for reporting framerate */

/* auto-generated; used in constructor */
#include "../../bin/Lore.h"

/* Used to set up the Game and get it running.
 
 @author	Neil
 @version	3.3, 2015-12
 @since		1.0, 1999 */

/* from Lore */
/*extern struct Image images[];
extern const int max_images;
extern const struct TypeOfObject type_of_object[];
extern const int max_type_of_object;*/
extern const struct ObjectInSpace object_in_space[];
extern const int max_object_in_space;
extern const struct Gate gate[];
extern const int max_gate;
/*extern const struct SpaceZone space_zone[];
extern const int max_space_zone;*/

#ifndef M_PI /* M_PI not defined in MSVC */
#define M_PI 3.14159265358979323846264338327950288419716939937510582097494459230781640628620899862803482534211706798214808651328230664
#endif

static int is_started;

/* public struct */
struct Game {
	struct Ship *player; /* camera moves with this */
	int turning, acceleration, shoot; /* input per frame, ms */
	/* defined in Lore.h (hopefully!) */
	const struct TypeOfObject *asteroid;
	const struct ShipClass *nautilus, *scorpion;
	const struct SpaceZone *start;
} game;

static const float asteroid_max_speed = 0.03f;

/* positions larger then this value will be looped around */
const float de_sitter = 8192.0f;

/* private */
static void quit(void);
static void pause(void);
static void fps(void);
/*static float rnd(const float limit);*/
static void add_sprites(void);
static void poll_sprites(void);
static void position(void);
static void bi(char *, int);

/* public */

/** constructor */
int Game(void) {
	struct Debris *asteroid;
	struct Ship   *alien;
	struct Far    *bg;
	const struct ObjectInSpace *ois;
	const struct Gate *gt;
	int i;

	if(is_started) return -1;

	/* register gameplay keys -- motion keys are polled in {@see GameUpdate} */
	KeyRegister(27,   &quit);
	KeyRegister('p',  &pause);
	KeyRegister(k_f1, &WindowToggleFullScreen);
	KeyRegister('f',  &fps);
	KeyRegister('p',  &position);
	/*if(KeyPress('q'))  printf("%dJ / %dJ\n", ShipGetHit(game.player), ShipGetMaxHit(game.player));
	if(KeyPress('f'))  printf("Foo!\n");
	if(KeyPress('a'))  SpritePrint("Game::update");*/

	/* game elements */
	if(!(game.asteroid = TypeOfObjectSearch("asteroid"))
	   || !(game.nautilus = ShipClassSearch("Nautilus"))
	   || !(game.scorpion = ShipClassSearch("Scorpion"))
	   || !(game.start    = SpaceZoneSearch("Earth"))) {
		Debug("Game: couldn't find required game elements.\n");
		return 0;
	};

	/* set drawing elements */
	DrawSetShield("Bar.png");

#if 0
	/* read Zones */
	for(i = 0; i < max_space_zone; i++) {
		sz = &space_zone[i];
		zone = Zone(sz);
		Debug("Set up Zone: %s.\n", sz->name);
	}

	/* set up ALL Objects in Space */
	for(i = 0; i < max_object_in_space; i++) {
		ois = &object_in_space[i];
		bg  = Far(ois);
		Debug("Set up Object in Space: %s.\n", ois->name);
	}

	/* set up gates */
	for(i = 0; i < max_gate; i++) {
		gt = &gate[i];
		EtherealGate(gt);
		Debug("Set up Gate \"%s.\"\n", gt->name);
	}

	/* sprinkle some ships */
	game.player = Ship(&game.player, game.nautilus, B_HUMAN);
	for(i = 0; i < 100; i++) {
		alien = Ship(0, game.scorpion, B_STUPID);
		ShipSetOrientation(alien, rnd(de_sitter), rnd(de_sitter), rnd((float)M_PI));
	}

	/* some asteroids; fixme: debris limit 4096; sometimes it crashes when
	 reaching; gets incresingly slow after 1500, but don't need debris when
	 it's really far (cpu!) */
	for(i = 0; i < 1000; i++) {
		float x = rnd(de_sitter), y = rnd(de_sitter), t = rnd((float)M_PI), vx = rnd(50.0f), vy = rnd(50.0f), o = rnd(1.0f);
		/*printf("Game: new Asteroid, checking:\n");*/
		/*if(SpriteGetCircle(x, y, 0.5f*ImageGetWidth(img))) {
		 Debug("Game: would cause collision with sprite; waiving asteroid.\n");
		 continue;
		 }*/
		asteroid = Debris(game.asteroid->image, 10.0f);
		DebrisSetOrientation(asteroid,
							 x, y, t, /* (x,y):t */
							 vx, vy, o); /* (vx,vy):o */
		/*printf("Game: Spr%u: (%f,%f):%f v(%f,%f):%f\n", SpriteGetId(DebrisGetSprite(asteroid)), x, y, t, vx, vy, o);*/
	}
#endif

	/*Event(1000, FN_RUNNABLE, &say_hello);*/
	Event(1000, FN_BICONSUMER, &bi, calloc(128, sizeof(char)), 1);

	Zone(game.start);
	game.player = Ship(&game.player, game.nautilus, B_HUMAN);

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
	struct Ship *player;
	float dt_s = dt_ms * 0.001f;

	if(!is_started) return;

	/* update game time (fixme: time is accessed multiple ways, confusing) */
	/*game.t_s = t_ms * 0.001f;*/ /* fixme: may cause overflow; wrap-around will cause crazyness if you run it for 49.8 days (I assume; it will just
		wrap around and not do anthing crazy? because, if so 24.9 days) */
	/*game.dt_s = dt_ms * 0.001f;*/
	/* fixme: where is this even used? it should be taken out; it's numerically
	 unstable */

	/* in-game */
	game.turning      = KeyTime(k_left) - KeyTime(k_right);
	game.acceleration = KeyTime(k_up)   - KeyTime(k_down);
	game.shoot        = KeyTime(32);
	if(game.acceleration < 0) game.acceleration = 0;

	/* apply to player */
	if((player = game.player)) {
		float x, y, theta;

		ShipSetVelocities(player, game.turning, game.acceleration, dt_s);
		if(game.shoot) ShipShoot(player, 2);
		ShipGetOrientation(player, &x, &y, &theta);
		DrawSetCamera(x, y);
	}

	/* O(n) + O(n + m); collision detect + move sprites; a lot of work */
	SpriteUpdate(dt_s);

	/* O(n); call the ai and stuff */
	ShipUpdate(dt_s);

	/* check events */
	EventDispatch();
}

struct Ship *GameGetPlayer(void) {
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

/** "Random" -- used for initialising. FIXME: this will be used a lot! have
 Rando.c include all sorts of random fuctions.
 @param limit
 @return		A uniformly distributed random variable in the range
				[-limit, limit]. */
/*static float rnd(const float limit) {
	return limit * (2.0f * rand() / RAND_MAX - 1.0f);
}*/

static void add_sprites(void) {
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
	Event(5000, FN_RUNNABLE, &add_sprites);
}

static void poll_sprites(void) {
	int w, h;
	DrawGetScreen(&w, &h);
	printf("%d\t%d\t%d\t%.1f\t%d\t%d\n", SpriteNo(), SpriteGetConsidered(), SpriteGetOnscreen(), 1000.0 / TimerGetMean(), w, h);
	Event(500, FN_RUNNABLE, &poll_sprites);
}

static void position(void) {
	float x, y, t;
	if(!(game.player)) {
		Info("You are dead.\n");
		return;
	}
	ShipGetOrientation(game.player, &x, &y, &t);
	Info("Position(%.1f,%.1f:%.1f)\n", x, y, t);
}

static void bi(char *a, const int i) {
	if(--a[0] < 'A') a[0] = 'z';
	printf("!!!! %s %d\n", a, i);
	Event(1000, FN_BICONSUMER, &bi, a, i + 1);
}
