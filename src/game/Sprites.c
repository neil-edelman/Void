/** 2017 Neil Edelman, distributed under the terms of the GNU General
 Public License 3, see copying.txt, or
 \url{ https://opensource.org/licenses/GPL-3.0 }.

 Sprites is a polymorphic static structure. Initialise it with {Sprites} and
 before exiting, when all references have been resolved, {Sprites_}.

 @title		Sprites
 @author	Neil
 @std		C89/90
 @version	2017-10 Broke into smaller pieces.
 @since		2017-05 Generics.
			2016-01
			2015-06 */

#include <stdio.h> /* fprintf */
#include "../../build/Auto.h" /* for AutoImage, AutoShipClass, etc */
#include "../general/Orcish.h" /* for human-readable ship names */
/*#include "../general/Events.h"*/ /* for delays */
#include "../general/Layer.h" /* for descritising */
#include "../system/Poll.h" /* input */
#include "../system/Draw.h" /* DrawSetCamera, DrawGetScreen */
#include "../system/Timer.h" /* for expiring */
#include "Light.h" /* for glowing */
#include "Sprites.h"

#define LAYER_SIDE_SIZE (64)
#define LAYER_SIZE (LAYER_SIDE_SIZE * LAYER_SIDE_SIZE)
static const float layer_space = 256.0f;

enum CollisionMask {
	MASK_SHIP = 1,
	MASK_POINT_DEFENSE = 2,
	MASK_DEBIS = 4
};

/* This is used for small floating-point values. The value doesn't have any
 significance. */
static const float epsilon = 0.0005f;
/* Mass is a measure of a rigid body's resistance to a change of state of it's
 motion; it is a divisor, thus cannot be (near) zero. */
static const float minimum_mass = 0.01f; /* mg (t) */
/* Controls how far past the bounding radius the ship's shots appear. 1.415? */
static const float wmd_distance_mod = 1.3f;
/* Controls the acceleration of a turn; low values are sluggish; (0, 1]. */
static const float turn_acceleration = 0.005f;
/* {0.995^t} Taylor series at {t = 25ms}. */
static const float turn_damping_per_25ms = 0.88222f;
static const float turn_damping_1st_order = 0.00442217f;



/******************* Declare types. ***************/

/* Define {SpriteList} and {SpriteListNode}. */
struct SpriteVt;
struct Collision;
/** Define abstract {Sprite}. */
struct Sprite {
	const struct SpriteVt *vt; /* virtual table pointer */
	const struct AutoImage *image, *normals; /* what the sprite is */
	float bounding; /* radius, fixed to function of the image */
	struct Ortho3f x, v; /* where it is and where it is going */
	unsigned bin; /* which bin is it in, set by {x} */
	/* The following are temporary: */
	struct Vec2f dx; /* temporary displacement */
	struct Rectangle4f box; /* bounding box between one frame and the next */
	struct Collision *collision; /* temporary, \in {sprites.collisions} */
};
static void sprite_to_string(const struct Sprite *this, char (*const a)[12]);
#define LIST_NAME Sprite
#define LIST_TYPE struct Sprite
#define LIST_TO_STRING &sprite_to_string
#include "../templates/List.h"

/* Declare {ShipVt}. */
struct ShipVt;
/** Define {ShipPool} and {ShipPoolNode}, a subclass of {Sprite}. */
struct Ship {
	struct SpriteListNode sprite;
	struct ShipVt *vt;
	float mass;
	float shield, ms_recharge, max_speed2, acceleration, turn;
	char name[16];
	const struct AutoWmdType *wmd;
	unsigned ms_recharge_wmd;
};
#define POOL_NAME Ship
#define POOL_TYPE struct Ship
#include "../templates/Pool.h"

/** Define {DebrisPool} and {DebrisPoolNode}, a subclass of {Sprite}. */
struct Debris {
	struct SpriteListNode sprite;
	float mass;
};
#define POOL_NAME Debris
#define POOL_TYPE struct Debris
#include "../templates/Pool.h"

/** Define {WmdPool} and {WmdPoolNode}, a subclass of {Sprite}. */
struct Wmd {
	struct SpriteListNode sprite;
	const struct AutoWmdType *class;
	const struct Sprite *from;
	float mass;
	unsigned expires;
	unsigned light;
};
#define POOL_NAME Wmd
#define POOL_TYPE struct Wmd
#include "../templates/Pool.h"

/** Define {GatePool} and {GatePoolNode}, a subclass of {Sprite}. */
struct Gate {
	struct SpriteListNode sprite;
	const struct AutoSpaceZone *to;
};
#define POOL_NAME Gate
#define POOL_TYPE struct Gate
#include "../templates/Pool.h"



/** Define a temporary reference to sprites for collision-detection; these will
 go in each bin that is covered by the sprite and be consumed by the functions
 responsible. The sprite cannot be modified during this operation to simplify
 processing. */
struct Cover {
	struct Sprite *sprite;
	int is_corner;
};
#define STACK_NAME Cover
#define STACK_TYPE struct Cover
#include "../templates/Stack.h"



/* * While {SpriteListForEach} is running, we may have to transfer a sprite to
 another bin, or delete a sprite, or whatever; this causes causality problems
 for iteration. We introduce a delay function that is called right after the
 loop for dealing with that. Defines {DelayPool}, {DelayPoolNode}. */
/*struct Delay {
	SpriteAction action;
	struct Sprite *sprite;
};
static void delay_to_string(const struct Delay *this, char (*const a)[12]) {
	assert(this);
	sprite_to_string(this->sprite, a);
}
#define POOL_NAME Delay
#define POOL_TYPE struct Delay
#define POOL_TO_STRING &delay_to_string
#include "../templates/Pool.h"*/

/** Collisions between sprites to apply later. This is a pool that sprites can
 use. Defines {CollisionPool}, {CollisionPoolNode}. */
