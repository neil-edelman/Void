/** 2017 Neil Edelman, distributed under the terms of the GNU General
 Public License 3, see copying.txt, or
 \url{ https://opensource.org/licenses/GPL-3.0 }.

 Used to map a floating point zero-centred position in space into an array of
 discrete size, bins.

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

/*void Layer(struct Layer *const layer, const int i_bin_side,
	const float f_space_per_bin) {
	assert(layer && i_bin_side > 0 && f_space_per_bin > 0.0f);
	layer->full_side_size = i_bin_side;
	layer->half_space     = f_space_per_bin * i_bin_side / 2.0f;
	const float half_space, one_each_bin;
	const struct Rectangle4i full;
	struct Rectangle4i mask, area;
}*/

/** Clips integer vector {i} to {clip}, inclusive.
 @param i: A non-null vector.
 @param clip: A valid rectangle. */
static void clip_ivec(struct Vec2i *const i,
	const struct Rectangle4i *const clip) {
	assert(i && clip
		&& clip->x_min <= clip->x_max && clip->y_min <= clip->y_max);
	if(i->x      < clip->x_min) i->x = clip->x_min;
	else if(i->x > clip->x_max) i->x = clip->x_max;
	if(i->y      < clip->y_min) i->y = clip->y_min;
	else if(i->y > clip->y_max) i->y = clip->y_max;
}

/** Clips integer rectangle {r} to {clip}, inclusive.
 @param r, clip: Valid rectangles touching each other. */
static void clip_touching_irect(struct Rectangle4i *const r,
	const struct Rectangle4i *const clip) {
	assert(r && clip
		&& r->x_min    <= r->x_max    && r->y_min    <= r->y_max
		&& clip->x_min <= clip->x_max && clip->y_min <= clip->y_max
		&& r->x_min <= clip->x_max && r->x_max >= clip->x_min
		&& r->y_min <= clip->y_max && r->y_max >= clip->y_min);
	if(r->x_min < clip->x_min) r->x_min = clip->x_min;
	if(r->x_max > clip->x_max) r->x_max = clip->x_max;
	if(r->y_min < clip->y_min) r->y_min = clip->y_min;
	if(r->y_max > clip->y_max) r->y_max = clip->y_max;
}

/** Descretise space co\:oridinates in {area} to bins in {l} fitting in bin
 mask {mask}. Must not be empty. */
static struct Rectangle4i descritise_frect(const struct Layer *const l,
	const struct Rectangle4f *const area,
	const struct Rectangle4i *const mask) {
	const float h = l->half_space, o = l->one_each_bin;
	struct Rectangle4i fit = {
		(area->x_min + h) * o,
		(area->x_max + h) * o,
		(area->y_min + h) * o,
		(area->y_max + h) * o
	};
	assert(l && area && mask);
	clip_touching_irect(&fit, mask);
	return fit;
}

/** Serialise bin vector {i} according to {l}. */
static unsigned hash_ivec(const struct Layer *const l,
	const struct Vec2i *const i) {
	assert(l && i
		&& i->x >= 0 && i->x < l->full_side_size
		&& i->y >= 0 && i->y < l->full_side_size);
	return i->y * l->full_side_size + i->x;
}

/** Serialise space vector {v} according to {l}. */
static unsigned hash_fvec(const struct Layer *const l,
	const struct Vec2f *const v) {
	struct Vec2i i = {
		(v->x + l->half_space) * l->one_each_bin,
		(v->y + l->half_space) * l->one_each_bin
	};
	assert(l && v);
	clip_ivec(&i, &l->full);
	return hash_ivec(l, &i);
}

/** Does no bounds checking. */
static struct Vec2i hash_to_ivec(const struct Layer *const layer,
	const unsigned hash) {
	const struct Vec2i vec = {
		(hash % layer->full_side_size),
		(hash / layer->full_side_size)
	};
	return vec;
}

/** @return A {bin} in {[0, side_size^2)}. */
unsigned LayerHashOrtho(const struct Layer *const layer,
	struct Ortho3f *const o) {
	assert(layer && o);
	return hash_fvec(layer, (struct Vec2f *)o);
}

/** Used only in debugging.
 @return If true, {vec} will be set from {bin}. */
