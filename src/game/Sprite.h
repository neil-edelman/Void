#include "../general/OrthoMath.h"

struct Sprite;
struct Ship;
struct AutoShipClass;
struct AutoDebris;
struct AutoWmdType;
struct AutoGate;

typedef void (*SpriteOutput)(const struct Ortho3f *const x,
	const struct AutoImage *const sprite,
	const struct AutoImage *const normals);

struct Ship *Ship(const struct AutoShipClass *const class,
	const struct Ortho3f *const x);
struct Debris *Debris(const struct AutoDebris *const class,
	const struct Ortho3f *const x);
struct Wmd *Wmd(const struct AutoWmdType *const class,
	const struct Ship *const from);
struct Gate *Gate(const struct AutoGate *const class);
int Sprites(void);
void Sprites_(void);
void SpriteUpdate(const int dt_ms_passed);
void SpriteDraw(const SpriteOutput draw);
void SpritePlot(void);
