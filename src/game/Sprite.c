/* Copyright 2015 Neil Edelman, distributed under the terms of the GNU General
 Public License, see copying.txt */

#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* fprintf */
#include <math.h>   /* sqrtf, atan2f, cosf, sinf */
#include <string.h> /* memset */
#include "Sprite.h"
#include "../general/Sorting.h"
#include "../system/Timer.h" /* hmmm, Wmd should go in Wmd */
#include "../system/Draw.h"
/* types of sprites (subclasses) */
#include "Debris.h"
#include "Ship.h"
#include "Wmd.h"

/** Sprites have a (world) position, a rotation, and a bitmap. They are sorted
 by bitmap and drawn by the gpu in ../system/Draw based on the lighting. These
 are static; they have sprites_capacity maximum sprites. Sprites detect
 collisions. Sprites can be part of ships, asteroids, all sorts of gameplay
 elements, but this doesn't know that. However, background stuff is not
 sprites; ie, not collision detected, not lit.
 <p>
 These have a sort of dynamic inheritance controlled by
 {@code Sprite::setContainer}. It's C, so you have to call explicitly. Eg,
 Debris, Ship, Wmd, are all subclasses.
 <p>
 Fixme: a quadtree would maybe speed up the display? mmmmhhhhh, maybe a little
 with very large screens?

 @author	Neil
 @version	3.2, 2015-06
 @since		3.2, 2015-06 */

/* from Draw */
extern int screen_width, screen_height;

/* hmm, 256 is a lot of pixel space for the front layer, should be enough?
 the larger you set this, the farther it has to go to determine whether there's
 a collision, for every sprite! however, this means that the maximum value for
 a sprite is 256x256 to be guaranteed to be without clipping and collision
 artifacts */
static const int half_max_size = 128;
static const float epsilon = 0.0005f;
extern const float de_sitter;

struct Sprite {
	float x, y;     /* orientation */
	float theta;
	float vx, vy;
	float bounding; /* bounding circle radius */
	int   size;     /* the (x, y) size; they are the same */
	int   texture;  /* in the gpu */

	enum Sprites  type;       /* polymorphism */
	void          *container;
	struct Sprite **notify;   /* inheritance */

	int           is_selected;   /* temp; per object per frame */
	int           no_collisions; /* temp; per frame */
	float         x_min, x_max, y_min, y_max; /* temp; bounding box */
	float         t0_dt_collide; /* temp; time to collide over frame time */
	float         x1, y1;        /* temp; the spot where you want to go */
	float         vx1, vy1;      /* temp; the velocity after colliding */

	struct Sprite *prev_x, *next_x, *prev_y, *next_y; /* sort by axes */
} sprites[1024];
static const int sprites_capacity = sizeof(sprites) / sizeof(struct Sprite);
static int       sprites_size;

static struct Sprite *first_x, *first_y; /* the projected axis sorting thing */

static struct Sprite *first_x_window, *first_y_window, *window_iterator;
static struct Sprite *iterator = sprites; /* for drawing and stuff */

/* keep track of the dimensions of the window; it doesn't matter what the
 initial values are, they will be erased by {@code SpriteSetViewport} */
/* static int viewport_width  = 300;
static int viewport_height = 200; taken care of by global screen_* */

/* private prototypes */

static struct Sprite *iterate(void);
static void sort(void);
static void sort_notify(struct Sprite *s);
static void collide(struct Sprite *s);
static int collide_circles(const float a_x, const float a_y,
						   const float b_x, const float b_y,
						   const float c_x, const float c_y,
						   const float d_x, const float d_y,
						   const float r, float *t0_ptr);
static void elastic_bounce(struct Sprite *a, struct Sprite *b, const float t0);
static void push(struct Sprite *a, const float angle, const float amount);
static float mass(const struct Sprite *s);
static int compare_x(const struct Sprite *a, const struct Sprite *b);
static int compare_y(const struct Sprite *a, const struct Sprite *b);
static struct Sprite **address_prev_x(struct Sprite *const a);
static struct Sprite **address_next_x(struct Sprite *const a);
static struct Sprite **address_prev_y(struct Sprite *const a);
static struct Sprite **address_next_y(struct Sprite *const a);
static void deb_deb(struct Sprite *, struct Sprite *, const float);
static void shp_deb(struct Sprite *, struct Sprite *, const float);
static void deb_shp(struct Sprite *, struct Sprite *, const float);
static void wmd_deb(struct Sprite *, struct Sprite *, const float);
static void deb_wmd(struct Sprite *, struct Sprite *, const float);
static void wmd_shp(struct Sprite *, struct Sprite *, const float);
static void shp_wmd(struct Sprite *, struct Sprite *, const float);

static void (*const collision_matrix[3][3])(struct Sprite *, struct Sprite *, const float) = {
	{ &deb_deb, &shp_deb, &wmd_deb },
	{ &deb_shp, 0,        &wmd_shp },
	{ &deb_wmd, &shp_wmd, 0 }
};
static const int collision_matrix_size = sizeof(collision_matrix[0]) / sizeof(void *);

/* public */

/** Get a new sprite from the pool of unused. Should only be called from the
 consturctors of another class. You will most probably do an immediate
 {@code Sprite::setContainer} after to complete the "subclassing."
 @param type		Type of sprite.
 @param texture		On the GPU.
 @param size		Pixels.
 @return			True on success; the sprite will go in notify. */
struct Sprite *Sprite(const enum Sprites type, const int texture, const int size) {
	struct Sprite *sprite;

	if(sprites_size >= sprites_capacity) {
		fprintf(stderr, "Sprite: couldn't be created; reached maximum of %u.\n", sprites_capacity);
		return 0;
	}
	if(!texture || size <= 0) {
		fprintf(stderr, "Sprite: invalid.\n");
		return 0;
	}
	sprite = &sprites[sprites_size++];

