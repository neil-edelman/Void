/** Copyright 2015 Neil Edelman, distributed under the terms of the GNU General
 Public License, see copying.txt */

#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* fprintf */
#include <string.h> /* memcpy */
#include "../EntryPosix.h" /* Debug, Pedantic */
#include "Debris.h"
#include "Sprite.h"

/* Debris is everything that doesn't have license, but moves around on a linear
 path, can be damaged, killed, and moved. Astroids and stuff. It would be nice
 if Debris could inherit from Sprite.

 @author	Neil
 @version	3.2, 2015-06
 @since		3.2, 2015-06 */

static const float epsilon              = 0.001f;
static const float hit_per_mass         = 5.0f;
static const float maximum_speed_2      = 20.0f; /* 2000? */
static const float explosion_elasticity = 0.5f; /* < 1 */
static const float minimum_mass         = 1.0f;

struct Debris {
	struct Sprite *sprite;
	float         omega, mass; /* Mg (t) */
	int           hit;
	/*void          (*on_kill)(void); minerals? */
} debris[4096];
static const int debris_capacity = sizeof(debris) / sizeof(struct Debris);
static int       debris_size;

/* public */

/** Get a new debris from the pool of unused.
 @return		A debris or null. */
struct Debris *Debris(const int texture, const int size, const float mass) {
	struct Debris *deb;

	if(debris_size >= debris_capacity) {
		fprintf(stderr, "Debris: couldn't be created; reached maximum of %u.\n", debris_capacity);
		return 0;
	}
	deb = &debris[debris_size];
	/* superclass */
	if(!(deb->sprite = Sprite(S_DEBRIS, texture, size))) return 0;
	SpriteSetContainer(deb, &deb->sprite);
	deb->omega      = 0.0f;
	deb->mass       = mass;
	deb->hit        = mass * hit_per_mass;
	/*deb->on_kill    = 0;*/
	debris_size++;
	if(Pedantic()) fprintf(stderr, "Debris: created from pool, Deb%u->Spr%u.\n", DebrisGetId(deb), SpriteGetId(deb->sprite));

	return deb;
}

/** Erase a debris from the pool (array of static debris.)
 @param debris_ptr	A pointer to the debris; gets set null on success. */
void Debris_(struct Debris **deb_ptr) {
	struct Debris *deb;
	int index;

	if(!deb_ptr || !(deb = *deb_ptr)) return;
	index = deb - debris;
	if(index < 0 || index >= debris_size) {
		fprintf(stderr, "~Debris: Deb%u not in range Deb%u.\n", DebrisGetId(deb), debris_size);
		return;
	}
	if(Pedantic()) fprintf(stderr, "~Debris: returning to pool, Deb%u->Spr%u.\n", DebrisGetId(deb), SpriteGetId(deb->sprite));

	/* superclass */
	Sprite_(&deb->sprite);

	/* move the terminal debris to replace this one */
	if(index < --debris_size) {
		memcpy(deb, &debris[debris_size], sizeof(struct Debris));
		SpriteSetContainer(deb, &deb->sprite);
		if(Pedantic()) fprintf(stderr, "~Debris: Deb%u has become Deb%u.\n", debris_size + 1, DebrisGetId(deb));
	}

	*deb_ptr = deb = 0;
}

/** Sets the orientation with respect to the screen, pixels and (0, 0) is at
 the centre.
 @param sprite	Which sprite to set.
 @param x		x (pixels)
 @param y		y (pixels)
 @param theta	\theta (radians)
 @param vx		dx/dt (pixels/second)
 @param vy		dy/dt (pixels/second)
 @param omega	\omega (radians/second) */
