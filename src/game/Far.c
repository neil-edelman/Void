/** Copyright 2015 Neil Edelman, distributed under the terms of the GNU General
 Public License, see copying.txt.

 Sprites in the background have a (world) position, a rotation, and a bitmap.
 They are sorted by bitmap and drawn by the gpu in ../system/Draw but not lit.

 @title		Far
 @author	Neil
 @version	3.2, 2015-07
 @since		3.2, 2015-07 */

#include <string.h> /* memset */
#include "../../build/Auto.h"
#include "../general/OrthoMath.h"
#include "../Print.h"
#include "Far.h"
#include "../general/Sorting.h"
#include "../system/Draw.h"

/* the backgrounds can be larger than the sprites, 1024x1024? */
static const int half_max_size    = 512;
/* changed? update shader Far.vs! should be 0.00007:14285.714; space is
 big! too slow to get anywhere, so fudge it; basically, this makes space much
 smaller */
static const float foreshortening = 0.2f, one_foreshortening = 5.0f;

struct Far {
	const char *name;
	struct Ortho3f x;
	int   size;     /* the (x, y) size; they are the same */
	int   texture;  /* in the gpu */
	struct Far *prev_x, *next_x, *prev_y, *next_y; /* sort by axes */
	int   is_selected;
} backgrounds[1024];
static const unsigned backgrounds_capacity = sizeof(backgrounds) / sizeof(struct Far);
static unsigned       backgrounds_size;

static struct Far *first_x, *first_y; /* the projected axis sorting thing */

static struct Far *first_x_window, *first_y_window, *window_iterator;
/*static struct Far *iterator = backgrounds;*/ /* for drawing and stuff */

/* private prototypes */

/*static struct Far *iterate(void);*/
static void sort_notify(struct Far *);
static int compare_x(const struct Far *a, const struct Far *b);
static int compare_y(const struct Far *a, const struct Far *b);
static struct Far **address_prev_x(struct Far *const a);
static struct Far **address_next_x(struct Far *const a);
static struct Far **address_prev_y(struct Far *const a);
static struct Far **address_next_y(struct Far *const a);

/* public */

/** Get a new background sprite from the pool of unused.
 @param texture		On the GPU.
 @param size		Pixels.
 @return			The Far. */
struct Far *Far(const struct AutoObjectInSpace *ois) {
	struct Far *far;

	/* fixme: diurnal variation */

	if(!ois) return 0;
	if(backgrounds_size >= backgrounds_capacity) {
		warn("Far: couldn't be created; reached maximum of %u.\n",
			backgrounds_capacity);
		return 0;
	}
	far = &backgrounds[backgrounds_size++];

	far->name    = ois->name;
	far->x.x     = (float)ois->x;
	far->x.y     = (float)ois->y;
	far->x.theta = 0.0f;
	far->size    = ois->sprite->image->width;
	far->texture = ois->sprite->image->texture;

	far->prev_x                  = 0;
	far->next_x                  = first_x;
	far->prev_y                  = 0;
	far->next_y                  = first_y;
	if(first_x) first_x->prev_x = far;
	if(first_y) first_y->prev_y = far;
	first_x = first_y = far;
	sort_notify(far);

	debug("Far: created from pool \"%s,\" Far%u->Tex%u.\n", far->name,
		FarGetId(far), far->texture);

	return far;
}

/** Erase a sprite from the pool (array of static sprites.)
 @param sprite_ptr	A pointer to the sprite; gets set null on success. */
