/** 2017 Neil Edelman, distributed under the terms of the GNU General
 Public License 3, see copying.txt, or
 \url{ https://opensource.org/licenses/GPL-3.0 }.

 Space {Item}s are a polymorphic static structure representing all normal
 physical game entites that are in the foreground, have collisions, and
 lighting. This is handled internally but a static variable, {items}, that is
 already initialised to zero; to re-initialise and clear up all the memory, do
 \see{ItemsReset}; then, all references to the old items are invalidated.

 @title		Items
 @author	Neil
 @std		C89/90
 @version	2018-11 Sprites -> Items
 @since		2017-10 Broke into smaller pieces.
			2017-05 Generics.
			2016-01
			2015-06 */

#include <stdio.h> /* fprintf */
#include "../../build/Auto.h" /* for AutoImage, AutoShipClass, etc */
#include "../Ortho.h" /* Vec2f, etc */
#include "../general/Orcish.h" /* for human-readable ship names */
#include "../general/Events.h" /* Event for delays */
#include "../general/Layer.h" /* for descritising */
#include "../system/Poll.h" /* input */
#include "../system/Draw.h" /* DrawSetCamera, DrawGetScreen */
#include "../system/Timer.h" /* for expiring */
#include "Zone.h" /* ZoneChange */
#include "Game.h" /* GameGetPlayer */
/*#include "Light.h"*/ /* for glowing */
#include "Items.h"

#define LAYER_SIDE_SIZE (64)
#define LAYER_SIZE (LAYER_SIDE_SIZE * LAYER_SIDE_SIZE)
static const float layer_space = 256.0f;
/* Must be the same as in {Lighting.fs}! */
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
static const float turn_acceleration = 0.007f;
/* {0.995^t} Taylor series at {t = 25ms}. */
static const float turn_damping_per_25ms = 0.88222f;
static const float turn_damping_1st_order = 0.00442217f;
/* Damage/mass */
static const float mass_damage = 5.0f;
/* Max speed for a Debris. */
static const float max_debris_speed2 = (0.2f)*(0.2f);



/******************* Declare types. ***************/

struct ItemVt;
struct Collision;

/** Abstract {Item}. */
struct Item {
	const struct ItemVt *vt; /* virtual table pointer */
	const struct AutoImage *image, *normals; /* extracted from sprite */
	float bounding; /* radius, fixed to function of the image */
	struct Ortho3f x, v; /* where it is and where it is going */
	unsigned bin; /* which bin is it in, set by {x}, it can move */
	/* The following are temporary: */
	struct Vec2f dx; /* temporary displacement */
	struct Rectangle4f box; /* bounding box between one frame and the next */
	struct Fore *fore; /* needed in case of migrate */
	struct Collision *collision; /* needed in case of migrate */
	struct Light *light; /* pointer to a limited number of lights */
};
typedef void (*ItemToString)(const struct Item *const, char (*const)[12]);
typedef void (*ItemAction)(struct Item *const);
typedef int (*ItemPredicate)(const struct Item *const);
static void item_to_string(const struct Item *this, char (*const a)[12]);
#define LIST_NAME Item
#define LIST_TYPE struct Item
#define LIST_TO_STRING &item_to_string
#include "../templates/List.h"

/* Declare {ShipVt}. */
struct ShipVt;
/** {Ship} extends {Item}. */
struct Ship {
	struct ItemLink base;
	struct ShipVt *vt;
	float mass; /* T */
	struct Vec2f hit; /* F */
	float recharge /* mS */, max_speed2 /* (m/ms)^2 */,
		acceleration /* m/ms^2 */, turn /* radians/ms */;
	char name[16];
	const struct AutoWmdType *wmd;
	unsigned ms_recharge_wmd;
};
static void ship_migrate(struct Ship *const this,
	const struct Migrate *const migrate) {
	assert(this && migrate);
	ItemLinkMigrate(&this->base.data, migrate);
	/* @fixme */
}
#define POOL_NAME Ship
#define POOL_TYPE struct Ship
#define POOL_MIGRATE_EACH &ship_migrate
#include "../templates/Pool.h"

/** {Debris} extends {Item}. */
struct Debris {
	struct ItemLink base;
	float mass, energy;
};
static void debris_migrate(struct Debris *const this,
	const struct Migrate *const migrate) {
	assert(this && migrate);
	ItemLinkMigrate(&this->base.data, migrate);
	/* @fixme */
}
#define POOL_NAME Debris
#define POOL_TYPE struct Debris
#define POOL_MIGRATE_EACH &debris_migrate
#include "../templates/Pool.h"

