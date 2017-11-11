#include <stdint.h>
#include "../general/OrthoMath.h"
struct Rectangle4f;
struct Bins;
typedef void (*BinsAction)(const unsigned);
typedef void (*BinsBiAction)(const unsigned, void *const);

void Bins_(struct Bins **const pthis);
struct Bins *Bins(const size_t size_side, const float each_bin);
unsigned BinsVector(struct Bins *const this, struct Vec2f *const vec);
int BinsSetScreenRectangle(struct Bins *const this,
	struct Rectangle4f *const rect);
int BinsSetSpriteRectangle(struct Bins *const this,
	struct Rectangle4f *const rect);
void BinsForEachScreen(struct Bins *const this, const BinsAction action);
void BinsForEachSprite(struct Bins *const this, const BinsAction action);

/*void BinsBiForEach(struct Bins *const this, const BinsBiAction action,
	void *const param);*/
