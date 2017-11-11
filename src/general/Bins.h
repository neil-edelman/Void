#include <stdint.h>
#include "../general/OrthoMath.h"
struct Rectangle4f;
struct Sprite;
struct Bins;
typedef void (*BinsAction)(const unsigned);
typedef void (*BinsSpriteAction)(const unsigned, struct Sprite *);

void Bins_(struct Bins **const pthis);
struct Bins *Bins(const size_t size_side, const float each_bin);
unsigned BinsGetOrtho(struct Bins *const this, struct Ortho3f *const o);
int BinsSetScreenRectangle(struct Bins *const this,
	struct Rectangle4f *const rect);
int BinsSetSpriteRectangle(struct Bins *const this,
	struct Rectangle4f *const rect);
void BinsForEachScreen(struct Bins *const this, const BinsAction action);
void BinsSpriteForEachSprite(struct Bins *const this,
	struct Sprite *const sprite, const BinsSpriteAction action);
