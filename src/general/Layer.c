/** 2017 Neil Edelman, distributed under the terms of the GNU General
 Public License 3, see copying.txt, or
 \url{ https://opensource.org/licenses/GPL-3.0 }.

 Used to map a floating point zero-centred position into an array of discrete
 size.

 @title		Layer
 @author	Neil
 @std		C89/90
 @version	2017-10 Broke off from Sprites.
 @since		2017-05 Generics.
			2016-01
			2015-06 */

#include <stdio.h> /* stderr */
#include <limits.h> /* INT_MAX */
#include "../general/OrthoMath.h" /* Rectangle4f, etc */
#include "Layer.h"

static const float epsilon = 0.0005f;

#define STACK_NAME Int
#define STACK_TYPE int
#include "../templates/Stack.h"

enum { LAYER_SCREEN, LAYER_SPRITE, LAYER_NO };

struct Layer {
	int side_size;
	float half_space, one_each_bin;
	struct Vec2i min_screen; /* For collision detection. */
	struct IntStack *pool[LAYER_NO];
};

void Layer_(struct Layer **const pthis) {
	struct Layer *this;
	unsigned i;
	if(!pthis || !(this = *pthis)) return;
	for(i = 0; i < LAYER_NO; i++) IntStack_(&this->pool[i]);
	this = *pthis = 0;
}

/** @param side_size: The size of a side; the {LayerTake} returns
 {[0, side_size^2-1]}.
 @param each_bin: How much space per bin.
 @fixme Have max values. */
struct Layer *Layer(const size_t side_size, const float each_bin) {
	struct Layer *this;
	unsigned i;
	if(!side_size || side_size > INT_MAX || each_bin < epsilon)
		{ fprintf(stderr, "Layer: parameters out of range.\n"); return 0; }
	if(!(this = malloc(sizeof *this))) { perror("Layer"); Layer_(&this);return 0;}
	this->side_size = (int)side_size;
	this->half_space = (float)side_size * each_bin / 2.0f;
	this->one_each_bin = 1.0f / each_bin;
	this->min_screen.x = 0, this->min_screen.y = 0;
	this->pool[0] = this->pool[1] = 0;
	for(i = 0; i < LAYER_NO; i++) {
		if(!(this->pool[i] = IntStack())) { fprintf(stderr, "Layer: %s.\n",
			IntStackGetError(0)); Layer_(&this); return 0; }
	}
	return this;
}

unsigned LayerGetOrtho(const struct Layer *const this, struct Ortho3f *const o) {
	struct Vec2i v2i;
	if(!this || !o) return 0;
	v2i.x = (o->x + this->half_space) * this->one_each_bin;
	if(v2i.x < 0) v2i.x = 0;
	else if(v2i.x >= this->side_size) v2i.x = this->side_size - 1;
	v2i.y = (o->y + this->half_space) * this->one_each_bin;
	if(v2i.y < 0) v2i.y = 0;
	else if(v2i.y >= this->side_size) v2i.y = this->side_size - 1;
	return v2i.y * this->side_size + v2i.x;
}

/** Private code for \see{LayerSet*Rectangle}.
 @param rect: The rectangle that will be tranformed.
 @param layer: A IntStack layer, eg. {this->pool[BIN_LAYER_SCREEN]}.
 @param min: Optional.
 @return Success. */
static int set_rect_layer(struct Layer *const this,
	struct Rectangle4f *const rect, struct IntStack *const layer,
	struct Vec2i *const min) {
	struct Rectangle4i bin4;
	struct Vec2i bin2i;
	int *bin;
	assert(this && rect && layer); /* ALSO layer \in this */
	IntStackClear(layer);
	/* Map floating point rectangle {rect} to index rectangle {bin4}. */
	bin4.x_min = (rect->x_min + this->half_space) * this->one_each_bin;
	if(bin4.x_min < 0) bin4.x_min = 0;
	bin4.x_max = (rect->x_max + this->half_space) * this->one_each_bin;
	if(bin4.x_max >= this->side_size) bin4.x_max = this->side_size - 1;
	bin4.y_min = (rect->y_min + this->half_space) * this->one_each_bin;
	if(bin4.y_min < 0) bin4.y_min = 0;
	bin4.y_max = (rect->y_max + this->half_space) * this->one_each_bin;
	if(bin4.y_max >= this->side_size) bin4.y_max = this->side_size - 1;
	/* Generally goes faster when you follow the scan-lines; not sure whether
	 this is so important with buffering. */
	for(bin2i.y = bin4.y_max; bin2i.y >= bin4.y_min; bin2i.y--) {
		for(bin2i.x = bin4.x_min; bin2i.x <= bin4.x_max; bin2i.x++) {
			/*printf("sprite bin2i(%u, %u)\n", bin2i.x, bin2i.y);*/
			if(!(bin = IntStackNew(layer))) { fprintf(stderr,
				"Layer rectangle: %s.\n", IntStackGetError(layer)); return 0; }
			*bin = bin2i.y * this->side_size + bin2i.x;
			/*bin_to_Vec2i(bin2i_to_fg_bin(bin2i), &bin_pos);
			fprintf(gnu_glob, "set arrow from %f,%f to %d,%d lw 0.2 "
				"lc rgb \"#CCEEEE\" front;\n",
				this->x_5.x, this->x_5.y, bin_pos.x, bin_pos.y);*/
		}
	}
	/* Hack to get min settings of screen. */
	if(min) min->x = bin4.x_min, min->y = bin4.y_min;
	return 1;
}

/** Set screen rectangle.
 @return Success. */
int LayerSetScreenRectangle(struct Layer *const this,
	struct Rectangle4f *const rect) {
	if(!this || !rect) return 0;
	return set_rect_layer(this, rect, this->pool[LAYER_SCREEN],
		&this->min_screen);
}

/** Set sprite rectangle.
 @return Success. */
int LayerSetSpriteRectangle(struct Layer *const this,
	struct Rectangle4f *const rect) {
	if(!this || !rect) return 0;
	return set_rect_layer(this, rect, this->pool[LAYER_SPRITE], 0);
}

/** For each bin on screen. */
void LayerForEachScreen(struct Layer *const this, const LayerAction action) {
	struct IntStack *layer;
	size_t i, size;
	if(!this || !action) return;
	layer = this->pool[LAYER_SCREEN];
	size = IntStackGetSize(layer);
	for(i = 0; i < size; i++) action(*IntStackGetElement(layer, i));
}

/** For each bin crossing the sprite. */
void LayerSpriteForEachSprite(struct Layer *const this,
	struct Sprite *const sprite, const LayerSpriteAction action) {
	struct IntStack *layer;
	size_t i, size;
	if(!this || !sprite || !action) return;
	layer = this->pool[LAYER_SPRITE];
	size = IntStackGetSize(layer);
	for(i = 0; i < size; i++) action(*IntStackGetElement(layer, i), sprite);
}

/** Only makes sense after \see{LayerSetScreenRectangle} has been called. */
void LayerSetResponsible(const struct Layer *const this, const unsigned bin) {
	struct Vec2i bin2i;
	if(!this) return vec;
	bin2i.x = bin % this->side_size;
	bin2i.y = bin / this->side_size;
}

/** Only makes sense after \see{LayerSetResponsible} has been called.
 @return Whether it should check the collision detection, or another bin will
 check it. */
int LayerIsResponsible(const struct Layer *const this,
	const struct Sprite *const a, const struct Sprite *const b) {
	if(!this || !a || !b) return 0;
	min_x = ();
	min_y = ();
}
