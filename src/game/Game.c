/* Copyright 2000, 2013 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt */

#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* fprintf */
#include <math.h>   /* M_PI */
#include "Ship.h"
#include "Game.h"
#include "Sprite.h"
#include "Background.h"
#include "Debris.h"
#include "Wmd.h"
#include "Light.h"
#include "Resources.h"
#include "../general/Map.h"
#include "../general/Image.h"
#include "../system/Key.h"
#include "../system/Window.h"
#include "../system/Draw.h"
#include "../EntryPosix.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288419716939937510582097494459230781640628620899862803482534211706798214808651328230664
#endif

static int is_started;

/* public struct */
struct Game {
	float t_s/*, dt_s*/;
	struct Ship *player; /* camera moves with this */
	int turning, acceleration, shoot; /* input per frame, ms */
} game;

const static float asteroid_max_speed = 0.03f;

/* positions larger then this value will be looped around */
const float de_sitter = 8192.0f;

/* private */
float rnd(const float limit);

/* public */

/** constructor */
int Game(void) {
/*	struct TypeOfObject *too;
	struct ObjectsInSpace *ois;
	struct Image *bitmap, *bmp[3];
	struct Game   *game;*/
	struct Map    *imgs;
	struct Image  *img;
	struct Debris *asteroid;
	struct Ship   *bad;
	struct Background *bg;
	int i;

	if(is_started) return -1;
	if(!(imgs = ResourcesGetImages())) {
		fprintf(stderr, "Game: couldn't get images.\n");
		return 0;
	}

	/* initilise */
	game.t_s /*= game.dt_s*/ = 0;

	/* start human */
	/*if(!(game->pilot = Pilot())) {
		fprintf(stderr, "Couldn't load human.\n");
		Game_(&game);
		return 0;
	}*/
	/* fixme: have a function that randomises the names */
	/*game->player = ship(game, Sprite(bmp[0]->id, bmp[0]->width, bmp[0]->height), "Nautilus");
	game->ai     = ship(game, Sprite(bmp[1]->id, bmp[1]->width, bmp[1]->height), "Scorpion");
	{
		int i;
		struct Ship *asteroid;

		for(i = 0; i < 64; i++) {
			asteroid = ship(game, Sprite(bmp[2]->id, bmp[2]->width, bmp[2]->height), "Asteriod");
			SpritePush(asteroid->sprite, rand() * randNorm * asteroid_max_speed);
		}
	}
	while((ois = ResourcesEnumObjectsInSpace(game->resources))) {
		fprintf(stderr, "Game: %d %s (%f, %f)\n", ois->id, ois->type_name, ois->position.x, ois->position.y);
		if(!(too = ois->type)) continue;
		if(!(bitmap = too->bmp)) continue;
		ship(game, Sprite(bitmap->id, bitmap->width, bitmap->height), "Resource");
	}*/
	/*fprintf(stderr, "Game: ship_%d(p[%f,%f],r%d,s%d).\n", i, ship->p.x, ship->p.y, ship->r, ship->shield);*/
	/* set the star in the middle of the screen to light! */

	/* initial conditions */
	/*game.star_light = Light(32.0f, 1.0f, 1.0f, 1.0f);
	game.cool_light = Light(16.0f, 0.0f, 0.0f, 1.0f);
	LightSetPosition(game.cool_light, -100.0f, -10.0f);*/

	/* width == height for sprites! it's your own fault if you do not respect this! (actually I'm lazy, and didn't want to stick another variable in
	    my calculations) */
	/* resource use on my computer:
	 100 -- 29%
	 150 -- 40%
	 300 -- 59%
	1000 -- 95%
	 collision detection is negligible, polygons are not (gpu-cpu bound?) */

	img = MapGet(imgs, "Asteroid_bmp");
	ImagePrint(img, stdout);

	/* some asteroids */
	for(i = 0; i < 512; i++) {
		float x = rnd(de_sitter), y = rnd(de_sitter), t = rnd((float)M_PI), vx = rnd(50.0f), vy = rnd(50.0f), o = rnd(1.0f);
		/*printf("Game: new Asteroid, checking:\n");*/
		if(SpriteGetCircle(x, y, 0.5f*ImageGetWidth(img))) {
			fprintf(stderr, "Game: would cause collision with sprite; waiving asteroid.\n");
			continue;
		}
		asteroid = Debris(ImageGetTexture(img), ImageGetWidth(img), 10.0f);
		DebrisSetOrientation(asteroid,
							 x, y, t, /* (x,y):t */
							 vx, vy, o); /* (vx,vy):o */
		/*printf("Game: Spr%u: (%f,%f):%f v(%f,%f):%f\n", SpriteGetId(DebrisGetSprite(asteroid)), x, y, t, vx, vy, o);*/
	}

	/* sprinkle some ships */
	img = MapGet(imgs, "Nautilus_bmp");
	game.player = Ship(&game.player, ImageGetTexture(img), ImageGetWidth(img), B_HUMAN);
	img = MapGet(imgs, "Scorpion_bmp");
	bad = Ship(0, ImageGetTexture(img), ImageGetWidth(img), B_STUPID);
	ShipSetOrientation(bad, 300.0f, 100.0f, -2.0f);
	bad = Ship(0, ImageGetTexture(img), ImageGetWidth(img), B_STUPID);
	ShipSetOrientation(bad, -300.0f, -100.0f, 1.0f);
	bad = Ship(0, ImageGetTexture(img), ImageGetWidth(img), B_STUPID);
	ShipSetOrientation(bad, 100.0f, -600.0f, 0.0f);
	bad = Ship(0, ImageGetTexture(img), ImageGetWidth(img), B_STUPID);
	ShipSetOrientation(bad, 300.0f, 600.0f, 0.0f);
	bad = Ship(0, ImageGetTexture(img), ImageGetWidth(img), B_STUPID);
	ShipSetOrientation(bad, -300.0f, 500.0f, 0.0f);

	/* planets */
	img = MapGet(imgs, "Mercury_bmp");
	bg  = Background(ImageGetTexture(img), ImageGetWidth(img));
	/*BackgroundSetOrientation(bg, -300.0f, 500.0f, 0.0f);*/
	img = MapGet(imgs, "Venus_bmp");
	bg  = Background(ImageGetTexture(img), ImageGetWidth(img));
	BackgroundSetOrientation(bg, 600.0f, 500.0f, 0.0f);
	img = MapGet(imgs, "Pluto_bmp");
	bg  = Background(ImageGetTexture(img), ImageGetWidth(img));
	BackgroundSetOrientation(bg, 200.0f, -400.0f, 0.0f);

#if 1
	img = MapGet(imgs, "Dorado_bmp");
	/*img = MapGet(imgs, "Pluto_bmp");*/
	fprintf(stderr, "Game: background Tex%u.\n", ImageGetTexture(img));
	DrawSetDesktop(img);
#endif

	fprintf(stderr, "Game: on.\n");
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
	fprintf(stderr, "~Game: deleting #%p.\n", (void *)game);
	free(game);
	*gptr = game = 0;*/
	fprintf(stderr, "~Game: over.\n");
	is_started = 0;
}