	sprite->x  = sprite->y  = 0.0f;
	sprite->theta   = 0.0f;
	sprite->vx = sprite->vy = 0.0f;
	sprite->bounding= size * 0.5f; /* fixme! have a more sutble way */
	sprite->size    = size;
	sprite->texture = texture;

	sprite->type      = type;
	sprite->container = 0;
	sprite->notify    = 0;

	/* stick sprite onto the head of the lists */
	sprite->is_selected         = 0;
	sprite->no_collisions       = 0;
	sprite->x1 = sprite->y1 = 0.0f;
	sprite->prev_x              = 0;
	sprite->next_x              = first_x;
	sprite->prev_y              = 0;
	sprite->next_y              = first_y;
	if(first_x) first_x->prev_x = sprite;
	if(first_y) first_y->prev_y = sprite;
	first_x = first_y = sprite;

	/* sort it (a waste of time; we will immediately call another function to
	 change it's location; just in the interest of being pedantic) */
	sort_notify(sprite);

	fprintf(stderr, "Sprite: created from pool, Spr%u->Tex%u.\n", SpriteGetId(sprite), texture);

	return sprite;
}

/** Erase a sprite from the pool (array of static sprites.)
 @param sprite_ptr	A pointer to the sprite; gets set null on success. */
void Sprite_(struct Sprite **sprite_ptr) {
	struct Sprite *sprite, *replace, *neighbor;
	int index;

	if(!sprite_ptr || !(sprite = *sprite_ptr)) return;
	index = sprite - sprites;
	if(index < 0 || index >= sprites_size) {
		fprintf(stderr, "~Sprite: Spr%u not in range Spr%u.\n", index + 1, sprites_size);
		return;
	}
	fprintf(stderr, "~Sprite: returning to pool, Spr%u->Tex%u.\n", SpriteGetId(sprite), sprite->texture);

	/* deal with deleting it while iterating; fixme: have more complex? */
	if(sprite <= iterator) iterator--; /* <? */

	/* if it's the first x in the window, advance the first x */
	if(first_x_window == sprite) first_x_window = sprite->next_x;

	/* take it out of the lists */
	if(sprite->prev_x) sprite->prev_x->next_x = sprite->next_x;
	else               first_x                = sprite->next_x;
	if(sprite->next_x) sprite->next_x->prev_x = sprite->prev_x;
	if(sprite->prev_y) sprite->prev_y->next_y = sprite->next_y;
	else               first_y                = sprite->next_y;
	if(sprite->next_y) sprite->next_y->prev_y = sprite->prev_y;

	/* move the terminal sprite to replace this one */
	if(index < --sprites_size) {

		replace = &sprites[sprites_size];
		memcpy(sprite, replace, sizeof(struct Sprite));

		sprite->prev_x = replace->prev_x;
		sprite->next_x = replace->next_x;
		sprite->prev_y = replace->prev_y;
		sprite->next_y = replace->next_y;

		/* prev, next, have to know about the replacement */
		if((neighbor = replace->prev_x)) neighbor->next_x = sprite;
		else                             first_x          = sprite;
		if((neighbor = replace->next_x)) neighbor->prev_x = sprite;
		if((neighbor = replace->prev_y)) neighbor->next_y = sprite;
		else                             first_y          = sprite;
		if((neighbor = replace->next_y)) neighbor->prev_y = sprite;

		/* update child */
		if(sprite->notify) *sprite->notify = sprite;

		fprintf(stderr, "~Sprite: Spr%u has become Spr%u.\n", SpriteGetId(replace), SpriteGetId(sprite));
	}

	*sprite_ptr = sprite = 0;
}

/** Subclass informs us of new name (viz, location) on rename or create.
 @param container	The new subclass.
 @param notify		The pointer to the Sprite data in the subclass. */
void SpriteSetContainer(void *const container, struct Sprite **const notify) {
	struct Sprite *sprite;
	if(!notify || !(sprite = *notify)) return;
	sprite->container = container;
	sprite->notify    = notify;
}

#if 0
/** Returns true while there are more sprites, sets the values. The pointers
 need to all be there or else there will surely be a segfault.
 <p>
 Called in ../system/Draw to draw spites.
 <p>
 Supports deleting a Sprite with Sprite_().
 @param x_ptr		x
 @param y_ptr		y
 @param t_ptr		\theta
 @param texture_ptr	OpenGl texture unit.
 @param size_ptr	Size of the texture.
 @return			True if the values have been set. */
int SpriteIterate(float *x_ptr, float *y_ptr, float *theta_ptr, int *texture_ptr, int *size_ptr) {
	if(iterator >= sprites + sprites_size) {
		iterator = sprites;
		return 0;
	}
	*x_ptr       = iterator->x;
	*y_ptr       = iterator->y;
	*theta_ptr   = iterator->theta;
	*texture_ptr = iterator->texture;
	*size_ptr    = iterator->size;
	iterator++;
	return -1;
}

/** This allows you to go part-way though and reset. Not very useful. */
void SpriteResetIterator(void) { iterator = 0; }

#else

/** Returns true while there are more sprites, sets the values. The pointers
 need to all be there or else there will surely be a segfault.
 <p>
 Called in ../system/Draw to draw spites.
 <p>
 Only draws the sprites within the window.
 @param x_ptr		x
 @param y_ptr		y
 @param t_ptr		\theta
 @param texture_ptr	OpenGl texture unit.
 @param size_ptr	Size of the texture.
 @return			True if the values have been set. */
