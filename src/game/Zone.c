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
#include "Game.h"
#include "Sprites.h"
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

/* public struct */
/*struct Zone {
	char *name;
	struct Gate *gate[1];
	struct ObjectInSpace ois[2];
} zone;*/

const struct AutoSpaceZone *current_zone;

#if 0
/** @fixme Player should be on it's own. */
static int remove_all_except_player(struct Sprite *const this) {
	const struct Ship *const player = GameGetPlayer();
	return !player || (struct Sprite *)player != this;
}

static int remove_all_events_except(struct Event *const victim) {
	/* we don't erase the player's recharge event nor any (one?) event that uses
	 ZoneChange because it's probably happening right now */
	return 0;/*fixme ShipGetEventRecharge(GameGetPlayer()) != victim
	&& EventGetConsumerAccept(victim) != (void (*)(void *))&ZoneChange;*/
}
#endif

/* public */

/** Clears, then sets up a new zone.
 @return		Success. */
void Zone(const struct AutoSpaceZone *const sz) {
	/*const struct TypeOfObject *asteroid_type = TypeOfObjectSearch("asteroid");*/
	const struct AutoShipClass *blob_class = AutoShipClassSearch("Blob");
	/*const struct AutoShipClass *fox_class = AutoShipClassSearch("Fox");*/
	const struct AutoDebris *asteroid = AutoDebrisSearch("Asteroid");
	int i;

	debug("Zone: SpaceZone %s is controlled by %s, contains gate %s and fars %s, %s.\n", sz->name, sz->government->name, sz->gate1->name, sz->ois1->name, sz->ois2->name);

	/* clear all objects */
	/*SpriteRemoveIf(&remove_all_except_player);*/
	/*EventRemoveIf(&remove_all_events_except);*/
	/*PlanetoidClear();*/

	/* set drawing elements */
	DrawSetBackground("Dorado.jpeg");
	/* fixme: set sunlight */

	/*Planetoid(sz->ois1);
	Planetoid(sz->ois2);
	Planetoid(sz->ois3);*/

	/*Gate(sz->gate1);*/

	/* update the current zone */
	current_zone = sz;

#if 0
	/* some asteroids */
	for(i = 0; i < 70/*00*/; i++) SpritesDebris(asteroid, 0);

	/* sprinkle some ships */
	for(i = 0; i < 1000; i++) SpritesShip(blob_class, 0, AI_DUMB);
#endif

}

#if 0
/** Zone change with the {gate}. */
void ZoneChange(const struct Gate *const gate) {
#if 0
	const struct AutoSpaceZone *const new_zone = GateGetTo(gate), *old_zone = current_zone;
	struct Sprite *new_gate;
	struct Sprite *player;
	const struct Ortho3f oldg;
	float oldg_x,   oldg_y,   oldg_theta,   oldg_vx,   oldg_vy;
	float newg_x,   newg_y,   newg_theta,   newg_vx,   newg_vy;
	float dg_x,     dg_y,     dg_theta,     dg_vx,     dg_vy;
	float dg_cos, dg_sin;
	float player_x, player_y, player_theta, player_vx, player_vy;
	float dp_x,     dp_y,     /*dp_theta,*/ dp_vx,     dp_vy;

	/* get old gate parametres */
	SpriteGetOrtho(&gate->sprite, &oldg);
	oldg_theta = SpriteGetTheta(gate);
	SpriteGetVelocity(gate, &oldg_vx, &oldg_vy);

	/* new zone */
	if(!new_zone) {
		warn("ZoneChange: %s does not have information about zone.\n", SpriteToString(gate));
		return;
	}
	Zone(new_zone);

	/* get new gate parametres; after the Zone changes! */
	if(!(new_gate = SpriteOutgoingGate(old_zone))) {
		warn("ZoneChange: there doesn't seem to be a gate back.\n");
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
		warn("ZoneChange: there doesn't seem to be a player.\n");
		return;
	}
	SpriteGetPosition(player, &player_x, &player_y);
	player_theta = SpriteGetTheta(player);
	SpriteGetVelocity(player, &player_vx, &player_vy);

	/* difference between the player and the old gate */
	dp_x     = player_x     - oldg_x;
	dp_y     = player_y     - oldg_y;
	/*dp_theta = player_theta - oldg_theta;*/
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
#endif
}

#endif