/** updates the gameplay */
void GameUpdate(const int t_ms, const int dt_ms) {
	struct Ship *player;
	float dt_s = dt_ms * 0.001f;

	if(!is_started) return;

	/* update game time (fixme: time is accessed multiple ways, confusing) */
	game.t_s = t_ms * 0.001f; /* fixme: may cause overflow; wrap-around will cause crazyness if you run it for 49.8 days (I assume; it will just
		wrap around and not do anthing crazy? because, if so 24.9 days) */
	/*game.dt_s = dt_ms * 0.001f;*/

	/* handle non-in-game related keypresses; fixme: have the keys be variable. */
	if(KeyPress(27)) {
		fprintf(stderr, "Escape key pressed.\n");
		exit(EXIT_SUCCESS); /* meh */
	}
	if(KeyPress('f'))  printf("Foo!\n");
	if(KeyPress(k_f1)) WindowToggleFullScreen();
	if(KeyPress('a'))  SpritePrint("Game::update");

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
}

struct Ship *GameGetPlayer(void) {
	return game.player;
}

/* private */

/** "Random" -- used for initialising.
 @param limit
 @return		A uniformly distributed random variable in the range
				[-limit, limit]. */
float rnd(const float limit) {
	return limit * (2.0f * rand() / RAND_MAX - 1.0f);
}