/** {Wmd} extends {Item}. */
struct Wmd {
	struct ItemLink base;
	const struct AutoWmdType *class;
	/*const struct Item *from;*/
	float mass;
	unsigned expires;
	unsigned light;
};
static void wmd_migrate(struct Wmd *const this,
	const struct Migrate *const migrate) {
	assert(this && migrate);
	ItemLinkMigrate(&this->base.data, migrate);
	/* @fixme */
}
#define POOL_NAME Wmd
#define POOL_TYPE struct Wmd
#define POOL_MIGRATE_EACH &wmd_migrate
#include "../templates/Pool.h"

/** {Gate} extends {Item}. */
struct Gate {
	struct ItemLink base;
	const struct AutoSpaceZone *to;
};
static void gate_migrate(struct Gate *const this,
	const struct Migrate *const migrate);
#define POOL_NAME Gate
#define POOL_TYPE struct Gate
#define POOL_MIGRATE_EACH &gate_migrate
#include "../templates/Pool.h"



/** {Fore} and {Cover} are collision-detection mechanisms; {Fore} defines a
 temporary pointer to an item that can be nullified if the item is destroyed. */
struct Fore {
	struct Item *item;
};
#define POOL_NAME Fore
#define POOL_TYPE struct Fore
/*#define POOL_MIGRATE_EACH &...*/
#define POOL_STACK
#include "../templates/Pool.h"

/** Many {Cover}s, one in each bin, can point at one {Fore}. One {Cover} for
 each {Fore} has {is_corner}, the authoritative {Cover}. */
struct Cover {
	struct Fore *fore;
	int is_corner;
};
#define POOL_NAME Cover
#define POOL_TYPE struct Cover
#define POOL_STACK
#include "../templates/Pool.h"


/** Private: {container_of}. */
/*static struct Gate *upcast_to_gate(struct Item *const item) {
	return (struct Gate *)(void *)
		((char *)item - offsetof(struct Gate, base));
}
*/

static void gate_migrate(struct Gate *const this,
	const struct Migrate *const migrate) {
	assert(this && migrate);
	ItemLinkMigrate(&this->base.data, migrate);
	/*if(this->base.data.fore)
		GatePoolMigratePointer(upcast_to_gate(this->base.data.fore->item), migrate);*/
	/* @fixme */
}



/** Debug. */
struct Info {
	struct Vec2f x;
	const struct AutoImage *image;
};
#define POOL_NAME Info
#define POOL_TYPE struct Info
#define POOL_STACK
#include "../templates/Pool.h"



/** {Collision}s between sprites to apply later. */
struct Collision {
	unsigned no;
	struct Vec2f v;
	float t;
};
#define POOL_NAME Collision
#define POOL_TYPE struct Collision
/*#define POOL_MIGRATE_EACH &...*/
#define POOL_STACK
#include "../templates/Pool.h"



/** All {Items} together. */
static struct Items {
	/* We can have a huge number of {Items}; to that end, space is separated
	 into rectangular bins, sort of like a hash map, to improve speed. */
	struct Bin {
		struct ItemList items;
		struct CoverPool covers;
	} bins[LAYER_SIZE];
	/* Polymorphic backing for {bins}. */
	struct ShipPool ships;
	struct DebrisPool debris;
	struct WmdPool wmds;
	struct GatePool gates;
	/* If we want to do collision detection, we have to account for items that
	 cross bins. Backing for temporary {fores} (eg, pointer-to-object) and
	 actual {collisions}; these will be built-up and torn down every frame. */
	struct ForePool fores;
	struct CollisionPool collisions;
	/* Contains calculations for the {bins}. */
	struct Layer *layer;
	/* Constantly updating frame time. */
	float dt_ms;
	struct InfoPool info; /* Debug. */
	/* Keep track of the player's ship. */
	struct { int is_ship; size_t ship_index; } player;
	/* Lights are a static structure with a hard-limit defined by the shader. */
	struct Lights {
		size_t size;
		struct Light {
			struct Item *item;
		} light_table[MAX_LIGHTS];
		struct Vec2f x_table[MAX_LIGHTS];
		struct Colour3f colour_table[MAX_LIGHTS];
	} lights;
	enum Plots { PLOT_NOTHING, PLOT_SPACE = 1 } plot; /* debug */
} items;



/* Include Light functions. */
#include "ItemsLight.h"

/** Update the bins when the {this} moves.
 @fixme I have a sneaking suspicion that this makes it wobbly because we're
 doing this sequentially. */
static void sprite_moved(struct Item *const this) {
	const unsigned bin = LayerGetOrtho(items.layer, &this->x);
	if(bin == this->bin) return;
	ItemListRemove(/*&items.bins[this->bin].sprites,*/ this);
	this->bin = bin;
	ItemListPush(&items.bins[bin].items, this);
}

