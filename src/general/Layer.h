#include <stdint.h>
#include "../general/OrthoMath.h"
struct Rectangle4f;
struct Layer;
struct Onscreen;
struct PlotData;
typedef void (*LayerAction)(const unsigned);
typedef void (*LayerAcceptPlot)(const unsigned, struct PlotData *const);
typedef void (*LayerNoOnscreenAction)(const unsigned, const unsigned,
	struct Onscreen *const);

void Layer_(struct Layer **const pthis);
struct Layer *Layer(const size_t size_side, const float each_bin);
unsigned LayerGetOrtho(const struct Layer *const this, struct Ortho3f *const o);
int LayerGetBin2(const struct Layer *const this, const unsigned bin,
	struct Vec2i *const vec);
int LayerSetScreenRectangle(struct Layer *const this,
	struct Rectangle4f *const rect);
int LayerSetSpriteRectangle(struct Layer *const this,
	struct Rectangle4f *const rect);
void LayerSetRandom(struct Layer *const this, struct Ortho3f *const o);
void LayerForEachScreen(struct Layer *const this, const LayerAction action);
void LayerForEachScreenPlot(struct Layer *const this,
	const LayerAcceptPlot accept, struct PlotData *const plot);
void LayerSpriteForEachSprite(struct Layer *const this,
	struct Onscreen *const onscreen, const LayerNoOnscreenAction action);
