#include "../general/OrthoMath.h"

struct Sprites;
struct Sprite;
struct Ship;
struct AutoShipClass;
struct AutoDebris;
struct AutoWmdType;
struct AutoGate;
struct AutoObjectInSpace;

enum AiType { AI_DUMB, AI_HUMAN };

typedef void (*InfoOutput)(const struct Vec2f *const x,
	const struct AutoImage *const sprite);
typedef void (*LambertOutput)(const struct Ortho3f *const x,
	const struct AutoImage *const image,
	const struct AutoImage *const normals);

void Sprites_(struct Sprites **const thisp);
struct Sprites *Sprites(void);
struct Ship *SpritesShip(struct Sprites *const this,
	const struct AutoShipClass *const class,
	const struct Ortho3f *const x, const enum AiType ai);
struct Debris *SpritesDebris(struct Sprites *const sprites,
	const struct AutoDebris *const class, const struct Ortho3f *const x);
struct Wmd *SpritesWmd(struct Sprites *const sprites,
	const struct AutoWmdType *const class, const struct Ship *const from);

/* fixme: stubs */
void SpritesUpdate(float, struct Ship *);
void SpritesPlot(void);
