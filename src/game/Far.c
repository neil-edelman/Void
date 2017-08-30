/** Copyright 2015 Neil Edelman, distributed under the terms of the GNU General
 Public License, see copying.txt.

 Fars in the background have a (world) position, a rotation, and a sprite. They
 are not affected by point lights, just directional.

 @title		Far
 @author	Neil
 @version	3.3, 2017-07
 @since		3.2, 2015-07 */

#include <string.h>	/* sprintf fprintf */
#include "../../build/Auto.h" /* for AutoImage, AutoObjectInSpace, etc */
#include "../general/OrthoMath.h" /* for measurements and types */
#include "Far.h"
#include "../system/Draw.h" /* DrawGetCamera, DrawGetScreen */

#if 0
/*
 ObjectInSpace
 string name
 Sprite sprite
 int x
 int y
 */

/* the backgrounds can be larger than the sprites, 1024x1024? */
static const int half_max_size    = 512;
/* changed? update shader Far.vs! should be 0.00007:14285.714; space is
 big! too slow to get anywhere, so fudge it; basically, this makes space much
 smaller */
static const float foreshortening = 0.2f, one_foreshortening = 5.0f;
#endif

struct Far {
	const struct AutoImage *image, *normals;
	struct Ortho3f x;
	const char *name;
	unsigned bin;
};

/* declare */
static void Far_to_string(const struct Far *this, char (*const a)[12]);
/* Define {FarList} and {FarListNode}. */
#define LIST_NAME Far
#define LIST_TYPE struct Far
#define LIST_UA_NAME Unorder
#define LIST_TO_STRING &Far_to_string
#include "../general/List.h"
/** Every Far has one {bin} based on their position. \see{OrthoMath.h}. */
static struct FarList bins[BIN_BIN_BG_SIZE];

#define SET_NAME FarNode
#define SET_TYPE struct FarListNode
#include "../general/Set.h"
static struct FarNodeSet *fars;

/** @implements <Far>ToString */
static void Far_to_string(const struct Far *this, char (*const a)[12]) {
	assert(this && a);
	sprintf(*a, "%.11s", this->name);
}

/** Get a new background sprite from {ObjectInSpace} resource.
 @return The Far or null. */
struct Far *Far(const struct AutoObjectInSpace *ois) {
	struct FarListNode *fn;
	struct Far *far;
	/* fixme: diurnal variation */
	if(!ois) return 0;
	if(!(fn = FarNodeSetNew(fars)))
		{ fprintf(stderr, "Far: %s.\n", FarNodeSetGetError(fars)); return 0; }
	far = &fn->data;
	far->image   = ois->sprite->image;
	far->normals = ois->sprite->normals;
	far->x.x     = (float)ois->x;
	far->x.y     = (float)ois->y;
	far->x.theta = 0.0f;
	far->name    = ois->name;
	Vec2f_to_bg_bin((struct Vec2f *)&far->x, &far->bin);
	FarListPush(bins + far->bin, fn);
	fprintf(stderr, "Far: created \"%s.\"\n", far->name);
	return far;
}

/** @param far_ptr A pointer to the sprite in a list; gets set null on
 success.
 @order O(n)? */
void Far_(struct Far **far_ptr) {
	struct Far *far;
	struct FarListNode *fn;

	if(!far_ptr || !(far = *far_ptr)) return;
	fn = (struct FarListNode *)far;
	FarListRemove(bins + far->bin, far);
	FarNodeSetRemove(fars, fn);
	*far_ptr = far = 0;
}

/** Sets the size to zero; very fast. */
void FarClear(void) {
	unsigned i;
	fprintf(stderr, "FarClear: clearing Fars.\n");
	for(i = 0; i < BIN_BIN_BG_SIZE; i++) FarListClear(bins + i);
	FarNodeSetClear(fars);
}

/* @fixme This. */
void FarDrawLambert(FarOutput draw) {
	struct Rectangle4f rect;
	struct Rectangle4i bin4;
	int x, y;
	/* Use {DrawGetScreen} to clip. */
	DrawGetScreen(&rect);
	rect.x_min -= BIN_BG_HALF_SPACE;
	rect.x_max += BIN_BG_HALF_SPACE;
	rect.y_min -= BIN_BG_HALF_SPACE;
	rect.y_max += BIN_BG_HALF_SPACE;
	Rectangle4f_to_bg_bin4(&rect, &bin4);
	/* Draw the square. */
	for(y = bin4.y_min; y <= bin4.y_max; y++) {
		for(x = bin4.x_min; x <= bin4.x_max; x++) {
			/*FarListUnorderForEach(); . . . */
		}
	}
}







#if 0



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

#endif
