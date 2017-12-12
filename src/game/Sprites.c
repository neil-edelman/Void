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
#include "../general/Events.h" /* Event for delays */
#include "../general/Layer.h" /* for descritising */
#include "../system/Poll.h" /* input */
#include "../system/Draw.h" /* DrawSetCamera, DrawGetScreen */
#include "../system/Timer.h" /* for expiring */
#include "Zone.h" /* ZoneChange */
#include "Game.h" /* GameGetPlayer */
/*#include "Light.h"*/ /* for glowing */
#include "Sprites.h"

#define LAYER_SIDE_SIZE (64)
#define LAYER_SIZE (LAYER_SIDE_SIZE * LAYER_SIDE_SIZE)
static const float layer_space = 256.0f;
/* must be the same as in Lighting.fs */
#define MAX_LIGHTS (64)

/* This is used for small floating-point values. The value doesn't have any
 significance. */
static const float epsilon = 0.0005f;
/* Mass is a measure of a rigid body's resistance to a change of motion; it is
 a divisor, thus cannot be (near) zero. */
static const float minimum_mass = 0.01f; /* mg (t) */
/* Controls how far past the bounding radius the ship's shots appear. 1.415?
 Kind of hacky. */
static const float wmd_distance_mod = 1.3f;
/* Controls the acceleration of a turn; low values are sluggish; (0, 1]. */
static const float turn_acceleration = 0.005f;
/* {0.995^t} Taylor series at {t = 25ms}. */
static const float turn_damping_per_25ms = 0.88222f;
static const float turn_damping_1st_order = 0.00442217f;



/* Sprites used in debugging; initialised in {Sprites}. */
static const struct AutoImage *icon_expand;

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
	struct Light *light; /* pointer to a limited number of lights */
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
	float dist_to_horizon;
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
	int is_deleted;
};
#define STACK_NAME Cover
#define STACK_TYPE struct Cover
#include "../templates/Stack.h"



/** Debug. */
struct Info {
	struct Vec2f x;
	const struct AutoImage *image;
};
#define STACK_NAME Info
#define STACK_TYPE struct Info
#include "../templates/Stack.h"



/** Collisions between sprites to apply later. This is a pool that sprites can
 use. Defines {CollisionPool}, {CollisionPoolNode}. {Pool} is used instead of
 {Stack} because we must have migrate. */
struct Collision {
	unsigned no;
	struct Vec2f v;
	float t;
};
#define STACK_NAME Collision
#define STACK_TYPE struct Collision
#define STACK_MIGRATE
#include "../templates/Stack.h"



/** Sprites all together. */
static struct Sprites {
	struct Bin {
		struct SpriteList sprites;
		struct CoverStack *covers;
	} bins[LAYER_SIZE];
	/* Backing for the {SpriteList} in the {Bin}s. */
	struct ShipPool *ships;
	struct DebrisPool *debris;
	struct WmdPool *wmds;
	struct GatePool *gates;
	/* Contains calculations for the {bins}. */
	struct Layer *layer;
	/* Constantly updating frame time. */
	float dt_ms;
	struct CollisionStack *collisions;
	struct InfoStack *info; /* Debug. */
	struct {
		int is_ship;
		size_t ship_index;
	} player;
	struct Lights {
		size_t size;
		struct Light {
			struct Sprite *sprite;
		} light[MAX_LIGHTS];
		struct Vec2f x_table[MAX_LIGHTS];
		struct Colour3f colour_table[MAX_LIGHTS];
	} lights;
} *sprites;



/* Include Light functions. */
#include "SpritesLight.h"

/*********** Define virtual functions. ***********/

typedef float (*SpriteFloatAccessor)(const struct Sprite *const);

/** Sometimes, the sprite class is important; ie, {typeof(sprite)};
 eg collision resolution. */
enum SpriteClass { SC_SHIP, SC_DEBRIS, SC_WMD, SC_GATE };

