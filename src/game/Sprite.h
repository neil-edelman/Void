#include "../general/Ortho.h"

struct AutoGate;
struct AutoSpaceZone;

struct Sprite;
struct Debris;
struct Ship;
struct Wmd;
struct Gate;

/** Sprite AI behaviour. */
enum SpriteBehaviour { SB_NONE, SB_HUMAN, SB_STUPID };

struct Debris *Debris(const struct AutoDebris *const class,
	struct Ortho *r, const struct Ortho *v);
struct Ship *Ship(const struct AutoShipClass *const class,
	struct Ortho *const r, const enum SpriteBehaviour behaviour);
int Gate(const struct AutoGate *const class);

int Sprite(void);
void Sprite_(void);
void SpriteOut(struct Sprite *const this);

void ShipInput(struct Ship *const this, const int turning,
	const int acceleration, const int dt_ms);
void ShipShoot(struct Ship *const this);
void ShipGetPosition(const struct Ship *this, struct Ortho2f *pos);
void ShipSetPosition(struct Ship *const this, struct Ortho2f *const pos);
unsigned ShipGetMaxHit(const struct Ship *const this);

int WmdGetDamage(const struct Wmd *const this);

const struct AutoSpaceZone *GateGetTo(const struct Gate *const this);



/*void SpriteRemoveIf(int (*const predicate)(struct Sprite *const));
int SpriteIterate(float *x_ptr, float *y_ptr, float *theta_ptr, unsigned *image_ptr, unsigned *normals_ptr, unsigned *size_ptr);
void SpriteList(void);*/
