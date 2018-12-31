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

#include <stdio.h>    /* stderr */
#include <limits.h>   /* INT_MAX */
#include <errno.h>
#include "../Ortho.h" /* Rectangle4f, etc */
#include "Layer.h"

static const float epsilon = 0.0005f;

#define POOL_NAME Int
#define POOL_TYPE unsigned
#define POOL_STACK
#include "../templates/Pool.h"

enum LayerStep { LAYER_SCREEN, LAYER_SPRITE, LAYER_NO };

/* This needs to be updated.
 LayerMask(items.layer, &rect);
 and then all layer ops fit within that rect! */

struct Layer {
	int side_size;
	float half_space, one_each_bin;
	struct Rectangle4i bin_mask;
	struct IntPool step[LAYER_NO];
};

void Layer_(struct Layer **const pthis) {
	struct Layer *this;
	unsigned i;
	if(!pthis || !(this = *pthis)) return;
	for(i = 0; i < LAYER_NO; i++) IntPool_(this->step + i);
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
		{ errno = ERANGE; return 0; }
	if(!(this = malloc(sizeof *this))) return 0;
	this->side_size = (int)side_size;
	this->half_space = (float)side_size * each_bin / 2.0f;
	this->one_each_bin = 1.0f / each_bin;
	rectangle4i_init(&this->bin_mask);
	for(i = 0; i < LAYER_NO; i++) IntPool(this->step + i);
	rectangle4i_init(&this->bin_mask);
	return this;
}

/** @return A {bin} in {[0, side_size^2[}. */
unsigned LayerGetOrtho(const struct Layer *const this, struct Ortho3f *const o){
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

/** Used only in debugging.
 @return If true, {vec} will be set from {bin}. */
int LayerGetBinMarker(const struct Layer *const this, const unsigned bin,
	struct Vec2f *const vec) {
	if(!this || bin >= (unsigned)this->side_size * this->side_size || !vec)
		return 0;
	vec->x = (bin % this->side_size) / this->one_each_bin - this->half_space;
	vec->y = (bin / this->side_size) / this->one_each_bin - this->half_space;
	return 1;
}

/** Private code for \see{LayerSet*Rectangle}.
 @param rect: The rectangle that will be tranformed.
 @param layer: A IntStack layer, eg. {this->pool[BIN_LAYER_SCREEN]}.
 @param min: Optional.
 @return Success. */
static int set_rect_layer(struct Layer *const this,
	struct Rectangle4f *const rect, const enum LayerStep layer_step) {
	struct Rectangle4i bin4;
	struct Vec2i bin2i;
	unsigned *bin;
	struct IntPool *step = this->step + layer_step;
	assert(this && rect && layer_step < LAYER_NO);
	IntPoolClear(step);
	/* Map floating point rectangle {rect} to index rectangle {bin4}. */
	bin4.x_min = (rect->x_min + this->half_space) * this->one_each_bin;
	bin4.x_max = (rect->x_max + this->half_space) * this->one_each_bin;
	bin4.y_min = (rect->y_min + this->half_space) * this->one_each_bin;
	bin4.y_max = (rect->y_max + this->half_space) * this->one_each_bin;
	if(layer_step == LAYER_SCREEN) {
		if(bin4.x_min < 0) bin4.x_min = 0;
		if(bin4.x_max >= this->side_size) bin4.x_max = this->side_size - 1;
		if(bin4.y_min < 0) bin4.y_min = 0;
		if(bin4.y_max >= this->side_size) bin4.y_max = this->side_size - 1;
		/* Save the bin_mask rectangle. */
		rectangle4i_assign(&this->bin_mask, &bin4);
		/*printf("Far (%d:%d, %d:%d)\n", bin4.x_min, bin4.x_max, bin4.y_min, bin4.y_max);*/
	} else {
		const struct Rectangle4i *const bin_mask = &this->bin_mask;
		/* Clip it to the bin_mask. */
		if(bin4.x_min < bin_mask->x_min) bin4.x_min = bin_mask->x_min;
		if(bin4.x_max > bin_mask->x_max) bin4.x_max = bin_mask->x_max;
		if(bin4.y_min < bin_mask->x_min) bin4.y_min = bin_mask->y_min;
		if(bin4.y_max > bin_mask->y_max) bin4.y_max = bin_mask->y_max;
	}
	/* Generally goes faster when you follow the scan-lines; not sure whether
	 this is so important with buffering. fixme: uhm, is this pop or dequeue? */
	for(bin2i.y = bin4.y_max; bin2i.y >= bin4.y_min; bin2i.y--) {
		for(bin2i.x = bin4.x_min; bin2i.x <= bin4.x_max; bin2i.x++) {
			/*printf("sprite bin2i(%u, %u)\n", bin2i.x, bin2i.y);*/
			if(!(bin = IntPoolNew(step))) return 0;
			*bin = bin2i.y * this->side_size + bin2i.x;
			/*bin_to_Vec2i(bin2i_to_fg_bin(bin2i), &bin_pos);
			fprintf(gnu_glob, "set arrow from %f,%f to %d,%d lw 0.2 "
				"lc rgb \"#CCEEEE\" front;\n",
				this->x_5.x, this->x_5.y, bin_pos.x, bin_pos.y);*/
		}
	}
	return 1;
}

/** Set screen rectangle.
 @return Success. */
int LayerMask(struct Layer *const this, struct Rectangle4f *const rect) {
	if(!this || !rect) return 0;
	return set_rect_layer(this, rect, LAYER_SCREEN);
}

/** Set sprite rectangle.
 @return Success. */
int LayerSetItemRectangle(struct Layer *const this,
	struct Rectangle4f *const rect) {
	if(!this || !rect) return 0;
	return set_rect_layer(this, rect, LAYER_SPRITE);
}

/** Set random. */
void LayerSetRandom(struct Layer *const this, struct Ortho3f *const o) {
	if(!this || !o) return;
	o->x = random_pm_max(this->half_space);
	o->y = random_pm_max(this->half_space);
	o->theta = random_pm_max(M_PI_F);
}

/** For each bin on screen; used for drawing. */
void LayerForEachScreen(struct Layer *const this, const LayerAction action) {
	struct IntPool *const step = this->step + LAYER_SCREEN;
	unsigned *i = 0;
	static int is_rep = 1;
	if(!this || !action) return;
	if(is_rep) {
		is_rep = 0;
		printf("LayerForEachScreen:\n");
	}
	while((i = IntPoolNext(step, i))) action(*i);
}

/** For each bin on screen; used for plotting. */
void LayerForEachScreenPlot(struct Layer *const this,
	const LayerAcceptPlot accept, struct PlotData *const plot) {
	struct IntPool *const step = this->step + LAYER_SCREEN;
	unsigned *i = 0;
	if(!this || !accept) return;
	while((i = IntPoolNext(step, i))) accept(*i, plot);
}

/** For each bin crossing the space item; used for collision-detection. */
void LayerForEachItem(struct Layer *const this, const size_t active_index,
	const LayerTriConsumer action) {
	struct IntPool *const step = this->step + LAYER_SPRITE;
	unsigned *i = 0, c = 0;
	if(!this || !action) return;
	while((i = IntPoolNext(step, i))) action(*i, active_index, c++);
}