int SpriteIterate/*Window*/(float *x_ptr, float *y_ptr, float *theta_ptr, int *texture_ptr, int *size_ptr) {
	static int x_min_window, x_max_window, y_min_window, y_max_window;
	static int x_min, x_max, y_min, y_max;
	static int is_reset = -1; /* oy, static */
	struct Sprite *s, *feeler;
	float camera_x, camera_y;

	/* go to the first spot in the window */
	if(is_reset/*!window_iterator*/) {
		/* fixme: -50 is for debugging! take it out */
		int w = (screen_width  >> 1) + 1 - 50;
		int	h = (screen_height >> 1) + 1 - 50;
		/* determine the window */
		DrawGetCamera(&camera_x, &camera_y);
		x_min_window = camera_x - w;
		x_max_window = camera_x + w;
		y_min_window = camera_y - h;
		y_max_window = camera_y + h;
		x_min = x_min_window - half_max_size;
		x_max = x_max_window + half_max_size;
		y_min = y_min_window - half_max_size;
		y_max = y_max_window + half_max_size;
		/*fprintf(stderr, "window(%d:%d,%d,%d)\n", x_min, x_max, y_min, y_max);*/
		/* no sprite anywhere? */
		if(!first_x_window && !(first_x_window = first_x)) return 0;
		/* place the first_x_window on the first x in the window */
		if(first_x_window->x < x_min) {
			do {
				if(!(first_x_window = first_x_window->next_x)) return 0;
			} while(first_x_window->x < x_min);
		} else {
			while((feeler = first_x_window->prev_x)
				  && (x_min <= feeler->x)) first_x_window = feeler;
		}
		/*fprintf(stderr, "first_x_window (%f,%f) %d\n", first_x_window->x, first_x_window->y, first_x_window->texture);*/
		/* mark x; O(n) :[ */
		for(s = first_x_window; s && s->x <= x_max; s = s->next_x)
			s->is_selected = -1;
		/* now repeat for y; no sprite anywhere (this shouldn't happen!) */
		if(!first_y_window && !(first_y_window = first_y)) return 0;
		/* place the first_y_window on the first y in the window */
		if(first_y_window->y < y_min) {
			do {
				if(!(first_y_window = first_y_window->next_y)) return 0;
			} while(first_y_window->y < y_min);
		} else {
			while((feeler = first_y_window->prev_y)
				  && (y_min <= feeler->y)) first_y_window = feeler;
		}
		/* select the first y; fixme: should probably go the other way around,
		 less to mark since the y-extent is probably more; or maybe it's good
		 to mark more? */
		window_iterator = first_y_window;
		is_reset = 0;
	}

	/* consider y */
	while(window_iterator && window_iterator->y <= y_max) {
		if(window_iterator->is_selected) {
			int extent = (window_iterator->size >> 1) + 1;
			/* tighter bounds -- slow, but worth it; fixme: optimise for b-t */
			if(   window_iterator->x > x_min_window - extent
			   && window_iterator->x < x_max_window + extent
			   && window_iterator->y > y_min_window - extent
			   && window_iterator->y < y_max_window + extent) {
				/*fprintf(stderr, "Sprite (%.3f, %.3f : %.3f) Tex%d size %d.\n", window_iterator->x, window_iterator->y, window_iterator->theta, window_iterator->texture, window_iterator->size);*/
				*x_ptr       = window_iterator->x;
				*y_ptr       = window_iterator->y;
				*theta_ptr   = window_iterator->theta;
				*texture_ptr = window_iterator->texture;
				*size_ptr    = window_iterator->size;
				window_iterator = window_iterator->next_y;
				return -1;
			}/* else {
				fprintf(stderr, "Tighter bounds rejected Spr%u(%f,%f)\n", SpriteGetId(window_iterator), window_iterator->x, window_iterator->y);
			}*/
		}
		window_iterator = window_iterator->next_y;
	}

	/* reset for next time */
	for(s = first_x_window; s && s->x <= x_max; s = s->next_x)
		s->is_selected = 0;
	is_reset = -1;
	return 0;
}
#endif

/** Sets the orientation with respect to the screen, pixels and (0, 0) is at
 the centre.
 @param sprite	Which sprite to set.
 @param x		x
 @param y		y
 @param t		\theta */
void SpriteSetOrientation(struct Sprite *sprite, const float x, const float y, const float theta) {
	if(!sprite) return;
	sprite->x     = x;
	sprite->y     = y;
	sprite->theta = theta;
	inotify((void **)&first_x, (void *)sprite, (int (*)(const void *, const void *))&compare_x, (void **(*)(void *const))&address_prev_x, (void **(*)(void *const))&address_next_x);
	inotify((void **)&first_y, (void *)sprite, (int (*)(const void *, const void *))&compare_y, (void **(*)(void *const))&address_prev_y, (void **(*)(void *const))&address_next_y);
}

void SpriteSetVelocity(struct Sprite *sprite, const float vx, const float vy) {
	if(!sprite) return;
	sprite->vx    = vx;
	sprite->vy    = vy;
}

void SpriteSetTheta(struct Sprite *sprite, const float theta) {
	if(!sprite) return;
	sprite->theta = theta;
}

/** Allows you to get a single sprite if you know it's pointer. Again, all
 pointers must be valid.
 @param sprite	The sprite (this one can not be valid, in which case the others
				are not modified.)
 @param x_ptr	x
 @param y_ptr	y
 @param t_ptr	\theta */
void SpriteGetOrientation(const struct Sprite *sprite, float *x_ptr, float *y_ptr, float *theta_ptr) {
	if(!sprite) return;
	*x_ptr       = sprite->x;
	*y_ptr       = sprite->y;
	*theta_ptr   = sprite->theta;
}

void SpriteGetVelocity(const struct Sprite *sprite, float *vx_ptr, float *vy_ptr) {
	if(!sprite) return;
	*vx_ptr      = sprite->vx;
	*vy_ptr      = sprite->vy;
}