/** Gets the player's ship. */
static struct Ship *get_player(void) {
	if(!items.player.is_ship) return 0;
	return ShipPoolGet(&items.ships, items.player.ship_index);
}

/*********** Define virtual functions. ***********/

typedef float (*ItemFloatAccessor)(const struct Item *const);
typedef int (*ItemFloatPredicate)(struct Item *const, const float);

/** Sometimes, the sprite class is important; ie, {typeof(sprite)};
 eg collision resolution. */
enum ItemClass { SC_SHIP, SC_DEBRIS, SC_WMD, SC_GATE };

/** Define {ItemVt}. */
struct ItemVt {
	enum ItemClass class;
	ItemToString to_string;
	ItemAction delete;
	ItemPredicate update;
	ItemAction on_collision;
	ItemFloatAccessor get_mass;
	ItemFloatAccessor get_damage;
	ItemFloatPredicate put_damage;
};


/** @implements <Item>ToString */
static void item_to_string(const struct Item *this, char (*const a)[12]) {
	assert(this && a);
	this->vt->to_string(this, a);
}
/** @implements <Ship>ToString */
static void ship_to_string(const struct Ship *this, char (*const a)[12]) {
	sprintf(*a, "%.11s", this->name);
}
/** @implements <Debris>ToString */
static void debris_to_string(const struct Debris *this, char (*const a)[12]) {
	(void)this;
	sprintf(*a, "Debris",
		(unsigned)DebrisPoolIndex(&items.debris, this) % 100000);
}
/** @implements <Wmd>ToString */
static void wmd_to_string(const struct Wmd *this, char (*const a)[12]) {
	(void)this;
	sprintf(*a, "Wmd%u",
		(unsigned)WmdPoolIndex(&items.wmds, this) % 100000000);
}
/** @implements <Gate>ToString */
static void gate_to_string(const struct Gate *this, char (*const a)[12]) {
	sprintf(*a, "%.7sGate", this->to->name);
}


/** @implements ItemsAction */
static void sprite_delete(struct Item *const this) {
	assert(this);
	Light_(this->light);
	ItemListRemove(this);
	this->bin = (unsigned)-1; /* Makes debugging easier. */
	this->vt->delete(this);
}
/** @implements <Ship>Action */
static void ship_delete(struct Ship *const this) {
	/* The player is deleted; update. */
	if(items.player.is_ship
		&& ShipPoolIndex(&items.ships, this) == items.player.ship_index) {
		items.player.is_ship = 0;
		printf("You died! :0\n");
	}
	ShipPoolRemove(&items.ships, this);
}
/** @implements <Debris>Action */
static void debris_delete(struct Debris *const this) {
	DebrisPoolRemove(&items.debris, this);
}
/** @implements <Wmd>Action */
static void wmd_delete(struct Wmd *const this) {
	WmdPoolRemove(&items.wmds, this);
}
/** @implements <Gate>Action */
static void gate_delete(struct Gate *const this) {
	GatePoolRemove(&items.gates, this);
}


/** If it returns false, it deleted the sprite.
 @implements <Item>Predicate */
static int sprite_update(struct Item *const this) {
	assert(this);
	return this->vt->update(this);
}
/* Includes {ship_update*} Human/AI. */
#include "ItemsAi.h"
/** Does nothing; just debris.
 @implements <Debris>Predicate */
static int debris_update(struct Debris *const this) {
	(void)this;
	return 1;
}
/** @implements <Wmd>Predicate
 @fixme Replace delete with more dramatic death. */
static int wmd_update(struct Wmd *const this) {
	if(TimerIsGameTime(this->expires))
		return sprite_delete(&this->base.data), 0;
	return 1;
}
/** @implements <Gate>Predicate */
static int gate_update(struct Gate *const this) {
	(void)this;
	return 1;
}


/** @fixme Stub.
 @implements <Item>Action */
