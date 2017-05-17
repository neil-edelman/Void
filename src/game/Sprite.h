struct AutoGate;
struct AutoSpaceZone;

/** See \see{Sprite}. */
struct Sprite;

/** Sprite type. */
enum SpType { SP_DEBRIS, SP_SHIP, SP_WMD, SP_ETHEREAL };
/** Sprite AI behaviour. */
enum Behaviour { B_NONE, B_HUMAN, B_STUPID };

struct Sprite *Sprite(const enum SpType sp_type, ...);
struct Sprite *SpriteGate(const struct AutoGate *gate);
void Sprite_(struct Sprite **sprite_ptr);
int SpriteGetConsidered(void);
int SpriteGetOnscreen(void);
void SpriteGetPosition(const struct Sprite *sprite, float *const x_ptr, float *const y_ptr);
void SpriteSetPosition(struct Sprite *const sprite, const float x, const float y);
float SpriteGetTheta(const struct Sprite *const sprite);
void SpriteSetTheta(struct Sprite *const sprite, const float theta);
void SpriteAddTheta(struct Sprite *sprite, const float theta);
void SpriteGetVelocity(const struct Sprite *const sprite, float *vx_ptr, float *vy_ptr);
void SpriteSetVelocity(struct Sprite *const sprite, const float vx, const float vy);
float SpriteGetOmega(const struct Sprite *const sprite);
void SpriteSetOmega(struct Sprite *const sprite, const float omega);
float SpriteGetBounding(const struct Sprite *const sprite);
float SpriteGetMass(const struct Sprite *const s);
unsigned SpriteGetSize(const struct Sprite *const s);
void SpriteSetNotify(struct Sprite **const s_ptr);
enum SpType SpriteGetType(const struct Sprite *const sprite);
char *SpriteToString(const struct Sprite *const s);
const struct AutoSpaceZone *SpriteGetTo(const struct Sprite *const s);
int SpriteGetDamage(const struct Sprite *const s);
int SpriteGetHit(const struct Sprite *const s);
int SpriteGetMaxHit(const struct Sprite *const s);
struct Event *SpriteGetEventRecharge(const struct Sprite *const s);
const struct AutoWmdType *SpriteGetWeapon(const struct Sprite *const s);
void SpriteRecharge(struct Sprite *const s, const int recharge);
int SpriteIsDestroyed(const struct Sprite *const s);
void SpriteDestroy(struct Sprite *const s);
void SpriteDebris(const struct Sprite *const s);
void SpriteInput(struct Sprite *s, const int turning, const int acceleration, const int dt_ms);
void SpriteUpdate(const int dt_ms);
void SpriteShoot(struct Sprite *const s);
struct Sprite *SpriteOutgoingGate(const struct AutoSpaceZone *to);
void SpriteRemoveIf(int (*const predicate)(struct Sprite *const));
int SpriteIterate(float *x_ptr, float *y_ptr, float *theta_ptr, unsigned *texture_ptr, unsigned *size_ptr);
void SpriteList(void);