/** Gets the Sprite type that was assigned at the beginning. */
enum Sprites SpriteGetType(const struct Sprite *sprite) {
	if(!sprite) return S_NONE;
	return sprite->type;
}

/** You will need to {@code SpriteGetType} to determine the type. */
void *SpriteGetContainer(const struct Sprite *sprite) {
	if(!sprite) return 0;
	return sprite->container;
}

/** @return		The first sprite it sees in the circle. Not really useful now
				that it's negatively stable, but, whatever. */
struct Sprite *SpriteGetCircle(const float x, const float y, const float r) {
	struct Sprite *t, *ret = 0, *s_x0, *s_y0;
	float explore_x_min, explore_x_max, explore_y_min, explore_y_max;

	/* extend the bounds to check all of this space for collisions */
	explore_x_min = x - r - half_max_size;
	explore_x_max = x + r + half_max_size;
	explore_y_min = y - r - half_max_size;
	explore_y_max = y + r + half_max_size;

	/* this is probably a big waste of time, but don't call it in rt
	 fixme: always have it sorted? (isn't it?) */
	/*sort();*/

	/*SpritePrint("getCircle");
	printf("* checking (%f, %f)r%f\n", x, y, r);*/

	for(s_x0 = first_x; s_x0 && s_x0->x < explore_x_min; s_x0 = s_x0->next_x);
	/*if(!s_x0) { printf("*** no x in range; EXIT\n"); return 0; }
	printf("* x just before x-r: Spr%u (%.1f)\n", SpriteGetId(s_x0), s_x0->x);*/
	for(s_y0 = first_y; s_y0 && s_y0->y < explore_y_min; s_y0 = s_y0->next_y);
	/*if(!s_y0) { printf("*** no y in range; EXIT\n"); return 0; }
	printf("* y just before y-r: Spr%u (%.1f)\n", SpriteGetId(s_y0), s_y0->y);*/

	/* mark x */
	/*printf("* compiling list of x:");*/
	for(t = s_x0; t && t->x <= explore_x_max; t = t->next_x) {
		/*printf(" #%u", SpriteGetId(t));*/
		t->is_selected = -1;
	}

	/* consider y and maybe add it to the list of colliders */
	/*printf("\n* compiling list of y:");*/
	for(t = s_y0; t && t->y <= explore_y_max; t = t->next_y) {
		/*printf(" #%u", SpriteGetId(t));*/
		if(t->is_selected) {
			float dx = t->x - x, dy = t->y - y, dr = t->bounding + r;
			/*printf("(maybe: (%.1f, %.1f)r%.1f)", t->x, t->y, t->bounding);*/
			if(dx*dx + dy*dy <= dr*dr) {
				ret = t;
				break;
			}
		}
	}

	/* reset for next time; fixme: ugly */
	for(t = s_x0; t && t->x <= explore_x_max; t = t->next_x) {
		t->is_selected = 0;
	}
	/*printf("\n");*/
	/*printf("getCircle: found: #%u\n", ret ? SpriteGetId(ret) : 0);*/

	return ret;
}

/** This is where most of the work gets done. Called every frame, O(n). Also,
 this calls appropriate handlers for subclasses.
 @param dt	Seconds since the last frame. */
void SpriteUpdate(const float dt) {
	struct Sprite *s;
	struct Debris *debris;
	struct Ship *ship;
	struct Wmd *wmd;
	/*float omega;*/
	int t_ms = TimerLastTime();

	/* sorts the sprites; they're (hopefully) almost sorted already from last
	 frame, just freshens with insertion sort, O(n + m) where m is the
	 related to the dynamicness of the scene.
	 Nah, I changed it so they're always in order; probably change it back; it's
	 slightly more correct this way, but slower? */
	/*sort();*/

	/* pre-compute bounding boxes of where the sprite wants to go this frame,
	 containing the projected circle along a vector x -> x + v*dt */
	while((s = iterate())) {
		/* where they should be if there's no collision */
		s->x1 = s->x + s->vx * dt;
		s->y1 = s->y + s->vy * dt;
		/* extend the bounding box along the circle's trajectory */
		s->x_min = ((s->x <= s->x1) ? s->x  : s->x1) - s->bounding;
		s->x_max = ((s->x <= s->x1) ? s->x1 : s->x)  + s->bounding;
		s->y_min = ((s->y <= s->y1) ? s->y  : s->y1) - s->bounding;
		s->y_max = ((s->y <= s->y1) ? s->y1 : s->y)  + s->bounding;
	}

	/* dynamic; move the sprites around;
	 FIXME: unstable on interpenetration --fixed */
	while((s = iterate())) {

		/* this sets is_collision and (possibly) v[xy]1 */
		collide(s);

		if(!s->no_collisions) {
			/* no collisions; apply position change, calculated above */
			s->x = s->x1;
			s->y = s->y1;
		} else {
			/* if you skip this step, it's unstable on multiple collisions */
			const float one_collide = 1.0f / s->no_collisions;
			const float s_vx1 = s->vx1 * one_collide;
			const float s_vy1 = s->vy1 * one_collide;

			/* before and after collision;  */
			s->x += (s->vx * s->t0_dt_collide + s_vx1 * (1.0f - s->t0_dt_collide)) * dt;
			s->y += (s->vy * s->t0_dt_collide + s_vy1 * (1.0f - s->t0_dt_collide)) * dt;
			s->vx = s_vx1;
			s->vy = s_vy1;

			/* unmark for next frame */
			s->no_collisions = 0;
		}

		/* apply De Sitter spacetime */
		if(s->x      < -de_sitter) s->x =  de_sitter - 20.0f;
		else if(s->x >  de_sitter) s->x = -de_sitter + 20.0f;
		if(s->y      < -de_sitter) s->y =  de_sitter - 20.0f;
		else if(s->y >  de_sitter) s->y = -de_sitter + 20.0f;

		/* keep it sorted */
		sort_notify(s);

		/* static; each element does what it wants after the commotion */
		switch(SpriteGetType(s)) {
				/* fixme: have more drama then just deleting */
			case S_DEBRIS:
				if(DebrisIsDestroyed(debris = s->container)) {
					DebrisExplode(debris);
					Debris_(&debris);
				}
				break;
			case S_SHIP:
				if(ShipIsDestroyed(ship = s->container)) Ship_(&ship);
				break;
			case S_WMD:
				wmd = s->container;
				/* expire? note: wmd after is null and s is invalid */
				if(t_ms >= WmdGetExpires(wmd)) { Wmd_(&wmd); break; }
				/* update light */
				WmdUpdateLight(wmd);
				break;
			case S_NONE:
			default:
				fprintf(stderr, "Sprite::update: unknown type.\n");
		}
		/* if we were going to continue, we would check if s is valid (somehow) */
		/* Debris::hit: Deb80 destroyed.
		!!!!!!! ********* Deb80 is exploding, (0.000, 0.000).
	Sprite: created from pool, Spr97->Tex2.
	Debris: created from pool, Deb95->Spr97.
		!!!!nooooooo it's not at (0.000000, 0.000000)
		~Debris: returning to pool, Deb80->Spr80.
		~Sprite: returning to pool, Spr80->Tex2.
		~Sprite: Spr97 has become Spr80.
		~Debris: Deb95 has become Deb80.
		Debris::hit: Deb80 destroyed.
...
		uninteded interactions with new sprites? (what new sprites?) */
	}
}

