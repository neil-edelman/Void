#include "../general/OrthoMath.h"

/** See \see{Far}. */
struct Far;
struct AutoObjectInSpace;

struct Far *Far(const struct AutoObjectInSpace *ois);
void Far_(struct Far **far_ptr);
void FarClear(void);
int FarIterate(struct Ortho3f *const r_ptr, unsigned *texture_ptr, unsigned *size_ptr);
int FarGetId(const struct Far *far);
void FarSetOrientation(struct Far *back, const float x, const float y, const float theta);
