#include "../general/OrthoMath.h"

struct Sprite;
struct Ship;
struct AutoShipClass;

typedef void (*SpriteOutput)(const struct Ortho3f *const x,
	const struct AutoImage *const sprite,
	const struct AutoImage *const normals);

struct Ship *Ship(const struct AutoShipClass *const class,
	const struct Ortho3f *const x);
int Sprites(void);
void Sprites_(void);
void SpriteUpdate(const int dt_ms_passed);
void SpriteDraw(const SpriteOutput draw);
void SpritePlot(void);