/** This gets called from {@code Draw::resize}.
 @param width	px
 @param height	px
 @depriciated		extern screen_* takes care of it */
/*void SpriteSetViewport(const int width, const int height) {
	viewport_width  = width;
	viewport_height = height;
}*/

int SpriteGetId(const struct Sprite *s) {
	if(!s) return 0;
	return (int)(s - sprites) + 1;
}

void SpritePrint(const char *location) {
	struct Sprite *sprite = first_x;
	int i = 0;
	int q = 0;
	char *s;
	fprintf(stderr, "%s: x-list, l-r: {\n", location);
	while(sprite && i++ < 200) {
		switch(sprite->type) {
			case S_DEBRIS: s="Deb"; q = DebrisGetId(sprite->container); break;
			case S_SHIP: s = "Shp"; q = ShipGetId(sprite->container); break;
			case S_WMD:  s = "Wmd"; q = WmdGetId(sprite->container); break;
			default:     s = "???"; q = 0; break;
		}
		fprintf(stderr, " Spr%d (%.1f, %.1f) is %s%u,\n", (int)(sprite - sprites) + 1, sprite->x, sprite->y, s, q);
		sprite = sprite->next_x;
	}
	fprintf(stderr, "}\n%s: y-list, b-t: {\n", location);
	i = 0;
	sprite = first_y;
	while(sprite && i++ < 200) {
		switch(sprite->type) {
			case S_DEBRIS: s="Deb"; q = DebrisGetId(sprite->container); break;
			case S_SHIP: s = "Shp"; q = ShipGetId(sprite->container); break;
			case S_WMD:  s = "Wmd"; q = WmdGetId(sprite->container); break;
			default:     s = "???"; q = 0; break;
		}
		fprintf(stderr, " Spr%d (%.1f, %.1f) is %s%u,\n", (int)(sprite - sprites) + 1, sprite->x, sprite->y, s, q);
		sprite = sprite->next_y;
	}
	fprintf(stderr, "}\n");
}

/* private */

/** This is a private iteration which uses the same variable as Sprite::iterate.
 This actually returns a Sprite. Supports deleting Sprites (see @see{Sprite_}.)
 This iterates over the whole Sprite list, not only the ones in the window. */
static struct Sprite *iterate(void) {
	if(iterator >= sprites + sprites_size) {
		iterator = sprites;
		return 0;
	}
	return iterator++;
}

/** Sorts the sprites; they're (hopefully) almost sorted already from last
 frame, just freshens with insertion sort, O(n + m) where m is the
 related to the dynamicness of the scene
 @return	The number of equvalent-swaps (doesn't do this anymore.) */
static void sort(void) {
	isort((void **)&first_x,
		  (int (*)(const void *, const void *))&compare_x,
		  (void **(*)(void *const))&address_prev_x,
		  (void **(*)(void *const))&address_next_x);
	isort((void **)&first_y,
		  (int (*)(const void *, const void *))&compare_y,
		  (void **(*)(void *const))&address_prev_y,
		  (void **(*)(void *const))&address_next_y);
}

/** Keep it sorted when there is one element out-of-place. */
static void sort_notify(struct Sprite *s) {
	inotify((void **)&first_x,
			(void *)s,
			(int (*)(const void *, const void *))&compare_x,
			(void **(*)(void *const))&address_prev_x,
			(void **(*)(void *const))&address_next_x);
	inotify((void **)&first_y,
			(void *)s,
			(int (*)(const void *, const void *))&compare_y,
			(void **(*)(void *const))&address_prev_y,
			(void **(*)(void *const))&address_next_y);
}

/** First it uses the Hahn–Banach separation theorem (viz, hyperplane
 separation theorem) and the ordered list of projections on the x and y axis
 that we have been keeping, to build up a list of possible colliders based on
 their bounding circles. Calls {@see collide_circles}, and if it passed,
 {@see elastic_bounce}. Calls the items in the collision matrix.
 @return	False if the sprite has been destroyed. */