void Far_(struct Far **far_ptr) {
	struct Far *far, *replace, *neighbor;
	unsigned idx;

	if(!far_ptr || !(far = *far_ptr)) return;
	idx = (unsigned)(far - backgrounds);
	if(idx >= backgrounds_size) {
		warn("~Far: Far%u not in range Far%u.\n", idx + 1,backgrounds_size);
		return;
	}
	debug("~Far: returning to pool \"%s,\" Far%u->Tex%u.\n", far->name,
		FarGetId(far), far->texture);

	/* take it out of the lists */
	if(far->prev_x) far->prev_x->next_x = far->next_x;
	else           first_x            = far->next_x;
	if(far->next_x) far->next_x->prev_x = far->prev_x;
	if(far->prev_y) far->prev_y->next_y = far->next_y;
	else           first_y            = far->next_y;
	if(far->next_y) far->next_y->prev_y = far->prev_y;

	/* move the terminal far to replace this one */
	if(idx < --backgrounds_size) {

		replace = &backgrounds[backgrounds_size];
		memcpy(far, replace, sizeof(struct Far));

		/* prev, next, have to know about the replacement */
		if((neighbor = replace->prev_x)) neighbor->next_x = far;
		else                             first_x          = far;
		if((neighbor = replace->next_x)) neighbor->prev_x = far;
		if((neighbor = replace->prev_y)) neighbor->next_y = far;
		else                             first_y          = far;
		if((neighbor = replace->next_y)) neighbor->prev_y = far;

		pedantic("~Far: Far%u has become Far%u.\n",
			FarGetId(replace), FarGetId(far));
	}

	*far_ptr = far = 0;
}

/** Sets the size to zero; very fast. */
void FarClear(void) {
	debug("FarClear: clearing Fars.\n");
	first_x = first_y = first_x_window = first_y_window = 0;
	backgrounds_size = 0;
}

/** @return True while there are more Fars, sets the values. */
int FarIterate(struct Ortho3f *const r_ptr, unsigned *texture_ptr, unsigned *size_ptr) {
	static int x_min_window, x_max_window, y_min_window, y_max_window;
	static int x_min, x_max, y_min, y_max; /* these are more expansive */
	static int is_reset = -1; /* oy, static */
	struct Far *b, *feeler;
	struct Vec2f camera;

	/* go to the first spot in the window */
	if(is_reset) {
		unsigned w, h;
		struct Vec2i screen;
		/*DrawGetScreen(&screen);*/
		w = (screen.x  >> 1) + (screen.x & 1);
		h = (screen.y >> 1) + (screen.y & 1);
		/* determine the window */
		/*DrawGetCamera(&camera);*/
		camera.x *= foreshortening;
		camera.y *= foreshortening;
		x_min_window = (int)camera.x - w;
		x_max_window = (int)camera.x + w;
		y_min_window = (int)camera.y - h;
		y_max_window = (int)camera.y + h;
		x_min = x_min_window - half_max_size;
		x_max = x_max_window + half_max_size;
		y_min = y_min_window - half_max_size;
		y_max = y_max_window + half_max_size;
		/* no sprite anywhere? */
		if(!first_x_window && !(first_x_window = first_x)) return 0;
		/* place the first_x_window on the first x in the window */
		if(first_x_window->x.x < x_min) {
			do {
				if(!(first_x_window = first_x_window->next_x)) return 0;
			} while(first_x_window->x.x < x_min);
		} else {
			while((feeler = first_x_window->prev_x)
				  && (x_min <= feeler->x.x)) first_x_window = feeler;
		}
		/*debug("first_x_window (%f,%f) %d\n", first_x_window->x, first_x_window->y, first_x_window->texture);*/
		/* mark x; O(n) :[ */
		for(b = first_x_window; b && b->x.x <= x_max; b = b->next_x)
			b->is_selected = -1;
		/* now repeat for y; no sprite anywhere (this shouldn't happen!) */
		if(!first_y_window && !(first_y_window = first_y)) return 0;
		/* place the first_y_window on the first y in the window */
		if(first_y_window->x.y < y_min) {
			do {
				if(!(first_y_window = first_y_window->next_y)) return 0;
			} while(first_y_window->x.y < y_min);
		} else {
			while((feeler = first_y_window->prev_y)
				  && (y_min <= feeler->x.y)) first_y_window = feeler;
		}
		/* select the first y; fixme: should probably go the other way around,
		 less to mark since the y-extent is probably more; or maybe it's good
		 to mark more? */
		window_iterator = first_y_window;
		is_reset = 0;
	}
	
	/* consider y */
	while(window_iterator && window_iterator->x.y <= y_max) {
		if(window_iterator->is_selected) {
			int extent = (int)((window_iterator->size >> 1) + 1) * one_foreshortening;
			/* tighter bounds -- slow, but worth it; fixme: optimise for b-t */
			if(   window_iterator->x.x > x_min_window - extent
			   && window_iterator->x.x < x_max_window + extent
			   && window_iterator->x.y > y_min_window - extent
			   && window_iterator->x.y < y_max_window + extent) {
				/*debug("Sprite (%.3f, %.3f : %.3f) Tex%d size %d.\n", window_iterator->x, window_iterator->y, window_iterator->theta, window_iterator->texture, window_iterator->size);*/
				*r_ptr       = window_iterator->x;
				*texture_ptr = window_iterator->texture;
				*size_ptr    = window_iterator->size;
				window_iterator = window_iterator->next_y;
				return -1;
			}/* else {
			  debug("Tighter bounds rejected Spr%u(%f,%f)\n", SpriteGetId(window_iterator), window_iterator->x, window_iterator->y);
			  }*/
		}
		window_iterator = window_iterator->next_y;
	}

	/* reset for next time */
	for(b = first_x_window; b && b->x.x <= x_max; b = b->next_x)
		b->is_selected = 0;
	is_reset = -1;
	return 0;
}