struct Vec2f LayerHashToVec(const struct Layer *const layer,
	const unsigned hash) {
	const struct Vec2f vec = {
		(hash % layer->full_side_size) / layer->one_each_bin -layer->half_space,
		(hash / layer->full_side_size) / layer->one_each_bin -layer->half_space
	};
	assert(layer && (int)hash < layer->full_side_size * layer->full_side_size);
	return vec;
}

/** Set screen rectangle.
 @return Success. */
void LayerMask(struct Layer *const layer, struct Rectangle4f *const area) {
	layer->mask = descritise_frect(layer, area, &layer->full);
}

/** Debug. Returns true if the {hash} is in the mask. */
int LayerIsMask(const struct Layer *const layer, const unsigned hash) {
	const struct Vec2i bin = hash_to_ivec(layer, hash);
	if(bin.x < layer->mask.x_min || bin.x > layer->mask.x_max
		|| bin.y < layer->mask.y_min || bin.y > layer->mask.y_max) return 0;
	return 1;
}

/** Set sprite rectangle; clips to the \see{LayerMask}.
 @return Success. */
void LayerArea(struct Layer *const layer, struct Rectangle4f *const area) {
	layer->area = descritise_frect(layer, area, &layer->mask);
}

/** Set random. */
void LayerSetRandom(struct Layer *const this, struct Ortho3f *const o) {
	if(!this || !o) return;
	o->x = random_pm_max(this->half_space);
	o->y = random_pm_max(this->half_space);
	o->theta = random_pm_max(M_PI_F);
}

static void for_each_action(const struct Layer *const layer,
	const struct Rectangle4i *const rect,
	const LayerAction action) {
	struct Vec2i x;
	assert(layer && rect && action);
	for(x.y = rect->y_max; x.y >= rect->y_min; x.y--) {
		for(x.x = rect->x_min; x.x <= rect->x_max; x.x++) {
			/* @fixme I hope it optimises {hash_ivec} out; check. */
			action(hash_ivec(layer, &x));
		}
	}
}

static void for_each_accept(const struct Layer *const layer,
	const struct Rectangle4i *const rect,
	const LayerAcceptPlot accept, struct PlotData *const plot) {
	struct Vec2i x;
	assert(layer && rect && accept && plot);
	for(x.y = rect->y_max; x.y >= rect->y_min; x.y--) {
		for(x.x = rect->x_min; x.x <= rect->x_max; x.x++) {
			accept(hash_ivec(layer, &x), plot);
		}
	}
}

static void for_each_consumer(const struct Layer *const layer,
	const struct Rectangle4i *const rect,
	const LayerTriConsumer consumer, const size_t param) {
	struct Vec2i x;
	unsigned c = 0;
	assert(layer && rect && consumer);
	for(x.y = rect->y_max; x.y >= rect->y_min; x.y--) {
		for(x.x = rect->x_min; x.x <= rect->x_max; x.x++) {
			consumer(hash_ivec(layer, &x), param, c++);
		}
	}
}

/** For each bin on screen; used for drawing.
 @param layer, action: Must be non-null. */
void LayerForEachMask(struct Layer *const layer, const LayerAction action) {
	assert(layer && action);
	for_each_action(layer, &layer->mask, action);
}

/** For each bin on screen; used for plotting.
 @param layer, accept, plot: Must be non-null. */
void LayerForEachMaskPlot(const struct Layer *const layer,
	const LayerAcceptPlot accept, struct PlotData *const plot) {
	assert(layer && accept && plot);
	for_each_accept(layer, &layer->mask, accept, plot);
}

/** For each bin crossing the space item; used for collision-detection.
 @param layer, consumer: Must be non-null. */
void LayerForEachArea(struct Layer *const layer,
	const LayerTriConsumer consumer, const size_t param) {
	assert(layer && consumer);
	for_each_consumer(layer, &layer->area, consumer, param);
}

/** Debug. For each bin on screen; used for plotting.
 @param layer, accept, plot: Must be non-null. */
void LayerForEachAreaPlot(const struct Layer *const layer,
	const LayerAcceptPlot accept, struct PlotData *const plot) {
	assert(layer && accept && plot);
	for_each_accept(layer, &layer->area, accept, plot);
}

/** Debug. */
const char *LayerMaskToString(const struct Layer *const layer) {
	static char a[80];
	snprintf(a, sizeof a, "[%d-%d,%d-%d]", layer->mask.x_min,
		layer->mask.x_max, layer->mask.y_min, layer->mask.y_max);
	return a;
}
