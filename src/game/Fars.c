/** 2017 Neil Edelman, distributed under the terms of the GNU General
 Public License 3, see copying.txt, or
 \url{ https://opensource.org/licenses/GPL-3.0 }.

 Fars are like sprites, but drawn far away; they are simpler.

 @title		Fars
 @author	Neil
 @std		C89/90
 @version	1.0, 2017-10 start */

#include <assert.h>
#include "../../build/Auto.h" /* for AutoImage, AutoShipClass, etc */
#include "../general/OrthoMath.h" /* for measurements and types */
#include "../general/Layer.h" /* for descritising */
#include "../system/Draw.h" /* DrawGetScreen, DrawDisplayFar */
#include "Fars.h"

#define LAYER_FORESHORTENING_F 0.2f
#define LAYER_SIDE_SIZE (3)
#define LAYER_SIZE (LAYER_SIDE_SIZE * LAYER_SIDE_SIZE)
static const float layer_space = 1024.0f / LAYER_FORESHORTENING_F;

/**************** Declare types. **************/

struct FarVt;
/** Define abstract {Far}. */
struct Far {
	const struct FarVt *vt;
	const struct AutoImage *image, *normals; /* what the sprite is */
	unsigned bin; /* which bin is it in */
	struct Ortho3f x; /* where it is */
};
static void far_to_string(const struct Far *this, char (*const a)[12]);
#define LIST_NAME Far
#define LIST_TYPE struct Far
#define LIST_TO_STRING &far_to_string
#include "../templates/List.h"

/** Define {PlanetoidPool} and {PlanetoidPoolNode}, a subclass of {Far}.
 Planets and asteroids, everything that has a name and you can optionally land
 on, is a {Planetoid}. */
struct Planetoid {
	struct FarListNode far;
	const char *name;
};
#define POOL_NAME Planetoid
#define POOL_TYPE struct Planetoid
#define POOL_MIGRATE struct Fars
#include "../templates/Pool.h"

/** Fars all together. */
static struct Fars {
	struct FarList bins[LAYER_SIZE];
	struct PlanetoidPool *planetoids;
	struct Layer *layer;
} *fars;



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

/** @implements Migrate */
static void bin_migrate(struct Fars *const f,
	const struct Migrate *const migrate) {
	unsigned i;
	assert(f && f == fars && migrate);
	for(i = 0; i < LAYER_SIZE; i++) {
		FarListMigrate(f->bins + i, migrate);
	}
}

/** Destructor. */
void Fars_(void) {
	unsigned i;
	if(!fars) return;
	for(i = 0; i < LAYER_SIZE; i++) FarListClear(fars->bins + i);
	PlanetoidPool_(&fars->planetoids);
	free(fars), fars = 0;
}

/** Constructor.
 @return New {Fars}. */
int Fars(void) {
	unsigned i;
	enum { NO, PLANETOID, LAYER } e = NO;
	const char *ea = 0, *eb = 0;
	if(fars) return 1;
	if(!(fars = malloc(sizeof *fars)))
		{ perror("Fars"); Fars_(); return 0; }
	for(i = 0; i < LAYER_SIZE; i++) FarListClear(fars->bins + i);
	fars->planetoids = 0;
	fars->layer = 0;
	do {
		if(!(fars->planetoids = PlanetoidPool(&bin_migrate, fars)))
			{ e = PLANETOID; break; }
		if(!(fars->layer = Layer(LAYER_SIDE_SIZE, layer_space))){e=LAYER;break;}
	} while(0); switch(e) {
		case NO: break;
		case PLANETOID: ea = "planetoids",
			eb = PlanetoidPoolGetError(fars->planetoids); break;
		case LAYER: ea = "layer", eb = "couldn't get layer"; break;
	} if(e) {
		fprintf(stderr, "Fars %s buffer: %s.\n", ea, eb);
		Fars_();
		return 0;
	}
	return 1;
}

/** Clears all planets in preparation for jump. */
void FarsClear(void) {
	unsigned i;
	if(!fars) return;
	for(i = 0; i < LAYER_SIZE; i++) FarListClear(fars->bins + i);
	PlanetoidPoolClear(fars->planetoids);
}



/*************** Sub-type constructors. ******************/

/** Only called from constructors of children. */
static void far_filler(struct Far *const this,
	const struct FarVt *const vt, const struct AutoObjectInSpace *const class) {
	assert(fars && this && vt && class && class->sprite);
 	this->vt = vt;
	this->image = class->sprite->image;
	this->normals = class->sprite->normals;
	this->x.x = class->x;
	this->x.y = class->y;
	this->x.theta = 0.0f;
	this->bin = LayerGetOrtho(fars->layer, &this->x);
	FarListPush(fars->bins + this->bin, this);
}

/** Extends {far_filler}. */
struct Planetoid *FarsPlanetoid(const struct AutoObjectInSpace *const class) {
	struct Planetoid *this;
	if(!fars || !class) return 0;
	assert(class->sprite && class->name);
	if(!(this = PlanetoidPoolNew(fars->planetoids)))
		{ fprintf(stderr, "FarsPlanetoid: %s.\n",
		PlanetoidPoolGetError(fars->planetoids)); return 0; }
	far_filler(&this->far.data, &planetoid_vt, class);
	this->name = class->name;
	return this;
}

/** Called from \see{draw_bin}.
 @implements <Sprite, ContainsLambertOutput>BiAction */
static void draw_far(struct Far *const this) {
	assert(fars);
	DrawDisplayFar(&this->x, this->image, this->normals);
}
/** Called from \see{SpritesDraw}.
 @implements LayerAction */
static void draw_bin(const unsigned idx) {
	assert(fars && idx < LAYER_SIZE);
	FarListForEach(fars->bins + idx, &draw_far);
}
/* Must call \see{SpriteUpdate} because it sets the camera. Use when Far GPU
 shader is loaded. */
void FarsDraw(void) {
	struct Rectangle4f rect;
	if(!fars) return;
	DrawGetScreen(&rect);
	rectangle4f_expand(&rect, layer_space * 0.5f); /* fixme: maybe? */
	rectangle4f_scale(&rect, LAYER_FORESHORTENING_F);
	LayerSetScreenRectangle(fars->layer, &rect);
	LayerForEachScreen(fars->layer, &draw_bin);
}
