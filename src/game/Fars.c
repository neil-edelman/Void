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
#include "Fars.h"



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
#include "../templates/Pool.h"

/** Fars all together. */
struct Fars {
	struct FarList bins[BIN_BIN_BG_SIZE];
	struct PlanetoidPool *planetoids;
};



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
static void Planetoid_to_string(const struct Planetoid *this,
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
	(FarToString)&Planetoid_to_string
};



/************* Type functions. **************/

/** @implements Migrate */
static void bin_migrate(void *const fars_void,
	const struct Migrate *const migrate) {
	struct Fars *const fars = fars_void;
	unsigned i;
	assert(fars && migrate);
	for(i = 0; i < BIN_BIN_BG_SIZE; i++) {
		FarListMigrate(fars->bins + i, migrate);
	}
}

/** Destructor. */
void Fars_(struct Fars **const thisp) {
	struct Fars *this;
	unsigned i;
	if(!thisp || (this = *thisp)) return;
	for(i = 0; i < BIN_BIN_BG_SIZE; i++) FarListClear(this->bins + i);
	PlanetoidPool_(&this->planetoids);
	this = *thisp = 0;
}

/** Constructor.
 @return New {Fars}. */
struct Fars *Fars(void) {
	struct Fars *this;
	unsigned i;
	if(!(this = malloc(sizeof *this)))
		{ perror("Fars"); Fars_(&this); return 0; }
	for(i = 0; i < BIN_BIN_BG_SIZE; i++) FarListClear(this->bins + i);
	this->planetoids = 0;
	if(!(this->planetoids = PlanetoidPool(&bin_migrate, this))) {
		fprintf(stderr,
			"Fars.Planetoid: %s.\n", PlanetoidPoolGetError(this->planetoids));
		Fars_(&this);
		return 0;
	}
	return this;
}

/** Clears all planets in preparation for jump. */
void FarsClear(struct Fars *const this) {
	unsigned i;
	if(!this) return;
	for(i = 0; i < BIN_BIN_BG_SIZE; i++) FarListClear(this->bins + i);
	PlanetoidPoolClear(this->planetoids);
}



/*************** Sub-type constructors. ******************/

/** Only called from constructors of children. */
static void far_filler(struct Fars *const fars, struct Far *const this,
	const struct FarVt *const vt, const struct AutoObjectInSpace *const class) {
	assert(fars && this && vt && class && class->sprite);
 	this->vt = vt;
	this->image = class->sprite->image;
	this->normals = class->sprite->normals;
	this->x.x = class->x;
	this->x.y = class->y;
	this->x.theta = 0.0f;
	Vec2f_to_bg_bin((struct Vec2f *)&this->x, &this->bin);
	FarListPush(fars->bins + this->bin, (struct FarListNode *)this);
}

/** Extends {far_filler}. */
struct Planetoid *FarsPlanetoid(struct Fars *const fars,
	const struct AutoObjectInSpace *const class) {
	struct Planetoid *this;
	struct Ortho3f x;
	if(!fars || !class) return 0;
	assert(class->sprite && class->name);
	if(!(this = PlanetoidPoolNew(fars->planetoids)))
		{ fprintf(stderr, "FarsPlanetoid: %s.\n",
		PlanetoidPoolGetError(fars->planetoids)); return 0; }
	far_filler(fars, &this->far.data, &planetoid_vt, class);
	this->name = class->name;
	printf("Planetoid %s at (%f, %f)!!!\n", this->name, x.x, x.y);
	return this;
}
