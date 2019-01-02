#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* fprintf */

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
#include "../src/Ortho.h" /* Rectangle4f, etc */

struct Vec2f;
struct Ortho3f;
struct Rectangle4f;
struct Layer;
struct Onscreen;
struct PlotData;
typedef void (*LayerAction)(const unsigned);
typedef void (*LayerAcceptPlot)(const unsigned, struct PlotData *const);
typedef void (*LayerTriConsumer)(const unsigned, const size_t, const unsigned);

static const float epsilon = 0.0005f;

/** Debug. */
static void int_to_string(const unsigned *const i, char (*const a)[12]) {
	sprintf(*a, "%u", *i);
}
#define POOL_NAME Int
#define POOL_TYPE unsigned
#define POOL_TO_STRING &int_to_string
#define POOL_STACK
#include "../src/templates/Pool.h"

enum LayerStep { LAYER_SCREEN, LAYER_SPRITE, LAYER_NO };

struct Layer {
	const int full_side_size;
	const float half_space, one_each_bin;
	const struct Rectangle4i full;
	struct Rectangle4i mask, area;
};

static void vec_clip(struct Vec2i *const i,
	const struct Rectangle4i *const area) {
	assert(i && area
		&& area->x_min <= area->x_max && area->y_min <= area->y_max);
	if(i->x      < area->x_min) i->x = area->x_min;
	else if(i->x > area->x_max) i->x = area->x_max;
	if(i->y      < area->y_min) i->y = area->y_min;
	else if(i->y > area->y_max) i->y = area->y_max;
}

static void rect_clip(struct Rectangle4i *const r,
	const struct Rectangle4i *const area) {
	assert(r && area
		&& r->x_min    <= r->x_max    && r->y_min    <= r->y_max
		&& area->x_min <= area->x_max && area->y_min <= area->y_max
		/* Only clips the rectangle if it is touching. */
		&& r->x_min <= area->x_max && r->x_max >= area->x_min
		&& r->y_min <= area->y_max && r->y_max >= area->y_min);
	if(r->x_min < area->x_min) r->x_min = area->x_min;
	if(r->x_max > area->x_max) r->x_max = area->x_max;
	if(r->y_min < area->y_min) r->y_min = area->y_min;
	if(r->y_max > area->y_max) r->y_max = area->y_max;
}

static struct Rectangle4i space_rect_fit_in(const struct Layer *const l,
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
	rect_clip(&fit, mask);
	return fit;
}

static unsigned vec_to_bin(const struct Layer *const l,
	const struct Vec2i *const i) {
	assert(l && i
		&& i->x >= 0 && i->x < l->full_side_size
		&& i->y >= 0 && i->y < l->full_side_size);
	return i->y * l->full_side_size + i->x;
}

static unsigned space_vec_to_bin(const struct Layer *const l,
	const struct Vec2f *const v) {
	struct Vec2i i = {
		(v->x + l->half_space) * l->one_each_bin,
		(v->y + l->half_space) * l->one_each_bin
	};
	assert(l && v);
	vec_clip(&i, &l->full);
	return vec_to_bin(l, &i);
}

/** @return A {bin} in {[0, side_size^2)}. */
unsigned LayerOrthoToBin(const struct Layer *const layer,
	struct Ortho3f *const o) {
	assert(layer && o);
	return space_vec_to_bin(layer, (struct Vec2f *)o);
}

/** Used only in debugging.
 @return If true, {vec} will be set from {bin}. */
struct Vec2f LayerBinToVec(const struct Layer *const layer,
	const unsigned bin) {
	const struct Vec2f vec = {
		(bin % layer->full_side_size) / layer->one_each_bin - layer->half_space,
		(bin / layer->full_side_size) / layer->one_each_bin - layer->half_space
	};
	assert(layer && (int)bin < layer->full_side_size * layer->full_side_size);
	return vec;
}

#if 0

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
	return 1;
}

#endif

/** Set screen rectangle.
 @return Success. */
void LayerMask(struct Layer *const layer, struct Rectangle4f *const area) {
	layer->mask = space_rect_fit_in(layer, area, &layer->full);
}

/** Set sprite rectangle; clips to the \see{LayerMask}.
 @return Success. */
void LayerArea(struct Layer *const layer, struct Rectangle4f *const area) {
	layer->area = space_rect_fit_in(layer, area, &layer->mask);
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
			action(vec_to_bin(layer, &x));
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
			accept(vec_to_bin(layer, &x), plot);
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
			consumer(vec_to_bin(layer, &x), param, c++);
		}
	}
}

/** For each bin on screen; used for drawing. */
void LayerForEachMask(struct Layer *const layer, const LayerAction action) {
	for_each_action(layer, &layer->mask, action);
}

/** For each bin on screen; used for plotting. */
void LayerForEachMaskPlot(const struct Layer *const layer,
	const LayerAcceptPlot accept, struct PlotData *const plot) {
	for_each_accept(layer, &layer->mask, accept, plot);
}

