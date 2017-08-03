#include "../general/OrthoMath.h"

struct Sprite;
struct Ship;
struct AutoShipClass;
struct AutoDebris;
struct AutoWmdType;
struct AutoGate;

enum AiType { AI_DUMB, AI_HUMAN };

typedef void (*SpriteOutput)(const struct Ortho3f *const x,
	const struct AutoImage *const sprite,
	const struct AutoImage *const normals);

struct Ship *Ship(const struct AutoShipClass *const class,
	const struct Ortho3f *const x, const enum AiType ai);
struct Debris *Debris(const struct AutoDebris *const class,
	const struct Ortho3f *const x);
struct Wmd *Wmd(const struct AutoWmdType *const class,
	const struct Ship *const from);
struct Gate *Gate(const struct AutoGate *const class);
int Sprites(void);
void Sprites_(void);
void SpritesUpdate(const int dt_ms_passed, struct Ship *const centre);
void SpritesDraw(const SpriteOutput draw);
void SpritesPlot(void);
