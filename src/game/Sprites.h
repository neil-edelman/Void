#include "../general/OrthoMath.h"

struct Sprites;
struct Sprite;
struct Ship;
struct AutoImage;
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

void Sprites_(void);
int Sprites(void);
struct Ship *SpritesShip(const struct AutoShipClass *const class,
	const struct Ortho3f *const x, const enum AiType ai);
struct Debris *SpritesDebris(const struct AutoDebris *const class,
	const struct Ortho3f *const x);
struct Wmd *SpritesWmd(const struct AutoWmdType *const class,
	const struct Ship *const from);
struct Gate *SpritesGate(const struct AutoGate *const class);
void SpritesUpdate(const int dt_ms, struct Sprite *const target);
void SpritesDrawForeground(const LambertOutput draw);


/* fixme: stubs */
void SpritesPlot(void);