struct Collision {
	struct Vec2f v;
	float t;
};
#define POOL_NAME Collision
#define POOL_TYPE struct Collision
#include "../templates/Pool.h"

/** Used in {Sprites}. */
struct Bin {
	struct SpriteList sprites;
	struct CoverStack *covers;
};

/** Sprites all together. */
static struct Sprites {
	struct Bin bins[LAYER_SIZE];
	/* Backing for the {SpriteList} in the {Bin}s. */
	struct ShipPool *ships;
	struct DebrisPool *debris;
	struct WmdPool *wmds;
	struct GatePool *gates;
	/* Contains calculations for the {bins}. */
	struct Layer *layer;
	/* Constantly updating frame time. */
	float dt_ms;
	struct CollisionPool *collisions;
} *sprites;



/*********** Define virtual functions. ***********/

typedef float (*SpriteFloatAccessor)(const struct Sprite *const);
typedef enum CollisionMask (*SpriteMaskAccessor)(const struct Sprite *const);

/** Define {SpriteVt}. */
struct SpriteVt {
	SpriteToString to_string;
	SpriteAction delete, update;
	SpriteFloatAccessor get_mass;
	SpriteMaskAccessor get_self_mask, get_others_mask;
};


/** @implements <Sprite>ToString */
static void sprite_to_string(const struct Sprite *this, char (*const a)[12]) {
	assert(this && a);
	this->vt->to_string(this, a);
}
/** @implements <Ship>ToString */
static void ship_to_string(const struct Ship *this, char (*const a)[12]) {
	sprintf(*a, "%.11s", this->name);
}
/** @implements <Debris>ToString */
static void debris_to_string(const struct Debris *this, char (*const a)[12]) {
	UNUSED(this);
	assert(sprites);
	sprintf(*a, "Debris",
		(unsigned)DebrisPoolGetIndex(sprites->debris, this) % 100000);
}
/** @implements <Wmd>ToString */
static void wmd_to_string(const struct Wmd *this, char (*const a)[12]) {
	UNUSED(this);
	assert(sprites);
	sprintf(*a, "Wmd",
		(unsigned)WmdPoolGetIndex(sprites->wmds, this) % 100000000);
}
/** @implements <Gate>ToString */
static void gate_to_string(const struct Gate *this, char (*const a)[12]) {
	sprintf(*a, "%.7sGate", this->to->name);
}


/** @implements SpritesAction */
static void sprite_delete(struct Sprite *const this) {
	assert(sprites && this);
	SpriteListRemove(&sprites->bins[this->bin].sprites, this); /* Safe-ish. */
	this->vt->delete(this);
}
/** @implements <Ship>Action */
static void ship_delete(struct Ship *const this) {
	ShipPoolRemove(sprites->ships, this);
}
/** @implements <Debris>Action */
static void debris_delete(struct Debris *const this) {
	DebrisPoolRemove(sprites->debris, this);
}
/** @implements <Wmd>Action */
static void wmd_delete(struct Wmd *const this) {
	Light_(&this->light);
	WmdPoolRemove(sprites->wmds, this);
}
/** @implements <Gate>Action */
static void gate_delete(struct Gate *const this) {
	GatePoolRemove(sprites->gates, this);
}


/** @implements <Sprite>Action */
static void sprite_update(struct Sprite *const this) {
	assert(this);
	this->vt->update(this);
}
/* Includes {ship_update*} Human/AI. */
#include "SpritesAi.h"
/** Does nothing; just debris.
 @implements <Debris>Action */
static void debris_update(struct Debris *const this) {
	UNUSED(this);
}
/** @implements <Wmd>Action
 @fixme Replace delete with more dramatic death. */
static void wmd_update(struct Wmd *const this) {
	if(TimerIsTime(this->expires)) /*delay_delete(&this->sprite.data)*/;
}
/** @implements <Gate>Action */
static void gate_update(struct Gate *const this) {
	UNUSED(this);
}


/** @implements <Sprite>FloatAccessor */
static float sprite_get_mass(const struct Sprite *const this) {
	assert(this);
	return this->vt->get_mass(this);
}
/** @implements <Ship>FloatAccessor */
static float ship_get_mass(const struct Ship *const this) {
	/* Mass is used for collisions, you don't want zero-mass objects. */
	assert(this && this->mass >= minimum_mass);
	return this->mass;
}
/** @implements <Debris>FloatAccessor */
static float debris_get_mass(const struct Debris *const this) {
	assert(this && this->mass >= minimum_mass);
	return this->mass;
}
/** @implements <Debris>FloatAccessor */
static float wmd_get_mass(const struct Wmd *const this) {
	assert(this);
	return 1.0f;
}
/** @implements <Gate>FloatAccessor */
static float gate_get_mass(const struct Gate *const this) {
	assert(this);
	return 1e36f; /* No moving. I don't think this is ever called. */
}


/** @implements <Sprite>MaskAccessor */
static enum CollisionMask sprite_get_self_mask(const struct Sprite *const this){
	assert(this);
	return this->vt->get_self_mask(this);
}
/** @implements <Sprite>MaskAccessor */
static enum CollisionMask sprite_get_others_mask(const struct Sprite *const this) {
	assert(this);
	return this->vt->get_others_mask(this);
}
static enum CollisionMask ship_get_self_mask(const struct Ship *const this) {
	assert(this); return MASK_SHIP;
}
static enum CollisionMask ship_get_others_mask(const struct Ship *const this) {
	assert(this); return MASK_SHIP | MASK_POINT_DEFENSE | MASK_DEBIS;
}
static enum CollisionMask debris_get_self_mask(const struct Ship *const this) {
	assert(this); return MASK_DEBIS;
}
static enum CollisionMask debris_get_others_mask(const struct Ship *const this){
	assert(this); return MASK_SHIP | MASK_POINT_DEFENSE | MASK_DEBIS;
}
static enum CollisionMask wmd_get_self_mask(const struct Ship *const this) {
	assert(this); return MASK_POINT_DEFENSE; /* fixme! allow lazers. */
}
static enum CollisionMask wmd_get_others_mask(const struct Ship *const this){
	assert(this); return MASK_SHIP | MASK_DEBIS;
}
static enum CollisionMask gate_get_self_mask(const struct Ship *const this) {
	assert(this); return 0;
}
static enum CollisionMask gate_get_others_mask(const struct Ship *const this) {
	assert(this); return 0;
}

