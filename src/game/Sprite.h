struct Image;

enum Sprites { S_NONE = 0, S_DEBRIS, S_SHIP, S_WMD };

struct Sprite;

struct Sprite *Sprite(const enum Sprites type, const struct Image *image);
void Sprite_(struct Sprite **spriteptr);
int SpriteGetSize(const struct Sprite *const s);
int SpriteGetConsidered(void);
int SpriteGetOnscreen(void);
int SpriteGetCapacity(void);
int SpriteNo(void);
int SpriteGetCapacity(void);
void SpriteSetContainer(void *const container, struct Sprite **const notify);
int SpriteIterate(float *x_ptr, float *y_ptr, float *theta_ptr, int *texture_ptr, int *size_ptr);
/*void SpriteResetIterator(void);*/
void SpriteSetOrientation(struct Sprite *sprite, const float x, const float y, const float theta);
void SpriteSetVelocity(struct Sprite *sprite, const float vx, const float vy);
void SpriteGetOrientation(const struct Sprite *sprite, float *x_ptr, float *y_ptr, float *theta_ptr);
void SpriteGetVelocity(const struct Sprite *sprite, float *vx_ptr, float *vy_ptr);
void SpriteSetTheta(struct Sprite *sprite, const float theta);
enum Sprites SpriteGetType(const struct Sprite *sprite);
void *SpriteGetContainer(const struct Sprite *sprite);
float SpriteGetBounding(const struct Sprite *sprite);
struct Sprite *SpriteGetCircle(const float x, const float y, const float r);
void SpriteUpdate(const float s);
/*void SpriteSetViewport(const int width, const int height);*/
int SpriteGetId(const struct Sprite *s);
void SpritePrint(const char *location);
