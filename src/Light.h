struct Light;

int Light(const struct Vec2f p, const struct Vec2f v, const struct Colour3f lux, const int damage, const int expires);
void Light_(const int no);
int LightGetNo(void);
const struct Colour3f *LightGetColours(void);
const struct Vec2f *LightGetPositions(void);
int LightGetMax(void);

/*void LightDraw(void);*/
void LightUpdate(const int ms);
int LightCollide(const struct Sprite *e, struct Vec2f *push);
