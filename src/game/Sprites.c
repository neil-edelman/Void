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
#include "../general/Bins.h" /* for bins */
#include "../system/Key.h" /* keys for input */
#include "../system/Draw.h" /* DrawSetCamera, DrawGetScreen */
#include "../system/Timer.h" /* for expiring */
#include "Light.h" /* for glowing */
#include "Sprites.h"

#define BINS_SIDE_SIZE (64)
#define BINS_SIZE (BINS_SIDE_SIZE * BINS_SIDE_SIZE)
enum { HOLDING_BIN = BINS_SIZE }; /* Special bins must match {Sprites}. */
static const float bin_space = 256.0f;

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
	struct Collision *collision_set; /* temporary, \in collisions */
	unsigned bin; /* which bin is it in, set by {x_5}, or {no_bin} */
	struct Ortho3f x, v; /* where it is and where it is going */
	struct Vec2f dx, x_5; /* temporary values */
	float bounding; /* radius, fixed to function of the image */
	struct Rectangle4f box; /* bounding box between one frame and the next */
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

/* Define {DebrisPool} and {DebrisPoolNode}, a subclass of {Sprite}. */
struct Debris {
	struct SpriteListNode sprite;
	float mass;
};
#define POOL_NAME Debris
#define POOL_TYPE struct Debris
#include "../templates/Pool.h"

/* Define {WmdPool} and {WmdPoolNode}, a subclass of {Sprite}. */
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

/* Define {GatePool} and {GatePoolNode}, a subclass of {Sprite}. */
struct Gate {
	struct SpriteListNode sprite;
	const struct AutoSpaceZone *to;
};
#define POOL_NAME Gate
#define POOL_TYPE struct Gate
#include "../templates/Pool.h"

/* Collisions between sprites to apply later. This is a pool that sprites can
 use. Defines {CollisionPool}, {CollisionPoolNode}. */
/*struct Collision {
	struct Collision *next;
	struct Vec2f v;
	float t;
};
#define POOL_NAME Collision
#define POOL_TYPE struct Collision
#include "../templates/Pool.h"*/

/** While {SpriteListForEach} is running, we may have to transfer a sprite to
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

/*#define POOL_NAME Ref
#define POOL_TYPE size_t
#include "../templates/Pool.h"*/

/** Sprites all together. */
static struct Sprites {
	/* The order of this list must mach the enum. */
	struct SpriteList bins[BINS_SIZE], holding;
	struct ShipPool *ships;
	struct DebrisPool *debris;
	struct WmdPool *wmds;
	struct GatePool *gates;
	float dt_ms; /* constantly updating frame time */
	struct Bins *foreground;
} *sprites;



/*********** Define virtual functions. ***********/

typedef float (*SpriteFloatAccessor)(const struct Sprite *const);

