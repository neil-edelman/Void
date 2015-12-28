/** Copyright 2015 Neil Edelman, distributed under the terms of the GNU General
 Public License, see copying.txt */

#include <stdlib.h> /* malloc free */
#include <string.h> /* memcpy */
#include "../Print.h"
#include "../../bin/Lore.h" /* auto-generated; used in constructor */
#include "Sprite.h"
#include "Ethereal.h"

/* They are in the forground but poll collision detection instead of
 interacting; they do something in the game. For example, gates are devices
 that can magically transport you faster than light, or powerups.

 @author	Neil
 @version	3.3, 2015-12
 @since		3.3, 2015-12 */

extern const struct Gate gate;

struct Ethereal {
	struct Sprite *sprite;
	int is_picked_up;
} ethereals[512];
static const int ethereals_capacity = sizeof ethereals / sizeof(struct Ethereal);
static int       ethereals_size;

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
	/* superclass */
	if(!(e->sprite = Sprite(S_ETHEREAL, image))) return 0;
	SpriteSetContainer(e, &e->sprite);
	e->is_picked_up = 0;
	ethereals_size++;
	Debug("Ethereal: created from pool, Eth%u->Spr%u.\n", EtherealGetId(e), SpriteGetId(e->sprite));

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
	Debug("~Ethereal: returning to pool, Eth%u->Spr%u.\n", EtherealGetId(e), SpriteGetId(e->sprite));

	/* superclass */
	Sprite_(&e->sprite);

	/* move the terminal ethereal to replace this one */
	if(index < --ethereals_size) {
		memcpy(e, &ethereals[ethereals_size], sizeof(struct Ethereal));
		SpriteSetContainer(e, &e->sprite);
		Pedantic("~Ethereal: Eth%u has become Eth%u.\n", ethereals_size + 1, EtherealGetId(e));
	}

	*e_ptr = e = 0;
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