static const struct SpriteVt ship_human_vt = {
	(SpriteToString)&ship_to_string,
	(SpriteAction)&ship_delete,
	(SpriteAction)&ship_update_human,
	(SpriteFloatAccessor)&ship_get_mass,
	(SpriteMaskAccessor)&ship_get_self_mask,
	(SpriteMaskAccessor)&ship_get_others_mask
}, ship_ai_vt = {
	(SpriteToString)&ship_to_string,
	(SpriteAction)&ship_delete,
	(SpriteAction)&ship_update_dumb_ai,
	(SpriteFloatAccessor)&ship_get_mass,
	(SpriteMaskAccessor)&ship_get_self_mask,
	(SpriteMaskAccessor)&ship_get_others_mask
}, debris_vt = {
	(SpriteToString)&debris_to_string,
	(SpriteAction)&debris_delete,
	(SpriteAction)&debris_update,
	(SpriteFloatAccessor)&debris_get_mass,
	(SpriteMaskAccessor)&debris_get_self_mask,
	(SpriteMaskAccessor)&debris_get_others_mask
}, wmd_vt = {
	(SpriteToString)&wmd_to_string,
	(SpriteAction)&wmd_delete,
	(SpriteAction)&wmd_update,
	(SpriteFloatAccessor)&wmd_get_mass,
	(SpriteMaskAccessor)&wmd_get_self_mask,
	(SpriteMaskAccessor)&wmd_get_others_mask
}, gate_vt = {
	(SpriteToString)&gate_to_string,
	(SpriteAction)&gate_delete,
	(SpriteAction)&gate_update,
	(SpriteFloatAccessor)&gate_get_mass,
	(SpriteMaskAccessor)&gate_get_self_mask,
	(SpriteMaskAccessor)&gate_get_others_mask
};



/****************** Type functions. **************/

/** Called from {bin_migrate}.
 @implements <Cover>Migrate */
static void cover_migrate(struct Cover *const this,
	const struct Migrate *const migrate) {
	SpriteListMigratePointer(&this->sprite, migrate);
}
/** @param sprites_void: We don't need this because there's only one static.
 Should always equal {sprites}.
 @implements Migrate */
static void bin_migrate(void *const sprites_void,
	const struct Migrate *const migrate) {
	struct Sprites *const sprites_pass = sprites_void;
	unsigned i;
	assert(sprites_pass && sprites_pass == sprites && migrate);
	for(i = 0; i < LAYER_SIZE; i++) {
		SpriteListMigrate(&sprites_pass->bins[i].sprites, migrate);
		CoverStackMigrateEach(sprites_pass->bins[i].covers, &cover_migrate,
			migrate);
	}
}
/** Called from \see{collision_migrate}.
 @implements <Sprite>ListMigrateElement */
static void collision_migrate_sprite(struct Sprite *const this,
	const struct Migrate *const migrate) {
	assert(this && migrate);
	if(this->collision) CollisionPoolMigratePointer(&this->collision, migrate);
}
/** @param sprites_void: We don't need this because there's only one static.
 Should always equal {sprites}.
 @implements Migrate */
static void collision_migrate(void *const sprites_void,
	const struct Migrate *const migrate) {
	struct Sprites *const sprites_pass = sprites_void;
	unsigned i;
	assert(sprites_pass && sprites_pass == sprites && migrate);
	for(i = 0; i < LAYER_SIZE; i++) {
		SpriteListMigrateEach(&sprites_pass->bins[i].sprites,
			&collision_migrate_sprite, migrate);
	}
}
/*static void collision_migrate_collision() {
}*/

/** Destructor. */
void Sprites_(void) {
	unsigned i;
	if(!sprites) return;
	for(i = 0; i < LAYER_SIZE; i++) {
		SpriteListClear(&sprites->bins[i].sprites);
		CoverStack_(&sprites->bins[i].covers);
	}
	CollisionPool_(&sprites->collisions);
	Layer_(&sprites->layer);
	GatePool_(&sprites->gates);
	WmdPool_(&sprites->wmds);
	DebrisPool_(&sprites->debris);
	ShipPool_(&sprites->ships);
	free(sprites), sprites = 0;
}

/** @return True if the sprite buffers have been set up. */
int Sprites(void) {
	unsigned i;
	enum { NO, BINS, SHIP, DEBRIS, WMD, GATE, LAYER, COLLISION } e = NO;
	const char *ea = 0, *eb = 0;
	if(sprites) return 1;
	if(!(sprites = malloc(sizeof *sprites)))
		{ perror("Sprites"); Sprites_(); return 0; }
	for(i = 0; i < LAYER_SIZE; i++) {
		SpriteListClear(&sprites->bins[i].sprites);
		sprites->bins[i].covers = 0;
	}
	sprites->ships = 0;
	sprites->debris = 0;
	sprites->wmds = 0;
	sprites->gates = 0;
	sprites->layer = 0;
	sprites->dt_ms = 20;
	sprites->collisions = 0;
	do {
		for(i = 0; i < LAYER_SIZE; i++) {
			if(!(sprites->bins[i].covers = CoverStack())) { e = BINS; break; }
		} if(e) break;
		if(!(sprites->ships = ShipPool(&bin_migrate, sprites))) { e=SHIP;break;}
		if(!(sprites->debris=DebrisPool(&bin_migrate,sprites))){e=DEBRIS;break;}
		if(!(sprites->wmds = WmdPool(&bin_migrate, sprites))) { e = WMD; break;}
		if(!(sprites->gates = GatePool(&bin_migrate, sprites))) { e=GATE;break;}
		if(!(sprites->layer=Layer(LAYER_SIDE_SIZE,layer_space))){e=LAYER;break;}
		if(!(sprites->collisions = CollisionPool(&collision_migrate, sprites)))
			{ e = COLLISION; break; }
	} while(0); switch(e) {
		case NO: break;
		case BINS: ea = "bins", eb = CoverStackGetError(0); break; /* hack */
		case SHIP: ea = "ships", eb = ShipPoolGetError(sprites->ships); break;
		case DEBRIS: ea = "debris",eb=DebrisPoolGetError(sprites->debris);break;
		case WMD: ea = "wmds", eb = WmdPoolGetError(sprites->wmds); break;
		case GATE: ea = "gates", eb = GatePoolGetError(sprites->gates); break;
		case LAYER: ea = "layer", eb = "couldn't get layer"; break;
		case COLLISION: ea = "collision",
			eb = CollisionPoolGetError(sprites->collisions); break;
	} if(e) {
		fprintf(stderr, "Sprites %s buffer: %s.\n", ea, eb);
		Sprites_();
		return 0;
	}
	return 1;
}



