/* Copyright 2000, 2016 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt */

#include <stdlib.h> /* rand (fixme) */
#include <math.h>	/* M_PI */
#include "../Print.h"
#include "../../bin/Lore.h" /* auto-generated; used in constructor */
#include "Ship.h"
#include "Game.h"
#include "Sprite.h"
#include "Far.h"
#include "Debris.h"
#include "Wmd.h"
#include "Ethereal.h"
#include "Light.h"
#include "Event.h"
#include "../system/Draw.h"

#ifndef M_PI /* M_PI not defined in MSVC */
#define M_PI 3.14159265358979323846264338327950288419716939937510582097494459230781640628620899862803482534211706798214808651328230664
#endif

/* from Lore */
extern const struct SpaceZone space_zone[];
extern const int max_space_zone;
extern const struct Gate gate[];
extern const struct ObjectInSpace object_in_space[];

/* from Game */
extern const float de_sitter;

/* public struct */
struct Zone {
	char *name;
	struct Gate *gate[1];
	struct ObjectInSpace ois[2];
} zones[1024];
static const int zones_capacity = sizeof zones / sizeof(struct Zone);
static int       zones_size;

/* private prototypes */
static float rnd(const float limit);

/* public */

/** Clears, then sets up a new zone.
 @return		Success. */
void Zone(const struct SpaceZone *sz) {
	struct Debris *asteroid;
	struct Ship   *alien;
	/*struct Far    *bg;
	const struct ObjectInSpace *ois;
	const struct Gate *gt;*/
	const struct TypeOfObject *asteroid_type = TypeOfObjectSearch("asteroid");
	const struct ShipClass *scorpion_class = ShipClassSearch("Scorpion");
	int i;

	/* clear all objects */
	LightClear();
	EventClear();
	SpriteClear();
	FarClear();

	/*string name
	Alignment government
	Gate gate1
	ObjectInSpace spob1
	ObjectInSpace spob2*/

	Debug("Zone: SpaceZone %s is controlled by %s, contains gate %s and fars %s, %s.\n", sz->name, sz->government->name, sz->gate1->name, sz->ois1->name, sz->ois2->name);

	/* set drawing elements */
	DrawSetBackground("Dorado.jpeg");
	/* fixme: set sunlight */

	{
		struct Ethereal *gt = EtherealGate(sz->gate1);
		float x, y, t;
		SpriteGetOrientation(EtherealGetSprite(gt), &x, &y, &t);
		Info("Zone: Eth%u (%f,%f:%f)\n", EtherealGetId(gt), x, y, t);
	}
	Far(sz->ois1);
	Far(sz->ois2);
	Far(sz->ois3);

	Debug("Zone: popping player.\n");
	ShipPopPlayer();

	/* sprinkle some ships */
	for(i = 0; i < 100; i++) {
		alien = Ship(0, scorpion_class, B_STUPID);
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
		asteroid = Debris(asteroid_type->image, 10.0f);
		DebrisSetOrientation(asteroid,
							 x, y, t, /* (x,y):t */
							 vx, vy, o); /* (vx,vy):o */
		/*printf("Game: Spr%u: (%f,%f):%f v(%f,%f):%f\n", SpriteGetId(DebrisGetSprite(asteroid)), x, y, t, vx, vy, o);*/
	}

	/*Event(1000, FN_RUNNABLE, &say_hello);*/
	/*Event(1000, FN_BICONSUMER, &bi, calloc(128, sizeof(char)), 1);*/
}

/** "Random" -- used for initialising. FIXME: this will be used a lot! have
 Rando.c include all sorts of random fuctions.
 @param limit
 @return		A uniformly distributed random variable in the range
				[-limit, limit]. */
static float rnd(const float limit) {
	return limit * (2.0f * rand() / RAND_MAX - 1.0f);
}
