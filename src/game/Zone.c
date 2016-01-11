/* Copyright 2000, 2016 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt */

#include <stdlib.h> /* rand (fixme) */
#include <math.h>	/* M_PI */
#include "../Print.h"
#include "../../bin/Lore.h" /* auto-generated; used in constructor */
#include "Game.h"
#include "Far.h"
#include "Sprite.h"
#include "Light.h"
#include "Event.h"
#include "../system/Draw.h"

#ifndef M_PI
#define M_PI 3.141592653589793238462643383279502884197169399375105820974944592307816406
#endif

/* from Lore */
extern const struct SpaceZone space_zone[];
extern const int max_space_zone;
extern const struct Gate gate[];
extern const struct ObjectInSpace object_in_space[];

/* from Game */
extern const float de_sitter;

/* public struct */
/*struct Zone {
	char *name;
	struct Gate *gate[1];
	struct ObjectInSpace ois[2];
} zone;*/

const struct SpaceZone *current_zone;

/* private prototypes */
static float rnd(const float limit);
static int remove_all_except_player(struct Sprite *const victim);

/* public */

/** Clears, then sets up a new zone.
 @return		Success. */
void Zone(const struct SpaceZone *const sz) {
	struct Sprite *s;
	const struct TypeOfObject *asteroid_type = TypeOfObjectSearch("asteroid");
	const struct ShipClass *scorpion_class = ShipClassSearch("Scorpion");
	int i;

	Debug("Zone: SpaceZone %s is controlled by %s, contains gate %s and fars %s, %s.\n", sz->name, sz->government->name, sz->gate1->name, sz->ois1->name, sz->ois2->name);

	/* clear all objects */
	SpriteRemoveIf(&remove_all_except_player);
	LightClear();
	EventClear();
	FarClear();

	/* set drawing elements */
	DrawSetBackground("Dorado.jpeg");
	/* fixme: set sunlight */

	Far(sz->ois1);
	Far(sz->ois2);
	Far(sz->ois3);

	SpriteGate(sz->gate1);

	/* update the current zone */
	current_zone = sz;

	//Sprite(SP_SHIP, rnd(de_sitter), rnd(de_sitter), rnd((float)M_PI), scorpion_class, B_STUPID);
	Sprite(SP_DEBRIS, ImageSearch("Asteroid.png"), -100, -100, 1.0f, 50);

#if 0
	/* sprinkle some ships */
	for(i = 0; i < 100; i++) {
		Sprite(SP_SHIP, rnd(de_sitter), rnd(de_sitter), rnd((float)M_PI), scorpion_class, B_STUPID);
	}
#endif

#if 0
	/* some asteroids; fixme: debris limit 4096; sometimes it crashes when
	 reaching; gets incresingly slow after 1500, but don't need debris when
	 it's really far (cpu!) */
	for(i = 0; i < 1000; i++) {
		s = Sprite(SP_DEBRIS, asteroid_type->image, (int)rnd(de_sitter), (int)rnd(de_sitter), rnd((float)M_PI), 10);
		SpriteSetVelocity(s, rnd(50.0f), rnd(50.0f));
		SpriteSetOmega(s, rnd(1.0f));
	}
#endif

	/*Event(1000, FN_RUNNABLE, &say_hello);*/
	/*Event(1000, FN_BICONSUMER, &bi, calloc(128, sizeof(char)), 1);*/
}

void ZoneChange(const struct Sprite *const gate) {
	const struct SpaceZone *const new_zone = SpriteGetTo(gate), *old_zone = current_zone;
	struct Sprite *new_gate;
	struct Sprite *player;
	float oldg_x,   oldg_y,   oldg_theta,   oldg_vx,   oldg_vy;
	float newg_x,   newg_y,   newg_theta,   newg_vx,   newg_vy;
	float dg_x,     dg_y,     dg_theta,     dg_vx,     dg_vy;
	float dg_cos, dg_sin;
	float player_x, player_y, player_theta, player_vx, player_vy;
	float dp_x,     dp_y,     dp_theta,     dp_vx,     dp_vy;

	/* get old gate parametres */
	SpriteGetPosition(gate, &oldg_x, &oldg_y);
	oldg_theta = SpriteGetTheta(gate);
	SpriteGetVelocity(gate, &oldg_vx, &oldg_vy);

	/* new zone */
	if(!new_zone) {
		Warn("Zone::change: %s does not have information about zone.\n", SpriteToString(gate));
		return;
	}
	Zone(new_zone);

	/* get new gate parametres; after the Zone changes! */
	if(!(new_gate = SpriteOutgoingGate(old_zone))) {
		Warn("Zone::change: there doesn't seem to be a gate back.\n");
		return;
	}
	SpriteGetPosition(new_gate, &newg_x, &newg_y);
	newg_theta = SpriteGetTheta(new_gate);
	SpriteGetVelocity(new_gate, &newg_vx, &newg_vy);

	/* difference between the gates */
	dg_x     = newg_x     - oldg_x;
	dg_y     = newg_y     - oldg_y;
	dg_theta = newg_theta - oldg_theta;
	dg_vx    = newg_vx    - oldg_vx;
	dg_vy    = newg_vy    - oldg_vy;

	dg_cos = cosf(dg_theta);
	dg_sin = sinf(dg_theta);

	/* get player parametres; after the Zone changes! */
	if(!(player = GameGetPlayer())) {
		Warn("Zone::change: there doesn't seem to be a player.\n");
		return;
	}
	SpriteGetPosition(player, &player_x, &player_y);
	player_theta = SpriteGetTheta(player);
	SpriteGetVelocity(player, &player_vx, &player_vy);

	/* difference between the player and the old gate */
	dp_x     = player_x     - oldg_x;
	dp_y     = player_y     - oldg_y;
	//dp_theta = player_theta - oldg_theta;
	dp_vx    = player_vx    - oldg_vx;
	dp_vy    = player_vy    - oldg_vy;

	/* calculate new player parametres */
	player_x     =  newg_x - dp_x*dg_cos - dp_y*dg_sin;
	player_y     =  newg_y + dp_x*dg_sin - dp_y*dg_cos;
	player_theta += dg_theta;
	player_vx    = -dp_vx*dg_cos - dp_vy*dg_sin;
	player_vy    =  dp_vx*dg_sin - dp_vy*dg_cos;

	SpriteSetPosition(player, player_x, player_y);
	SpriteSetTheta(player, player_theta);
	SpriteSetVelocity(player, player_vx, player_vy);
}

/** "Random" -- used for initialising. FIXME: this will be used a lot! have
 Rando.c include all sorts of random fuctions.
 @param limit
 @return		A uniformly distributed random variable in the range
				[-limit, limit]. */
static float rnd(const float limit) {
	return limit * (2.0f * rand() / RAND_MAX - 1.0f);
}

static int remove_all_except_player(struct Sprite *const victim) {
	return GameGetPlayer() == victim ? 0 : -1;
}