/*************** Sub-type constructors. ******************/

/** Abstract sprite constructor.
 @param auto: An auto-sprite prototype.
 @param vt: Virtual table.
 @param x: Where should the sprite go. Leave null to place it in a uniform
 distribution across space.
 @fixme I don't like random velocity. */
static void sprite_filler(struct Sprite *const this,
	const struct SpriteVt *const vt, const struct AutoSprite *const as,
	const struct Ortho3f *x) {
	assert(sprites && this && vt && as);
	this->vt      = vt;
	this->image   = as->image;
	this->normals = as->normals;
	this->bounding = (as->image->width >= as->image->height ?
		as->image->width : as->image->height) / 2.0f; /* fixme: Crude. */
	if(x) {
		Ortho3f_assign(&this->x, x);
	} else {
		Ortho3f_filler_fg(&this->x);
	}
	Ortho3f_filler_zero(&this->v);
	this->bin  = LayerGetOrtho(sprites->layer, &this->x);
	this->dx.x = this->dx.y = 0.0f;
	this->box.x_min = this->box.x_max = this->box.y_min = this->box.y_max =0.0f;
	this->collision = 0;
	/* Put this in space. */
	SpriteListPush(&sprites->bins[this->bin].sprites, this);
}

/** Creates a new {Ship}. */
struct Ship *SpritesShip(const struct AutoShipClass *const class,
	const struct Ortho3f *const x, const enum AiType ai) {
	struct Ship *this;
	const struct SpriteVt *vt;
	if(!sprites || !class) return 0;
	assert(class->sprite && class->sprite->image && class->sprite->normals);
	switch(ai) {
		case AI_DUMB:  vt = &ship_ai_vt;    break;
		case AI_HUMAN: vt = &ship_human_vt; break;
		default: assert(printf("Out of range.", 0));
	}
	if(!(this = ShipPoolNew(sprites->ships)))
		{ fprintf(stderr, "SpritesShip: %s.\n",
		ShipPoolGetError(sprites->ships)); return 0; }
	this->mass = class->mass;
	this->shield = class->shield;
	this->ms_recharge = class->ms_recharge;
	this->max_speed2 = class->speed * class->speed;
	this->acceleration = class->acceleration;
	this->turn = class->turn /* \deg/s */
		* 0.001f /* s/ms */ * M_2PI_F / 360.0f /* \rad/\deg */;
	Orcish(this->name, sizeof this->name);
	this->wmd = class->weapon;
	this->ms_recharge_wmd = 0;
	sprite_filler(&this->sprite.data, vt, class->sprite, x);
	return this;
}

/** Creates a new {Debris}. */
struct Debris *SpritesDebris(const struct AutoDebris *const class,
	const struct Ortho3f *const x) {
	struct Debris *this;
	if(!sprites || !class) return 0;
	assert(class->sprite && class->sprite->image && class->sprite->normals);
	if(!(this = DebrisPoolNew(sprites->debris)))
		{ fprintf(stderr, "SpriteDebris: %s.\n",
		DebrisPoolGetError(sprites->debris)); return 0; }
	this->mass = class->mass;
	sprite_filler(&this->sprite.data, &debris_vt, class->sprite, x);
	return this;
}

/** Creates a new {Wmd}. */
struct Wmd *SpritesWmd(const struct AutoWmdType *const class,
	const struct Ship *const from) {
	struct Wmd *this;
	struct Ortho3f x;
	struct Vec2f dir;
	if(!sprites || !class || !from) return 0;
	assert(class->sprite && class->sprite->image && class->sprite->normals);
	dir.x = cosf(from->sprite.data.x.theta);
	dir.y = sinf(from->sprite.data.x.theta);
	x.x = from->sprite.data.x.x
		+ dir.x * from->sprite.data.bounding * wmd_distance_mod;
	x.y = from->sprite.data.x.y
		+ dir.y * from->sprite.data.bounding * wmd_distance_mod;
	x.theta = from->sprite.data.x.theta;
	if(!(this = WmdPoolNew(sprites->wmds)))
		{ fprintf(stderr, "SpritesWmd: %s.\n",
		WmdPoolGetError(sprites->wmds)); return 0; }
	/* Speed is in [px/s], want it [px/ms]. */
	this->sprite.data.v.x = from->sprite.data.v.x + dir.x * class->speed*0.001f;
	this->sprite.data.v.y = from->sprite.data.v.y + dir.y * class->speed*0.001f;
	this->from = &from->sprite.data;
	this->mass = class->impact_mass;
	this->expires = TimerGetGameTime() + class->ms_range;
	sprite_filler(&this->sprite.data, &wmd_vt, class->sprite, &x);
	{
		const float length = sqrtf(class->r * class->r + class->g * class->g
			+ class->b * class->b);
		const float one_length = length > epsilon ? 1.0f / length : 1.0f;
		Light(&this->light, length, class->r * one_length,
			class->g * one_length, class->b * one_length);
	}
	return this;
}

