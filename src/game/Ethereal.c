/** Copyright 2015 Neil Edelman, distributed under the terms of the GNU General
 Public License, see copying.txt */

#include <stdlib.h> /* malloc free */
#include <string.h> /* memcpy */
#include <math.h>   /* ftrig */
#include "../Print.h"
#include "../../bin/Lore.h" /* auto-generated; used in constructor */
#include "Game.h"
#include "Zone.h"
#include "Sprite.h"
#include "Ship.h"
#include "Ethereal.h"

/* They are in the forground but poll collision detection instead of
 interacting; they do something in the game. For example, gates are devices
 that can magically transport you faster than light, or powerups.

 @author	Neil
 @version	3.3, 2015-12
 @since		3.3, 2015-12 */

extern const struct Gate gate;

struct Ethereal {
	const char *name;
	struct Sprite *sprite;
	void (*callback)(struct Ethereal *, struct Sprite *);
	const struct SpaceZone *sz;
	int is_picked_up;
} ethereals[512];
static const int ethereals_capacity = sizeof ethereals / sizeof(struct Ethereal);
static int       ethereals_size;

void gate_travel(struct Ethereal *gate, struct Sprite *s);

/* public */

/** Get a new ethereal from the pool of unused.
 @return		A ethereal or null. */
struct Ethereal *Ethereal(const struct Image *image) {
	struct Ethereal *e;

	if(!image) return 0;
	if(ethereals_size >= ethereals_capacity) {
		Warn("Ethereal: couldn't be created; reached maximum of %u.\n", ethereals_capacity);
		return 0;
	}
	e = &ethereals[ethereals_size];
	e->name = "Ethereal";
	/* superclass */
	if(!(e->sprite = Sprite(S_ETHEREAL, image))) return 0;
	SpriteSetContainer(e, &e->sprite);
	e->callback     = 0;
	e->sz           = 0;
	e->is_picked_up = 0;
	ethereals_size++;
	Pedantic("Ethereal: created from pool, \"%s,\" Eth%u->Spr%u.\n", e->name, EtherealGetId(e), SpriteGetId(e->sprite));

	return e;
}

/** Get a new ethereal for the Gate type resource.
 @return		A ethereal or null. */
struct Ethereal *EtherealGate(const struct Gate *gate) {
	struct Ethereal *e;
	struct Image *img;

	if(!(img = ImageSearch("Gate.png"))) {
		Warn("Ethereal(Gate): couldn't find Gate.png.\n");
		return 0;
	}
	if(!(e = Ethereal(img))) return 0;
	SpriteSetOrientation(e->sprite, gate->x, gate->y, gate->theta);
	e->name     = gate->name;
	e->callback = &gate_travel;
	e->sz       = gate->to;
	Debug("Ethereal(Gate): created from Ethereal, \"%s,\" Eth%u->Spr%u.\n", e->name, EtherealGetId(e), SpriteGetId(e->sprite));
	Debug("Ethereal(Gate): \"%s,\" Gate res: (%d,%d:%f).\n", e->name, gate->x, gate->y, gate->theta);
	{
		float x, y, t;
		SpriteGetOrientation(e->sprite, &x, &y, &t);
		Debug("Ethereal(Gate): \"%s,\" Actual: (%f,%f:%f).\n", e->name, x, y, t);
	}
	return e;
}

/** Erase a ethereal from the pool (array of static ethereal.)
 @param ethereal_ptr	A pointer to the ethereal; gets set null on success. */
void Ethereal_(struct Ethereal **e_ptr) {
	struct Ethereal *e;
	int index;

	if(!e_ptr || !(e = *e_ptr)) return;
	index = e - ethereals;
	if(index < 0 || index >= ethereals_size) {
		Warn("~Ethereal: Eth%u not in range Eth%u.\n", EtherealGetId(e), ethereals_size);
		return;
	}
	Debug("~Ethereal: returning to pool, \"%s,\" Eth%u->Spr%u.\n", e->name, EtherealGetId(e), SpriteGetId(e->sprite));

	/* superclass */
	Sprite_(&e->sprite);

	/* move the terminal ethereal to replace this one */
	if(index < --ethereals_size) {
		memcpy(e, &ethereals[ethereals_size], sizeof(struct Ethereal));
		SpriteSetContainer(e, &e->sprite);
		Pedantic("~Ethereal: \"%s,\" Eth%u has become Eth%u.\n", e->name, ethereals_size + 1, EtherealGetId(e));
	}

	*e_ptr = e = 0;
}

/** Sets the size to zero; very fast, but should be protected: no one should
 call this except @see{SpriteClear}. */
void EtherealClear(void) {
	ethereals_size = 0;
}

void (*EtherealGetCallback(struct Ethereal *e))(struct Ethereal *, struct Sprite *) {
	return e->callback;
}

/*void EtherealSetVelocity(struct Ethereal *e, const float vx, const float vy) {
	if(!e || !e->sprite) return;
	SpriteSetVelocity(e->sprite, vx, vy);
}*/

int EtherealGetId(const struct Ethereal *e) {
	if(!e) return 0;
	return (int)(e - ethereals) + 1;
}

/*struct Sprite *EtherealGetSprite(const struct Ethereal *e) {
	if(!e) return 0;
	return e->sprite;
}*/

int EtherealIsDestroyed(const struct Ethereal *e) {
	if(!e) return -1;
	return e->is_picked_up ? -1 : 0;
}

void gate_travel(struct Ethereal *gate, struct Sprite *s) {
	struct Ship *ship;
	float gate_x, gate_y, gate_theta, gate_vx, gate_vy;
	float ship_x, ship_y, ship_theta, ship_vx, ship_vy;
	float x, y, vx, vy, gate_norm_x, gate_norm_y, proj, h;

	if(SpriteGetType(s) != S_SHIP) return;
	SpriteGetOrientation(gate->sprite, &gate_x, &gate_y, &gate_theta);
	SpriteGetVelocity(gate->sprite, &gate_vx, &gate_vy);
	SpriteGetOrientation(s, &ship_x, &ship_y, &ship_theta);
	SpriteGetVelocity(s, &ship_vx, &ship_vy);
	x = ship_x - gate_x;
	y = ship_y - gate_y;
	vx = ship_vx - gate_vx;
	vy = ship_vy - gate_vy;
	gate_norm_x = cosf(gate_theta);
	gate_norm_y = sinf(gate_theta);
	proj = x * gate_norm_x + y * gate_norm_y; /* proj is the new h */
	ship = SpriteGetContainer(s);
	h = ShipGetHorizon(ship);
	if(h > 0 && proj < 0) {
		Debug("Shp%u crossed into the event horizon of Eth%u.\n", ShipGetId(ship), EtherealGetId(gate));
		if(ship == GameGetPlayer()) {
			/* trasport to zone */
			Zone(gate->sz);
		} else {
			/* disappear */
			Ship_(&ship);
		}
	} else {
		/*Info("Foo h %f proj %f #%p\n", h, proj, ship);*/
		ShipSetHorizon(ship, proj); /* fixme: two things could confuse it */
		/*Info("Bar h is now %f\n", ShipGetHorizon(ship));*/
	}
}
