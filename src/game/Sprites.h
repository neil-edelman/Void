struct Vec2f;
struct Ortho3f;
struct Sprites;
struct Sprite;
struct Ship;
struct AutoImage;
struct AutoShipClass;
struct AutoDebris;
struct AutoWmdType;
struct AutoGate;
struct AutoObjectInSpace;
typedef int (*SpritesPredicate)(const struct Sprite *const);

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
void SpritesUpdate(const int dt_ms);
void SpritesDraw(void);
void Info(const struct Vec2f *const x, const struct AutoImage *const image);
void SpritesInfo(void);
void SpritesRemoveIf(const SpritesPredicate predicate);
const struct Ortho3f *SpriteGetPosition(const struct Sprite *const this);
const struct Ortho3f *SpriteGetVelocity(const struct Sprite *const this);
void SpriteSetPosition(struct Sprite *const this,const struct Ortho3f *const x);
void SpriteSetVelocity(struct Sprite *const this,const struct Ortho3f *const v);
const struct AutoSpaceZone *GateGetTo(const struct Gate *const this);
struct Gate *FindGate(const struct AutoSpaceZone *const to);
struct Ship *SpritesGetPlayerShip(void);
const struct Vec2f *ShipGetHit(const struct Ship *const this);
char *SpritesToString(const struct Sprite *const this);
unsigned SpriteGetBin(const struct Sprite *const this);

/* In {SpritesLight.h}. */
void SpritesLightClear(void);
size_t SpritesLightGetSize(void);
struct Vec2f *SpritesLightPositions(void);
struct Colour3f *SpritesLightGetColours(void);

/* In {SpritesPlot.h} */
void SpritesPlotSpace(void);
