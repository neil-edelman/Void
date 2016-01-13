struct Vec2f;
struct Colour3f;

struct Light;

int Light(unsigned *const light_ptr, const float i, const float r, const float g, const float b);
void Light_(unsigned *light_ptr);
void LightClear(void);
int LightGetArraySize(void);
struct Vec2f *LightGetPositionArray(void);
struct Colour3f *LightGetColourArray(void);
void LightSetPosition(const unsigned light, const float x, const float y);
void LightSetNotify(const unsigned light, unsigned *const light_ptr);
char *LightToString(const unsigned light);
void LightList(void);
