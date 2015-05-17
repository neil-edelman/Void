/* Copyright 2000, 2013 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt */

/* 2012-12-25: overhauled the old version from MS-DOS ported to Windows 95,
 now it's OpenGL; I'm getting there! and merry Christmas */

/* 2014-07-28: so much shit to do */

#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* fprintf */
#include <math.h>   /* atan2 */
#include "Math.h"
#include "Open.h"
#include "Sprite.h"
#include "Light.h"
#include "Resources.h"
#include "Pilot.h"
#include "Game.h"

/* private structs */
struct Ship {
	char *name;
	int  shield;
	int  msShoot;
	int  msShield;
	struct Sprite *sprite;
};

/* public struct */
struct Game {
	struct Pilot *pilot;
	struct Vec2f camera;
	int noShips;
	struct Ship ship[256];
	struct Ship *player;
	struct Ship *ai;
	int enumSprites;
	struct Resources *resources;
};

const static int maxShips  = sizeof((struct Game *)0)->ship  / sizeof(struct Ship);

/* zero vector */
const struct Vec2f zero = { 0, 0 };
/* randomise */
const static float randNorm = 1.0f / RAND_MAX;
/* shadow engine (fixme: put in System picture) */
const static struct Colour3f starLux = { 64, 64, 64 }; /* lx */
/* ships */
const static int maxShields  = 320;     /* tesla */
const static int msRecharge  = 1024;    /* ms */
const static float turnRate  = 0.001f;  /* rad / ms */
const static float accelRate = 0.00008f;/* MN ms */
/* weapons */
/*const static float maxShotSpeed = 0.512;*/
const static float shotSpeed    = 0.26f;/* pixel / ms + ship */
const static float shotDistance = 4.0;  /* pixel; (5.0 :] 3.0 :[) so it won't hit the ship */
const static int   shotRange    = 768;  /* ms */
const static int   shotDamage   = 32;   /* tesla */
const static int   shotRechage  = 256;  /* ms */
const static float shotImpact   = 0.06f;/* MN ms */
/* ai */
const static float aiTurn     = 0.2f;  /* rad/ms */
const static float aiTooClose = 3200;  /* pixel^(1/2) */
const static float aiTooFar   = 32000; /* pixel^(1/2) */
const static float aiSpeed    = 15;    /* pixel^2 / ms; fixme: dependent on logic rate */
const static float aiTurnSloppy = 1;   /* rad */
/* asteroids */
const static float asteroid_max_speed = 0.03f;

/* private */

void do_player(struct Ship *player);
void do_ai(struct Ship *ai, const struct Ship *player);
struct Ship *ship(struct Game *game, struct Sprite *sprite, char *name);
void ship_(struct Game *game, const int no);

/* public */

/** constructor */
struct Game *Game(void) {
	struct TypeOfObject *too;
	struct ObjectsInSpace *ois;
	struct Bitmap *bitmap, *bmp[3];
	struct Game   *game;

	if(!(game = malloc(sizeof(struct Game)))) {
		perror("Game constructor");
		Game_(&game);
		return 0;
	}
	game->pilot    = 0;
	game->camera.x = 0.f;
	game->camera.y = 0.f;
	game->noShips  = 0;
	game->player = game->ai = 0;
	game->enumSprites = 0;
	game->resources = 0;
	/* load in stuff */
	if(!(game->resources = Resources())) {
		fprintf(stderr, "Couldn't load resources.\n");
		Game_(&game);
		return 0;
	}
	/* start human */
	if(!(game->pilot = Pilot())) {
		fprintf(stderr, "Couldn't load human.\n");
		Game_(&game);
		return 0;
	}
	/* fixme: have a function that randomises the names */
	if(!(bmp[0] = ResourcesFindBitmap(game->resources, "Nautilus_bmp"))
	   || !(bmp[1] = ResourcesFindBitmap(game->resources, "Scorpion_bmp"))
	   || !(bmp[2] = ResourcesFindBitmap(game->resources, "Asteroid_bmp"))) {
		Game_(&game);
		return 0;
	}
	game->player = ship(game, Sprite(bmp[0]->id, bmp[0]->width, bmp[0]->height), "Nautilus");
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
	}
	/*fprintf(stderr, "Game: ship_%d(p[%f,%f],r%d,s%d).\n", i, ship->p.x, ship->p.y, ship->r, ship->shield);*/
	/* set the star in the middle of the screen to light! */
	Light(zero, zero, starLux, 0, 0);

	fprintf(stderr, "Game: started #%p.\n", (void *)game);
	return game;
}

/** destructor */
void Game_(struct Game **gptr) {
	int i;
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
	*gptr = game = 0;
}

