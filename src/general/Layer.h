struct Vec2f;
struct Ortho3f;
struct Rectangle4f;
struct Layer;
struct Onscreen;
struct PlotData;
typedef void (*LayerAction)(const unsigned);
typedef void (*LayerAcceptPlot)(const unsigned, struct PlotData *const);
typedef void (*LayerTriConsumer)(const unsigned, const unsigned,const unsigned);

void Layer_(struct Layer **const pthis);
struct Layer *Layer(const size_t size_side, const float each_bin);
unsigned LayerGetOrtho(const struct Layer *const this, struct Ortho3f *const o);
int LayerGetBinMarker(const struct Layer *const this, const unsigned bin,
	struct Vec2f *const vec);
int LayerSetScreenRectangle(struct Layer *const this,
	struct Rectangle4f *const rect);
int LayerSetItemRectangle(struct Layer *const this,
	struct Rectangle4f *const rect);
void LayerSetRandom(struct Layer *const this, struct Ortho3f *const o);
void LayerForEachScreen(struct Layer *const this, const LayerAction action);
void LayerForEachScreenPlot(struct Layer *const this,
	const LayerAcceptPlot accept, struct PlotData *const plot);
void LayerForEachItem(struct Layer *const this,
	unsigned proxy_index, const LayerTriConsumer action);