void collide(struct Sprite *a) {
	struct Sprite *b, *b_adj_y;
	void (*response)(struct Sprite *, struct Sprite *, const float);
	float explore_x_min, explore_x_max, explore_y_min, explore_y_max;
	float t0;

	if(!a) return;

	/* extend the bounds to check all of this space for collisions */
	explore_x_min = a->x_min - half_max_size;
	explore_x_max = a->x_max + half_max_size;
	explore_y_min = a->y_min - half_max_size;
	explore_y_max = a->y_max + half_max_size;

	/* mark x */
	for(b = a->prev_x; b && b->x >= explore_x_min; b = b->prev_x) {
		if(a < b && a->x_min < b->x_max) b->is_selected = -1;
	}
	for(b = a->next_x; b && b->x <= explore_x_max; b = b->next_x) {
		if(a < b && a->x_max > b->x_min) b->is_selected = -1;
	}

	/* consider y and maybe add it to the list of colliders */
	for(b = a->prev_y; b && b->y >= explore_y_min; b = b_adj_y) {
		b_adj_y = b->prev_y; /* b can be deleted; make a copy */
		if(b->is_selected
		   && a->y_min < b->y_max
		   && (response = collision_matrix[b->type - 1][a->type - 1])
		   && collide_circles(a->x, a->y, a->x1, a->y1, b->x, b->y, b->x1,
							  b->y1, a->bounding + b->bounding, &t0))
			response(a, b, t0);
	}
	for(b = a->next_y; b && b->y <= explore_y_max; b = b_adj_y) {
		b_adj_y = b->next_y;
		if(b->is_selected
		   && a->y_max > b->y_min
		   && (response = collision_matrix[b->type - 1][a->type - 1])
		   && collide_circles(a->x, a->y, a->x1, a->y1, b->x, b->y, b->x1,
							  b->y1, a->bounding + b->bounding, &t0))
			response(a, b, t0);
	}

	/* reset for next time; fixme: ugly */
	for(b = a->prev_x; b && b->x >= explore_x_min; b = b->prev_x) {
		b->is_selected = 0;
	}
	for(b = a->next_x; b && b->x <= explore_x_max; b = b->next_x) {
		b->is_selected = 0;
	}

	/*if(collide) printf("collide: Spr%u: Spr%u...\n", SpriteGetId(s), SpriteGetId(collide));*/
}

/** Given two seqments, u: a->b and v: c->d, and their combined radius, r,
 returns false if they do not collide, or true if they did, and in that case,
 t0_ptr will be assigned a time of collision.
 <p>
 This is called from {@see collide} as a more detailed check.
 @param a_x
 @param a_y
 @param b_x
 @param b_y		Point u moves from a to b.
 @param c_x
 @param c_y
 @param d_x
 @param d_y		Point v moves from c to d.
 @param r		The distance to test.
 @param t0_ptr	If collision is true, sets this to the intersecting time in
				[0, 1]. If it is 0, (u, v) is at (a, c), and 1, (c, d).
 @return		If they collided. */
static int collide_circles(const float a_x, const float a_y,
						   const float b_x, const float b_y,
						   const float c_x, const float c_y,
						   const float d_x, const float d_y,
						   const float r, float *t0_ptr) {
	/* t \in [0,1]
	             u = b - a
	             v = d - c
	 if(v-u ~= 0) t = doesn't matter, parallel-ish
	          p(t) = a + (b-a)t
	          q(t) = c + (d-c)t
	 distance(t)   = |q(t) - p(t)|
	 distance^2(t) = (q(t) - p(t))^2
	               = ((c+vt) - (a+ut))^2
	               = ((c-a) + (v-u)t)^2
	               = (v-u)*(v-u)t^2 + 2(c-a)*(v-u)t + (c-a)*(c-a)
	             0 = 2(v-u)*(v-u)t_min + 2(c-a)*(v-u)
	         t_min = -(c-a)*(v-u)/(v-u)*(v-u)
	 this is an optimisation, if t \notin [0,1] then pick the closest; if
	 distance^2(t_min) < r^2 then we have a collision, which happened at t0,
	           r^2 = (v-u)*(v-u)t0^2 + 2(c-a)*(v-u)t0 + (c-a)*(c-a)
	            t0 = [-2(c-a)*(v-u) - sqrt((2(c-a)*(v-u))^2
	                 - 4((v-u)*(v-u))((c-a)*(c-a) - r^2))] / 2(v-u)*(v-u)
	            t0 = [-(c-a)*(v-u) - sqrt(((c-a)*(v-u))^2
	                 - ((v-u)*(v-u))((c-a)*(c-a) - r^2))] / (v-u)*(v-u) */
	const float u_x   = b_x - a_x, u_y  = b_y - a_y;
	const float v_x   = d_x - c_x, v_y  = d_y - c_y;
	const float vu_x  = v_x - u_x, vu_y = v_y - u_y; /* vu = v - u */
	const float vu_2  = vu_x * vu_x + vu_y * vu_y;
	const float ca_x  = c_x - a_x, ca_y = c_y - a_y;
	const float ca_vu = ca_x * vu_x + ca_y * vu_y;
	float       t, dist_x, dist_y, dist_2, ca_2, det;

	/* time (t_min) of closest approach; we want the first contact */
	if(vu_2 < epsilon) {
		t = 0.0f;
	} else {
		t = -ca_vu / vu_2;
		if(     t < 0.0f) t = 0.0f;
		else if(t > 1.0f) t = 1.0f;
	}

	/* distance(t_min) of closest approach */
	dist_x = ca_x + vu_x * t;
	dist_y = ca_y + vu_y * t;
	dist_2 = dist_x * dist_x + dist_y * dist_y;

	/* not colliding */
	if(dist_2 >= r * r) return 0;

	/* collision time, t_0 */
	ca_2  = ca_x * ca_x + ca_y * ca_y;
	det   = (ca_vu * ca_vu - vu_2 * (ca_2 - r * r));
	t = (det <= 0.0f) ? 0.0f : (-ca_vu - sqrtf(det)) / vu_2;
	if(t < 0.0f) t = 0.0f; /* it can not be > 1 because dist < r^2 */
	*t0_ptr = t;

	return -1;
}

