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

/* private prototypes */
static float rnd(const float limit);
static int remove_all_except_player(struct Sprite *const victim);

/* public */

/** Clears, then sets up a new zone.
 @return		Success. */
void Zone(const struct SpaceZone *sz) {
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

void ZoneFoo(void) {
	Info("Z\n");
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
	const struct Sprite *p = GameGetPlayer();
	Info("remove_all_except_player: player %s; victim %s\n", SpriteToString(p), SpriteToString(victim));
	if(p == victim) {
		Debug("remove_all_except_player: spare %s\n", SpriteToString(victim));
		return 0;
	}
	Debug("remove_all_except_player: remove %s\n", SpriteToString(victim));
	return -1;
}