/** Define {SpriteVt}. */
struct SpriteVt {
	enum SpriteClass class;
	SpriteToString to_string;
	SpriteAction delete;
	SpritePredicate update;
	SpriteFloatAccessor get_mass;
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
	sprintf(*a, "Wmd%u",
		(unsigned)WmdPoolGetIndex(sprites->wmds, this) % 100000000);
}
/** @implements <Gate>ToString */
static void gate_to_string(const struct Gate *this, char (*const a)[12]) {
	sprintf(*a, "%.7sGate", this->to->name);
}


/** @implements SpritesAction */
static void sprite_delete(struct Sprite *const this) {
	assert(sprites && this);
	Light_(this->light);
	SpriteListRemove(&sprites->bins[this->bin].sprites, this);
	this->bin = (unsigned)-1; /* Makes debugging easier. */
	this->vt->delete(this);
}
/** @implements <Ship>Action */
static void ship_delete(struct Ship *const this) {
	/* The player is deleted; update. */
	if(sprites->player.is_ship
		&& ShipPoolGetIndex(sprites->ships, this) ==sprites->player.ship_index){
		sprites->player.is_ship = 0;
		printf("You died! :0\n");
	}
	ShipPoolRemove(sprites->ships, this);
}
/** @implements <Debris>Action */
static void debris_delete(struct Debris *const this) {
	DebrisPoolRemove(sprites->debris, this);
}
/** @implements <Wmd>Action */
static void wmd_delete(struct Wmd *const this) {
	WmdPoolRemove(sprites->wmds, this);
}
/** @implements <Gate>Action */
static void gate_delete(struct Gate *const this) {
	GatePoolRemove(sprites->gates, this);
}


/** If it returns false, it deleted the sprite.
 @implements <Sprite>Predicate */
static int sprite_update(struct Sprite *const this) {
	assert(this);
	return this->vt->update(this);
}
/* Includes {ship_update*} Human/AI. */
#include "SpritesAi.h"
/** Does nothing; just debris.
 @implements <Debris>Predicate */
static int debris_update(struct Debris *const this) {
	UNUSED(this);
	return 1;
}
/** @implements <Wmd>Predicate
 @fixme Replace delete with more dramatic death. */