/** Creates a new {Gate}. */
struct Gate *SpritesGate(const struct AutoGate *const class) {
	const struct AutoSprite *const gate_sprite = AutoSpriteSearch("Gate");
	struct Gate *this;
	struct Ortho3f x;
	if(!sprites || !class) return 0;
	assert(gate_sprite && gate_sprite->image && gate_sprite->normals);
	x.x     = class->x;
	x.y     = class->y;
	x.theta = class->theta;
	if(!(this = GatePoolNew(sprites->gates)))
		{ fprintf(stderr, "SpritesGate: %s.\n",
		GatePoolGetError(sprites->gates)); return 0; }
	this->to = class->to;
	sprite_filler(&this->sprite.data, &gate_vt, gate_sprite, &x);
	return this;
}





/*************** Functions. *****************/

/** Called in \see{extrapolate}.
 @implements LayerNoSpriteAction */
static void put_cover(const unsigned bin, unsigned no, struct Sprite *this) {
	struct Cover *cover;
	/*char a[12];*/
	assert(this && bin < LAYER_SIZE);
	if(!(cover = CoverStackNew(sprites->bins[bin].covers)))
		{ fprintf(stderr, "put_cover: %s.\n",
		CoverStackGetError(sprites->bins[bin].covers)); return;}
	cover->sprite = this;
	cover->is_corner = !no;
	/*sprite_to_string(this, &a);
	printf("put_cover: %s -> %u.\n", a, bin);*/
}
/** Moves the sprite. Calculates temporary values, {dx}, and {box}; sticks it
 into the appropriate {covers}. Called in \see{extrapolate_bin}.
 @implements <Sprite>Action */
static void extrapolate(struct Sprite *const this) {
	assert(sprites && this);
	sprite_update(this);
	/* Kinematics. */
	this->dx.x = this->v.x * sprites->dt_ms;
	this->dx.y = this->v.y * sprites->dt_ms;
	/* Dynamic bounding rectangle. */
	this->box.x_min = this->x.x - this->bounding;
	this->box.x_max = this->x.x + this->bounding;
	if(this->dx.x < 0) this->box.x_min += this->dx.x;
	else this->box.x_max += this->dx.x;
	this->box.y_min = this->x.y - this->bounding;
	this->box.y_max = this->x.y + this->bounding;
	if(this->dx.y < 0) this->box.y_min += this->dx.y;
	else this->box.y_max += this->dx.y;
	/* Put it into appropriate {covers}. This is like a hashmap in space, but
	 it is spread out, so it may cover multiple bins. */
	LayerSetSpriteRectangle(sprites->layer, &this->box);
	LayerSpriteForEachSprite(sprites->layer, this, &put_cover);
}
/** Called in \see{SpritesUpdate}.
 @implements <Bin>Action */
static void extrapolate_bin(const unsigned idx) {
	assert(sprites);
	SpriteListForEach(&sprites->bins[idx].sprites, &extrapolate);
}

/* Branch cut to the principal branch (-Pi,Pi] for numerical stability. We
 should really use normalised {ints}, so this would not be a problem, but
 {OpenGl} doesn't like that. Called from \see{timestep}. */
static void branch_cut_pi_pi(float *theta_ptr) {
	assert(theta_ptr);
	*theta_ptr -= M_2PI_F * floorf((*theta_ptr + M_PI_F) / M_2PI_F);
}
/** Relies on \see{extrapolate}; all pre-computation is finalised in this step
 and values are advanced. Collisions are used up and need to be cleared after.
 Called from \see{timestep_bin}.
 @implements <Sprite>Action */
static void timestep(struct Sprite *const this) {
	const float t_full = sprites->dt_ms;
	float t0 = sprites->dt_ms;
	struct Vec2f v1 = { 0.0f, 0.0f };
	unsigned bin;
#if 0
	struct Collision *c;
	struct Vec2f v1 = { 0.0f, 0.0f }, d;
	assert(sprites);
	/* The earliest time to collision and sum the collisions together. */
	for(c = sprites->collision_set; c; c = c->next) {
		/*char a[12];*/
		d.x = sprites->x.x + sprites->v.x * c->t;
		d.y = sprites->x.y + sprites->v.y * c->t;
		/*fprintf(gnu_glob, "set arrow from %f,%f to %f,%f lw 0.5 "
		 "lc rgb \"#EE66AA\" front;\n", d.x, d.y,
		 d.x + c->v.x * (1.0f - c->t), d.y + c->v.y * (1.0f - c->t));*/
		/*this->vt->to_string(this, &a);
		 printf("%s collides at %.1f and ends up going (%.1f, %.1f).\n", a,
		 c->t, c->v.x * 1000.0f, c->v.y * 1000.0f);*/
		if(c->t < t0) t0 = c->t;
		v1.x += c->v.x, v1.y += c->v.y;
		/* fixme: stability! do a linear search O(n) to pick out the 2 most
		 recent, then divide by, { 1, 2, 4, 4, 4, . . . }? */
	}
#endif
	this->x.x = this->x.x + this->v.x * t0 + v1.x * (t_full - t0);
	this->x.y = this->x.y + this->v.y * t0 + v1.y * (t_full - t0);
	/*if(sprites->collision_set) {
		sprites->collision_set = 0;
		sprites->v.x = v1.x, sprites->v.y = v1.y;
	}*/
	/* Angular velocity. */
	this->x.theta += this->v.theta /* omega */ * t_full;
	branch_cut_pi_pi(&this->x.theta);
	bin = LayerGetOrtho(sprites->layer, &this->x);
	/* This happens when the sprite wanders out of the bin. */
	if(bin != this->bin) {
		char a[12];
		this->vt->to_string(this, &a);
		printf("Sprite %s has transferred bins %u -> %u.\n", a, this->bin, bin);
		SpriteListRemove(&sprites->bins[this->bin].sprites, this);
		this->bin = bin;
		SpriteListPush(&sprites->bins[bin].sprites, this);
	}
}
/** Called in \see{SpritesUpdate}.
 @implements <Bin>Action */