void DebrisSetOrientation(struct Debris *deb, const float x, const float y, const float theta, const float vx, const float vy, const float omega) {
	if(!deb || !deb->sprite) return;
	SpriteSetOrientation(deb->sprite, x, y, theta);
	SpriteSetVelocity(deb->sprite, vx, vy);
	/*deb->vx    = vx;
	deb->vy    = vy;*/
	deb->omega = omega;
	/*deb->is_stopped =
		(vx < epsilon && vx > -epsilon && vy < epsilon && vy > -epsilon
		 && omega < epsilon && omega > -epsilon) ? -1 : 0;*/
}

/* not called */
void DebrisGetOrientation(struct Debris *deb, float *x_ptr, float *y_ptr, float *theta_ptr) {
	if(!deb) return;
	SpriteGetOrientation(deb->sprite, x_ptr, y_ptr, theta_ptr);
}

void DebrisGetVelocity(const struct Debris *deb, float *vx_ptr, float *vy_ptr) {
	if(!deb || !deb->sprite) return;
	SpriteGetVelocity(deb->sprite, vx_ptr, vy_ptr);
}

void DebrisSetVelocity(struct Debris *deb, const float vx, const float vy) {
	if(!deb || !deb->sprite) return;
	SpriteSetVelocity(deb->sprite, vx, vy);
}

float DebrisGetMass(const struct Debris *d) {
	if(!d) return 1.0f;
	return d->mass;
}

int DebrisGetId(const struct Debris *d) {
	if(!d) return 0;
	return (int)(d - debris) + 1;
}

struct Sprite *DebrisGetSprite(const struct Debris *deb) {
	if(!deb) return 0;
	return deb->sprite;
}

void DebrisHit(struct Debris *deb, const int hit) {
	if(!deb) return;
	if(deb->hit > hit) {
		deb->hit -= hit;
		if(Debug()) fprintf(stderr, "Debris::hit: Deb%u hit %d, now %d.\n", DebrisGetId(deb), hit, deb->hit);
	} else {
		/*deb->hit = 0; should be carried forward to the next */
		deb->hit -= hit;
		if(Debug()) fprintf(stderr, "Debris::hit: Deb%u destroyed.\n", DebrisGetId(deb));
	}
}

int DebrisIsDestroyed(const struct Debris *d) {
	if(!d) return -1;
	return d->hit <= 0 ? -1 : 0;
}

/** Enforces the maximum speed by breaking the debris into smaller chunks; the
 process is not elastic, so it loses energy (speed.) */
void DebrisEnforce(struct Debris *deb) {
	float vx, vy;
	float speed_2;

	if(!deb || !deb->sprite) return;

	SpriteGetVelocity(deb->sprite, &vx, &vy);
	if((speed_2 = vx * vx + vy * vy) > maximum_speed_2) {
		if(Debug()) fprintf(stderr, "Debris::enforce: maximum %.3f\\,(pixels/s)^2, Deb%u is moving %.3f\\,(pixels/s)^2.\n", maximum_speed_2, DebrisGetId(deb), speed_2);
		DebrisExplode(deb);
	}
}

/** Spawns smaller Debris. Leaves you to destroy the large one. */
void DebrisExplode(struct Debris *deb) {
	struct Debris *sub;
	float x, y, theta, vx, vy;

	if(!deb || !deb->sprite) return;
	/*explosion_elasticity*/

	/* destroy, keeping values */
	SpriteGetOrientation(deb->sprite, &x, &y, &theta);
	SpriteGetVelocity(deb->sprite, &vx, &vy);
	if(Debug()) fprintf(stderr, "Deb%u is exploding at (%.3f, %.3f).\n", DebrisGetId(deb), x, y);

	/* how to break up */
	/*mass_div = */
	vx *= explosion_elasticity;
	vy *= explosion_elasticity;

	/* new debris */
	sub = Debris(2, 16.0f, 5.0f);
	SpriteSetOrientation(sub->sprite, x, y, theta);
	/*SpriteSetOrientation(sub->sprite, 100.0f, 100.0f, theta);*/
	SpriteGetOrientation(sub->sprite, &x, &y, &theta);
	SpriteSetVelocity(sub->sprite, vx, vy);
}