/** Sets the orientation with respect to the screen, pixels and (0, 0) is at
 the centre.
 @param back	Which background to set.
 @param x		x
 @param y		y
 @param t		\theta */
void FarSetOrientation(struct Far *back, const float x, const float y, const float theta) {
	if(!back) return;
	back->x.x     = x/* * one_foreshortening*/;
	back->x.y     = y/* * one_foreshortening*/;
	back->x.theta = theta;
	inotify((void **)&first_x, (void *)back, (int (*)(const void *, const void *))&compare_x, (void **(*)(void *const))&address_prev_x, (void **(*)(void *const))&address_next_x);
	inotify((void **)&first_y, (void *)back, (int (*)(const void *, const void *))&compare_y, (void **(*)(void *const))&address_prev_y, (void **(*)(void *const))&address_next_y);
}

int FarGetId(const struct Far *b) {
	if(!b) return 0;
	return (int)(b - backgrounds) + 1;
}

/* private */

/** This is a private iteration which uses the same variable as Far::iterate.
 This actually returns a Far. Use it when you want to delete a sprite as you
 go though the list. */
/*static struct Far *iterate(void) {
	if(iterator >= backgrounds + backgrounds_size) {
		iterator = backgrounds;
		return 0;
	}
	return iterator++;
}*/

/* Sorts the sprites; they're (hopefully) almost sorted already from last
 frame, just freshens with insertion sort, O(n + m) where m is the
 related to the dynamicness of the scene
 @return	The number of equvalent-swaps (doesn't do this anymore.) */
/*static void sort(void) {
	isort((void **)&first_x,
		  (int (*)(const void *, const void *))&compare_x,
		  (void **(*)(void *const))&address_prev_x,
		  (void **(*)(void *const))&address_next_x);
	isort((void **)&first_y,
		  (int (*)(const void *, const void *))&compare_y,
		  (void **(*)(void *const))&address_prev_y,
		  (void **(*)(void *const))&address_next_y);
}*/

/** Keep it sorted when there is one element out-of-place. */
static void sort_notify(struct Far *s) {
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

/* for isort */

static int compare_x(const struct Far *a, const struct Far *b) {
	return a->x.x > b->x.x;
}

static int compare_y(const struct Far *a, const struct Far *b) {
	return a->x.y > b->x.y;
}

static struct Far **address_prev_x(struct Far *const a) {
	return &a->prev_x;
}

static struct Far **address_next_x(struct Far *const a) {
	return &a->next_x;
}

static struct Far **address_prev_y(struct Far *const a) {
	return &a->prev_y;
}

static struct Far **address_next_y(struct Far *const a) {
	return &a->next_y;
}
