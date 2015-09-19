/* Copyright 2015 Neil Edelman, distributed under the terms of the GNU General
 Public License, see copying.txt */

#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* fprintf */
#include "../general/Sorting.h"
#include "../system/Draw.h"

/** This is my attempt to abstract functionality in C.
 <p>
 Fixme: a quadtree would maybe speed up the display? mmmmhhhhh, maybe a little
 with very large screens?
 <p>
 Simplified, now only goes one direction.

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

struct Layer {
	void *stuff;
	unsigned no_stuff;
	int (*compare)(const void *const, const void *const);
	void **(*address_prev)(const void *const);
	void **(*address_next)(const void *const);
	int (*bounding)(const void *const);
	void *first, *window_first, *iterator;
	int x_min, x_max, y_min, y_max;
	void *iterator;
	int half_max_size; /* pixels */
};

/* private prototypes */

static struct Sprite *iterate(void);
static void sort(void);
static void sort_notify(struct Sprite *s);
static int compare_x(const struct Sprite *a, const struct Sprite *b);
static int compare_y(const struct Sprite *a, const struct Sprite *b);

/* public */

/** */
struct Layer *Layer(const void *const stuff, const size_t size, const size_t x,
					const size_t y, const size_t prev, const size_t next,
					const size_t bounding, int half_max_size) {
	struct Layer *layer;

	if(!stuff || !size || x >= size || y >= size || prev_x >= size
	   || next_x >= size || prev_y >= size || next_y >= size || bounding >= size
	   || half_max_size <= 0) {
		fprintf(stderr, "Layer: passed invalid arguments.\n");
		return 0;
	}
	if(!(layer = malloc(sizeof(struct Layer)))) {
		perror("layer");
		return 0;
	}
	layer->stuff_size = size;
	layer->n          = 0;
	layer->stuff      = stuff;
	layer->x          = x;
	layer->y          = y;
	layer->prev       = prev;
	layer->next       = next;
	layer->bounding   = bounding;
	layer->first      = 0;
	layer->window_first = 0;
	layer->iterator   = 0;
	layer->x_min = layer->x_max = layer->y_min = layer->y_max = 0;
	layer->half_max_size = half_max_size;

	fprintf(stderr, "Layer: created Layer, #%p.\n", (void *)layer);

	return layer;
}

/** Erase.
 @param layer_ptr	A pointer to the layer; gets set null on success. */
void Layer_(struct Layer **const layer_ptr) {
	struct Layer *layer;

	if(!layer_ptr || !(layer = *layer_ptr)) return;
	fprintf(stderr, "~Layer: erase, #%p.\n", (void *)layer);
	free(layer);
	*layer_ptr = layer = 0;
}

/** Returns true while there is more stuff.
 <p>
 Supports deleting a Sprite with Sprite_().
 @param x_ptr		x
 @param y_ptr		y
 @param t_ptr		\theta
 @param texture_ptr	OpenGl texture unit.
 @param size_ptr	Size of the texture.
 @return			True if the values have been set. */
int LayerIterateAll(void *const stuff_ptr) {
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

/** Goes from left to right in the window.
 @param stuff_ptr	Must be an address of stuff.
 @return			True if the stuff has been set, false if the window has no
					more stuff. */
int LayerIterate(const struct Layer *const layer, void *const stuff_ptr) {
	struct void *stuff, *feeler;
	float camera_x, camera_y;

	if(!layer || !stuff_ptr) return 0;

	/* go to the first y in the window */
	if(!layer->iterator) {
		int y_min;
		/* fixme: no, confusing, have DrawGetScreen() */
		int w = (screen_width  >> 1) + (screen_width  & 1);
		int	h = (screen_height >> 1) + (screen_height & 1);
		/* determine the window */
		DrawGetCamera(&camera_x, &camera_y);
		/*layer->x_min = camera_x - w;
		layer->x_max = camera_x + w;
		layer->y_min = camera_y - h;
		layer->y_max = camera_y + h;*/
		y_min = camera_y - h - layer->half_max_size;
		/*fprintf(stderr, "window(%d:%d,%d,%d)\n", x_min, x_max, y_min, y_max);*/
		/* no sprite anywhere? */
		if(!layer->first_window && !(layer->first_window = layer->first)) return 0;
		/* place the first_window on the first y in the window;
		 fixme: this is messy and I just wrote it up ad-hoc */
		y = *(float *)((char *)layer->first_window + layer->y);
		if(y < y_min) {
			/* go forward */
			do {
				if(!(next = *(void **)((char *)layer->first_window + layer->next))) return 0;
				layer->first_window = next;
				y = *(float *)((char *)layer->first_window + layer->y);
			} while(y < y_min);
		} else {
			/* backtrack */
			while((feeler = *(void **)((char *)layer->first_window + layer->prev)
				   && (y_min <= *(float *)((char *)feeler + layer->y))) {
				layer->first_window = feeler;
			}
		}
		layer->x_min = camera_x - w - layer->half_max_size;
		layer->x_max = camera_x + w + layer->half_max_size;
		layer->y_min = y_min;
		layer->y_max = camera_y + h + layer->half_max_size;
		layer->iterator = layer->first_window;
	}

	/* iterate */
	while((it = layer->iterator)
		  && *(float *)((char *)it + layer->y) <= layer->y_max) {
		x = *(float *)((char *)it + layer->x);
		if(x < layer->min_x || x > layer->max_x) continue;
		/* tighter bounds */
		y        = *(float *)((char *)it + layer->y);
		bounding = *(float *)((char *)it + layer->bounding);
		extent   = (int)bounding - layer->half_max_size;
		if(   x > layer->x_min - extent
		   && x < layer->x_max + extent
		   && y > layer->y_min - extent
		   && y < layer->y_max + extent) {
			/*fprintf(stderr, "Sprite (%.3f, %.3f : %.3f) Tex%d size %d.\n", window_iterator->x, window_iterator->y, window_iterator->theta, window_iterator->texture, window_iterator->size);*/
			*stuff_ptr      = it;
			layer->iterator = it->next;
			return -1;
		}
		layer->iterator = it->next;
	}

	/* done */
	return 0;
}

/** This allows you to go part-way though and reset. Not very useful. */
void LayerResetIterator(struct Layer *const layer) {
	if(!layer) return;
	layer->iterator = 0;
}
  
/* private */

/** Sorts the sprites; they're (hopefully) almost sorted already from last
 frame, just freshens with insertion sort, O(n + m) where m is the
 related to the dynamicness of the scene
 @return	The number of equvalent-swaps (doesn't do this anymore.) */
static void sort(void) {
	isort((void **)&first_y,
		  (int (*)(const void *, const void *))&compare_y,
		  (void **(*)(void *const))&address_prev_y,
		  (void **(*)(void *const))&address_next_y);
}

/** Keep it sorted when there is one element out-of-place. */
void LayerNotify(struct Layer *const layer, void *element) {
	if(!layer || !element) return;
	inotify(&layer->first,
			(void *)element,
			(int (*)(const void *, const void *))&compare_y,
			(void **(*)(void *const))&address_prev_y,
			(void **(*)(void *const))&address_next_y);
}

/* for isort */

static int compare_x(const struct Sprite *a, const struct Sprite *b) {
	return a->x > b->x;
}

static int compare_y(const struct Sprite *a, const struct Sprite *b) {
	return a->y > b->y;
}

static void **address_prev(void *const a) {
	return &((char *)a + /* gahh, layer-> */prev_y;
}

static void **address_next_y(void *const a) {
	return &a->next_y;
}