/** updates the gameplay */
void GameUpdate(struct Game *game, const int ms) {
	int i, damage;
	struct Vec2f impact;
	struct Ship *ship;

	/* quit */
	if(!game || OpenKeyPress(27)) exit(EXIT_SUCCESS);
	/* fullscreen */
	if(OpenKeyPress(k_f1)) OpenToggleFullScreen();
	if(OpenKeyPress('a')) {
		struct Vec2f cam;

		OpenPrint("ha\nha");
		OpenDebug();
		cam = GameGetCamera(game);
		printf("Camera (%f, %f).\n", cam.x, cam.y);
		OpenPrintMatrix();
	}
	/* user/ai */
	if(game->player) {
		struct Vec2f v;

		do_player(game->player);
		if(game->ai) {
			do_ai(game->ai, game->player);
		}
		/* update camera */
		v = SpriteGetPosition(game->player->sprite);
		game->camera.x = -v.x;
		game->camera.y = -v.y;
	}
	/* move the shots */
	LightUpdate(ms);
	/* move the ships */
	for(i = 0, ship = game->ship; i < game->noShips; ) {
		/* do collision detection; fixme: isn't that calculated already? */
		/* fixme: O(n^2)! */
		damage = LightCollide(ship->sprite, &impact);
		if(damage) {
			ship->shield -= damage;
			/* apply impact */
			impact.x *= shotImpact;
			impact.y *= shotImpact;
			SpriteImpact(ship->sprite, &impact);
		}
		/* move */
		SpriteUpdate(ship->sprite, ms);
		/* recharge shields (fixme: I had it better in the previous ver, whenShield) */
		if(ship->shield <= 0) {
			fprintf(stderr, "Ship %s damaged %d; destroyed!\n", ship->name, damage);
			ship_(game, i);
			continue;
		}
		if(damage) fprintf(stderr, "Ship %s damaged %d, shields %d.\n", ship->name, damage, ship->shield);
		if(ship->msShield >= maxShields) {
			ship->msShield = 0;
		} else if((ship->msShield += ms) >= msRecharge) {
			ship->shield += ship->msShield / msRecharge;
			ship->msShield %= msRecharge;
			if(ship->shield > maxShields) ship->shield = maxShields;
		}
		/* recharge weapons (fixme: whenShield) */
		if(ship->msShoot < shotRechage) ship->msShoot += ms;
		/* next ship */
		i++;
		ship++;
	}
}

struct Sprite *GameEnumSprites(struct Game *game) {
	if(!game) return 0;
	if(game->enumSprites >= game->noShips) {
		game->enumSprites = 0;
		return 0;
	}
	return game->ship[game->enumSprites++].sprite;
}

/** get the player's ship */
struct Vec2f GameGetCamera(const struct Game *game) {
	if(!game) return zero;
	return game->camera;
}

/** get the resources */
struct Resources *GameGetResources(const struct Game *game) {
	if(!game) return 0;
	return game->resources;
}

/* private */

void do_player(struct Ship *player) {
	int key;

	key = OpenKeyTime(k_left) - OpenKeyTime(k_right);
	SpriteSetOmega(player->sprite, key * turnRate);
	key = OpenKeyTime(k_up) - OpenKeyTime(k_down);
	if(key > 0) SpriteSetAcceleration(player->sprite, key * accelRate);
	if(OpenKeyTime(32) && player->msShoot >= shotRechage) {
		struct Colour3f colour = { 7.0f, 0, 0 };
		float        t = SpriteGetAngle(player->sprite);
		struct Vec2f p = SpriteGetPosition(player->sprite);
		struct Vec2f v = SpriteGetVelocity(player->sprite);
		struct Vec2f size = SpriteGetHalf(player->sprite);
		float s = sinf(t), c = cosf(t);
		/* randomise for prettyness */
		colour.r += 3.0f * randNorm * rand();
		/* take it off the ship and add some zing */
		p.x += c * (size.x + shotDistance);
		p.y += s * (size.y + shotDistance);
		v.x += c * shotSpeed;
		v.y += s * shotSpeed;
		/* put it all together */
		Light(p, v, colour, shotDamage, shotRange);
		player->msShoot = 0;
	}
}

