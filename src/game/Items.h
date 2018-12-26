struct Vec2f;
struct Ortho3f;
struct Items;
struct Item;
struct Ship;
struct AutoImage;
struct AutoShipClass;
struct AutoDebris;
struct AutoWmdType;
struct AutoGate;
struct AutoObjectInSpace;
typedef int (*ItemsPredicate)(const struct Item *const);

enum AiType { AI_DUMB, AI_HUMAN };

typedef void (*InfoOutput)(const struct Vec2f *const x,
	const struct AutoImage *const sprite);
typedef void (*LambertOutput)(const struct Ortho3f *const x,
	const struct AutoImage *const image,
	const struct AutoImage *const normals);

void Items_(void);
int Items(void);
struct Ship *Ship(const struct AutoShipClass *const class,
	const struct Ortho3f *const x, const enum AiType ai);
struct Debris *Debris(const struct AutoDebris *const class,
	const struct Ortho3f *const x);
struct Wmd *Wmd(const struct AutoWmdType *const class,
	const struct Ship *const from);
struct Gate *Gate(const struct AutoGate *const class);
void ItemsUpdate(const int dt_ms);
void ItemsDraw(void);
void Info(const struct Vec2f *const x, const struct AutoImage *const image);
void ItemsInfo(void);
void ItemsRemoveIf(const ItemsPredicate predicate);
const struct Ortho3f *ItemGetPosition(const struct Item *const this);
const struct Ortho3f *ItemGetVelocity(const struct Item *const this);
void ItemSetPosition(struct Item *const this,const struct Ortho3f *const x);
void ItemSetVelocity(struct Item *const this,const struct Ortho3f *const v);
const struct AutoSpaceZone *GateGetTo(const struct Gate *const this);
struct Gate *FindGate(const struct AutoSpaceZone *const to);
struct Ship *ItemsGetPlayerShip(void);
const struct Vec2f *ShipGetHit(const struct Ship *const this);
char *ItemsToString(const struct Item *const this);
unsigned ItemGetBin(const struct Item *const this);

/* In {ItemsLight.h}. */
void ItemsLightClear(void);
size_t ItemsLightGetSize(void);
struct Vec2f *ItemsLightPositions(void);
struct Colour3f *ItemsLightGetColours(void);

/* In {ItemsPlot.h} */
void ItemsPlotSpace(void);
