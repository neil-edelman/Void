/** 2-vector of floats. */
struct Vec2f;
/** Normalised 3-vector of floats. */
struct Colour3f;

/** See \see{Light}. */
struct Light;

int Light(int *const id_ptr, const float i, const float r, const float g, const float b);
void Light_(int *light_ptr);
unsigned LightGetArraySize(void);
struct Vec2f *LightGetPositionArray(void);
struct Colour3f *LightGetColourArray(void);
void LightSetPosition(const int id, const float x, const float y);
void LightSetNotify(int *const id_ptr);
const char *LightToString(const int id);
void LightList(void);
/*void BubblePush(void);
void BubblePop(void);*/