static int wmd_update(struct Wmd *const this) {
	if(TimerIsTime(this->expires)) return sprite_delete(&this->sprite.data), 0;
	return 1;
}
/** @implements <Gate>Predicate */
static int gate_update(struct Gate *const this) {
	UNUSED(this);
	return 1;
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
/** @implements <Wmd>FloatAccessor */
static float wmd_get_mass(const struct Wmd *const this) {
	assert(this);
	return this->mass;
}
/** @implements <Gate>FloatAccessor */
static float gate_get_mass(const struct Gate *const this) {
	assert(this);
	return 1e36f; /* No moving. This should not be called, anyway. */
}

static const struct SpriteVt ship_human_vt = {
	SC_SHIP,
	(SpriteToString)&ship_to_string,
	(SpriteAction)&ship_delete,
	(SpritePredicate)&ship_update_human,
	(SpriteFloatAccessor)&ship_get_mass
}, ship_ai_vt = {
	SC_SHIP,
	(SpriteToString)&ship_to_string,
	(SpriteAction)&ship_delete,
	(SpritePredicate)&ship_update_dumb_ai,
	(SpriteFloatAccessor)&ship_get_mass
}, debris_vt = {
	SC_DEBRIS,
	(SpriteToString)&debris_to_string,
	(SpriteAction)&debris_delete,
	(SpritePredicate)&debris_update,
	(SpriteFloatAccessor)&debris_get_mass
}, wmd_vt = {
	SC_WMD,
	(SpriteToString)&wmd_to_string,
	(SpriteAction)&wmd_delete,
	(SpritePredicate)&wmd_update,
	(SpriteFloatAccessor)&wmd_get_mass
}, gate_vt = {
	SC_GATE,
	(SpriteToString)&gate_to_string,
	(SpriteAction)&gate_delete,
	(SpritePredicate)&gate_update,
	(SpriteFloatAccessor)&gate_get_mass
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
	/* There is a dependancy in Lights. */
	for(i = 0; i < sprites_pass->lights.size; i++) {
		SpriteListMigrate(&sprites_pass->lights.light[i].sprite, migrate);
	}
	/* fixme: also in Events. */
}
/** Called from \see{collision_migrate}.
 @implements <Sprite>ListMigrateElement */
static void collision_migrate_sprite(struct Sprite *const this,
	const struct Migrate *const migrate) {
	assert(this && migrate);
	if(this->collision) CollisionMigratePointer(&this->collision, migrate);
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

/** Destructor. */
void Sprites_(void) {
	unsigned i;
	/* We don't have to do the lights; all static. */
	if(!sprites) return;
	for(i = 0; i < LAYER_SIZE; i++) {
		SpriteListClear(&sprites->bins[i].sprites);
		CoverStack_(&sprites->bins[i].covers);
	}
	InfoStack_(&sprites->info);
	CollisionStack_(&sprites->collisions);
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
	enum { NO, BINS, SHIP, DEBRIS, WMD, GATE, LAYER, COLLISION, INFO } e = NO;
	const char *ea = 0, *eb = 0;
	if(sprites) return 1;
	/* Static, if it were possible. */
	if(!icon_expand) icon_expand = AutoImageSearch("Expand16.png");
	assert(icon_expand);
	/* Keep going. */
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
	sprites->info = 0;
	sprites->player.is_ship = 0;
	sprites->player.ship_index = 0;
	sprites->lights.size = 0;
	do {
		for(i = 0; i < LAYER_SIZE; i++) {
			if(!(sprites->bins[i].covers = CoverStack())) { e = BINS; break; }
		} if(e) break;
		if(!(sprites->ships = ShipPool(&bin_migrate, sprites))) { e=SHIP;break;}
		if(!(sprites->debris=DebrisPool(&bin_migrate,sprites))){e=DEBRIS;break;}
		if(!(sprites->wmds = WmdPool(&bin_migrate, sprites))) { e = WMD; break;}
		if(!(sprites->gates = GatePool(&bin_migrate, sprites))) { e=GATE;break;}
		if(!(sprites->layer=Layer(LAYER_SIDE_SIZE,layer_space))){e=LAYER;break;}
		if(!(sprites->collisions = CollisionStack(&collision_migrate, sprites)))
			{ e = COLLISION; break; }
		if(!(sprites->info = InfoStack())) { e = INFO; break; }
	} while(0); switch(e) {
		case NO: break;
		case BINS: ea = "bins", eb = CoverStackGetError(0); break; /* hack */
		case SHIP: ea = "ships", eb = ShipPoolGetError(sprites->ships); break;
		case DEBRIS: ea = "debris",eb=DebrisPoolGetError(sprites->debris);break;
		case WMD: ea = "wmds", eb = WmdPoolGetError(sprites->wmds); break;
		case GATE: ea = "gates", eb = GatePoolGetError(sprites->gates); break;
		case LAYER: ea = "layer", eb = "couldn't get layer"; break;
		case COLLISION: ea = "collisions",
			eb = CollisionStackGetError(sprites->collisions); break;
		case INFO: ea = "info", eb = InfoStackGetError(sprites->info); break;
	} if(e) {
		fprintf(stderr, "Sprites %s buffer: %s.\n", ea, eb);
		Sprites_();
		return 0;
	}
	return 1;
}

/** Clear all space. */
void SpritesRemoveIf(const SpritesPredicate predicate) {
	struct Bin *bin;
	size_t i;
	if(!sprites) return;
	for(i = 0; i < LAYER_SIZE; i++) {
		bin = sprites->bins + i;
		SpriteListTakeIf(0, &bin->sprites, predicate);
		CoverStackClear(bin->covers); /* Should be empty anyway. */
	}
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
		ortho3f_assign(&this->x, x);
	} else {
		LayerSetRandom(sprites->layer, &this->x);
	}
	ortho3f_init(&this->v);
	this->bin       = LayerGetOrtho(sprites->layer, &this->x);
	this->dx.x      = this->dx.y = 0.0f;
	this->box.x_min = this->box.x_max = this->box.y_min = this->box.y_max =0.0f;
	this->collision = 0;
	this->light     = 0;
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
	sprite_filler(&this->sprite.data, vt, class->sprite, x);
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
	this->dist_to_horizon = 0.0f;
	if(ai == AI_HUMAN) {
		if(sprites->player.is_ship)
			fprintf(stderr, "SpritesShip: overriding previous player.\n");
		sprites->player.is_ship = 1;
		sprites->player.ship_index = ShipPoolGetIndex(sprites->ships, this);
	}
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
	sprite_filler(&this->sprite.data, &debris_vt, class->sprite, x);
	this->mass = class->mass;
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
	sprite_filler(&this->sprite.data, &wmd_vt, class->sprite, &x);
	/* Speed is in [px/s], want it [px/ms]. */
	this->sprite.data.v.x = from->sprite.data.v.x + dir.x * class->speed*0.001f;
	this->sprite.data.v.y = from->sprite.data.v.y + dir.y * class->speed*0.001f;
	this->from = &from->sprite.data;
	this->mass = class->impact_mass;
	this->expires = TimerGetGameTime() + class->ms_range;
	Light(&this->sprite.data, class->r, class->g, class->b);
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
	sprite_filler(&this->sprite.data, &gate_vt, gate_sprite, &x);
	this->to = class->to;
	printf("SpritesGate: to %s.\n", this->to->name);
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
	cover->is_deleted = 0;
	/*sprite_to_string(this, &a);
	printf("put_cover: %s -> %u.\n", a, bin);*/
}
/** Moves the sprite. Calculates temporary values, {dx}, and {box}; sticks it
 into the appropriate {covers}. Called in \see{extrapolate_bin}.
 @implements <Sprite>Action */
static void extrapolate(struct Sprite *const this) {
	assert(sprites && this);
	/* If it returns false, just leave it; it's probably invalid now. */
	if(!sprite_update(this)) return;
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
	const float t = sprites->dt_ms;
	unsigned bin;
	assert(sprites && this);
	/* Velocity. */
	if(this->collision) {
		const float t0 = this->collision->t, t1 = t - t0;
		const struct Vec2f *const v1 = &this->collision->v;
		this->x.x = this->x.x + this->v.x * t0 + v1->x * t1;
		this->x.y = this->x.y + this->v.y * t0 + v1->y * t1;
		this->v.x = v1->x, this->v.y = v1->y;
		/* Erase the reference; will be erased all at once in {timestep_bin}. */
		this->collision = 0;
	} else {
		this->x.x += this->v.x * t;
		this->x.y += this->v.y * t;
	}
	/* Angular velocity -- this is {\omega}. */
	this->x.theta += this->v.theta * t;
	branch_cut_pi_pi(&this->x.theta);
	bin = LayerGetOrtho(sprites->layer, &this->x);
	/* This happens when the sprite wanders out of the bin. */
	if(bin != this->bin) {
		/*char a[12]; this->vt->to_string(this, &a); printf("Sprite %s has "
			"transferred bins %u -> %u.\n", a, this->bin, bin);*/
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
	/* Collisions are consumed, but not erased; erase them. */
	CollisionStackClear(sprites->collisions);
}

/** Gets the player's ship. */
static struct Ship *get_player(void) {
	assert(sprites);
	if(!sprites->player.is_ship) return 0;
	return ShipPoolGetElement(sprites->ships, sprites->player.ship_index);
}

/* This is where \see{collide_bin} is located; but lots of helper functions. */
#include "SpritesCollide.h"

/** Update each frame.
 @param target: What the camera focuses on; could be null. */
void SpritesUpdate(const int dt_ms) {
	if(!sprites) return;
	/* Update with the passed parameter. */
	sprites->dt_ms = dt_ms;
	/* Clear info on every frame. */
	InfoStackClear(sprites->info);
	/* Centre on the the player. */
	if(sprites->player.is_ship) {
		struct Ship *const player = ShipPoolGetElement(sprites->ships,
			sprites->player.ship_index);
		if(player) {
			DrawSetCamera((struct Vec2f *)&player->sprite.data.x);
		}
	}
	/* Foreground drawable sprites are a function of screen position. */
	{ 	struct Rectangle4f rect;
		DrawGetScreen(&rect);
		rectangle4f_expand(&rect, layer_space * 0.5f);
		LayerSetScreenRectangle(sprites->layer, &rect); }
	/* Dynamics; puts temp values in {cover} for collisions. */
	LayerForEachScreen(sprites->layer, &extrapolate_bin);
	/* Collision has to be called after {extrapolate}; it consumes {cover}.
	 (fixme: really? 3 passes?) */
	LayerForEachScreen(sprites->layer, &collide_bin);
	/* Time-step. */
	LayerForEachScreen(sprites->layer, &timestep_bin);
}

/* fixme: This is bullshit. Have it all in Draw? */

/** Called from \see{draw_bin}.
 @implements <Sprite>Action */
static void draw_sprite(struct Sprite *const this) {
	assert(sprites);
	DrawDisplayLambert(&this->x, this->image, this->normals);
}
/** Called from \see{SpritesDraw}.
 @implements LayerAction */
static void draw_bin(const unsigned idx) {
	assert(sprites && idx < LAYER_SIZE);
	SpriteListForEach(&sprites->bins[idx].sprites, &draw_sprite);
}
/** Must call \see{SpriteUpdate} before this, because it sets
 {sprites.layer}. Use when the Lambert GPU shader is loaded. */
void SpritesDraw(void) {
	if(!sprites) return;
	LayerForEachScreen(sprites->layer, &draw_bin);
}

/* Debug info. */
void Info(const struct Vec2f *const x, const struct AutoImage *const image) {
	struct Info *info;
	if(!x || !image) return;
	/*char a_str[12], b_str[12];
	 sprite_to_string(a, &a_str), sprite_to_string(b, &b_str);
	 printf("Degeneracy pressure between %s and %s.\n", a_str, b_str);*/
	/* Debug show hair. */
	if(!(info = InfoStackNew(sprites->info))) return;
	info->x.x = x->x;
	info->x.y = x->y;
	info->image = image;
}
/* @implements <Vec2f>Action */
static void draw_info(struct Info *const this) {
	DrawDisplayInfo(&this->x, this->image);
}
/** Use when the Info GPU shader is loaded. */
void SpritesInfo(void) {
	if(!sprites) return;
	InfoStackForEach(sprites->info, &draw_info);
}



/** Specific sprites. */

/** Allows access to {this}' position as a read-only value. */
const struct Ortho3f *SpriteGetPosition(const struct Sprite *const this) {
	if(!this) return 0;
	return &this->x;
}
/** Allows access to {this}' velocity as a read-only value. */
const struct Ortho3f *SpriteGetVelocity(const struct Sprite *const this) {
	if(!this) return 0;
	return &this->v;
}
/** Modifies {this}' position. */
void SpriteSetPosition(struct Sprite *const this,const struct Ortho3f *const x){
	if(!this || !x) return;
	ortho3f_assign(&this->x, x);
}
/** Modifies {this}' velocity. */
void SpriteSetVelocity(struct Sprite *const this,const struct Ortho3f *const v){
	if(!this || !v) return;
	ortho3f_assign(&this->v, v);
}

/** Gets an {AutoSpaceZone} that it goes to, if it exists. */
const struct AutoSpaceZone *GateGetTo(const struct Gate *const this) {
	if(!this) return 0;
	return this->to;
}

/** Gets a {Sprite} that goes to the {AutoSpaceZone}, if it exists.
 @order O({|max Gates|}) */
struct Gate *FindGate(const struct AutoSpaceZone *const to) {
	struct GatePool *const gates = sprites->gates;
	struct Gate *g;
	size_t i;
	if(!to || !sprites) return 0;
	if((g = GatePoolElement(gates))) {
		i = GatePoolGetIndex(gates, g);
		for( ; ; ) {
			if(g->to == to) return g;
			do if(!i) return 0; while(GatePoolIsElement(gates, --i));
			g = GatePoolGetElement(gates, i);
		}
	}
	return 0;
}

/** Gets the player's ship or null. */
struct Ship *SpritesGetPlayerShip(void) {
	if(!sprites) return 0;
	return get_player();
}



/* This includes some debuging functions, namely, {SpritesPlot}. */
#include "SpritesPlot.h"
