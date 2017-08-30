#include "../general/OrthoMath.h"

/** See \see{Far}. */
struct Far;
struct AutoObjectInSpace;

typedef void (*FarOutput)(const struct Ortho3f *const x,
	const struct AutoImage *const image,
	const struct AutoImage *const normals);

struct Far *Far(const struct AutoObjectInSpace *ois);
void Far_(struct Far **far_ptr);
void FarClear(void);
void FarDrawLambert(FarOutput draw);
