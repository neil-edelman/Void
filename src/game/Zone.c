/** Copyright 2000, 2016 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt.

 @title Zone
 @fixme SpaceZone -> Sector */

#include <stdlib.h> /* rand (fixme) */
#include <math.h>	/* M_PI */
#include "../../build/Auto.h"
#include "../Print.h"
#include "../general/Events.h"
#include "../system/Draw.h"
#include "Sprites.h"
#include "Fars.h"
#include "Zone.h"

#ifndef M_PI
#define M_PI 3.141592653589793238462643383279502884197169399375105820974944592307816406
#endif

/* from Lore */
extern const struct AutoSpaceZone auto_space_zone[];
extern const int max_auto_space_zone;
extern const struct AutoGate auto_gate[];
extern const struct AutoObjectInSpace auto_object_in_space[];

/* from Game */
extern const float de_sitter;

const struct AutoSpaceZone *current_zone;

/** @implements <Sprite>Predicate */
static int all_except_player(const struct Sprite *const this) {
	return this != (struct Sprite *)SpritesGetPlayerShip();
}
/* * @implements <Event>Predicate
 @fixme We don't erase the player's recharge event nor any (one?) event that
 uses ZoneChange because it's probably happening right now. */
/*static int all_events_except_gate(const struct Event *const this) {
	UNUSED(this);
	return 1;
} <- We need to have more in {Events.c}, perhaps? */
/** Clears, then sets up a new zone. */
void Zone(const struct AutoSpaceZone *const sz) {
	const struct AutoShipClass *blob_class = AutoShipClassSearch("Blob");
	const struct AutoDebris *asteroid = AutoDebrisSearch("Asteroid");
	unsigned i;

	debug("Zone: SpaceZone %s is controlled by %s, contains gate %s and fars "
		"%s, %s.\n", sz->name, sz->government->name, sz->gate1->name,
		sz->ois1->name, sz->ois2->name);

	/* clear all objects */
	SpritesRemoveIf(&all_except_player);
	/* @fixme LightsClear();*/
	EventsClear();
	FarsClear();
	SpritesLightClear();

	/* set drawing elements */
	DrawSetBackground("Dorado.jpeg");
	/* fixme: set sunlight */

	FarsPlanetoid(sz->ois1);
	FarsPlanetoid(sz->ois2);
	FarsPlanetoid(sz->ois3);

	SpritesGate(sz->gate1);

	/* update the current zone */
	current_zone = sz;

	/* some asteroids */
	/*for(i = 0; i < 6400; i++) SpritesDebris(asteroid, 0);*/

	/* sprinkle some ships */
	for(i = 0; i < 3/*000*/; i++) SpritesShip(blob_class, 0, AI_DUMB);

}

/** Zone change with the {gate}.
 @implements SpriteConsumer<Gate>
 @fixme Broken. */
void ZoneChange(struct Gate *const gate) {
	const struct AutoSpaceZone *const new_zone = GateGetTo(gate),
		*old_zone = current_zone;
	struct Gate *new_gate;
	struct Ship *player;
	const struct Ortho3f *oldx, *oldv, *newx, *newv, *playerx, *playerv;
	struct Ortho3f dx, dv, playerdx, playerdv, finalx, finalv;
	float dx_cos, dx_sin;

	/* Get old gate parametres. */
	oldx = SpriteGetPosition((struct Sprite *)gate);
	oldv = SpriteGetVelocity((struct Sprite *)gate);
	assert(oldx && oldv);
	/* New zone. */
	if(!new_zone) { fprintf(stderr,
		"ZoneChange: does not have information about zone.\n"); return; }
	Zone(new_zone);
	/* Get new gate parametres; after the Zone changes. */
	if(!(new_gate = FindGate(old_zone)))
		{ fprintf(stderr, "ZoneChange: missing gate back.\n"); return; }
	newx = SpriteGetPosition((struct Sprite *)new_gate);
	newv = SpriteGetVelocity((struct Sprite *)new_gate);
	assert(newx && newv);
	/* Difference between the gates. */
	ortho3f_sub(&dx, newx, oldx);
	ortho3f_sub(&dv, newv, oldv);
	dx_cos = cosf(dx.theta), dx_sin = sinf(dx.theta);
	/* Get player parametres; after the Zone changes! */
	if(!(player = SpritesGetPlayerShip())) { fprintf(stderr,
		"ZoneChange: there doesn't seem to be a player.\n"); return; }
	playerx = SpriteGetPosition((struct Sprite *)player);
	playerv = SpriteGetVelocity((struct Sprite *)player);
	/* Difference between the player and the old gate. */
	ortho3f_sub(&playerdx, playerx, oldx);
	ortho3f_sub(&playerdv, playerv, oldv);
	/* Calculate new player parametres. */
	finalx.x     = newx->x - playerdx.x * dx_cos - playerdx.y * dx_sin;
	finalx.y     = newx->y + playerdx.x * dx_sin - playerdx.y * dx_cos;
	finalx.theta = playerx->theta + dx.theta;
	finalv.x     =-playerdv.x * dx_cos - playerdv.y * dx_sin;
	finalv.y     = playerdv.x * dx_sin - playerdv.y * dx_cos;
	finalv.theta = playerv->theta;
	SpriteSetPosition((struct Sprite *)player, &finalx);
	SpriteSetVelocity((struct Sprite *)player, &finalv);
}
