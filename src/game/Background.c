/* Copyright 2015 Neil Edelman, distributed under the terms of the GNU General
 Public License, see copying.txt */

#include <stdio.h>  /* fprintf */
#include <string.h> /* memset */
#include "Background.h"
#include "../general/Sorting.h"
#include "../general/Map.h" /* for Draw */
#include "../system/Draw.h"

/** Sprites in the background have a (world) position, a rotation, and a bitmap.
 They are sorted by bitmap and drawn by the gpu in ../system/Draw but not lit.
 <p>
 @author	Neil
 @version	3.2, 2015-07
 @since		3.2, 2015-07 */

/* from Draw */
extern int screen_width, screen_height;

/* the backgrounds can be larger than the sprites, 1024x1024? */
static int half_max_size = 512;

struct Background {
	float x, y;     /* orientation */
	float theta;
	int   size;     /* the (x, y) size; they are the same */
	int   texture;  /* in the gpu */
	struct Background *prev_x, *next_x, *prev_y, *next_y; /* sort by axes */
	int   is_selected;
} backgrounds[1024];
static const int backgrounds_capacity = sizeof(backgrounds) / sizeof(struct Background);
static int       backgrounds_size;

static struct Background *first_x, *first_y; /* the projected axis sorting thing */

static struct Background *first_x_window, *first_y_window, *window_iterator;
static struct Background *iterator = backgrounds; /* for drawing and stuff */

/* private prototypes */

static struct Background *iterate(void);
static void sort_notify(struct Background *);
static int compare_x(const struct Background *a, const struct Background *b);
static int compare_y(const struct Background *a, const struct Background *b);
static struct Background **address_prev_x(struct Background *const a);
static struct Background **address_next_x(struct Background *const a);
static struct Background **address_prev_y(struct Background *const a);
static struct Background **address_next_y(struct Background *const a);

/* public */

/** Get a new background sprite from the pool of unused.
 @param texture		On the GPU.
 @param size		Pixels.
 @return			The Background. */
struct Background *Background(const int texture, const int size) {
	struct Background *bg;

	if(backgrounds_size >= backgrounds_capacity) {
		fprintf(stderr, "Background: couldn't be created; reached maximum of %u.\n", backgrounds_capacity);
		return 0;
	}
	if(!texture || size <= 0) {
		fprintf(stderr, "Background: invalid.\n");
		return 0;
	}
	bg = &backgrounds[backgrounds_size++];

	bg->x  = bg->y  = 0.0f;
	bg->theta   = 0.0f;
	bg->size    = size;
	bg->texture = texture;

	bg->prev_x                  = 0;
	bg->next_x                  = first_x;
	bg->prev_y                  = 0;
	bg->next_y                  = first_y;
	if(first_x) first_x->prev_x = bg;
	if(first_y) first_y->prev_y = bg;
	first_x = first_y = bg;
	sort_notify(bg);

	fprintf(stderr, "Background: created from pool, Bgr%u->Tex%u.\n", BackgroundGetId(bg), texture);

	return bg;
}

/** Erase a sprite from the pool (array of static sprites.)
 @param sprite_ptr	A pointer to the sprite; gets set null on success. */
void Background_(struct Background **bg_ptr) {
	struct Background *bg, *replace, *neighbor;
	int index;

	if(!bg_ptr || !(bg = *bg_ptr)) return;
	index = bg - backgrounds;
	if(index < 0 || index >= backgrounds_size) {
		fprintf(stderr, "~Background: Bgr%u not in range Bgr%u.\n", index + 1, backgrounds_size);
		return;
	}
	fprintf(stderr, "~Background: returning to pool, Bgr%u->Tex%u.\n", BackgroundGetId(bg), bg->texture);

	/* take it out of the lists */
	if(bg->prev_x) bg->prev_x->next_x = bg->next_x;
	else           first_x            = bg->next_x;
	if(bg->next_x) bg->next_x->prev_x = bg->prev_x;
	if(bg->prev_y) bg->prev_y->next_y = bg->next_y;
	else           first_y            = bg->next_y;
	if(bg->next_y) bg->next_y->prev_y = bg->prev_y;

	/* move the terminal bg to replace this one */
	if(index < --backgrounds_size) {

		replace = &backgrounds[backgrounds_size];
		memcpy(bg, replace, sizeof(struct Background));

		bg->prev_x = replace->prev_x;
		bg->next_x = replace->next_x;
		bg->prev_y = replace->prev_y;
		bg->next_y = replace->next_y;

		/* prev, next, have to know about the replacement */
		if((neighbor = replace->prev_x)) neighbor->next_x = bg;
		else                             first_x          = bg;
		if((neighbor = replace->next_x)) neighbor->prev_x = bg;
		if((neighbor = replace->prev_y)) neighbor->next_y = bg;
		else                             first_y          = bg;
		if((neighbor = replace->next_y)) neighbor->prev_y = bg;

		fprintf(stderr, "~Background: Bgr%u has become Bgr%u.\n", BackgroundGetId(replace), BackgroundGetId(bg));
	}

	*bg_ptr = bg = 0;
}