/** Define {SpriteVt}. */
struct SpriteVt {
	SpriteToString to_string;
	SpriteAction delete, update;
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
	SpriteListRemove(sprites->bins + this->bin, this); /* Safe-ish. */
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
static void shoot(struct Ship *const ship);
/** @implements <Ship>Action */
static void ship_update_human(struct Ship *const this) { UNUSED(this); }
/** Called at beginning of frame with one ship, {player}. (fixme; hmm)
 @implements <Ship>Action */
static void ship_player(struct Ship *const this) {
	const int ms_turning = KeyTime(k_left) - KeyTime(k_right);
	const int ms_acceleration = KeyTime(k_up) - KeyTime(k_down);
	const int ms_shoot = KeyTime(32); /* fixme */
	/* fixme: This is for all. :[ */
	if(ms_acceleration > 0) { /* not a forklift */
		struct Vec2f v = { this->sprite.data.v.x, this->sprite.data.v.y };
		float a = ms_acceleration * this->acceleration, speed2;
		v.x += cosf(this->sprite.data.x.theta) * a;
		v.y += sinf(this->sprite.data.x.theta) * a;
		if((speed2 = v.x * v.x + v.y * v.y) > this->max_speed2) {
			float correction = sqrtf(this->max_speed2 / speed2);
			v.x *= correction, v.y *= correction;
		}
		this->sprite.data.v.x = v.x, this->sprite.data.v.y = v.y;
	}
	if(ms_turning) {
		float t = ms_turning * this->turn * turn_acceleration;
		this->sprite.data.v.theta += t;
		if(this->sprite.data.v.theta < -this->turn) {
			this->sprite.data.v.theta = -this->turn;
		} else if(this->sprite.data.v.theta > this->turn) {
			this->sprite.data.v.theta = this->turn;
		}
	} else {
		/* \${turn_damping_per_ms^dt_ms}
		 Taylor: d^25 + d^25(t-25)log d + O((t-25)^2).
		 @fixme What about extreme values? */
		this->sprite.data.v.theta *= turn_damping_per_25ms
		- turn_damping_1st_order * (sprites->dt_ms - 25.0f);
	}
	if(ms_shoot) shoot(this);
}
/** @implements <Ship>Action */
static void ship_update_dumb_ai(struct Ship *const this) {
	assert(this);
	this->sprite.data.v.theta = 0.0002f;
}
#if 0
static void ship_update_ai(struct Ship *const this) {
	const struct Ship *const b = GameGetPlayer();
	struct Vec2f c;
	float d_2, theta, t;
	int turning = 0, acceleration = 0;
	
	if(!b) return; /* fixme */
	c.x = b->sprite.data.r.x - this->sprite.data.r.x;
	c.y = b->sprite.data.r.y - this->sprite.data.r.y;
	d_2   = c.x * c.x + c.y * c.y;
	theta = atan2f(c.y, c.x);
	/* the ai ship aims where it thinks the player will be when it's shot gets
	 there, approx - too hard to beat! */
	/*target.x = ship[SHIP_PLAYER].p.x - Sin[ship[SHIP_PLAYER].r] * ship[SHIP_PLAYER].inertia.x * (disttoenemy / WEPSPEED) - ship[1].inertia.x;
	 target.y = ship[SHIP_PLAYER].p.y + Cos[ship[SHIP_PLAYER].r] * ship[SHIP_PLAYER].inertia.y * (disttoenemy / WEPSPEED) - ship[1].inertia.y;
	 disttoenemy = hypot(target.x - ship[SHIP_CPU].p.x, target.y - ship[SHIP_CPU].p.y);*/
	/* t is the error of where wants vs where it's at */
	t = theta - this->sprite.data.r.theta;
	if(t >= M_PI_F) { /* keep it in the branch cut */
		t -= M_2PI_F;
	} else if(t < -M_PI_F) {
		t += M_2PI_F;
	}
	/* too close; ai only does one thing at once, or else it would be hard */
	if(d_2 < shp_ai_too_close) {
		if(t < 0) t += (float)M_PI_F;
		else      t -= (float)M_PI_F;
	}
	/* turn */
	if(t < -shp_ai_turn || t > shp_ai_turn) {
		turning = (int)(t * shp_ai_turn_constant);
		/*if(turning * dt_s fixme */
		/*if(t < 0) {
		 t += (float)M_PI_F;
		 } else {
		 t -= (float)M_PI_F;
		 }*/
	} else if(d_2 > shp_ai_too_close && d_2 < shp_ai_too_far) {
		ShipShoot(this);
	} else if(t > -shp_ai_turn_sloppy && t < shp_ai_turn_sloppy) {
		acceleration = shp_ai_speed;
	}
	ShipInput(this, turning, acceleration);
}
#endif
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


static const struct SpriteVt ship_human_vt = {
	(SpriteToString)&ship_to_string,
	(SpriteAction)&ship_delete,
	(SpriteAction)&ship_update_human,
	(SpriteFloatAccessor)&ship_get_mass
}, ship_ai_vt = {
	(SpriteToString)&ship_to_string,
	(SpriteAction)&ship_delete,
	(SpriteAction)&ship_update_dumb_ai,
	(SpriteFloatAccessor)&ship_get_mass
}, debris_vt = {
	(SpriteToString)&debris_to_string,
	(SpriteAction)&debris_delete,
	(SpriteAction)&debris_update,
	(SpriteFloatAccessor)&debris_get_mass
}, wmd_vt = {
	(SpriteToString)&wmd_to_string,
	(SpriteAction)&wmd_delete,
	(SpriteAction)&wmd_update,
	(SpriteFloatAccessor)&wmd_get_mass
}, gate_vt = {
	(SpriteToString)&gate_to_string,
	(SpriteAction)&gate_delete,
	(SpriteAction)&gate_update,
	(SpriteFloatAccessor)&gate_get_mass
};



/****************** Type functions. **************/

/** @param sprites_void: We don't need this because there's only one static.
 Should always equal {sprites}.
 @implements Migrate */
static void bin_migrate(void *const sprites_void,
	const struct Migrate *const migrate) {
	struct Sprites *const sprites_pass = sprites_void;
	unsigned i;
	assert(sprites_pass && sprites_pass == sprites && migrate);
	for(i = 0; i < BINS_SIZE; i++) {
		SpriteListMigrate(sprites_pass->bins + i, migrate);
	}
	SpriteListMigrate(&sprites_pass->holding, migrate);
}

/** Destructor. */
void Sprites_(void) {
	unsigned i;
	if(!sprites) return;
	for(i = 0; i < BINS_SIZE; i++) SpriteListClear(sprites->bins + i);
	SpriteListClear(&sprites->holding);
	Bins_(&sprites->foreground);
	GatePool_(&sprites->gates);
	WmdPool_(&sprites->wmds);
	DebrisPool_(&sprites->debris);
	ShipPool_(&sprites->ships);
	free(sprites), sprites = 0;
}

/** @return True if the sprite buffers have been set up. */
int Sprites(void) {
	unsigned i;
	enum { NO, SHIP, DEBRIS, WMD, GATE, BIN } e = NO;
	const char *ea = 0, *eb = 0;
	if(sprites) return 1;
	if(!(sprites = malloc(sizeof *sprites)))
		{ perror("Sprites"); Sprites_(); return 0; }
	for(i = 0; i < BINS_SIZE; i++) SpriteListClear(sprites->bins + i);
	SpriteListClear(&sprites->holding);
	sprites->ships = 0;
	sprites->debris = 0;
	sprites->wmds = 0;
	sprites->gates = 0;
	sprites->dt_ms = 20;
	sprites->foreground = 0;
	do {
		if(!(sprites->ships = ShipPool(&bin_migrate, sprites))) { e=SHIP;break;}
		if(!(sprites->debris=DebrisPool(&bin_migrate,sprites))){e=DEBRIS;break;}
		if(!(sprites->wmds = WmdPool(&bin_migrate, sprites))) { e = WMD; break;}
		if(!(sprites->gates = GatePool(&bin_migrate, sprites))) { e=GATE;break;}
		if(!(sprites->foreground = Bins(BINS_SIDE_SIZE, bin_space)))
			{ e = BIN; break; }
	} while(0); switch(e) {
		case NO: break;
		case SHIP: ea = "ships", eb = ShipPoolGetError(sprites->ships); break;
		case DEBRIS: ea = "debris",eb=DebrisPoolGetError(sprites->debris);break;
		case WMD: ea = "wmds", eb = WmdPoolGetError(sprites->wmds); break;
		case GATE: ea = "gates", eb = GatePoolGetError(sprites->gates); break;
		case BIN: ea = "bins", eb = "couldn't get bins"; break;
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
	this->collision_set = 0;
	this->bin     = HOLDING_BIN;
	if(x) {
		Ortho3f_assign(&this->x, x);
	} else {
		Ortho3f_filler_fg(&this->x);
	}
	this->dx.x = this->dx.y = 0.0f;
	this->x_5.x = this->x.x, this->x_5.y = this->x.y;
	Ortho3f_filler_zero(&this->v);
	this->bounding = (as->image->width >= as->image->height ?
		as->image->width : as->image->height) / 2.0f; /* fixme: Crude. */
	this->box.x_min = this->box.x_max = this->box.y_min = this->box.y_max =0.0f;
	SpriteListPush(&sprites->holding, (struct SpriteListNode *)this);
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

/** This is used by AI and humans.
 @fixme Simplistic, shoot on frame. */
static void shoot(struct Ship *const ship) {
	assert(ship && ship->wmd);
	if(!TimerIsTime(ship->ms_recharge_wmd)) return;
	printf("Phew!\n");
	SpritesWmd(ship->wmd, ship);
	ship->ms_recharge_wmd = TimerGetGameTime() + ship->wmd->ms_recharge;
}

/** Each frame, calculate the bins that are visible. Called from
 @see{SpritesUpdate}. */
static void add_sprite_bins(struct Bins *const bins) {
	struct Rectangle4f rect;
	assert(bins);
	DrawGetScreen(&rect);
	BinsSetRectangle(bins, &rect);
}

/** The spawned and transfer sprites while iterating go into the holding bin.
 This sends them into their proper bins. Called from \see{SpritesUpdate}. */
static void clear_holding_bin(void) {
	assert(sprites);
	struct Sprite *sprite;
	while((sprite = SpriteListGetLast(&sprites->holding))) {
		SpriteListRemove(&sprites->holding, sprite);
		sprite->bin = BinsVector(sprites->foreground, &sprite->x_5);
		SpriteListPush(sprites->bins + sprite->bin,
			(struct SpriteListNode *)sprite);
	}
}

/** Moves the sprite. Calculates temporary values, {dx}, {x_5}, and {box}.
 Half-way through the frame, {x_5}, is used to divide the sprites into bins;
 {transfers} is used to stick sprites that have overflowed their bin, and
 should be dealt with next.
 @implements <Sprite>Action */
static void extrapolate(struct Sprite *const this) {
	unsigned bin;
	assert(sprites && this);
	sprite_update(this);
	/* Kinematics. */
	this->dx.x = this->v.x * sprites->dt_ms;
	this->dx.y = this->v.y * sprites->dt_ms;
	/* Determines into which bin it should be. */
	this->x_5.x = this->x.x + 0.5f * this->dx.x;
	this->x_5.y = this->x.y + 0.5f * this->dx.y;
	/* Dynamic bounding rectangle. */
	this->box.x_min = this->x.x - this->bounding;
	this->box.x_max = this->x.x + this->bounding;
	if(this->dx.x < 0) this->box.x_min += this->dx.x;
	else this->box.x_max += this->dx.x;
	this->box.y_min = this->x.y - this->bounding;
	this->box.y_max = this->x.y + this->bounding;
	if(this->dx.y < 0) this->box.y_min += this->dx.y;
	else this->box.y_max += this->dx.y;
	bin = BinsVector(sprites->foreground, &this->x_5);
	/* This happens when the sprite wanders out of the bin. The reason we don't
	 stick it in it's new bin immediately, is because the other bin may still
	 be on the iterating stack, causing it to call it again (x n.) */
	if(bin != this->bin) {
		char a[12];
		this->vt->to_string(this, &a);
		printf("Sprite %s has transferred bins %u -> %u.\n", a, this->bin, bin);
		SpriteListRemove(sprites->bins + this->bin, this);
		/* I don't think that this matters, we could set it to {bin} and save
		 the trouble of recalculating, but it feels dirty. */
		this->bin = HOLDING_BIN;
		SpriteListPush(&sprites->holding, (struct SpriteListNode *)this);
	}
}

static void for_each_screen(const SpriteAction action) {
	assert(action);
}

/** Update each frame.
 @param target: What the camera focuses on; could be null. */
void SpritesUpdate(const int dt_ms, struct Sprite *const target) {
	if(!sprites) return;
	/* Update with the passed parameter. */
	sprites->dt_ms = dt_ms;
	/* Centre on the sprite that was passed, if it is available. */
	if(target) DrawSetCamera((struct Vec2f *)&target->x);
	/* Foreground drawables are a function of screen position. */
	add_sprite_bins(sprites->foreground);
	/* Newly spawned sprites. */
	clear_holding_bin();
	/* Dynamics. */
	for_each_screen(&extrapolate);
	/* {extrapolate} puts the sprites that have wandered out of their bins in
	 the holding bin as well. */
	clear_holding_bin();

#if 0
	if(!DelayPoolIsEmpty(delays)) {
		/*BinPoolForEach(draw_bins, &Bin_print);*/
		printf("Delay: %s.\n", DelayPoolToString(delays));
	}
	if(!DelayPoolIsEmpty(removes)) {
		/*BinPoolForEach(draw_bins, &Bin_print);*/
		printf("Remove: %s.\n", DelayPoolToString(removes));
	}
	DelayPoolForEach(delays, &Delay_evaluate), DelayPoolClear(delays);
	DelayPoolForEach(removes, &Delay_evaluate), DelayPoolClear(removes);
	/* fixme: place things in to drawing area. */
	/* Collision relies on the values calculated in \see{extrapolate}. */
	for_each_update(&collide);
	/* Final time-step where new values are calculated. Takes data from
	 \see{extrapolate} and \see{collide}. */
	for_each_update(&timestep);
	CollisionPoolClear(collisions);
#endif
}

/* Messy messy; this is because function-pointers don't necessarily have the
 same size. */
struct ContainsLambertOutput { const LambertOutput out; };
/** Called from \see{draw_bin}.
 @implements <Sprite, ContainsLambertOutput>BiAction */
static void draw_sprite(struct Sprite *const this, void *const param) {
	const struct ContainsLambertOutput *const clo = param;
	assert(sprites && param);
	clo->out(&this->x, this->image, this->normals);
}
/** Called from \see{SpritesDrawForeground}.
 @implements BinsAction */
static void draw_bin(const unsigned idx, void *const param) {
	assert(sprites && idx < BINS_SIZE && param);
	SpriteListBiForEach(sprites->bins + idx, &draw_sprite, param);
}
/** Must call \see{SpriteUpdate} before this, because it sets
 {sprites.foreground}. Use when the Lambert GPU shader is loaded. */
void SpritesDrawForeground(const LambertOutput draw) {
	struct ContainsLambertOutput container = { draw };
	assert(sprites && draw);
	BinsBiForEach(sprites->foreground, &draw_bin, &container);
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

/** @implements <Sprite>Action */
static void sprite_update(struct Sprite *const sprites) {
	sprites->vt->update(sprites);
}

/* Compute bounding boxes of where the sprite wants to go this frame,
 containing the projected circle along a vector {x -> x + v*dt}. This is the
 first step in collision detection. */
static void collide_extrapolate(struct Sprite *const sprites) {
	/* where they should be if there's no collision */
	sprites->r1.x = sprites->r.x + sprites->v.x * dt_ms;
	sprites->r1.y = sprites->r.y + sprites->v.y * dt_ms;
	/* clamp */
	if(sprites->r1.x > de_sitter)       sprites->r1.x = de_sitter;
	else if(sprites->r1.x < -de_sitter) sprites->r1.x = -de_sitter;
	if(sprites->r1.y > de_sitter)       sprites->r1.y = de_sitter;
	else if(sprites->r1.y < -de_sitter) sprites->r1.y = -de_sitter;
	/* extend the bounding box along the circle's trajectory */
	if(sprites->r.x <= sprites->r1.x) {
		sprites->box.x_min = sprites->r.x  - sprites->bounding - bin_half_space;
		sprites->box.x_max = sprites->r1.x + sprites->bounding + bin_half_space;
	} else {
		sprites->box.x_min = sprites->r1.x - sprites->bounding - bin_half_space;
		sprites->box.x_max = sprites->r.x  + sprites->bounding + bin_half_space;
	}
	if(sprites->r.y <= sprites->r1.y) {
		sprites->box.y_min = sprites->r.y  - sprites->bounding - bin_half_space;
		sprites->box.y_max = sprites->r1.y + sprites->bounding + bin_half_space;
	} else {
		sprites->box.y_min = sprites->r1.y - sprites->bounding - bin_half_space;
		sprites->box.y_max = sprites->r.y  + sprites->bounding + bin_half_space;
	}
}

/** Used after \see{collide_extrapolate} on all the sprites you want to check.
 Uses Hahn–Banach separation theorem and the ordered list of projections on the
 {x} and {y} axis to build up a list of possible colliders based on their
 bounding box of where it moved this frame. Calls \see{collide_circles} for any
 candidites. */
static void collide_boxes(struct Sprite *const sprites) {
	/* assume it can be in a maximum of four bins at once as it travels? */
	/*...*/
#if 0
	struct Sprite *b, *b_adj_y;
	float t0;
	if(!sprites) return;
	/* mark x */
	for(b = sprites->prev_x; b && b->x >= explore_x_min; b = b->prev_x) {
		if(sprites < b && sprites->x_min < b->x_max) b->is_selected = -1;
	}
	for(b = sprites->next_x; b && b->x <= explore_x_max; b = b->next_x) {
		if(sprites < b && sprites->x_max > b->x_min) b->is_selected = -1;
	}
	/* consider y and maybe add it to the list of colliders */
	for(b = sprites->prev_y; b && b->y >= explore_y_min; b = b_adj_y) {
		b_adj_y = b->prev_y; /* b can be deleted; make a copy */
		if(b->is_selected
			&& sprites->y_min < b->y_max
			&& (response = collision_matrix[b->sp_type][sprites->sp_type])
			&& collide_circles(sprites->x, sprites->y, sprites->x1, sprites->y1, b->x,
			b->y, b->x1, b->y1, sprites->bounding + b->bounding, &t0))
			response(sprites, b, t0);
		/*debug("Collision %s--%s\n", SpriteToString(a), SpriteToString(b));*/
	}
	for(b = sprites->next_y; b && b->y <= explore_y_max; b = b_adj_y) {
		b_adj_y = b->next_y;
		if(b->is_selected
			&& sprites->y_max > b->y_min
			&& (response = collision_matrix[b->sp_type][sprites->sp_type])
			&& collide_circles(sprites->x, sprites->y, sprites->x1, sprites->y1, b->x,
			b->y, b->x1, b->y1, sprites->bounding + b->bounding, &t0))
			response(sprites, b, t0);
		/*debug("Collision %s--%s\n", SpriteToString(a), SpriteToString(b));*/
	}
	/* uhh, yes? I took this code from SpriteUpdate */
	if(!s->no_collisions) {
		/* no collisions; apply position change, calculated above */
		s->x = s->x1;
		s->y = s->y1;
	} else {
		/* if you skip this step, it's unstable on multiple collisions */
		const float one_collide = 1.0f / s->no_collisions;
		const float s_vx1 = s->vx1 * one_collide;
		const float s_vy1 = s->vy1 * one_collide;
		/* before and after collision;  */
		s->x += (s->vx * s->t0_dt_collide + s_vx1 * (1.0f - s->t0_dt_collide)) * dt_ms;
		s->y += (s->vy * s->t0_dt_collide + s_vy1 * (1.0f - s->t0_dt_collide)) * dt_ms;
		s->vx = s_vx1;
		s->vy = s_vy1;
		/* unmark for next frame */
		s->no_collisions = 0;
		/* apply De Sitter spacetime? already done? */
		if(s->x      < -de_sitter) s->x =  de_sitter - 20.0f;
		else if(s->x >  de_sitter) s->x = -de_sitter + 20.0f;
		if(s->y      < -de_sitter) s->y =  de_sitter - 20.0f;
		else if(s->y >  de_sitter) s->y = -de_sitter + 20.0f;
	}
	/* bin change */
#endif
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

/* branch cut (-Pi,Pi] */
/* branch cut (-Pi,Pi] */

/** Elastic collision between circles; called from {@see collide} with
 {@code t0_dt} from {@see collide_circles}. Use this when we've determined that
 {@code Sprite a} collides with {@code Sprite b} at {@code t0_dt}, where
 Sprites' {@code x, y} are at time 0 and {@code x1, y1} are at time 1 (we will
 not get here, just going towards.) Velocities are {@code vx, vy} and this
 function is responsible for setting {@code vx1, vy1}, after the collision.
 Also, it may alter (fudge) the positions a little to avoid interpenetration.
 @param a		Sprite a.
 @param b		Sprite b.
 @param t0_dt	The fraction of the frame that the collision occurs, [0, 1]. */
static void elastic_bounce(struct Sprite *u, struct Sprite *v, const float t0_dt) {
	/* interpolate position of collision */
	const struct Vec2f a = {
		u->r.x * t0_dt + u->r1.x * (1.0f - t0_dt),
		u->r.y * t0_dt + u->r1.y * (1.0f - t0_dt)
	}, b = {
		v->r.x * t0_dt + v->r1.x * (1.0f - t0_dt),
		v->r.y * t0_dt + v->r1.y * (1.0f - t0_dt)
	},
	/* delta */
	d = {
		b.x - a.x,
		b.y - a.y
	};
	/* normal at point of impact; fixme: iffy infinities */
	const float n_d2 = d.x * d.x + d.y * d.y;
	const float n_n  = (n_d2 < epsilon) ? 1.0f / epsilon : 1.0f / sqrtf(n_d2);
	const struct Vec2f n = {
		d.x * n_n,
		d.y * n_n
	};
	/* a's velocity, normal direction */
	const float a_nrm_s = u->v.x * n.x + u->v.y * n.y;
	const struct Vec2f a_nrm = {
		a_nrm_s * n.x,
		a_nrm_s * n.y
	},
	/* a's velocity, tangent direction */
	a_tan = {
		u->v.x - a_nrm.x,
		u->v.y - a_nrm.y
	};
	/* b's velocity, normal direction */
	const float b_nrm_s = v->v.x * n.x + v->v.y * n.y;
	const struct Vec2f b_nrm = {
		b_nrm_s * n.x,
		b_nrm_s * n.y
	},
	/* b's velocity, tangent direction */
	b_tan = {
		v->v.x - b_nrm.x,
		v->v.y - b_nrm.y
	};
	/* mass (assume all mass is positive!) */
	const float a_m = u->mass, b_m = v->mass;
	const float diff_m = a_m - b_m, invsum_m = 1.0f / (a_m + b_m);
	/* elastic collision */
	const struct Vec2f a_v = {
		a_tan.x + (a_nrm.x*diff_m + 2*b_m*b_nrm.x) * invsum_m,
		a_tan.y + (a_nrm.y*diff_m + 2*b_m*b_nrm.y) * invsum_m
	}, b_v = {
		(-b_nrm.x*diff_m + 2*a_m*a_nrm.x) * invsum_m + b_tan.x,
		(-b_nrm.y*diff_m + 2*a_m*a_nrm.y) * invsum_m + b_tan.y
	};
	/* check for sanity */
	const float bounding = u->bounding + v->bounding;
	/* fixme: float stored in memory? */

	/*pedantic("elasitic_bounce: colision between %s--%s norm_d %f; sum_r %f, %f--%ft\n",
		SpriteToString(u), SpriteToString(v), sqrtf(n_d2), bounding, a_m, b_m);*/

	/* interpenetation; happens about half the time because of IEEE754 numerics,
	 which could be on one side or the other; also, sprites that just appear,
	 multiple collisions interfering, and gremlins; you absolutely do not want
	 objects to get stuck orbiting each other (fixme: this happens) */
	if(n_d2 < bounding * bounding) {
		const float push = (bounding - sqrtf(n_d2)) * 0.5f;
		pedantic("elastic_bounce: \\pushing sprites %f distance apart\n", push);
		u->r.x -= n.x * push;
		u->r.y -= n.y * push;
		v->r.x += n.x * push;
		v->r.y += n.y * push;
	}

	if(!u->no_collisions) {
		/* first collision */
		u->no_collisions = 1;
		u->t0_dt_collide = t0_dt;
		u->v1.x = a_v.x;
		u->v1.y = a_v.y;
	} else {
		/* there are multiple collisions, rebound from the 1st one and add */
		/* fixme: shouldn't it be all inside? */
		if(t0_dt < u->t0_dt_collide) u->t0_dt_collide = t0_dt;
		u->v1.x += a_v.x;
		u->v1.y += a_v.y;
		u->no_collisions++;
		/*pedantic(" \\%u collisions %s (%s.)\n", u->no_collisions, SpriteToString(u), SpriteToString(v));*/
	}
	if(!v->no_collisions) {
		/* first collision */
		v->no_collisions = 1;
		v->t0_dt_collide = t0_dt;
		v->v1.x          = b_v.x;
		v->v1.y          = b_v.y;
	} else {
		/* there are multiple collisions, rebound from the 1st one and add */
		if(t0_dt < v->t0_dt_collide) v->t0_dt_collide = t0_dt;
		v->v1.x          += b_v.x;
		v->v1.y          += b_v.y;
		v->no_collisions++;
		/*pedantic(" \\%u collisions %s (%s.)\n", v->no_collisions, SpriteToString(v), SpriteToString(u));*/
	}

}

/** Pushes a from angle, amount.
 @param a		The object you want to push.
 @param angle	Standard co\:ordainates, radians, angle.
 @param impulse	tonne pixels / ms */
static void push(struct Sprite *a, const float angle, const float impulse) {
	const float deltav = a->mass ? impulse / a->mass : 1.0f; /* pixel / s */
	a->v.x += cosf(angle) * deltav;
	a->v.y += sinf(angle) * deltav;
}

/* type collisions; can not modify list of Sprites as it is in the middle of
 x/ylist or delete! */

#if 0

static void shp_shp(struct Sprite *a, struct Sprite *b, const float d0) {
	elastic_bounce(a, b, d0);
}

static void deb_deb(struct Sprite *a, struct Sprite *b, const float d0) {
	elastic_bounce(a, b, d0);
}

static void shp_deb(struct Sprite *s, struct Sprite *d, const float d0) {
	/*printf("shpdeb: collide Shp(Spr%u): Deb(Spr%u)\n", SpriteGetId(s), SpriteGetId(d));*/
	elastic_bounce(s, d, d0);
	/*d->vx += 32.0f * (2.0f * rand() / RAND_MAX - 1.0f);
	d->vy += 32.0f * (2.0f * rand() / RAND_MAX - 1.0f);*/
}

static void deb_shp(struct Sprite *d, struct Sprite *s, const float d0) {
	shp_deb(s, d, d0);
}

static void wmd_deb(struct Sprite *w, struct Sprite *d, const float d0) {
	/*pedantic("wmd_deb: %s -- %s\n", SpriteToString(w), SpriteToString(d));*/
	/* avoid inifinite destruction loop */
	if(SpriteIsDestroyed(w) || SpriteIsDestroyed(d)) return;
	push(d, atan2f(d->y - w->y, d->x - w->x), w->mass);
	SpriteRecharge(d, -SpriteGetDamage(w));
	SpriteDestroy(w);
	UNUSED(d0);
}

static void deb_wmd(struct Sprite *d, struct Sprite *w, const float d0) {
	wmd_deb(w, d, d0);
}

static void wmd_shp(struct Sprite *w, struct Sprite *s, const float d0) {
	pedantic("wmd_shp: %s -- %s\n", SpriteToString(w), SpriteToString(s));
	/* avoid inifinite destruction loop */
	if(SpriteIsDestroyed(w) || SpriteIsDestroyed(s)) return;
	push(s, atan2f(s->y - w->y, s->x - w->x), w->mass);
	SpriteRecharge(s, -SpriteGetDamage(w));
	SpriteDestroy(w);
	UNUSED(d0);
}

static void shp_wmd(struct Sprite *s, struct Sprite *w, const float d0) {
	wmd_shp(w, s, d0);
}

static void shp_eth(struct Sprite *s, struct Sprite *e, const float d0) {
	/*void (*fn)(struct Sprite *const, struct Sprite *);*/
	/*Info("Shp%u colliding with Eth%u . . . \n", ShipGetId(ship), EtherealGetId(eth));*/
	/*if((fn = EtherealGetCallback(eth))) fn(eth, s);*/
	/* while in iterate! danger! */
	if(e->sp.ethereal.callback) e->sp.ethereal.callback(e, s);
	UNUSED(d0);
}

static void eth_shp(struct Sprite *e, struct Sprite *s, const float d0) {
	shp_eth(s, e, d0);
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

#endif
