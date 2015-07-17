struct Vec2f;
struct Colour3f;

struct Light;

int Light(const float i, const float r, const float g, const float b);
void Light_(const int no);
void LightSetContainer(int *const no_p1_ptr);
int LightGetNo(void);
struct Vec2f *LightGetPositions(void);
void LightSetPosition(const int h, const float x, const float y);
struct Colour3f *LightGetColours(void);
void LightDebug(void);
