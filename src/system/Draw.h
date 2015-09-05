struct Map;
struct Image;

int Draw(void);
void Draw_(void);
void DrawSetCamera(const float x, const float y);
void DrawGetCamera(float *x_ptr, float *y_ptr);
void DrawSetBackground(const char *const str);
