/* Copyright 2015 Neil Edelman, distributed under the terms of the GNU General
 Public License, see copying.txt */

#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* fprintf */
#include <math.h>   /* sqrtf, atan2f, cosf, sinf */
#include <string.h> /* memset */
#include "Background.h"
#include "../general/Sorting.h"
#include "../system/Timer.h" /* hmmm, Wmd should go in Wmd */
/* types of sprites (subclasses) */
#include "Debris.h"
#include "Ship.h"
#include "Wmd.h"

/** Sprites in the background have a (world) position, a rotation, and a bitmap.
 They are sorted by bitmap and drawn by the gpu in ../system/Draw but not lit.
 <p>
 @author	Neil
 @version	3.2, 2015-06
 @since		3.2, 2015-06 */

/* hmm, 256 is a lot of pixel space for the front layer, should be enough?
 the larger you set this, the farther it has to go to determine whether there's
 a collision, for every sprite! */
static const int half_max_size = 128;
static const float epsilon = 0.0005f;
extern const float de_sitter;

struct Background {
	float x, y;     /* orientation */
	float theta;
	int   size;     /* the (x, y) size; they are the same */
	int   texture;  /* in the gpu */
	struct Background *prev_x, *next_x, *prev_y, *next_y; /* sort by axes */
} backgrounds[1024];
static const int backgrounds_capacity = sizeof(backgrounds) / sizeof(struct Background);
static int       backgrounds_size;

static struct Background *first_x, *first_y; /* the projected axis sorting thing */

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