static void sprite_on_collision(struct Item *const this) {
	this->vt->on_collision(this);
}
/** @implements <Ship>Action */
static void ship_on_collision(const struct Ship *const this) {
	/* @fixme If v > max then cap, damage. */
	(void)this;
}
/* @fixme This is not how explosions work. */
static void debris_breakup(struct Debris *const this) {
	const struct AutoDebris *small = AutoDebrisSearch("SmallAsteroid");
	struct Debris *d;
	struct Ortho3f v, perturb, error;
	int no;
	assert(small);
	no = this->mass / small->mass;
	if(no <= 1) no = 0;
	ortho3f_init(&error);
	while(no) {
		d = Debris(small, &this->base.data.x);
		ortho3f_assign(&v, &this->base.data.v);
		if(!--no) {
			ortho3f_sub(&v, &v, &error);
		} else {
			perturb.x = random_pm_max(0.05f);
			perturb.y = random_pm_max(0.05f);
			perturb.theta = random_pm_max(0.002f);
			ortho3f_sum(&v, &perturb);
			ortho3f_sum(&error, &perturb);
		}
		ItemSetVelocity(&d->base.data, &v);
	}
	sprite_delete(&this->base.data);
}
/** @implements <Debris>Action (sort-of-cheating) */
static void debris_on_collision(struct Debris *const this) {
	float speed2;
	speed2 = this->base.data.v.x * this->base.data.v.x
		+ this->base.data.v.y * this->base.data.v.y;
	if(speed2 > max_debris_speed2) debris_breakup(this);
}
/** @implements <Wmd>Action (sort-of-cheating) */
static void wmd_on_collision(struct Wmd *const this) {
	/* Just delete -- Wmd's are unstable. */
	sprite_delete(&this->base.data);
}
/** @implements <Gate>Action */
static void gate_on_collision(const struct Gate *const this) {
	char a[12];
	assert(this);
	item_to_string(&this->base.data, &a);
	printf("gate_on_collision(%s) strange because gates cannot collide.\n", a);
}


