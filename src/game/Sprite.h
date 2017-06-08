#include "../general/OrthoMath.h"

struct AutoGate;
struct AutoSpaceZone;

struct Sprite;
struct Debris;
struct Ship;
struct Wmd;
struct Gate;

/** Sprite AI behaviour. */
enum SpriteBehaviour { SB_NONE, SB_HUMAN, SB_STUPID };

void SpriteSetFrameTime(const float dt_ms);

int Debris(const struct AutoDebris *const class,
	struct Ortho3f *r, const struct Ortho3f *v);
struct Ship *Ship(const struct AutoShipClass *const class,
	struct Ortho3f *const r, const enum SpriteBehaviour behaviour);
int Gate(const struct AutoGate *const class);

int Sprite(void);
void Sprite_(void);
void SpriteOut(struct Sprite *const this);
const struct SpriteList *SpriteListBin(struct Vec2u bin2);
const struct Sprite *SpriteBinIterate(const struct Sprite *this);
void SpriteExtractDrawing(const struct Sprite *const this,
	const struct Ortho3f **const r_ptr, unsigned *const image_ptr,
	unsigned *const normals_ptr, const struct Vec2u **const size_ptr);
void ShipInput(struct Ship *const this, const int turning,
	const int acceleration, const int dt_ms);
void ShipShoot(struct Ship *const this);
void ShipGetPosition(const struct Ship *this, struct Vec2f *pos);
void ShipSetPosition(struct Ship *const this, struct Vec2f *const pos);
void ShipRechage(struct Ship *const this, const int recharge);
void ShipGetHit(const struct Ship *const this, struct Vec2u *const hit);

int WmdGetDamage(const struct Wmd *const this);

const struct AutoSpaceZone *GateGetTo(const struct Gate *const this);
struct Gate *GateFind(struct AutoSpaceZone *const to);



/*void SpriteRemoveIf(int (*const predicate)(struct Sprite *const));
int SpriteIterate(float *x_ptr, float *y_ptr, float *theta_ptr, unsigned *image_ptr, unsigned *normals_ptr, unsigned *size_ptr);
void SpriteList(void);*/