/** For each bin crossing the space item; used for collision-detection. */
void LayerForEachArea(struct Layer *const layer,
	const LayerTriConsumer consumer, const size_t param) {
	for_each_consumer(layer, &layer->area, consumer, param);
}

/** For each bin on screen; used for plotting. */
void LayerForEachAreaPlot(const struct Layer *const layer,
	const LayerAcceptPlot accept, struct PlotData *const plot) {
	for_each_accept(layer, &layer->area, accept, plot);
}

/* debug

const char *LayerMaskToString(const struct Layer *const layer) {
	return IntPoolToString(layer->step + LAYER_SCREEN);
}

const char *LayerRectangleToString(const struct Layer *const layer) {
	return IntPoolToString(layer->step + LAYER_SPRITE);
}

int LayerIsMask(const struct Layer *const layer, const unsigned bin) {
	const struct IntPool *step = layer->step + LAYER_SCREEN;
	unsigned *b = 0;
	while((b = IntPoolNext(step, b))) if(*b == bin) return 1;
	return 0;
}*/

#define LAYER_SIDE_SIZE (64)
#define LAYER_SIZE (LAYER_SIDE_SIZE * LAYER_SIDE_SIZE)

const char *data_fn = "Layer.data", *gnu_fn = "Layer.gnu",
	*eps_fn = "Layer.eps";

struct PlotData {
	FILE *fp;
	const struct Layer *layer;
	unsigned i, n, object, bin;
	const char *colour;
};

/** Draws squares for highlighting bins. Called in \see{space_plot}.
 @implements LayerAcceptPlot */
static void gnu_shade_bins(const unsigned bin, struct PlotData *const plot) {
	struct Vec2f marker = { 0.0f, 0.0f };
	assert(plot && bin < LAYER_SIZE);
	marker = LayerBinToVec(plot->layer, bin);
	fprintf(plot->fp, "# bin %u -> %.1f,%.1f\n", bin, marker.x, marker.y);
	fprintf(plot->fp, "set object %u rect from %f,%f to %f,%f fc rgb \"%s\" "
		"fs transparent pattern 4 noborder;\n", plot->object++,
		marker.x, marker.y, marker.x + 256.0f, marker.y + 256.0f, plot->colour);
}

static int bin_plot(const struct Layer *const layer) {
	FILE *data = 0, *gnu = 0;
	struct PlotData pd;
	int is_success = 0;
	do {
		if(!(data = fopen(data_fn, "w")) || !(gnu = fopen(gnu_fn, "w"))) break;
		fprintf(gnu, "set term postscript eps enhanced size 256cm, 256cm\n"
			"set output \"%s\"\n"
			"set size square;\n"
			"set palette defined (1 \"#0000FF\", 2 \"#00FF00\", 3 \"#FF0000\");"
			"\n"
			"set xtics 256 rotate; set ytics 256;\n"
			"set grid;\n"
			"set xrange [-8192:8192];\n"
			"set yrange [-8192:8192];\n"
			"set cbrange [0.0:1.0];\n", eps_fn);
		fprintf(gnu, "set style fill transparent solid 0.3 noborder;\n");
		pd.layer = layer, pd.fp = gnu, pd.colour = "#FF0000"/*"#ADD8E6"*/, pd.object = 1;
		LayerForEachMaskPlot(layer, &gnu_shade_bins, &pd);
		pd.colour = "#E6D8AD";
		LayerForEachAreaPlot(layer, &gnu_shade_bins, &pd);
		/* draw the items */
		fprintf(gnu, "plot \"%s\" using 5:6:7 with circles \\\n"
			"linecolor rgb(\"#00FF00\") fillstyle transparent "
			"solid 1.0 noborder title \"Velocity\", \\\n"
			"\"%s\" using 1:2:3:4 with circles \\\n"
			"linecolor palette fillstyle transparent solid 0.3 noborder \\\n"
			"title \"Sprites\", \\\n"
			"\"%s\" using 1:2:8 with labels notitle;\n",
			data_fn, data_fn, data_fn);		
		is_success = 1;
	} while(0); {
		if(data) fclose(data);
		if(gnu) fclose(gnu);
	}
	return is_success;
}

int main(void) {
	struct Layer layer = { 64, 64.0f * 256.0f / 2.0f, 1.0f / 256.0f,
		{ 0, 63, 0, 63 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } };
	struct Rectangle4f
		mask = { -655.594788, 200.405197, -214.927383, 441.072632 },
		area = { 157.001877, 173.586411, -261.022736, -238.160736 };
	LayerMask(&layer, &mask);
	LayerArea(&layer, &area);
	/*printf("mask: %s; rect: %s.\n", LayerMaskToString(&layer), LayerRectangleToString(&layer));*/
	bin_plot(&layer);
	return EXIT_SUCCESS;
}