void do_ai(struct Ship *ai, const struct Ship *player) {
	float d, t_delta, t;
	float        t_ai = SpriteGetAngle(ai->sprite);
	struct Vec2f p_ai = SpriteGetPosition(ai->sprite);
	struct Vec2f p_pl = SpriteGetPosition(player->sprite);
	struct Vec2f p;

	/* delta */
	p.x = p_pl.x - p_ai.x;
	p.y = p_pl.y - p_ai.y;
	/* the angle (atan2f should be used, but that's a unix ext) */
	t_delta = atan2f(p.y, p.x);
	/* distance */
	d = p.x * p.x + p.y * p.y;
	/* the ai ship aims where it thinks the player will be when it's shot gets
	 there, approx - too hard to beat! */
	/*target.x = ship[SHIP_PLAYER].p.x - Sin[ship[SHIP_PLAYER].r] * ship[SHIP_PLAYER].inertia.x * (disttoenemy / WEPSPEED) - ship[1].inertia.x;
	target.y = ship[SHIP_PLAYER].p.y + Cos[ship[SHIP_PLAYER].r] * ship[SHIP_PLAYER].inertia.y * (disttoenemy / WEPSPEED) - ship[1].inertia.y;
	disttoenemy = hypot(target.x - ship[SHIP_CPU].p.x, target.y - ship[SHIP_CPU].p.y);*/
	t = t_delta - t_ai;
	if(t >= (float)M_PI) {
		t -= 2 * (float)M_PI;
	} else if(t < -(float)M_PI) {
		t += 2 * (float)M_PI;
	}
	/* ai only does one thing at once, or else it would be hard */
	if(d < aiTooClose) {
		if(t < 0) t += (float)M_PI;
		else      t -= (float)M_PI;
	}
	if(t < -aiTurn || t > aiTurn) {
		SpriteSetOmega(ai->sprite, t * turnRate);
		if(t < 0) {
			t += (float)M_PI;
		} else {
			t -= (float)M_PI;
		}
	} else if(d > aiTooClose && d < aiTooFar) {
		if(ai->msShoot >= shotRechage) {
			struct Colour3f colour = { 0, 7.0f, 0 };
			struct Vec2f v = SpriteGetVelocity(ai->sprite);
			struct Vec2f size = SpriteGetHalf(ai->sprite);
			float s = sinf(t_ai), c = cosf(t_ai);
			/* randomise for prettyness */
			colour.g += 3.0f * randNorm * rand();
			/* take it off the ship and add some zing */
			p_ai.x += c * (size.x + shotDistance);
			p_ai.y += s * (size.y + shotDistance);
			v.x += c * shotSpeed;
			v.y += s * shotSpeed;
			/* put it all together */
			Light(p_ai, v, colour, shotDamage, shotRange);
			ai->msShoot = 0;
		}
	} else if(t > -aiTurnSloppy && t < aiTurnSloppy) {
		SpriteSetAcceleration(ai->sprite, aiSpeed * accelRate);
	}
}

/** new ship */
struct Ship *ship(struct Game *game, struct Sprite *sprite, char *name) {
	struct Ship *ship;
	if(!game || !sprite || game->noShips >= maxShips) {
		fprintf(stderr, "Game::ship: ship not created. :[\n");
		return 0;
	}
	ship = &game->ship[game->noShips];
	ship->name     = name;
	ship->shield   = maxShields;
	ship->msShoot  = 0;
	ship->msShield = 0;
	ship->sprite   = sprite;
	game->noShips++;
	SpriteRandomise(ship->sprite);
	fprintf(stderr, "Game::ship: ship %s (%d) created from pool.\n", name, game->noShips);
	return ship;
}
/*#include <string.h>*/
void ship_(struct Game *game, const int no) {
	char *name;

	if(!game || no < 0 || game->noShips <= no) {
		fprintf(stderr, "Game::ship~: couldn't delete ship %d because it does not exist.\n", no);
		return;
	}
	if(game->player == &game->ship[no]) {
		printf("Game::ship~: turning off input.\n");
		game->player = 0;
	} else if(game->ai == &game->ship[no]) {
		printf("Game::ship~: turning off AI.\n");
		game->ai = 0;
	}
	if(no < game->noShips - 1) {
		struct Ship *s, *z;
		s = &game->ship[no];
		z = &game->ship[game->noShips - 1];
		printf("Replacing ship %s with %s.\n", s->name, z->name);
		/* \/ statically linked
		printf("Sizeof %d\n", (int)sizeof(((struct Ship *)0)->name));
		strncpy(s->name, z->name, sizeof(((struct Ship *)0)->name) - 1);
		s->name[sizeof(((struct Ship *)0)->name) - 1] = '\0';*/
		name = s->name;
		s->name = z->name;
		s->shield = z->shield;
		s->msShoot = z->msShoot;
		s->msShield = z->msShield;
		s->sprite = z->sprite;
		/* !? Sprite_(&z->sprite); */
	} else {
		name = game->ship[no].name;
		Sprite_(&game->ship[no].sprite);
	}
	game->noShips--;
	fprintf(stderr, "Game::Ship~: deleted ship %s.\n", name);
}
