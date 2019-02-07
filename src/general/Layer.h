struct Vec2f;
struct Ortho3f;
struct Rectangle4f;
struct Layer;
struct Onscreen;
struct PlotData;
typedef void (*LayerAction)(const unsigned);
typedef void (*LayerAcceptPlot)(const unsigned, struct PlotData *const);
typedef void (*LayerTriConsumer)(const unsigned, const size_t, const unsigned);

/* Use {LAYER_STATIC_INIT} to initialise structure. */
struct Layer {
	const int full_side_size;
	const float half_space, one_each_bin;
	const struct Rectangle4i full;
	struct Rectangle4i mask, area;
};

/* @param i_bin_side: How many bins are on the side; the total number of bins
 is the square of this number, (because it's in 2d.)
 @param f_space_per_bin: Space is there between bins in a dimension.
 assert(i_bin_side > 0 && f_space_per_bin > 0.0f); */
#define LAYER_STATIC_INIT(i_bin_side, f_space_per_bin) { \
	i_bin_side, \
	f_space_per_bin * i_bin_side / 2.0f, \
	1.0f / f_space_per_bin, \
	{ 0, i_bin_side - 1, 0, i_bin_side - 1 }, \
	{ 0, 0, 0, 0 }, \
	{ 0, 0, 0, 0 } \
}

void Layer(struct Layer *const layer, const unsigned i_bin_side,
	const float f_space_per_bin);
unsigned LayerHashOrtho(const struct Layer *const layer,
	struct Ortho3f *const o);
struct Vec2f LayerHashToVec(const struct Layer *const layer,
	const unsigned hash);
void LayerMask(struct Layer *const layer, struct Rectangle4f *const area);
int LayerIsMask(const struct Layer *const layer, const unsigned hash);
void LayerArea(struct Layer *const layer, struct Rectangle4f *const area);
void LayerSetRandom(struct Layer *const this, struct Ortho3f *const o);
void LayerForEachMask(struct Layer *const layer, const LayerAction action);
void LayerForEachMaskPlot(const struct Layer *const layer,
	const LayerAcceptPlot accept, struct PlotData *const plot);
void LayerForEachArea(struct Layer *const layer,
	const LayerTriConsumer consumer, const size_t param);
void LayerForEachAreaPlot(const struct Layer *const layer,
	const LayerAcceptPlot accept, struct PlotData *const plot);
