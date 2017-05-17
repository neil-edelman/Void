/** See \see{Far}. */
struct Far;
struct AutoObjectInSpace;

struct Far *Far(const struct AutoObjectInSpace *ois);
void Far_(struct Far **far_ptr);
void FarClear(void);
int FarIterate(float *x_ptr, float *y_ptr, float *theta_ptr, unsigned *texture_ptr, unsigned *size_ptr);
int FarGetId(const struct Far *far);
void FarSetOrientation(struct Far *back, const float x, const float y, const float theta);