/** Elastic collision between circles; called from {@see collide} with
 {@code t0_dt} from {@see collide_circles}. Use this when we've determined that
 {@code Sprite a} collides with {@code Sprite b} at {@code t0_dt}, where
 Sprites' {@code x, y} are at time 0 and {@code x1, y1} are at time 1 (we will
 not get here, just going towards.) Velocities are {@code vx, vy} and this
 function is responsible for setting {@code vx1, vy1}, after the collision.
 Also, it may alter (fudge) the positions a little to avoid interpenetration.
 @param a		Sprite a.
 @param b		Sprite b.
 @param t0_dt	The fraction of the frame that the collision occurs, [0, 1]. */
static void elastic_bounce(struct Sprite *a, struct Sprite *b, const float t0_dt) {
	/* interpolate position of collision */
	const float a_x = a->x * t0_dt + a->x1 * (1.0f - t0_dt);
	const float a_y = a->y * t0_dt + a->y1 * (1.0f - t0_dt);
	const float b_x = b->x * t0_dt + b->x1 * (1.0f - t0_dt);
	const float b_y = b->y * t0_dt + b->y1 * (1.0f - t0_dt);
	/* delta */
	const float d_x = b_x - a_x;
	const float d_y = b_y - a_y;
	/* normal at point of impact; fixme: iffy infinities */
	const float n_d2 = d_x * d_x + d_y * d_y;
	const float n_n  = (n_d2 < epsilon) ? 1.0f / epsilon : 1.0f / sqrtf(n_d2);
	const float n_x  = d_x * n_n;
	const float n_y  = d_y * n_n;
	/* a's velocity, normal direction */
	const float a_nrm_s = a->vx * n_x + a->vy * n_y;
	const float a_nrm_x = a_nrm_s * n_x;
	const float a_nrm_y = a_nrm_s * n_y;
	/* a's velocity, tangent direction */
	const float a_tan_x = a->vx - a_nrm_x;
	const float a_tan_y = a->vy - a_nrm_y;
	/* b's velocity, normal direction */
	const float b_nrm_s = b->vx * n_x + b->vy * n_y;
	const float b_nrm_x = b_nrm_s * n_x;
	const float b_nrm_y = b_nrm_s * n_y;
	/* b's velocity, tangent direction */
	const float b_tan_x = b->vx - b_nrm_x;
	const float b_tan_y = b->vy - b_nrm_y;
	/* mass (assume all mass is positive!) */
	const float a_m = mass(a), b_m = mass(b);
	const float diff_m = a_m - b_m, invsum_m = 1.0f / (a_m + b_m);
	/* elastic collision */
	const float a_vx = a_tan_x + (a_nrm_x*diff_m + 2*b_m*b_nrm_x) * invsum_m;
	const float a_vy = a_tan_y + (a_nrm_y*diff_m + 2*b_m*b_nrm_y) * invsum_m;
	const float b_vx = (-b_nrm_x*diff_m + 2*a_m*a_nrm_x) * invsum_m + b_tan_x;
	const float b_vy = (-b_nrm_y*diff_m + 2*a_m*a_nrm_y) * invsum_m + b_tan_y;
	/* check for sanity */
	const float bounding = a->bounding + b->bounding;

	/*fprintf(stderr, "elasitic_bounce: colision between Spr%u-%u %f; sum_r %f\n",
			SpriteGetId(a), SpriteGetId(b), sqrtf(n_d2), bounding);*/

	/* interpenetation; happens about half the time because of IEEE754 numerics,
	 which could be on one side or the other; also, sprites that just appear,
	 multiple collisions interfering, and gremlins; you absolutly do not want
	 objects to get stuck orbiting each other */
	if(n_d2 < bounding * bounding) {
		const float push = (bounding - sqrtf(n_d2)) * 0.5f;
		/*fprintf(stderr, " \\pushing sprites %f distance apart\n", push);*/
		a->x -= n_x * push;
		a->y -= n_y * push;
		b->x += n_x * push;
		b->y += n_y * push;
	}

	if(!a->no_collisions) {
		/* first collision */
		a->no_collisions = 1;
		a->t0_dt_collide = t0_dt;
		a->vx1           = a_vx;
		a->vy1           = a_vy;
	} else {
		/* there are multiple collisions, rebound from the 1st one and add */
		if(t0_dt < a->t0_dt_collide) a->t0_dt_collide = t0_dt;
		/* fixme: UNSTABLE!!! --fixed */
		a->vx1           += a_vx;
		a->vy1           += a_vy;
		a->no_collisions++;
		/*fprintf(stderr, " \\%u collisions Spr%u (Spr%u.)\n", a->no_collisions, SpriteGetId(a), SpriteGetId(b));*/
	}
	if(!b->no_collisions) {
		/* first collision */
		b->no_collisions = 1;
		b->t0_dt_collide = t0_dt;
		b->vx1           = b_vx;
		b->vy1           = b_vy;
	} else {
		/* there are multiple collisions, rebound from the 1st one and add */
		if(t0_dt < b->t0_dt_collide) b->t0_dt_collide = t0_dt;
		b->vx1           += b_vx;
		b->vy1           += b_vy;
		b->no_collisions++;
		/*fprintf(stderr, " \\%u collisions Spr%u (Spr%u.)\n", b->no_collisions, SpriteGetId(b), SpriteGetId(a));*/
	}

}

/** Pushes a from angle, amount.
 @param a		The object you want to push.
 @param angle	Standard co\:ordainates, radians, angle.
 @param amount	tonne pixels / s, of course. */
static void push(struct Sprite *a, const float angle, const float amount) {
	a->vx += cosf(angle) * amount;
	a->vy += sinf(angle) * amount;
}