#if 0
/** Returns true while there are more, sets the values. The pointers need to
 all be there or else there will surely be a segfault.
 <p>
 Called in ../system/Draw to draw backgrounds.
 <p>
 Supports deleting.
 <p>
 Fixme: have a sliding window that's updated based on camera, as opposed to
 having to send all the geometry at every frame!
 @param x_ptr		x
 @param y_ptr		y
 @param t_ptr		\theta
 @param texture_ptr	OpenGl texture unit.
 @param size_ptr	Size of the texture.
 @return			True if the values have been set. */
int BackgroundIterate(float *x_ptr, float *y_ptr, float *theta_ptr, int *texture_ptr, int *size_ptr) {
	if(iterator >= backgrounds + backgrounds_size) {
		iterator = backgrounds;
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
#else
/** This is the exact copy of SpriteIterate. */
int BackgroundIterate(float *x_ptr, float *y_ptr, float *theta_ptr, int *texture_ptr, int *size_ptr) {
	static int x_min_window, x_max_window, y_min_window, y_max_window;
	static int x_min, x_max, y_min, y_max;
	static int is_reset = -1; /* oy, static */
	struct Background *b, *feeler;
	float camera_x, camera_y;

	/* go to the first spot in the window */
	if(is_reset) {
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
		for(b = first_x_window; b && b->x <= x_max; b = b->next_x)
			b->is_selected = -1;
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
	for(b = first_x_window; b && b->x <= x_max; b = b->next_x)
		b->is_selected = 0;
	is_reset = -1;
	return 0;
}
#endif

/** Sets the orientation with respect to the screen, pixels and (0, 0) is at
 the centre.
 @param back	Which background to set.
 @param x		x
 @param y		y
 @param t		\theta */
void BackgroundSetOrientation(struct Background *back, const float x, const float y, const float theta) {
	if(!back) return;
	back->x     = x;
	back->y     = y;
	back->theta = theta;
	inotify((void **)&first_x, (void *)back, (int (*)(const void *, const void *))&compare_x, (void **(*)(void *const))&address_prev_x, (void **(*)(void *const))&address_next_x);
	inotify((void **)&first_y, (void *)back, (int (*)(const void *, const void *))&compare_y, (void **(*)(void *const))&address_prev_y, (void **(*)(void *const))&address_next_y);
}

int BackgroundGetId(const struct Background *b) {
	if(!b) return 0;
	return (int)(b - backgrounds) + 1;
}

/* private */

/** This is a private iteration which uses the same variable as Background::iterate.
 This actually returns a Background. Use it when you want to delete a sprite as you
 go though the list. */
static struct Background *iterate(void) {
	if(iterator >= backgrounds + backgrounds_size) {
		iterator = backgrounds;
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
static void sort_notify(struct Background *s) {
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

static int compare_x(const struct Background *a, const struct Background *b) {
	return a->x > b->x;
}

static int compare_y(const struct Background *a, const struct Background *b) {
	return a->y > b->y;
}

static struct Background **address_prev_x(struct Background *const a) {
	return &a->prev_x;
}

static struct Background **address_next_x(struct Background *const a) {
	return &a->next_x;
}

static struct Background **address_prev_y(struct Background *const a) {
	return &a->prev_y;
}

static struct Background **address_next_y(struct Background *const a) {
	return &a->next_y;
}
