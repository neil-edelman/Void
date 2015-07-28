struct Background;

struct Background *Background(const int texture, const int size);
void Background_(struct Background **bg_ptr);
int BackgroundIterate(float *x_ptr, float *y_ptr, float *theta_ptr, int *texture_ptr, int *size_ptr);
void BackgroundSetOrientation(struct Background *back, const float x, const float y, const float theta);
int BackgroundGetId(const struct Background *b);