/* mass is used by {@see elasic_bounce} */
static float mass(const struct Sprite *s) {
	switch(SpriteGetType(s)) {
		case S_DEBRIS: return DebrisGetMass(SpriteGetContainer(s));
		case S_SHIP:   return ShipGetMass(SpriteGetContainer(s));
		case S_WMD:
		case S_NONE:
		default:       return 1.0f;
	}
}

/* for isort */

static int compare_x(const struct Sprite *a, const struct Sprite *b) {
	return a->x > b->x;
}

static int compare_y(const struct Sprite *a, const struct Sprite *b) {
	return a->y > b->y;
}

static struct Sprite **address_prev_x(struct Sprite *const a) {
	return &a->prev_x;
}

static struct Sprite **address_next_x(struct Sprite *const a) {
	return &a->next_x;
}

static struct Sprite **address_prev_y(struct Sprite *const a) {
	return &a->prev_y;
}

static struct Sprite **address_next_y(struct Sprite *const a) {
	return &a->next_y;
}

/* type collisions; can not modify list of Sprites as it is in the middle of
 x/ylist or delete! */

static void deb_deb(struct Sprite *a, struct Sprite *b, const float d0) {

	/* let's go for the elastic collision, later inelasic,
	 a_v = (a_v (a_m + b_m) + 2 b_m b_v) / (a_m + b_m)
	 b_v = (b_v (a_m + b_m) + 2 a_m a_v) / (a_m + b_m) */

	/* the first collision is the one from which we want to rebound */
	/*if(as->t0_dt_collide < bs->t0_dt_collide) as->t0_dt_collide = bs->t0_dt_collide; (now it's stored in the first)*/

	/*printf("debdeb: collide Deb(Spr%u): Deb(Spr%u)\n", SpriteGetId(a), SpriteGetId(b));*/
	/*printf("debdeb: #%u v(%.3f,%.3f) #%u v(%.3f,%.3f) -> ", SpriteGetId(as), as->vx1, as->vy1, SpriteGetId(bs), bs->vx1, bs->vy1);*/
	/*printf("#%u v(%.3f,%.3f) #%u v(%.3f,%.3f)\n", SpriteGetId(as), as->vx1, as->vy1, SpriteGetId(bs), bs->vx1, bs->vy1);*/

	elastic_bounce(a, b, d0);

	/* PV = nRT -> T = (PV)/(nR); enforce maximum temperature */
	/*DebrisEnforce(a->container);*/ /* FIXME!! */
	/*DebrisEnforce(b->container);*/

	/*{
		float limit = 500.0f;
		int foo = 0;

		if(a->vx1 > limit)       {a->vx1 = limit; foo=-1;}
		else if(a->vx1 < -limit) {a->vx1 = -limit; foo=-1;}
		if(a->vy1 > limit)       {a->vy1 = limit; foo=-1;}
		else if(a->vy1 < -limit) {a->vy1 = -limit; foo=-1;}

		if(b->vx1 > limit)       {b->vx1 = limit; foo=-1;}
		else if(b->vx1 < -limit) {b->vx1 = -limit; foo=-1;}
		if(b->vy1 > limit)       {b->vy1 = limit; foo=-1;}
		else if(b->vy1 < -limit) {b->vy1 = -limit; foo=-1;}

		if(foo) printf("debdeb: *******Spr%u-%u have to much kinetic energy; chill out.\n", SpriteGetId(a), SpriteGetId(b));
	}*/

	/*as->vx += 16.0f * rand() / RAND_MAX - 8.0f;
	as->vy += 16.0f * rand() / RAND_MAX - 8.0f;
	bs->vx += 16.0f * rand() / RAND_MAX - 8.0f;
	bs->vy += 16.0f * rand() / RAND_MAX - 8.0f;*/
}

static void shp_deb(struct Sprite *s, struct Sprite *d, const float d0) {
	/*printf("shpdeb: collide Shp(Spr%u): Deb(Spr%u)\n", SpriteGetId(s), SpriteGetId(d));*/
	elastic_bounce(s, d, d0);
	/*d->vx += 32.0f * (2.0f * rand() / RAND_MAX - 1.0f);
	d->vy += 32.0f * (2.0f * rand() / RAND_MAX - 1.0f);*/
}

static void deb_shp(struct Sprite *d, struct Sprite *s, const float d0) {
	shp_deb(s, d, d0);
}

static void wmd_deb(struct Sprite *w, struct Sprite *d, const float d0) {
	struct Wmd *wmd = SpriteGetContainer(w);
	struct Debris *deb = SpriteGetContainer(d);
	/* avoid inifinite destruction loop */
	if(WmdIsDestroyed(wmd) || DebrisIsDestroyed(deb)) return;
	push(d, atan2f(d->y - w->y, d->x - w->x), WmdGetImpact(wmd));
	DebrisHit(deb, WmdGetDamage(wmd));
	WmdForceExpire(wmd);
}

static void deb_wmd(struct Sprite *d, struct Sprite *w, const float d0) {
	wmd_deb(w, d, d0);
}

static void wmd_shp(struct Sprite *w, struct Sprite *s, const float d0) {
	struct Wmd *wmd = SpriteGetContainer(w);
	struct Ship *ship = SpriteGetContainer(s);
	/* avoid inifinite destruction loop */
	if(WmdIsDestroyed(wmd) || ShipIsDestroyed(ship)) return;
	push(s, atan2f(s->y - w->y, s->x - w->x), WmdGetImpact(wmd));
	ShipHit(ship, WmdGetDamage(wmd));
	WmdForceExpire(wmd);
}

static void shp_wmd(struct Sprite *s, struct Sprite *w, const float d0) {
	wmd_shp(w, s, d0);
}
