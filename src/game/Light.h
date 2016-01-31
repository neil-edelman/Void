struct Vec2f;
struct Colour3f;

struct Light;

///////// REMOVE
struct Sprite;

int Light(int *const id_ptr, const float i, const float r, const float g, const float b, const struct Sprite *const wmd);
void Light_(int *light_ptr);
unsigned LightGetArraySize(void);
struct Vec2f *LightGetPositionArray(void);
struct Colour3f *LightGetColourArray(void);
void LightSetPosition(const int id, const float x, const float y);
void LightSetNotify(int *const id_ptr);
char *LightToString(const int id);
void LightList(void);
void BubblePush(void);
void BubblePop(void);
