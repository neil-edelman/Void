struct Far;
struct ObjectInSpace;

struct Far *Far(const struct ObjectInSpace *ois);
void Far_(struct Far **far_ptr);
void FarClear(void);
int FarIterate(float *x_ptr, float *y_ptr, float *theta_ptr, int *texture_ptr, int *size_ptr);
int FarGetId(const struct Far *far);
