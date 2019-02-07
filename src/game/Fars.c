/** 2017 Neil Edelman, distributed under the terms of the GNU General
 Public License 3, see copying.txt, or
 \url{ https://opensource.org/licenses/GPL-3.0 }.

 Fars are sprites, like Items, but drawn far away; they are simpler because
 they have no rotation, and have a different shader associated. Also a
 different layer.

 @title		Fars
 @author	Neil
 @std		C89/90
 @version	1.0, 2017-10 start */

#include <assert.h>
#include "../Ortho.h" /* for measurements and types */
#include "../../build/Auto.h" /* for AutoImage, AutoShipClass, etc */
#include "../general/Layer.h" /* for discretising */
#include "../system/Draw.h" /* DrawGetScreen, DrawDisplayFar */
#include "Fars.h"

#define LAYER_FORESHORTENING_F 0.2f
#define LAYER_SIDE_SIZE (3)
#define LAYER_SIZE (LAYER_SIDE_SIZE * LAYER_SIDE_SIZE)
#define LAYER_SPACE (1024.0f / LAYER_FORESHORTENING_F)
static struct Layer layer = LAYER_STATIC_INIT(LAYER_SIDE_SIZE, LAYER_SPACE);

/**************** Declare types. **************/

struct FarVt;
/* Abstract. */
struct Far {
	const struct FarVt *vt;
	const struct AutoImage *image, *normals; /* Sprite. */
	unsigned bin;
	struct Ortho3f x; /* Where it is determines which bin; like a key. */
};
typedef void (*FarToString)(const struct Far *const, char (*const)[12]);

static void far_to_string(const struct Far *this, char (*const a)[12]);
#define LIST_NAME Far
#define LIST_TYPE struct Far
#define LIST_TO_STRING &far_to_string
#include "../templates/List.h"

/** {Planetoid} extends {Far}. Planets and asteroids, everything that has a
 name and you can optionally land on, is a {Planetoid}. */
struct Planetoid {
	struct FarLink base;
	const char *name;
};
/** @implements <Planetoid>Migrate */
static void planetoid_migrate(struct Planetoid *const this,
	const struct Migrate *const migrate) {
	assert(this && migrate);
	FarLinkMigrate(&this->base.data, migrate);
}
#define POOL_NAME Planetoid
#define POOL_TYPE struct Planetoid
#define POOL_MIGRATE_EACH &planetoid_migrate
#include "../templates/Pool.h"

/** Fars all together. */
static struct Fars {
	struct FarList bins[LAYER_SIZE];
	struct PlanetoidPool planetoids;
} fars; /* Not in a valid state until \see{FarsReset}. */



/************** Define virtual functions. ******************/

/** Define {FarVt}. */
struct FarVt {
	FarToString to_string;
};

/** @implements <Far>ToString */
static void far_to_string(const struct Far *this, char (*const a)[12]) {
	assert(this && a);
	this->vt->to_string(this, a);
}
/** @implements <Planetoid>ToString */
static void planetoid_to_string(const struct Planetoid *this,
	char (*const a)[12]) {
	sprintf(*a, "%.11s", this->name);
}

#if 0
/** @implements <Planetoid>Action */
static void Planetoid_delete(struct Planetoid *const this) {
	FarListRemove(fars->fars, &this->sprite.data);
	PlanetoidPoolRemove(planetoids, this);
}
#endif

static const struct FarVt planetoid_vt = {
	(FarToString)&planetoid_to_string
};



/************* Type functions. **************/

/** Destructor. */
void Fars_(void) {
	unsigned i;
	for(i = 0; i < LAYER_SIZE; i++) FarListClear(&fars.bins[i]);
	PlanetoidPool_(&fars.planetoids);
}

/** Constructor.
 @return New {Fars} or {errno} will be set (probably.) */
int Fars(void) {
	unsigned i;
	for(i = 0; i < LAYER_SIZE; i++) FarListClear(fars.bins + i);
	PlanetoidPool(&fars.planetoids);
	return 1;
}

/** Clears all planets in preparation for jump. */
void FarsClear(void) {
	unsigned i;
	for(i = 0; i < LAYER_SIZE; i++) FarListClear(fars.bins + i);
	PlanetoidPoolClear(&fars.planetoids);
}



/*************** Sub-type constructors. ******************/

/** Only called from constructors of children. */
static void far_filler(struct Far *const this,
	const struct FarVt *const vt, const struct AutoObjectInSpace *const class) {
	assert(this && vt && class && class->sprite);
 	this->vt = vt;
	this->image = class->sprite->image;
	this->normals = class->sprite->normals;
	this->x.x = class->x;
	this->x.y = class->y;
	this->x.theta = 0.0f;
	this->bin = LayerHashOrtho(&layer, &this->x);
	FarListPush(fars.bins + this->bin, this);
}

/** @extends far_filler
 @throws If return null, {errno} will probably be set. */
struct Planetoid *FarsPlanetoid(const struct AutoObjectInSpace *const class) {
	struct Planetoid *this;
	if(!class) return 0;
	assert(class->sprite && class->name);
	if(!(this = PlanetoidPoolNew(&fars.planetoids))) return 0;
	far_filler(&this->base.data, &planetoid_vt, class);
	this->name = class->name;
	return this;
}

/** Called from \see{draw_bin}.
 @implements <Sprite, ContainsLambertOutput>BiAction */
static void draw_far(struct Far *const this) {
	DrawDisplayFar(&this->x, this->image, this->normals);
}
/** Called from \see{SpritesDraw}.
 @implements LayerAction */
static void draw_bin(const unsigned idx) {
	assert(idx < LAYER_SIZE);
	FarListForEach(fars.bins + idx, &draw_far);
}
/* Must call \see{SpriteUpdate} because it sets the camera. Use when Far GPU
 shader is loaded. */
void FarsDraw(void) {
	struct Rectangle4f rect;
	DrawGetScreen(&rect);
	rectangle4f_expand(&rect, LAYER_SPACE * 0.5f); /* fixme: maybe? */
	rectangle4f_scale(&rect, LAYER_FORESHORTENING_F);
	LayerMask(&layer, &rect);
	LayerForEachMask(&layer, &draw_bin);
}
