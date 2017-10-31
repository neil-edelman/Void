#include <stdint.h>
#include "../general/OrthoMath.h"
struct Rectangle4f;
struct Bins;
typedef void (*BinsBiAction)(const unsigned, void *const);

void Bins_(struct Bins **const pthis);
struct Bins *Bins(const size_t size_side, const float each_bin);
unsigned BinsVector(struct Bins *const this, struct Vec2f *const vec);
int BinsSetRectangle(struct Bins *const this, struct Rectangle4f *const rect);
void BinsBiForEach(struct Bins *const this, const BinsBiAction action,
	void *const param);