static void timestep_bin(const unsigned idx) {
	assert(sprites);
	SpriteListForEach(&sprites->bins[idx].sprites, &timestep);
}

/* This is where \see{collide_bin} is located; but lots of helper functions. */
#include "SpritesCollide.h"

/** Update each frame.
 @param target: What the camera focuses on; could be null. */
void SpritesUpdate(const int dt_ms, struct Sprite *const target) {
	if(!sprites) return;
	/* Update with the passed parameter. */
	sprites->dt_ms = dt_ms;
	/* Centre on the sprite that was passed, if it is available. */
	if(target) DrawSetCamera((struct Vec2f *)&target->x);
	/* Foreground drawable sprites are a function of screen position. */
	{ 	struct Rectangle4f rect;
		DrawGetScreen(&rect);
		/* fixme: Add 128px for sprites off-screen but drawn. */
		LayerSetScreenRectangle(sprites->layer, &rect); }
	/* Dynamics; puts temp values in {cover} for collisions. */
	LayerForEachScreen(sprites->layer, &extrapolate_bin);
	/* Collision has to be called after {extrapolate}; (fixme: really?) */
	LayerForEachScreen(sprites->layer, &collide_bin);
	/* Time-step. */
	LayerForEachScreen(sprites->layer, &timestep_bin);
	/* Clear temp {cover}. (fixme: no) */
	{ int i; for(i = 0; i < LAYER_SIZE; i++)
		CoverStackClear(sprites->bins[i].covers); }
	CollisionPoolClear(sprites->collisions);
}

/* fixme: This is bullshit. Have it all in Draw? */

/** Called from \see{draw_bin}.
 @implements <Sprite, ContainsLambertOutput>BiAction */
static void draw_sprite(struct Sprite *const this) {
	assert(sprites);
	DrawDisplayLambert(&this->x, this->image, this->normals);
}
/** Called from \see{SpritesDrawForeground}.
 @implements LayerAction */
static void draw_bin(const unsigned idx) {
	assert(sprites && idx < LAYER_SIZE);
	SpriteListForEach(&sprites->bins[idx].sprites, &draw_sprite);
}
/** Must call \see{SpriteUpdate} before this, because it sets
 {sprites.foreground}. Use when the Lambert GPU shader is loaded. */
void SpritesDrawForeground(void) {
	if(!sprites) return;
	LayerForEachScreen(sprites->layer, &draw_bin);
}













#if 0

/** @implements <Bin*, InfoOutput*>DiAction */
static void draw_bin_literally(struct SpriteList **const pthis, void *const pout_void) {
	struct SpriteList *const sprites = *pthis;
	const InfoOutput *const pout = pout_void, out = *pout;
	const struct AutoImage *hair = AutoImageSearch("Hair.png");
	struct Vec2i bin_in_space;
	struct Vec2f bin_float;
	assert(pthis && sprites && out && hair);
	bin_to_Vec2i((unsigned)(sprites - bins), &bin_in_space);
	bin_float.x = bin_in_space.x, bin_float.y = bin_in_space.y;
	out(&bin_float, hair);
}
/** Must call \see{SpriteUpdate} before this, because it updates everything.
 Use when the Info GPU shader is loaded. */
void SpritesDrawInfo(InfoOutput draw) {
	/*const struct Vec2f a = { 10.0f, 10.0f };*/
	/*const struct AutoImage *hair = AutoImageSearch("Hair.png");*/
	assert(draw);
	BinPoolBiForEach(draw_bins, &draw_bin_literally, &draw);
	/*draw(&a, hair);*/
}

/** Use when the Far GPU shader is loaded. */
void SpritesDrawFar(LambertOutput draw) {
	BinPoolBiForEach(bg_bins, &draw_bin, &draw);
}

/* This is a debug thing. */

/* This is for communication with {sprite_data}, {sprite_arrow}, and
 {sprite_velocity} for outputting a gnu plot. */
struct OutputData {
	FILE *fp;
	unsigned i, n;
};
/** @implements <Sprite, OutputData>DiAction */
static void sprite_count(struct Sprite *sprites, void *const void_out) {
	struct OutputData *const out = void_out;
	UNUSED(sprites);
	out->n++;
}
/** @implements <Sprite, OutputData>DiAction */
static void print_sprite_data(struct Sprite *sprites, void *const void_out) {
	struct OutputData *const out = void_out;
	fprintf(out->fp, "%f\t%f\t%f\t%f\t%f\t%f\t%f\n", sprites->x.x, sprites->x.y,
			sprites->bounding, (double)out->i++ / out->n, sprites->x_5.x, sprites->x_5.y,
			sprites->bounding1);
}
/** @implements <Sprite, OutputData>DiAction */
static void print_sprite_velocity(struct Sprite *sprites, void *const void_out) {
	struct OutputData *const out = void_out;
	fprintf(out->fp, "set arrow from %f,%f to %f,%f lw 1 lc rgb \"blue\" "
		"front;\n", sprites->x.x, sprites->x.y,
		sprites->x.x + sprites->v.x * dt_ms * 1000.0f,
		sprites->x.y + sprites->v.y * dt_ms * 1000.0f);
}
/** For communication with {gnu_draw_bins}. */
struct ColourData {
	FILE *fp;
	const char *colour;
	unsigned object;
};
/** Draws squares for highlighting bins.
 @implements <Bin, ColourData>DiAction */
