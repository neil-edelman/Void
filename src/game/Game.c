/** Copyright 2000, 2013 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt.

 Used to set up the Game and get it running.

 @title		Game
 @author	Neil
 @version	3.3, 2015-12
 @since		1.0, 1999 */

#include <stdlib.h> /* malloc free rand (fixme) */
/*#include <math.h>*/   /* M_PI (fixme: no, GNU) */
#include <string.h> /* strcmp for bsearch */
#include <stdio.h>  /* printf */
#include "../../build/Auto.h"
#include "../Print.h"
#include "Sprites.h"
#include "Zone.h"
#include "../general/Events.h"
#include "../system/Key.h"
#include "../system/Poll.h"
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
static struct Game {
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

static void quit(void) {
	printf("quit: key pressed.\n");
	exit(EXIT_SUCCESS); /* meh */
}

static void pause(void) {
	if(TimerIsRunning()) {
		TimerPause();
		printf("pause: on.\n");
	} else {
		TimerRun();
		printf("pause: off.\n");
	}
}

static void fps(void) {
	unsigned mean = TimerGetMean();
	info("%.1f fps, %ums average frame-time; %ums game-time.\n", 1000.0 / mean, mean, TimerGetGameTime());
}

static void position(void) {
	const struct Ship *const player = SpritesGetPlayerShip();
	const struct Ortho3f *x;
	if(!player) { printf("You are scattered across space.\n"); return; }
	x = SpriteGetPosition((const struct Sprite *)player);
	printf("You are %s at (%.1f, %.1f: %.1f).\n",
		SpritesToString((struct Sprite *)player), x->x, x->y, x->theta);
}

/* public */

/** destructor: fixme: what if there's a second game */
void Game_(void) {
	
	if(!is_started) return;
	
	debug("~Game: over.\n");
	is_started = 0;
}

/** Constructor. */
int Game(void) {
	struct Ortho3f posn = { 0.0f, 0.0f, 0.0f };
	const unsigned keys[] = { k_left, k_right, k_down, k_up, 32 };

	if(is_started) return 1;

	/* game elements */
	if(!(game.asteroid = AutoDebrisSearch("Asteroid"))
		|| !(game.nautilus = AutoShipClassSearch("Fox"))
		|| !(game.scorpion = AutoShipClassSearch("Blob"))
		|| !(game.start    = AutoSpaceZoneSearch("Earth"))) {
		debug("Game: couldn't find required game elements.\n");
		return 0;
	};

	/* register gameplay keys -- motion keys are polled in {@see GameUpdate} */
	KeyRegister(27,   &quit);
	KeyRegister('p',  &pause);
	KeyRegister(k_f1, &WindowToggleFullScreen);
	KeyRegister('f',  &fps);
	KeyRegister('x',  &position);
	KeyRegister('1',  &SpritesPlotSpace);
	/*
	KeyRegister('l',  &LightList);*/
	/*KeyRegister('s',  &SpriteList);*/
	/*if(KeyPress('q'))  printf("%dJ / %dJ\n", ShipGetHit(game.player), ShipGetMaxHit(game.player));
	if(KeyPress('f'))  printf("Foo!\n");
	if(KeyPress('a'))  SpritePrint("Game::update");*/
	Poll(&keys);

	/* set drawing elements */
	DrawSetShield("Bar.png");

	Zone(game.start);
	SpritesShip(game.nautilus, &posn, AI_HUMAN);

	EventsRunnable(7000, &fps);

	debug("Game: on.\n");
	is_started = 1;

	return -1;
}

/** updates the gameplay */
void GameUpdate(const int dt_ms) {
	if(!is_started) return;
	/* Update keys. */
	PollUpdate();
	/* Collision detect, move sprites, center on the player; a lot of work. */
	SpritesUpdate(dt_ms);
	/* Dispatch events; after update so that immidiate can be immediate. */
	EventsUpdate();
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
