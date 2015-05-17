struct Sprite;

struct Sprite *Sprite(const int tex, const int width, const int height);
void Sprite_(struct Sprite **spriteptr);
int SpriteGetTexture(const struct Sprite *sprite);
int SpriteGetNo(const struct Sprite *sprite);
void SpriteSetNo(struct Sprite *sprite, const int no);
struct Vec2f SpriteGetPosition(const struct Sprite *sprite);
float SpriteGetAngle(const struct Sprite *sprite);
float SpriteGetDegrees(const struct Sprite *sprite);
struct Vec2f SpriteGetVelocity(const struct Sprite *sprite);
struct Vec2f SpriteGetHalf(const struct Sprite *sprite);
void SpriteImpact(struct Sprite *sprite, const struct Vec2f *impact);
void SpriteSetAcceleration(struct Sprite *sprite, const float a);
void SpriteSetOmega(struct Sprite *sprite, const float w);
void SpriteUpdate(struct Sprite *sprite, const int ms);
void SpriteRandomise(struct Sprite *sprite);
void SpritePush(struct Sprite *sprite, float push);