static void gnu_shade_bins(struct SpriteList **sprites, void *const void_col) {
	struct ColourData *const col = void_col;
	const unsigned bin = (unsigned)(*sprites - bins);
	struct Vec2i bin2i;
	assert(col);
	assert(bin < BIN_BIN_FG_SIZE);
	bin_to_Vec2i(bin, &bin2i);
	fprintf(col->fp, "# bin %u -> %d,%d\n", bin, bin2i.x, bin2i.y);
	fprintf(col->fp, "set object %u rect from %d,%d to %d,%d fc rgb \"%s\" "
		"fs solid noborder;\n", col->object++, bin2i.x, bin2i.y,
		bin2i.x + 256, bin2i.y + 256, col->colour);
}
/** Debugging plot.
 @implements Action */
void SpritesPlot(void) {
	FILE *data = 0, *gnu = 0;
	const char *data_fn = "sprite.data", *gnu_fn = "sprite.gnu",
		*eps_fn = "sprite.eps";
	enum { E_NO, E_DATA, E_GNU } e = E_NO;
	fprintf(stderr, "Will output %s at the current time, and a gnuplot script, "
		"%s of the current sprites.\n", data_fn, gnu_fn);
	do {
		struct OutputData out;
		struct ColourData col;
		unsigned i;
		if(!(data = fopen(data_fn, "w"))) { e = E_DATA; break; }
		if(!(gnu = fopen(gnu_fn, "w")))   { e = E_GNU;  break; }
		out.fp = data, out.i = 0; out.n = 0;
		for(i = 0; i < BIN_BIN_FG_SIZE; i++)
			SpriteListUnorderBiForEach(bins + i, &sprite_count, &out);
		for(i = 0; i < BIN_BIN_FG_SIZE; i++)
			SpriteListUnorderBiForEach(bins + i, &print_sprite_data, &out);
		fprintf(gnu, "set term postscript eps enhanced size 20cm, 20cm\n"
			"set output \"%s\"\n"
			"set size square;\n"
			"set palette defined (1 \"#0000FF\", 2 \"#00FF00\", 3 \"#FF0000\");"
			"\n"
			"set xtics 256 rotate; set ytics 256;\n"
			"set grid;\n"
			"set xrange [-8192:8192];\n"
			"set yrange [-8192:8192];\n"
			"set cbrange [0.0:1.0];\n", eps_fn);
		/* draw bins as squares behind */
		fprintf(gnu, "set style fill transparent solid 0.3 noborder;\n");
		col.fp = gnu, col.colour = "#ADD8E6", col.object = 1;
		BinPoolBiForEach(draw_bins, &gnu_shade_bins, &col);
		col.fp = gnu, col.colour = "#D3D3D3";
		BinPoolBiForEach(update_bins, &gnu_shade_bins, &col);
		/* draw arrows from each of the sprites to their bins */
		out.fp = gnu, out.i = 0;
		for(i = 0; i < BIN_BIN_FG_SIZE; i++)
			SpriteListUnorderBiForEach(bins + i, &print_sprite_velocity, &out);
		/* draw the sprites */
		fprintf(gnu, "plot \"%s\" using 5:6:7 with circles \\\n"
			"linecolor rgb(\"#00FF00\") fillstyle transparent "
			"solid 1.0 noborder title \"Velocity\", \\\n"
			"\"%s\" using 1:2:3:4 with circles \\\n"
			"linecolor palette fillstyle transparent solid 0.3 noborder \\\n"
			"title \"Sprites\";\n", data_fn, data_fn);
	} while(0); switch(e) {
		case E_NO: break;
		case E_DATA: perror(data_fn); break;
		case E_GNU:  perror(gnu_fn);  break;
	} {
		if(fclose(data) == EOF) perror(data_fn);
		if(fclose(gnu) == EOF)  perror(gnu_fn);
	}
}

/** Output sprites. */
void SpritesOut(void) {
	struct OutputData out;
	unsigned i;
	out.fp = stdout, out.i = 0; out.n = 0;
	for(i = 0; i < BIN_BIN_FG_SIZE; i++)
		SpriteListUnorderBiForEach(bins + i, &sprite_count, &out);
	for(i = 0; i < BIN_BIN_FG_SIZE; i++)
		SpriteListUnorderBiForEach(bins + i, &print_sprite_data, &out);
}

#endif


















/* THIS IS OLD CODE */

#if 0

/** Define {Ship} as a subclass of {Sprite}. */
struct Ship {
	struct SpriteListNode sprite;
	const struct AutoShipClass *class;
	unsigned     ms_recharge_wmd; /* ms */
	unsigned     ms_recharge_hit; /* ms */
	struct Event *event_recharge;
	int          hit, max_hit; /* GJ */
	float        max_speed2;
	float        acceleration;
	float        turn; /* deg/s -> rad/ms */
	float        turn_acceleration;
	float        horizon;
	enum SpriteBehaviour behaviour;
	char         name[16];
};
/* Define ShipPool. */
#define POOL_NAME Ship
#define POOL_TYPE struct Ship
#include "../templates/Pool.h"

/** Define {Wmd} as a subclass of {Sprite}. */
struct Wmd {
	struct SpriteListNode sprite;
	const struct AutoWmdType *class;
	const struct Sprite *from;
	float mass;
	unsigned expires;
	unsigned light;
};
/* Define ShipPool. */
#define POOL_NAME Wmd
#define POOL_TYPE struct Wmd
#include "../templates/Pool.h"

/** @implements <Ship>Action */
static void ship_out(struct Ship *const sprites) {
	assert(sprites);
	printf("Ship at (%f, %f) in %u integrity %u/%u.\n", sprites->sprite.data.r.x,
		sprites->sprite.data.r.y, sprites->sprite.data.bin, sprites->hit, sprites->max_hit);
}
/** @implements <Ship>Action */
static void ship_delete(struct Ship *const sprites) {
	/*char a[12];*/
	assert(sprites);
	/*Ship_to_string(this, &a);
	printf("Ship_delete %s#%p.\n", a, (void *)&this->sprite.data);*/
	Event_(&sprites->event_recharge);
	sprite_remove(&sprites->sprite.data);
	ShipPoolRemove(ships, sprites);
}
/** @implements <Ship>Action */
static void ship_death(struct Ship *const sprites) {
	sprites->hit = 0;
	/* fixme: explode */
}
/** @implements <Ship>Predicate */
static void ship_action(struct Ship *const sprites) {
	if(sprites->hit <= 0) ship_death(sprites);
}

