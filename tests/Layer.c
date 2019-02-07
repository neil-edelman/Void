#include <stdlib.h> /* EXIT_* */
#include <stdio.h>  /* fprintf */

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

struct Layer {
	const int full_side_size;
	const float half_space, one_each_bin;
	const struct Rectangle4i full;
	struct Rectangle4i mask, area;
};

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

/** Descritise space co\:oridinates in {area} to bins in {l} fitting in bin
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

/** For each bin on screen; used for plotting.
 @param layer, accept, plot: Must be non-null. */
void LayerForEachAreaPlot(const struct Layer *const layer,
	const LayerAcceptPlot accept, struct PlotData *const plot) {
	assert(layer && accept && plot);
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

/*#define LAYER_SIDE_SIZE (64)
#define LAYER_SIZE (LAYER_SIDE_SIZE * LAYER_SIDE_SIZE)*/

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
static void gnu_shade_bins(const unsigned hash, struct PlotData *const plot) {
	struct Vec2f marker = { 0.0f, 0.0f };
	assert(plot && hash < /*LAYER_SIZE*/(unsigned)plot->layer->full_side_size *
		plot->layer->full_side_size);
	marker = LayerHashToVec(plot->layer, hash);
	fprintf(plot->fp, "# bin %u -> %.1f,%.1f\n", hash, marker.x, marker.y);
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
	if(bin_plot(&layer)) {
		fprintf(stderr, "Saved data %s, gnuplot script %s, which outputs %s.\n",
			data_fn, gnu_fn, eps_fn);
		return EXIT_SUCCESS;
	} else {
		perror("Bad");
		return EXIT_FAILURE;
	}
}