/** @implements <Item>FloatAccessor */
static float get_mass(const struct Item *const this) {
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

/** @implements <Item>FloatAccessor */
static float sprite_get_damage(const struct Item *const this) {
	assert(this);
	return this->vt->get_damage(this);
}
/** @implements <Ship>FloatAccessor */
static float ship_get_damage(const struct Ship *const this) {
	assert(this);
	return this->mass * mass_damage;
}
/** @implements <Debris>FloatAccessor */
static float debris_get_damage(const struct Debris *const this) {
	assert(this);
	return this->mass * mass_damage;
}
/** @implements <Wmd>FloatAccessor */
static float wmd_get_damage(const struct Wmd *const this) {
	assert(this && this->class);
	return this->class->damage;
}
/** @implements <Gate>FloatAccessor */
static float gate_get_damage(const struct Gate *const this) {
	assert(this);
	return 1e36f; /* You are fucked. */
}

/** @implements <Item,Float>Action */
static void sprite_put_damage(struct Item *const this, const float damage) {
	assert(this);
	this->vt->put_damage(this, damage);
}
/** @implements <Ship,Float>Predicate */
static void ship_put_damage(struct Ship *const this, const float damage) {
	this->hit.x -= damage;
	if(this->hit.x <= 0.0f) sprite_delete(&this->base.data);
	if(this->hit.x > this->hit.y) this->hit.x = this->hit.y; /* Full. */
}
/** @implements <Debris,Float>Predicate */
static void debris_put_damage(struct Debris *const this, const float damage) {
	this->energy += damage;
	/* @fixme Arbitrary; depends on composition. */
	if(this->energy > this->mass * mass_damage) debris_breakup(this);
}
/** Just dies.
 @implements <Wmd,Float>Predicate */
static void wmd_put_damage(struct Wmd *const this, const float damage) {
	if(damage > 0.0f) sprite_delete(&this->base.data);
}
/** Just absorbs the damage.
 @implements <Gate,Float>Predicate */
static void gate_put_damage(const struct Gate *const this, const float damage) {
	(void)this, (void)damage;
}

static const struct ItemVt ship_human_vt = {
	SC_SHIP,
	(ItemToString)&ship_to_string,
	(ItemAction)&ship_delete,
	(ItemPredicate)&ship_update_human,
	(ItemAction)&ship_on_collision,
	(ItemFloatAccessor)&ship_get_mass,
	(ItemFloatAccessor)&ship_get_damage,
	(ItemFloatPredicate)&ship_put_damage
}, ship_ai_vt = {
	SC_SHIP,
	(ItemToString)&ship_to_string,
	(ItemAction)&ship_delete,
	(ItemPredicate)&ship_update_ai,
	(ItemAction)&ship_on_collision,
	(ItemFloatAccessor)&ship_get_mass,
	(ItemFloatAccessor)&ship_get_damage,
	(ItemFloatPredicate)&ship_put_damage
}, debris_vt = {
	SC_DEBRIS,
	(ItemToString)&debris_to_string,
	(ItemAction)&debris_delete,
	(ItemPredicate)&debris_update,
	(ItemAction)&debris_on_collision,
	(ItemFloatAccessor)&debris_get_mass,
	(ItemFloatAccessor)&debris_get_damage,
	(ItemFloatPredicate)&debris_put_damage	
}, wmd_vt = {
	SC_WMD,
	(ItemToString)&wmd_to_string,
	(ItemAction)&wmd_delete,
	(ItemPredicate)&wmd_update,
	(ItemAction)&wmd_on_collision,
	(ItemFloatAccessor)&wmd_get_mass,
	(ItemFloatAccessor)&wmd_get_damage,
	(ItemFloatPredicate)&wmd_put_damage	
}, gate_vt = {
	SC_GATE,
	(ItemToString)&gate_to_string,
	(ItemAction)&gate_delete,
	(ItemPredicate)&gate_update,
	(ItemAction)&gate_on_collision,
	(ItemFloatAccessor)&gate_get_mass,
	(ItemFloatAccessor)&gate_get_damage,
	(ItemFloatPredicate)&gate_put_damage	
};



/****************** Type functions. **************/

/* These are the helpers for migrate each; these pick one pointer field out of
 the struct and migrate it. */

/** @implements <Fore>StackMigrateElement */
static void fore_migrate_sprite(struct Fore *const this,
	const struct Migrate *const migrate) {
	assert(this && migrate);
	ItemMigratePointer(&this->item, migrate);
}

/** @implements <Item>ListMigrateElement */
static void sprite_migrate_collision(struct Item *const this,
	const struct Migrate *const migrate) {
	assert(this && migrate);
	CollisionMigratePointer(&this->collision, migrate);
}

/** @implements <Cover>StackMigrateElement */
static void cover_migrate_fore(struct Cover *const this,
	const struct Migrate *const migrate) {
	assert(this && migrate);
	ForeMigratePointer(&this->fore, migrate);
}

/* There are three migrate-nodes that have dependancies in other sub-types
 of {Items}. All of these do not need the {s} because it is available
 statically, but they are passed it anyway. */

/** @implements <Item>Migrate */
static void migrate_sprite(struct Items *const s,
	const struct Migrate *const migrate) {
	unsigned i;
	assert(s && s == &items && migrate);
	printf("migrate_sprite\n");
	/* {sub types}->{sprites}. */
	for(i = 0; i < LAYER_SIZE; i++)
		ItemListMigrate(&s->bins[i].items, migrate);
	/* {fores}->{sprites}. */
	ForePoolMigrateEach(&s->fores, &fore_migrate_sprite, migrate);
	/* {lights}->{sprites}. */
	for(i = 0; i < s->lights.size; i++)
		ItemMigratePointer(&s->lights.light_table[i].item, migrate);
	/* @fixme: also in Events. */
	/* @fixme: also in Wmd. */
	/* @fixme: also player? */
}

/** @implements <Collision>Migrate */
static void migrate_collision(struct Items *const s,
	const struct Migrate *const migrate) {
	unsigned i;
	assert(s && s == &items && migrate);
	printf("migrate_collision\n");
	/* {sprites}->{collisions} */
	for(i = 0; i < LAYER_SIZE; i++) ItemListMigrateEach(&s->bins[i].items,
		&sprite_migrate_collision, migrate);
}

/** @implements <Fore>Migrate */
static void migrate_fore(struct Items *const s,
	const struct Migrate *const migrate) {
	unsigned i;
	assert(s == &items && migrate);
	printf("migrate_fore\n");
	/* {covers}->{fores} */
	for(i = 0; i < LAYER_SIZE; i++) CoverStackMigrateEach(items.bins[i].covers, &cover_migrate_fore, migrate);
}



void ItemsReset(void) {
	unsigned i;
	/* We don't have to do the lights; all static. */
	for(i = 0; i < LAYER_SIZE; i++) {
		ItemListClear(&items.bins[i].items);
		CoverPool_(&items.bins[i].covers);
	}
	InfoPool_(&items.info);
	Layer_(&items.layer);
	CollisionPool_(&items.collisions);
	ForePool_(&items.fores);
	GatePool_(&items.gates);
	WmdPool_(&items.wmds);
	DebrisPool_(&items.debris);
	ShipPool_(&items.ships);
}

/** Clear all space. */
void ItemsRemoveIf(const ItemsPredicate predicate) {
	struct Bin *bin;
	size_t i;
	for(i = 0; i < LAYER_SIZE; i++) {
		bin = items.bins + i;
		ItemListTakeIf(0, &bin->items, predicate);
		CoverPoolClear(&bin->covers); /* Should be empty anyway. */
	}
}



/*************** Sub-type constructors. ******************/

/** Abstract sprite constructor.
 @param auto: An auto-sprite prototype.
 @param vt: Virtual table.
 @param x: Where should the sprite go. Leave null to place it in a uniform
 distribution across space.
 @fixme I don't like random velocity. */
static void item_filler(struct Item *const this,
	const struct ItemVt *const vt, const struct AutoSprite *const as,
	const struct Ortho3f *x) {
	assert(this && vt && as);
	this->vt      = vt;
	this->image   = as->image;
	/* @fixme normals can be null, in which case spherical normals? */
	this->normals = as->normals;
	this->bounding = (as->image->width >= as->image->height ?
		as->image->width : as->image->height) / 2.0f; /* fixme: Crude. */
	if(x) {
		ortho3f_assign(&this->x, x);
	} else {
		LayerSetRandom(items.layer, &this->x);
	}
	ortho3f_init(&this->v);
	this->bin       = LayerGetOrtho(items.layer, &this->x);
	this->dx.x      = this->dx.y = 0.0f;
	this->box.x_min = this->box.x_max = this->box.y_min = this->box.y_max =0.0f;
	this->fore      = 0;
	this->collision = 0;
	this->light     = 0;
	/* Put this in space. */
	ItemListPush(&items.bins[this->bin].items, this);
}

/** Creates a new {Ship}. */
struct Ship *Ship(const struct AutoShipClass *const class,
	const struct Ortho3f *const x, const enum AiType ai) {
	struct Ship *this;
	const struct ItemVt *vt;
	if(!class) return 0;
	assert(class->sprite && class->sprite->image && class->sprite->normals);
	switch(ai) {
		case AI_DUMB:  vt = &ship_ai_vt;    break;
		case AI_HUMAN: vt = &ship_human_vt; break;
		default: assert(printf("Out of range.", 0));
	}
	if(!(this = ShipPoolNew(&items.ships))) return 0;
	item_filler(&this->base.data, vt, class->sprite, x);
	this->mass = class->mass;
	this->hit.x = this->hit.y = class->shield; /* F */
	/* (1/1,000,000)F/ms = (1F/1000mF)(s/1000ms)mF/s = mS */
	assert(class->recharge >= 0);
	this->recharge = class->recharge * 0.000001f;
	/* @fixme m^2/s^2 = m/s */
	this->max_speed2 = class->speed * class->speed;
	/* @fixme */
	this->acceleration = class->acceleration;
	/* 2\Pi/(1000*360)\rad/ms = (1s/1000ms)(2\Pi\rad/360\deg)\deg/s */
	this->turn = class->turn * 0.001f * M_2PI_F / 360.0f;
	Orcish(this->name, sizeof this->name);
	this->wmd = class->weapon;
	this->ms_recharge_wmd = 0;
	if(ai == AI_HUMAN) {
		if(items.player.is_ship)
			fprintf(stderr, "ItemsShip: overriding previous player.\n");
		items.player.is_ship = 1;
		items.player.ship_index = ShipPoolIndex(&items.ships, this);
		strcpy(this->name, "Player");
	}
	return this;
}

/** Creates a new {Debris}. */
struct Debris *Debris(const struct AutoDebris *const class,
	const struct Ortho3f *const x) {
	struct Debris *this;
	if(!class) return 0;
	assert(class->sprite && class->sprite->image && class->sprite->normals);
	if(!(this = DebrisPoolNew(&items.debris))) return 0;
	item_filler(&this->base.data, &debris_vt, class->sprite, x);
	this->mass = class->mass;
	this->energy = 0.0f;
	return this;
}

/** Creates a new {Wmd}. */
struct Wmd *Wmd(const struct AutoWmdType *const class,
	const struct Ship *const from) {
	struct Wmd *this;
	struct Ortho3f x;
	struct Vec2f dir;
	if(!class || !from) return 0;
	assert(class->sprite && class->sprite->image && class->sprite->normals);
	dir.x = cosf(from->base.data.x.theta);
	dir.y = sinf(from->base.data.x.theta);
	x.x = from->base.data.x.x
		+ dir.x * from->base.data.bounding * wmd_distance_mod;
	x.y = from->base.data.x.y
		+ dir.y * from->base.data.bounding * wmd_distance_mod;
	x.theta = from->base.data.x.theta;
	if(!(this = WmdPoolNew(&items.wmds))) return 0;
	item_filler(&this->base.data, &wmd_vt, class->sprite, &x);
	this->class = class;
	/* Speed is in [px/s], want it [px/ms]. */
	this->base.data.v.x = from->base.data.v.x + dir.x * class->speed*0.001f;
	this->base.data.v.y = from->base.data.v.y + dir.y * class->speed*0.001f;
	/*this->from = &from->sprite.data;*/
	this->mass = class->impact_mass;
	this->expires = TimerGetGameTime() + class->ms_range;
	Light(&this->base.data, class->r, class->g, class->b);
	return this;
}

/** Creates a new {Gate}. */
struct Gate *Gate(const struct AutoGate *const class) {
	const struct AutoSprite *const gate_sprite = AutoSpriteSearch("Gate");
	struct Gate *this;
	struct Ortho3f x;
	if(!class) return 0;
	assert(gate_sprite && gate_sprite->image && gate_sprite->normals);
	x.x     = class->x;
	x.y     = class->y;
	x.theta = class->theta, deg_to_rad(&x.theta);
	if(!(this = GatePoolNew(&items.gates))) return 0;
	item_filler(&this->base.data, &gate_vt, gate_sprite, &x);
	this->to = class->to;
	printf("ItemsGate: to %s.\n", this->to->name);
	return this;
}





/*************** Functions. *****************/

/** Called in \see{extrapolate}.
 @implements LayerNoItemAction */
static void put_cover(const unsigned bin, const unsigned no,
	struct Fore *const on) {
	struct Cover *cover;
	/*char a[12];*/
	assert(on && on->item && bin < LAYER_SIZE);
	if(!(cover = CoverPoolNew(&items.bins[bin].covers)))
		{ perror("put_cover"); return; } /* Not that important I guess. */
	cover->fore = on;
	cover->is_corner = !no;
}
/** Moves the sprite. Calculates temporary values, {dx}, and {box}; sticks it
 into the appropriate {covers}. Called in \see{extrapolate_bin}.
 @implements <Item>Action */
static void extrapolate(struct Item *const this) {
	struct Fore *on;
	assert(this);
	/* If it returns false, it's deleted. */
	if(!sprite_update(this)) return;
	/* Kinematics. */
	this->dx.x = this->v.x * items.dt_ms;
	this->dx.y = this->v.y * items.dt_ms;
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
	 it is spread out, so it may cover multiple bins. {ItemRectangle} is
	 temporary, affecting {ItemForEachItem}. {Fore} is a pointer to a
	 {Item} that can go in multiple bins in the {Layer}. Until the {Layer} is
	 cleared, deleting a sprite causes dangling pointers. */
	LayerSetItemRectangle(items.layer, &this->box);
	if(!(on = ForePoolNew(&items.fores))) { perror("fore"); return; }
	on->item = this;
	LayerItemForEachItem(items.layer, on, &put_cover);
}
/** Called in \see{ItemsUpdate}.
 @implements <Bin>Action */
static void extrapolate_bin(const unsigned idx) {
	ItemListForEach(&items.bins[idx].items, &extrapolate);
}

/** Relies on \see{extrapolate}; all pre-computation is finalised in this step
 and values are advanced. Collisions are used up and need to be cleared after.
 Called from \see{timestep_bin}.
 @implements <Item>Action */
static void timestep(struct Item *const this) {
	const float t = items.dt_ms;
	assert(this);
	/* Velocity. */
	if(this->collision) {
		const float t0 = this->collision->t, t1 = t - t0;
		const struct Vec2f *const v1 = &this->collision->v;
		this->x.x = this->x.x + this->v.x * t0 + v1->x * t1;
		this->x.y = this->x.y + this->v.y * t0 + v1->y * t1;
		this->v.x = v1->x, this->v.y = v1->y;
	} else {
		this->x.x += this->v.x * t;
		this->x.y += this->v.y * t;
	}
	/* Angular velocity -- this is {\omega}. */
	this->x.theta += this->v.theta * t;
	branch_cut_pi_pi(&this->x.theta);
	sprite_moved(this);
	if(this->collision) {
		/* Erase the reference; will be erased all at once in {timestep_bin}. */
		this->collision = 0;
		sprite_on_collision(this);
	}
}
/** Called in \see{ItemsUpdate}.
 @implements <Bin>Action */
static void timestep_bin(const unsigned idx) {
	ItemListForEach(&items.bins[idx].items, &timestep);
	/* Collisions are consumed, but not erased; erase them. */
	CollisionPoolClear(&items.collisions);
}

/* This is where \see{collide_bin} is located, but lots of helper functions. */
#include "ItemsCollide.h"

/* This includes some debuging functions, namely, {ItemsPlot}. */
#include "ItemsPlot.h"

/** Update each frame.
 @param target: What the camera focuses on; could be null. */
void ItemsUpdate(const int dt_ms) {
	/* Update with the passed parameter. */
	items.dt_ms = dt_ms;
	/* Clear info on every frame. */
	InfoPoolClear(&items.info);
	/* Centre on the the player. */
	if(items.player.is_ship) {
		struct Ship *const player = ShipPoolGet(&items.ships,
			items.player.ship_index);
		if(player) DrawSetCamera((struct Vec2f *)&player->base.data.x);
	}
	/* Foreground drawable sprites are a function of screen position. */
	{ 	struct Rectangle4f rect;
		DrawGetScreen(&rect);
		rectangle4f_expand(&rect, layer_space * 0.5f);
		LayerSetScreenRectangle(items.layer, &rect); }
	/* Dynamics; puts temp values in {cover} for collisions. Don't delete a
	 sprite until {fores} has been cleared. */
	LayerForEachScreen(items.layer, &extrapolate_bin);
	/* Debug. */
	if(items.plot) {
		if(items.plot & PLOT_SPACE) space_plot();
		items.plot = PLOT_NOTHING;
	}
	/* Collision has to be called after {extrapolate}; it consumes {cover}.
	 (fixme: really? 3 passes?) */
	LayerForEachScreen(items.layer, &collide_bin);
	/* Clear out the temporary {fores} now that cover is consumed. */
	ForePoolClear(&items.fores);
	/* Time-step. */
	LayerForEachScreen(items.layer, &timestep_bin);
}

/** Called from \see{draw_bin}.
 @implements <Item>Action */
static void draw_sprite(struct Item *const this) {
	DrawDisplayLambert(&this->x, this->image, this->normals);
}
/** Called from \see{ItemsDraw}.
 @implements LayerAction */
static void draw_bin(const unsigned idx) {
	assert(idx < LAYER_SIZE);
	ItemListForEach(&items.bins[idx].items, &draw_sprite);
}
/** Must call \see{ItemUpdate} before this, because it sets
 {sprites.layer}. Use when the Lambert GPU shader is loaded. */
void ItemsDraw(void) {
	LayerForEachScreen(items.layer, &draw_bin);
}

/* Debug info. */
void Info(const struct Vec2f *const x, const struct AutoImage *const image) {
	struct Info *info;
	if(!x || !image) return;
	/*char a_str[12], b_str[12];
	 sprite_to_string(a, &a_str), sprite_to_string(b, &b_str);
	 printf("Degeneracy pressure between %s and %s.\n", a_str, b_str);*/
	/* Debug show hair. */
	if(!(info = InfoPoolNew(&items.info))) return;
	info->x.x = x->x;
	info->x.y = x->y;
	info->image = image;
}
/* @implements <Vec2f>Action */
static void draw_info(struct Info *const this) {
	DrawDisplayInfo(&this->x, this->image);
}
/** Use when the Info GPU shader is loaded. */
void ItemsInfo(void) {
	InfoPoolForEach(&items.info, &draw_info);
}



/** Specific sprites. */

/** Allows access to {this}' position as a read-only value. */
const struct Ortho3f *ItemGetPosition(const struct Item *const this) {
	if(!this) return 0;
	return &this->x;
}
/** Allows access to {this}' velocity as a read-only value. */
const struct Ortho3f *ItemGetVelocity(const struct Item *const this) {
	if(!this) return 0;
	return &this->v;
}
/** Modifies {this}' position. */
void ItemSetPosition(struct Item *const this,const struct Ortho3f *const x){
	if(!this || !x) return;
	ortho3f_assign(&this->x, x);
	sprite_moved(this);
}
/** Modifies {this}' velocity. */
void ItemSetVelocity(struct Item *const this,const struct Ortho3f *const v){
	if(!this || !v) return;
	ortho3f_assign(&this->v, v);
}

/** Gets an {AutoSpaceZone} that it goes to, if it exists. */
const struct AutoSpaceZone *GateGetTo(const struct Gate *const this) {
	if(!this) return 0;
	return this->to;
}

/** Gets a {Item} that goes to the {AutoSpaceZone}, if it exists.
 @order O({|max Gates|}) */
struct Gate *FindGate(const struct AutoSpaceZone *const to) {
	struct GatePool *const gates = &items.gates;
	struct Gate *g = 0;
	if(!to) return 0;
	while((g = GatePoolNext(gates, g))) if(g->to == to) return g;
	return 0;
}

/** Gets the player's ship or null. */
struct Ship *ItemsGetPlayerShip(void) {
	return get_player();
}

/** How much shields are left. */
const struct Vec2f *ShipGetHit(const struct Ship *const this) {
	if(!this) return 0;
	return &this->hit;
}

/** Debug; only prints four at a time. */
char *ItemsToString(const struct Item *const this) {
	static char as[4][12];
	static unsigned i;
	char (*a)[12];
	if(!this) return 0;
	a = as + i++, i &= 3;
	item_to_string(this, a);
	return *a;
}

/** Debug; called in Game.c or something once. */
unsigned ItemGetBin(const struct Item *const this) {
	if(!this) return 0;
	return this->bin;
}