/** @implements <Wmd>Action */
static void wmd_out(struct Wmd *const sprites) {
	assert(sprites);
	printf("Wmd%u.\n", (WmdListNode *)sprites - wmds);
}
/** @implements <Wmd>Action */
static void wmd_delete(struct Wmd *const sprites) {
	/*char a[12];*/
	assert(sprites);
	/*printf("wmd_delete %s#%p.\n", a, (void *)&this->sprite.data);*/
	sprite_remove(&sprites->sprite.data);
	WmdPoolRemove(gates, sprites);
	/* fixme: explode */
}
/** @implements <Wmd>Action */
static void wmd_death(struct Wmd *const sprites) {
	fprintf(stderr, "Tryi to %s.\n", sprites->);
}
/** @implements <Wmd>Action */
static void wmd_action(struct Wmd *const sprites) {
	UNUSED(sprites);
}

/** @implements <Gate>Action */
static void gate_out(struct Gate *const sprites) {
	assert(sprites);
	printf("Gate at (%f, %f) in %u goes to %s.\n", sprites->sprite.data.r.x,
		sprites->sprite.data.r.y, sprites->sprite.data.bin, sprites->to->name);
}
/** @implements <Gate>Action */
static void gate_delete(struct Gate *const sprites) {
	/*char a[12];*/
	assert(sprites);
	/*Gate_to_string(this, &a);
	printf("Gate_delete %s#%p.\n", a, (void *)&this->sprite.data);*/
	sprite_remove(&sprites->sprite.data);
	GatePoolRemove(gates, sprites);
	/* fixme: explode */
}
/** @implements <Gate>Action */
static void gate_death(struct Gate *const sprites) {
	fprintf(stderr, "Trying to destroy gate to %s.\n", sprites->to->name);
}
/** @implements <Gate>Action */
static void gate_action(struct Gate *const sprites) {
	UNUSED(sprites);
}



/** @implements <Ship>Action */
static void ship_recharge(struct Ship *const sprites) {
	if(!sprites) return;
	debug("ship_recharge: %s %uGJ/%uGJ\n", sprites->name, sprites->hit,sprites->max_hit);
	if(sprites->hit >= sprites->max_hit) return;
	sprites->hit++;
	/* this checks if an Event is associated to the sprite, we momentarily don't
	 have an event, even though we are getting one soon! alternately, we could
	 call Sprite::recharge and not stick the event down there.
	 SpriteRecharge(a, 1);*/
	if(sprites->hit >= sprites->max_hit) {
		debug("ship_recharge: %s shields full %uGJ/%uGJ.\n",
			sprites->name, sprites->hit, sprites->max_hit);
		return;
	}
	Event(&sprites->event_recharge, sprites->ms_recharge_hit, shp_ms_sheild_uncertainty, FN_CONSUMER, &ship_recharge, sprites);
}
void ShipRechage(struct Ship *const sprites, const int recharge) {
	if(recharge + sprites->hit >= sprites->max_hit) {
		sprites->hit = sprites->max_hit; /* cap off */
	} else if(-recharge < sprites->hit) {
		sprites->hit += (unsigned)recharge; /* recharge */
		if(!sprites->event_recharge
		   && sprites->hit < sprites->max_hit) {
			pedantic("SpriteRecharge: %s beginning charging cycle %d/%d.\n", sprites->name, sprites->hit, sprites->max_hit);
			Event(&sprites->event_recharge, sprites->ms_recharge_hit, 0, FN_CONSUMER, &ship_recharge, sprites);
		}
	} else {
		sprites->hit = 0; /* killed */
	}
}

/** Gets a SpaceZone that it goes to, if it exists. */
const struct AutoSpaceZone *GateGetTo(const struct Gate *const sprites) {
	if(!sprites) return 0;
	return sprites->to;
}

/** See \see{GateFind}.
 @implements <Gate>Predicate */
static int find_gate(struct Gate *const sprites, void *const vgate) {
	struct Gate *const gate = vgate;
	return sprites != gate;
}
/** Linear search for {gates} that go to a specific place. */
struct Gate *GateFind(struct AutoSpaceZone *const to) {
	pedantic("GateIncoming: SpaceZone to: #%p %s.\n", to, to->name);
	GatePoolPoolParam(gates, to);
	return GatePoolShortCircuit(gates, &find_gate);
}


/** can be a callback for an Ethereal, whenever it collides with a Ship.
 IT CAN'T MODIFY THE LIST */
static void gate_travel(struct Gate *const gate, struct Ship *this) {
	struct Vec2f diff, gate_norm;
	float proj;
	if(!gate || !this) return;
	diff.x = this->sprite.r.x - gate->sprite.data.r.x;
	diff.y = this->sprite.r.y - gate->sprite.r.y;
	/* unneccesary?
	 vx = ship_vx - gate_vx;
	 vy = ship_vy - gate_vy;*/
	gate_norm_x = cosf(gate->theta);
	gate_norm_y = sinf(gate->theta);
	proj = x * gate_norm_x + y * gate_norm_y; /* proj is the new h */
	if(this->sp.this.horizon > 0 && proj < 0) {
		debug("gate_travel: %s crossed into the event horizon of %s.\n",
			SpriteToString(this), SpriteToString(gate));
		if(this == GameGetPlayer()) {
			/* trasport to zone immediately */
			Event(0, 0, 0, FN_CONSUMER, &ZoneChange, gate);
		} else {
			/* disappear */
			/* fixme: test! */
			SpriteDestroy(this);
		}
	}/* else*/
	/* fixme: unreliable */
	this->sp.this.horizon = proj;
}

#endif
