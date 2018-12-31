struct Vec2f;
struct Ortho3f;
struct Rectangle4f;
struct Layer;
struct Onscreen;
struct PlotData;
typedef void (*LayerAction)(const unsigned);
typedef void (*LayerAcceptPlot)(const unsigned, struct PlotData *const);
typedef void (*LayerTriConsumer)(const unsigned, const size_t, const unsigned);

void Layer_(struct Layer **const pthis);
struct Layer *Layer(const size_t size_side, const float each_bin);
unsigned LayerGetOrtho(const struct Layer *const this, struct Ortho3f *const o);
int LayerGetBinMarker(const struct Layer *const this, const unsigned bin,
	struct Vec2f *const vec);
int LayerMask(struct Layer *const this, struct Rectangle4f *const rect);
int LayerRectangle(struct Layer *const this, struct Rectangle4f *const rect);
void LayerSetRandom(struct Layer *const this, struct Ortho3f *const o);
void LayerForEachMask(struct Layer *const this, const LayerAction action);
void LayerForEachMaskPlot(struct Layer *const this,
	const LayerAcceptPlot accept, struct PlotData *const plot);
void LayerForEachRectangle(struct Layer *const this, const size_t proxy_index,
	const LayerTriConsumer action);
const char *LayerMaskToString(const struct Layer *const layer);
const char *LayerRectangleToString(const struct Layer *const layer);
int LayerIsMask(const struct Layer *const layer, const unsigned bin);
