/** Copyright 2015 Neil Edelman, distributed under the terms of the GNU General
 Public License, see copying.txt.

 Sprites have an {Ortho} location and are lit by \cite{lambert1892photometrie}.
 They are drawn by the gpu in {../system/Draw} as the main foreground. Sprites
 detect collisions. The sprite system must startup by calling \see{Sprite} and
 can shutdown by calling \see{Sprite_}. To instatiate an abstract {Sprite}, one
 must call one of it's subclasses:

 * Debris: is everything that doesn't have license, but moves around on a
 linear path, can be damaged, killed, and moved. Astroids and stuff.
 * Ship: has license and is controlled by a player or an ai.
 * Wmd: cannons, bombs, and other cool stuff that has sprites and can hurt.
 * Ethereal: are in the forground but poll collision detection instead of
 interacting; they generally do something in the game. For example, gates are
 devices that can magically transport you faster than light, or powerups.

 @title		Sprite
 @author	Neil
 @version	3.4, 2017-05 generics
 @since		3.3, 2016-01
 			3.2, 2015-06 */

#include <stdarg.h> /* va_* */
#include <math.h>   /* sqrtf, atan2f, cosf, sinf */
#include <string.h> /* memset */
#include <stdio.h>	/* snprintf */
#include "../../build/Auto.h"
#include "../Print.h"
#include "../general/Orcish.h"
#include "../system/Timer.h"
#include "../system/Draw.h"
#include "../system/Key.h"
#include "Light.h"
#include "Zone.h"
#include "Game.h"
#include "Event.h"
#include "Sprite.h"

/* dependancy from Lore
extern const struct Gate gate; */

static const float epsilon = 0.0005f;

static const float minimum_mass        = 0.01f;

static const float debris_hit_per_mass      = 5.0f;
static const float deb_maximum_speed_2      = 20.0f; /* 2000? */
static const float deb_explosion_elasticity = 0.5f; /* < 1 */

static const float deg_sec_to_rad_ms  = (float)(M_PI_F / (180.0 * 1000.0));
static const float px_s_to_px_ms      = 0.001f;
static const float px_s_to_px2_ms2    = 0.000001f;
static const float deg_to_rad         = (float)(M_PI_F / 180.0);

static const float shp_ai_turn        = 0.2f;     /* rad/ms */
static const float shp_ai_too_close   = 3200.0f;  /* pixel^(1/2) */
static const float shp_ai_too_far     = 32000.0f; /* pixel^(1/2) */
static const int   shp_ai_speed       = 15;       /* pixel^2 / ms */
static const float shp_ai_turn_sloppy = 0.4f;     /* rad */
static const float shp_ai_turn_constant = 10.0f;
static const int   shp_ms_sheild_uncertainty = 50;

/* fixme: 1.415? */
static const float wmd_distance_mod         = 1.3f; /* to clear ship */



/* global variable: frame time */
static float dt_ms;



/* Define the bins that each frame will concern itself with; these are erased
 and updated every frame. See \see{SpriteUpdate}. */
#define LIST_NAME Bin
#define LIST_TYPE unsigned
#define LIST_UA_NAME Order
#include "../general/List.h" /* defines BinList, BinListNode */
static struct BinList draw_bins, update_bins;
#define SET_NAME BinList
#define SET_TYPE struct BinListNode
#include "../general/Set.h" /* defines BinListSet, BinListSetNode */
static struct BinListSet *bins;



/* Define class {Sprite}. */

struct SpriteVt;
struct Sprite {
	const struct SpriteVt *vt;
	unsigned bin;
	struct Ortho3f r, v; /* position, orientation, velocity, angular velocity */
	struct Vec2f r1, v1; /* temp position on the next frame */
	struct Rectangle4f box; /* temp bounding box */
	float t0_dt_collide; /* temp time to collide over frame time */
	unsigned no_collisions; /* temp number of collisions */
	float mass; /* t (ie Mg) */
	float bounding;
	unsigned image, normals; /* gpu texture refs */
	struct Vec2u size;
};
/** @implements <Sprite>Comparator */
static int Sprite_x_cmp(const struct Sprite *a, const struct Sprite *b) {
	return (b->r.x < a->r.x) - (a->r.x < b->r.x);
}
/** @implements <Sprite>Comparator */
static int Sprite_y_cmp(const struct Sprite *a, const struct Sprite *b) {
	return (b->r.x < a->r.x) - (a->r.x < b->r.x);
}
/** {vt} is left undefined; this is, if you will, an abstract struct (haha.)
 Initialises {this} with {AutoSprite} resource, {as}. */
static void Sprite_filler(struct Sprite *const this,
	const struct AutoSprite *const as, const unsigned bin,
	struct Ortho3f *const r, const struct Ortho3f *const v) {
	assert(this);
	assert(as);
	assert(as->image->texture);
	assert(as->normals->texture);
	assert(bin < BIN_BIN_SIZE);
	assert(r);
	this->vt = 0;
	this->bin = bin;
	Ortho3f_clip_position(r);
	Ortho3f_assign(&this->r, r);
	Ortho3f_assign(&this->v, v);
	this->r1.x = r->x;
	this->r1.y = r->y;
	this->v1.x = v->x;
	this->v1.y = v->y;
	Rectangle4f_init(&this->box);
	this->t0_dt_collide = 0.0f;
	this->no_collisions = 0;
	this->mass = 1.0f;
	this->bounding = as->image->width * 0.5f;
	this->image = as->image->texture;
	this->normals = as->normals->texture;
	this->size.x = as->image->width;
	this->size.y = as->image->height;
}
/* Awkward declarations for {List.h}. */
struct ListMigrate;
struct SpriteList;
/* Define {SpriteList} and {SpriteListNode}. */
#define LIST_NAME Sprite
#define LIST_TYPE struct Sprite
#define LIST_UA_NAME X
#define LIST_UA_COMPARATOR &Sprite_x_cmp
#define LIST_UB_NAME Y
#define LIST_UB_COMPARATOR &Sprite_y_cmp
#include "../general/List.h"
/** Every sprite has one {bin} based on their position. \see{OrthoMath.h}. */
static struct SpriteList sprites[BIN_BIN_SIZE];

/** Define {Debris} as a subclass of {Sprite}. */
struct Debris {
	struct SpriteListNode sprite;
	int hit;
	/*void (*on_kill)(void); minerals? */
};
/* Define DebrisSet. */
#define SET_NAME Debris
#define SET_TYPE struct Debris
#include "../general/Set.h"

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
/* Define ShipSet. */
#define SET_NAME Ship
#define SET_TYPE struct Ship
#include "../general/Set.h"

/** Define {Wmd} as a subclass of {Sprite}. */
struct Wmd {
	struct SpriteListNode sprite;
	const struct AutoWmdType *class;
	const struct Sprite *from;
	float mass;
	unsigned expires;
	unsigned light;
};
/* Define ShipSet. */
#define SET_NAME Wmd
#define SET_TYPE struct Wmd
#include "../general/Set.h"

/** Define {Gate} as a subclass of {Sprite}. */
struct Gate {
	struct SpriteListNode sprite;
	const struct AutoSpaceZone *to;
};
/* Define GateSet. */
#define SET_NAME Gate
#define SET_TYPE struct Gate
#include "../general/Set.h"

/* Storage. */
struct DebrisSet *debris;
struct ShipSet *ships;
struct WmdSet *wmds;
struct GateSet *gates;

/** Changes the window parameters so all of them don't point to {this} and
 deletes {this} from the sprite list.
 @implements <Sprite>Action */
static void sprite_remove(struct Sprite *const this) {
	/* fixme: window parameters */
	SpriteListRemove(sprites + this->bin, this);
}

/** @implements <Debris>Action */
static void debris_out(struct Debris *const this) {
	assert(this);
	printf("Debris at (%f, %f) in %u integrity %u.\n", this->sprite.data.r.x,
		this->sprite.data.r.y, this->sprite.data.bin, this->hit);
}
/** @implements <Debris>Action */
static void debris_delete(struct Debris *const this) {
	assert(this);
	sprite_remove(&this->sprite.data);
	DebrisSetRemove(debris, this);
	/* fixme: explode */
}
/** @implements <Debris>Action */
static void debris_death(struct Debris *const this) {
#if 0
	this->hit = 0;
	/** Spawns smaller Debris (fixme: stub.) */
	void SpriteDebris(const struct Sprite *const s) {
		struct AutoImage *small_image;
		struct Sprite *sub;
		
		if(!s || !(small_image = AutoImageSearch("AsteroidSmall.png")) || s->size <= small_image->width) return;
		
		pedantic("SpriteDebris: %s is exploding at (%.3f, %.3f).\n", SpriteToString(s), s->x, s->y);
		
		/* break into pieces -- new debris */
		sub = Sprite(SP_DEBRIS, AutoDebrisSearch("SmallAsteroid"), (float)s->x, (float)s->y, s->theta * deg_to_rad, 10.0f);
		SpriteSetVelocity(sub, deb_explosion_elasticity * s->vx, deb_explosion_elasticity * s->vy);
	}
	/** Enforces the maximum speed by breaking the debris into smaller chunks; the
	 process is not elastic, so it loses energy (speed.) */
	/*void DebrisEnforce(struct Debris *deb) {
	 float vx, vy;
	 float speed_2;
	 
	 if(!deb || !deb->sprite) return;
	 
	 SpriteGetVelocity(deb->sprite, &vx, &vy);
	 if((speed_2 = vx * vx + vy * vy) > maximum_speed_2) {
	 debug("DebrisEnforce: maximum %.3f\\,(pixels/s)^2, Deb%u is moving %.3f\\,(pixels/s)^2.\n", maximum_speed_2, DebrisGetId(deb), speed_2);
	 DebrisExplode(deb);
	 }
	 }*/	
#endif
	debris_delete(this);
}
/** @implements <Debris>Action */
static void debris_action(struct Debris *const this) {
	if(this->hit <= 0) debris_death(this);
}

/** @implements <Ship>Action */
static void ship_out(struct Ship *const this) {
	assert(this);
	printf("Ship at (%f, %f) in %u integrity %u/%u.\n", this->sprite.data.r.x,
		this->sprite.data.r.y, this->sprite.data.bin, this->hit, this->max_hit);
}
/** @implements <Ship>Action */
static void ship_delete(struct Ship *const this) {
	assert(this);
	Event_(&this->event_recharge);
	sprite_remove(&this->sprite.data);
	ShipSetRemove(ships, this);
}
/** @implements <Ship>Action */
static void ship_death(struct Ship *const this) {
	this->hit = 0;
	/* fixme: explode */
}
/** @implements <Ship>Predicate */
static void ship_action(struct Ship *const this) {
	if(this->hit <= 0) ship_death(this);
}
static void ship_action_ai(struct Ship *const this) {
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

/** @implements <Wmd>Action */
static void wmd_out(struct Wmd *const this) {
	assert(this);
	printf("Wmd at (%f, %f) in %u.\n", this->sprite.data.r.x,
		this->sprite.data.r.y, this->sprite.data.bin);
}
/** @implements <Wmd>Action */
static void wmd_delete(struct Wmd *const this) {
	assert(this);
	Light_(&this->light);
	sprite_remove(&this->sprite.data);
	WmdSetRemove(wmds, this);
}
/** @implements <Wmd>Action */
static void wmd_death(struct Wmd *const this) {
	/* fixme: explode */
	wmd_delete(this);
}
/** @implements <Wmd>Action */
static void wmd_action(struct Wmd *const this) {
	if(TimerIsTime(this->expires)) wmd_death(this);
}

/** @implements <Gate>Action */
static void gate_out(struct Gate *const this) {
	assert(this);
	printf("Gate at (%f, %f) in %u goes to %s.\n", this->sprite.data.r.x,
		this->sprite.data.r.y, this->sprite.data.bin, this->to->name);
}
/** @implements <Gate>Action */
static void gate_delete(struct Gate *const this) {
	assert(this);
	sprite_remove(&this->sprite.data);
	GateSetRemove(gates, this);
	/* fixme: explode */
}
/** @implements <Gate>Action */
static void gate_death(struct Gate *const this) {
	fprintf(stderr, "Trying to destroy gate to %s.\n", this->to->name);
}
/** @implements <Gate>Action */
static void gate_action(struct Gate *const this) {
	UNUSED(this);
}

/* define the virtual table */
static const struct SpriteVt {
	const SpriteAction out, update, delete;
} debris_vt = {
	(SpriteAction)&debris_out,
	(SpriteAction)&debris_action,
	(SpriteAction)&debris_delete
}, ship_vt = {
	(SpriteAction)&ship_out,
	(SpriteAction)&ship_action_ai,
	(SpriteAction)&ship_delete
}, human_ship_vt = {
	(SpriteAction)&ship_out,
	(SpriteAction)&ship_action,
	(SpriteAction)&ship_delete
}, wmd_vt = {
	(SpriteAction)&wmd_out,
	(SpriteAction)&wmd_action,
	(SpriteAction)&wmd_delete
}, gate_vt = {
	(SpriteAction)&gate_out,
	(SpriteAction)&gate_action,
	(SpriteAction)&gate_delete
};

/** @implements <Sprite>Action */
static void sprite_update(struct Sprite *const this) {
	this->vt->update(this);
}

/** @implements <Debris>Action */
static void debris_filler(struct Debris *const this,
	const struct AutoDebris *const class,
	struct Ortho3f *const r, const struct Ortho3f *const v) {
	unsigned bin;
	assert(this);
	assert(class);
	assert(r);
	assert(v);
	bin = location_to_bin(*(struct Vec2f *)r);
	Sprite_filler(&this->sprite.data, class->sprite, bin, r, v);
	this->sprite.data.vt = &debris_vt;
	this->sprite.data.mass = class->mass;
	this->hit = class->mass * debris_hit_per_mass;
	SpriteListPush(sprites + bin, &this->sprite);
}
/** Constructor.
 @return Success. */
int Debris(const struct AutoDebris *const class,
	struct Ortho3f *r, const struct Ortho3f *v) {
	struct Ortho3f zero = { 0.0f, 0.0f, 0.0f };
	struct Debris *d;
	if(!class) return 0;
	if(!r) r = &zero;
	if(!v) v = &zero;
	if(!(d = DebrisSetNew(debris)))
		{ fprintf(stderr, "Debris: %s.\n", DebrisSetGetError(debris));return 0;}
	debris_filler(d, class, r, v);
	return 1;
}

/** @implements <Ship>Action */
static void ship_filler(struct Ship *const this,
	const struct AutoShipClass *const class,
	struct Ortho3f *const r, const enum SpriteBehaviour behaviour) {
	unsigned bin;
	const struct SpriteVt *vt;
	/* all ships start off stationary */
	const struct Ortho3f v = { 0.0f, 0.0f, 0.0f };
	assert(this);
	assert(class);
	assert(bin < BIN_BIN_SIZE);
	assert(r);
	bin = location_to_bin(*(struct Vec2f *)r);
	Sprite_filler(&this->sprite.data, class->sprite, bin, r, &v);
	switch(behaviour) {
		case SB_HUMAN: vt = &human_ship_vt; break;
		case SB_NONE:
		case SB_STUPID:
		default: vt = &ship_vt; break;
	}
	this->sprite.data.vt = vt;
	this->class = class;
	this->ms_recharge_wmd = TimerGetGameTime();
	this->ms_recharge_hit = class->ms_recharge;
	this->event_recharge  = 0; /* full shields, not recharging */
	this->hit = this->max_hit = class->shield;
	this->max_speed2      = class->speed * class->speed * px_s_to_px2_ms2;
	/* fixme:units! should be t/s^2 -> t/ms^2 */
	this->acceleration    = class->acceleration;
	this->turn            = class->turn * deg_sec_to_rad_ms;
	/* fixme: have it explicity settable */
	this->turn_acceleration = class->turn * deg_sec_to_rad_ms * 0.01f;
	/* fixme: horizon is broken w/o a way to reset (for gate effect?) */
	this->horizon         = 0.0f;
	this->behaviour       = behaviour;
	Orcish(this->name, sizeof this->name);
	SpriteListPush(sprites + bin, &this->sprite);
}
/** Constructor.
 @return Newly created ship or null. */
struct Ship *Ship(const struct AutoShipClass *const class,
	struct Ortho3f *const r, const enum SpriteBehaviour behaviour) {
	struct Ship *ship;
	unsigned bin;
	if(!class || !r) return 0;
	if(!(ship = ShipSetNew(ships)))
		{ fprintf(stderr, "Ship: %s.\n", ShipSetGetError(ships)); return 0; }
	bin = location_to_bin(*(struct Vec2f *)r);
	ship_filler(ship, class, r, behaviour);
	/* fixme: add to temp list, sort, merge */
	SpriteListPush(&sprites[bin], &ship->sprite);
	return ship;
}

/** @implements <Wmd>Action */
static void wmd_filler(struct Wmd *const this,
	const struct AutoWmdType *const class,
	const struct Ship *const from) {
	struct Ortho3f r, v;
	float cosine, sine, lenght, one_lenght;
	unsigned bin;
	assert(this);
	assert(class);
	assert(from);
	/* Set the wmd's position as a function of the {from} position. */
	cosine = cosf(from->sprite.data.r.theta);
	sine   = sinf(from->sprite.data.r.theta);
	r.x = from->sprite.data.r.x + cosine * from->class->sprite->image->width
		* 0.5f * wmd_distance_mod;
	r.y = from->sprite.data.r.y + sine   * from->class->sprite->image->width
		* 0.5f * wmd_distance_mod;
	r.theta = from->sprite.data.r.theta;
	v.x = from->sprite.data.v.x + cosine * class->speed * px_s_to_px_ms;
	v.y = from->sprite.data.v.y + sine   * class->speed * px_s_to_px_ms;
	v.theta = 0.0f; /* \omega */
	{ struct Vec2f x; x.x = r.x, x.y = r.y; bin = location_to_bin(x); }
	Sprite_filler(&this->sprite.data, class->sprite, bin, &r, &v);
	this->sprite.data.vt = &wmd_vt;
	this->class = class;
	this->from = &from->sprite.data;
	this->mass = class->impact_mass;
	this->expires = TimerGetGameTime() + class->ms_range;
	lenght = sqrtf(class->r * class->r + class->g * class->g
		+ class->b * class->b);
	one_lenght = lenght > epsilon ? 1.0f / lenght : 1.0f;
	SpriteListPush(sprites + bin, &this->sprite);
	Light(&this->light, lenght, class->r * one_lenght, class->g * one_lenght,
		class->b * one_lenght);
}

/** @implements <Gate>Action */
static void gate_filler(struct Gate *const this,
	const struct AutoGate *const class) {
	/* all gates (start off) stationary */
	const struct Ortho3f v = { 0.0f, 0.0f, 0.0f };
	const struct AutoSprite *const gate_sprite = AutoSpriteSearch("Gate");
	struct Ortho3f r;
	unsigned bin;
	assert(this);
	assert(class);
	assert(gate_sprite);
	{ struct Vec2f x; x.x = class->x, x.y = class->y; bin = location_to_bin(x);}
	/* Set the wmd's position as a function of the {from} position. */
	r.x     = class->x;
	r.y     = class->y;
	r.theta = class->theta;
	Sprite_filler(&this->sprite.data, gate_sprite, bin, &r, &v);
	this->sprite.data.vt = &gate_vt;
	this->to = class->to;
	SpriteListPush(sprites + bin, &this->sprite);
}
/** Constructor.
 @return Success. */
int Gate(const struct AutoGate *const class) {
	struct Gate *gate;
	if(!class) return 0;
	assert(gates);
	if(!(gate = GateSetNew(gates)))
		{ fprintf(stderr, "Gate: %s.\n", GateSetGetError(gates)); return 0; }
	gate_filler(gate, class);
	return 1;
}

/** Initailises all sprite buffers.
 @return Success. */
int Sprite(void) {
	unsigned i;
	if(!(bins = BinListSet())
		|| !(debris = DebrisSet())
		|| !(ships = ShipSet())
		|| !(wmds = WmdSet())
		|| !(gates = GateSet()))
		return Sprite_(), 0;
	BinListClear(&draw_bins);
	BinListClear(&update_bins);
	for(i = 0; i < BIN_BIN_SIZE; i++) SpriteListClear(sprites + i);
	return 1;
}

/** Erases all sprite buffers. */
void Sprite_(void) {
	unsigned i;
	for(i = 0; i < BIN_BIN_SIZE; i++) SpriteListClear(sprites + i);
	BinListClear(&update_bins);
	BinListClear(&draw_bins);
	GateSet_(&gates);
	WmdSet_(&wmds);
	ShipSet_(&ships);
	DebrisSet_(&debris);
	BinListSet_(&bins);
}

/** Prints out debug info. */
void SpriteOut(struct Sprite *const this) {
	if(!this) return;
	this->vt->out(this);
}

/** Gets a read-only copy of the position. */
/*void SpriteGetOrtho(const struct Sprite *const this, const struct Ortho3f *ortho) {
	if(!this) return;
	ortho = &this->r;
}*/

/** @implements BinAction */
static void print_bin(unsigned *bin) {
	printf("(bin:%u)", *bin);
}

/** Choose which bins we will update/draw this frame.
 @order {O(n^2)} where {n} is the screen size */
static void new_bins(void) {
	static int count;
	struct Rectangle4f rect;
	struct Rectangle4i bin4, grow4;
	struct Vec2i bin2i;
	struct BinListNode *bin_node;
	BinListClear(&draw_bins), BinListClear(&update_bins), BinListSetClear(bins);
	DrawGetScreen(&rect);
	Rectangle4f_to_bin4(&rect, &bin4);
	/* the updating region extends past the drawing region */
	Rectangle4i_assign(&grow4, &bin4);
	if(grow4.x_min > 0)                 grow4.x_min--;
	if(grow4.x_max + 1 < (int)bin_size) grow4.x_max++;
	if(grow4.y_min > 0)                 grow4.y_min--;
	if(grow4.y_max + 1 < (int)bin_size) grow4.y_max++;
	/* draw in the centre */
	for(bin2i.y = bin4.y_max; bin2i.y >= bin4.y_min; bin2i.y--) {
		for(bin2i.x = bin4.x_min; bin2i.x <= bin4.y_max; bin2i.x++) {
			if(!(bin_node = BinListSetNew(bins))){ perror("new_bins"); return; }
			bin_node->data = bin2i_to_bin(bin2i);
			BinListPush(&draw_bins, bin_node);
		}
	}
	printf("draw in centre:"), BinListOrderForEach(&draw_bins, &print_bin), printf("\n");
#if 0
	/* update along the outside (fixme: ugh) */
	/*if(!(bin_node = BinListSetNew(bins))) { perror("new_bins"); return; }
	bin_node->data = rand() / (1.0f + RAND_MAX / BIN_BIN_SIZE);
	assert(bin_node->data < BIN_BIN_SIZE);
	printf("*********%u\n", bin_node->data);
	bin_node->data = 42;
	BinListPush(&update_bins, bin_node);*/
	if(!(bin_node = BinListSetNew(bins))){ perror("new_bins"); return; }
	bin_node->data = 42;
	BinListPush(&update_bins, bin_node);
	printf("Ddraw:"), BinListOrderForEach(&draw_bins, &print_bin), printf("\n");
	if(!(bin_node = BinListSetNew(bins))){ perror("new_bins"); return; }
	bin_node->data = 1024;
	BinListPush(&update_bins, bin_node);
	printf("Edraw:"), BinListOrderForEach(&draw_bins, &print_bin), printf("\n");
	if(!(bin_node = BinListSetNew(bins))){ perror("new_bins"); return; }
	bin_node->data = 1100;
	BinListPush(&update_bins, bin_node);
	printf("0draw:"), BinListOrderForEach(&draw_bins, &print_bin), printf("\n");
	if((bin2i.y = grow4.y_min) < bin4.y_min) {
		for(bin2i.x = grow4.x_min; bin2i.x <= grow4.x_max; bin2i.x++) {
			printf("1draw:"), BinListOrderForEach(&draw_bins, &print_bin), printf("\n");
			if(!(bin_node = BinListSetNew(bins))){ perror("new_bins"); return; }
			printf("2draw:"), BinListOrderForEach(&draw_bins, &print_bin), printf("\n");
			bin_node->data = bin2i_to_bin(bin2i);
			printf("fyou update %u\n", bin_node->data);
			printf("3draw:"), BinListOrderForEach(&draw_bins, &print_bin), printf("\n");
			BinListPush(&update_bins, bin_node);
			printf("4draw:"), BinListOrderForEach(&draw_bins, &print_bin), printf("\n");
		}
	}
	/*printf("5draw:"), BinListOrderForEach(&draw_bins, &print_bin), printf("\n");*/
	/*printf("5update:"), BinListOrderForEach(&update_bins, &print_bin), printf("\n");*/
#endif
#if 0
	if((bin2i.y = grow4.y_max) > bin4.y_max) {
		for(bin2i.x = grow4.x_min; bin2i.x <= grow4.x_max; bin2i.x++) {
			if(!(bin_node = BinListSetNew(bins))){ perror("new_bins"); return; }
			bin_node->data = bin2i_to_bin(bin2i);
			BinListPush(&update_bins, bin_node);
		}
	}
	if((bin2i.x = grow4.x_min) < bin4.x_min) {
		for(bin2i.y = bin4.y_min; bin2i.y <= bin4.y_max; bin2i.y++) {
			if(!(bin_node = BinListSetNew(bins))){ perror("new_bins"); return; }
			bin_node->data = bin2i_to_bin(bin2i);
			BinListPush(&update_bins, bin_node);
		}
	}
	if((bin2i.x = grow4.x_max) > bin4.x_max) {
		for(bin2i.y = bin4.y_min; bin2i.y <= bin4.y_max; bin2i.y++) {
			if(!(bin_node = BinListSetNew(bins))){ perror("new_bins"); return; }
			bin_node->data = bin2i_to_bin(bin2i);
			BinListPush(&update_bins, bin_node);
		}
	}
#endif
	if(!count) {
		printf("*** get screen: (x:%.3f, %.3f)(y:%.3f, %.3f).\n", rect.x_min, rect.x_max, rect.y_min, rect.y_max);
		printf("*** get bin: (x:%d, %d)(y:%d, %d).\n", bin4.x_min, bin4.x_max, bin4.y_min, bin4.y_max);
		printf("draw:"), BinListOrderForEach(&draw_bins, &print_bin), printf("\n");
		printf("update:"), BinListOrderForEach(&update_bins, &print_bin), printf("\n");
	}
	count++, count &= 65535;
}
/** {{act} \in { Draw }}. */
static void for_each_draw(const SpriteAction act) {
	struct BinListNode *iter;
	unsigned bin;
	assert(act);
	for(iter = BinListOrderGetFirst(&draw_bins); iter;
		iter = BinListNodeOrderGetNext(iter)) {
		bin = iter->data;
		SpriteListXForEach(sprites + bin, act);
	}
}
/** {{act} \in { Update, Draw }}. */
static void for_each_update(const SpriteAction act) {
	struct BinListNode *iter;
	unsigned bin;
	assert(act);
	for(iter = BinListOrderGetFirst(&update_bins); iter;
		iter = BinListNodeOrderGetNext(iter)) {
		bin = iter->data;
		SpriteListXForEach(sprites + bin, act);
	}
	for_each_draw(act);
}

/* Compute bounding boxes of where the sprite wants to go this frame,
 containing the projected circle along a vector {x -> x + v*dt}. This is the
 first step in collision detection. */
static void collide_extrapolate(struct Sprite *const this) {
	/* where they should be if there's no collision */
	this->r1.x = this->r.x + this->v.x * dt_ms;
	this->r1.y = this->r.y + this->v.y * dt_ms;
	/* clamp */
	if(this->r1.x > de_sitter)       this->r1.x = de_sitter;
	else if(this->r1.x < -de_sitter) this->r1.x = -de_sitter;
	if(this->r1.y > de_sitter)       this->r1.y = de_sitter;
	else if(this->r1.y < -de_sitter) this->r1.y = -de_sitter;
	/* extend the bounding box along the circle's trajectory */
	if(this->r.x <= this->r1.x) {
		this->box.x_min = this->r.x  - this->bounding - bin_half_space;
		this->box.x_max = this->r1.x + this->bounding + bin_half_space;
	} else {
		this->box.x_min = this->r1.x - this->bounding - bin_half_space;
		this->box.x_max = this->r.x  + this->bounding + bin_half_space;
	}
	if(this->r.y <= this->r1.y) {
		this->box.y_min = this->r.y  - this->bounding - bin_half_space;
		this->box.y_max = this->r1.y + this->bounding + bin_half_space;
	} else {
		this->box.y_min = this->r1.y - this->bounding - bin_half_space;
		this->box.y_max = this->r.y  + this->bounding + bin_half_space;
	}
}

/** Used after \see{collide_extrapolate} on all the sprites you want to check.
 Uses Hahn–Banach separation theorem and the ordered list of projections on the
 {x} and {y} axis to build up a list of possible colliders based on their
 bounding box of where it moved this frame. Calls \see{collide_circles} for any
 candidites. */
static void collide_boxes(struct Sprite *const this) {
	/* assume it can be in a maximum of four bins at once as it travels? */
	/*...*/
#if 0
	struct Sprite *b, *b_adj_y;
	float t0;
	if(!this) return;
	/* mark x */
	for(b = this->prev_x; b && b->x >= explore_x_min; b = b->prev_x) {
		if(this < b && this->x_min < b->x_max) b->is_selected = -1;
	}
	for(b = this->next_x; b && b->x <= explore_x_max; b = b->next_x) {
		if(this < b && this->x_max > b->x_min) b->is_selected = -1;
	}
	/* consider y and maybe add it to the list of colliders */
	for(b = this->prev_y; b && b->y >= explore_y_min; b = b_adj_y) {
		b_adj_y = b->prev_y; /* b can be deleted; make a copy */
		if(b->is_selected
		   && this->y_min < b->y_max
		   && (response = collision_matrix[b->sp_type][this->sp_type])
		   && collide_circles(this->x, this->y, this->x1, this->y1, b->x, b->y, b->x1,
							  b->y1, this->bounding + b->bounding, &t0))
			response(this, b, t0);
		/*debug("Collision %s--%s\n", SpriteToString(a), SpriteToString(b));*/
	}
	for(b = this->next_y; b && b->y <= explore_y_max; b = b_adj_y) {
		b_adj_y = b->next_y;
		if(b->is_selected
		   && this->y_max > b->y_min
		   && (response = collision_matrix[b->sp_type][this->sp_type])
		   && collide_circles(this->x, this->y, this->x1, this->y1, b->x, b->y, b->x1,
							  b->y1, this->bounding + b->bounding, &t0))
			response(this, b, t0);
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

/** Called from \see{collide_boxes}.
 @param a, b: Point {u} moves from {a} to {b}.
 @param c, d: Point {v} moves from {c} to {d}.
 @param r: Combined radius.
 @param t0_ptr: If true, sets the time of collision, normalised to the frame
 time, {[0, 1]}.
 @return If they collided. */
static int collide_circles(const struct Vec2f a, const struct Vec2f b,
	const struct Vec2f c, const struct Vec2f d, const float r, float *t0_ptr) {
	/* t \in [0,1]
	             u = b - a
	             v = d - c
	 if(v-u ~= 0) t = doesn't matter, parallel-ish
	          p(t) = a + (b-a)t
	          q(t) = c + (d-c)t
	 distance(t)   = |q(t) - p(t)|
	 distance^2(t) = (q(t) - p(t))^2
	               = ((c+vt) - (a+ut))^2
	               = ((c-a) + (v-u)t)^2
	               = (v-u)*(v-u)t^2 + 2(c-a)*(v-u)t + (c-a)*(c-a)
	             0 = 2(v-u)*(v-u)t_min + 2(c-a)*(v-u)
	         t_min = -(c-a)*(v-u)/(v-u)*(v-u)
	 this is an optimisation, if t \notin [0,1] then pick the closest; if
	 distance^2(t_min) < r^2 then we have a collision, which happened at t0,
	           r^2 = (v-u)*(v-u)t0^2 + 2(c-a)*(v-u)t0 + (c-a)*(c-a)
	            t0 = [-2(c-a)*(v-u) - sqrt((2(c-a)*(v-u))^2
	                 - 4((v-u)*(v-u))((c-a)*(c-a) - r^2))] / 2(v-u)*(v-u)
	            t0 = [-(c-a)*(v-u) - sqrt(((c-a)*(v-u))^2
	                 - ((v-u)*(v-u))((c-a)*(c-a) - r^2))] / (v-u)*(v-u) */
	const struct Vec2f
		u = { b.x - a.x, b.y - a.y }, v = { d.x - c.x, d.y - c.y },
		vu = { v.x - u.x, v.y - u.y }, ca = { c.x - a.x, c.y - a.y };
	const float
		vu_2  = vu.x * vu.x + vu.y * vu.y,
		ca_vu = ca.x * vu.x + ca.y * vu.y;
	struct Vec2f dist;
	float t, dist_2, ca_2, det;
	/* fixme: float stored in memory? can this be more performant? */
	assert(t0_ptr);
	/* time (t_min) of closest approach; we want the first contact */
	if(vu_2 < epsilon) {
		t = 0.0f;
	} else {
		t = -ca_vu / vu_2;
		if(     t < 0.0f) t = 0.0f;
		else if(t > 1.0f) t = 1.0f;
	}
	/* distance(t_min) of closest approach */
	dist.x = ca.x + vu.x * t; dist.y = ca.y + vu.y * t;
	dist_2 = dist.x * dist.x + dist.y * dist.y;
	/* not colliding */
	if(dist_2 >= r * r) return 0;
	/* collision time, t_0 */
	ca_2  = ca.x * ca.x + ca.y * ca.y;
	det   = (ca_vu * ca_vu - vu_2 * (ca_2 - r * r));
	t = (det <= 0.0f) ? 0.0f : (-ca_vu - sqrtf(det)) / vu_2;
	if(t < 0.0f) t = 0.0f; /* it can not be > 1 because dist < r^2 */
	*t0_ptr = t;
	return 1;
}


/** This is where most of the work gets done. Called every frame, O(n).
 @param frame_ms: Milliseconds since the last frame. */
void SpriteUpdate(const int frame_ms) {
	/* going to use it a lot to scale the time evolution: put in in a static */
	dt_ms = frame_ms;
	/* set up the bins according to the camera; do not move the camera after
	 this in the frame */
	new_bins();
	/* collision detection */
	/*for_each_update(&collide_extrapolate);
	for_each_update(&collide_boxes);*/
	/* sort the sprites */
	/* do some stuff */
	for_each_update(&sprite_update);
}

/** @fixme C99
 @return The first sprite in the {bin2}. */
inline const struct SpriteList *SpriteListBin(struct Vec2u bin2) {
	return sprites + bin2_to_bin(bin2);
}

/** @fixme C99 */
inline void SpriteExtractDrawing(const struct Sprite *const this,
	const struct Ortho3f **const r_ptr, unsigned *const image_ptr,
	unsigned *const normals_ptr, const struct Vec2u **const size_ptr) {
	assert(this);
	assert(r_ptr && image_ptr && normals_ptr && size_ptr);
	*r_ptr       = (struct Ortho3f *)&this->r;
	*image_ptr   = this->image;
	*normals_ptr = this->normals;
	*size_ptr    = &this->size;
}

/** Gets input to a Ship.
 @param this			Which ship to set.
 @param turning			How many {ms} was this pressed? left is positive.
 @param acceleration	How many {ms} was this pressed?
 @param dt_ms			Frame time in milliseconds. */
void ShipInput(struct Ship *const this, const int turning,
	const int acceleration) {
	if(!this) return;
	if(acceleration != 0) {
		float vx = this->sprite.data.r.x, vy = this->sprite.data.r.y;
		float a = this->acceleration * acceleration, speed2;
		vx += cosf(this->sprite.data.r.theta) * a;
		vy += sinf(this->sprite.data.r.theta) * a;
		if((speed2 = vx * vx + vy * vy) > this->max_speed2) {
			float correction = sqrtf(this->max_speed2 / speed2);
			vx *= correction;
			vy *= correction;
		}
		this->sprite.data.r.x = vx;
		this->sprite.data.r.y = vy;
	}
	if(turning) {
		this->sprite.data.v.theta += this->turn_acceleration * turning * dt_ms;
		if(this->sprite.data.v.theta < -this->turn) {
			this->sprite.data.v.theta = -this->turn;
		} else if(this->sprite.data.v.theta > this->turn) {
			this->sprite.data.v.theta = this->turn;
		}
	} else {
		const float damping = this->turn_acceleration * dt_ms;
		if(this->sprite.data.v.theta > damping) {
			this->sprite.data.v.theta -= damping;
		} else if(this->sprite.data.v.theta < -damping) {
			this->sprite.data.v.theta += damping;
		} else {
			this->sprite.data.v.theta = 0;
		}
	}
	/* fixme: this should be gone! instead have it for all sprites and get rid
	 of dt_ms */
	/*SpriteAddTheta(this, this->omega * dt_ms); isn't that what we did??? */
}

void ShipShoot(struct Ship *const this) {
	struct Wmd *wmd;
	assert(this);
	if(!TimerIsTime(this->ms_recharge_wmd)) return;
	if(!(wmd = WmdSetNew(wmds)))
		{ fprintf(stderr, "Wmd: %s.\n", WmdSetGetError(wmds)); return; }
	wmd_filler(wmd, this->class->weapon, this);
	this->ms_recharge_wmd = TimerGetGameTime()
		+ this->class->weapon->ms_recharge;
}

void ShipGetPosition(const struct Ship *this, struct Vec2f *pos) {
	if(!this || !pos) return;
	pos->x = this->sprite.data.r.x;
	pos->y = this->sprite.data.r.y;
}

void ShipSetPosition(struct Ship *const this, struct Vec2f *const pos) {
	if(!this || !pos) return;
	this->sprite.data.r.x = pos->x;
	this->sprite.data.r.y = pos->y;
	/* fixme: update bin */
}

/** @implements <Ship>Action */
static void ship_recharge(struct Ship *const this) {
	if(!this) return;
	debug("ship_recharge: %s %uGJ/%uGJ\n", this->name, this->hit,this->max_hit);
	if(this->hit >= this->max_hit) return;
	this->hit++;
	/* this checks if an Event is associated to the sprite, we momentarily don't
	 have an event, even though we are getting one soon! alternately, we could
	 call Sprite::recharge and not stick the event down there.
	 SpriteRecharge(a, 1);*/
	if(this->hit >= this->max_hit) {
		debug("ship_recharge: %s shields full %uGJ/%uGJ.\n",
			this->name, this->hit, this->max_hit);
		return;
	}
	Event(&this->event_recharge, this->ms_recharge_hit, shp_ms_sheild_uncertainty, FN_CONSUMER, &ship_recharge, this);
}
void ShipRechage(struct Ship *const this, const int recharge) {
	if(recharge + this->hit >= this->max_hit) {
		this->hit = this->max_hit; /* cap off */
	} else if(-recharge < this->hit) {
		this->hit += (unsigned)recharge; /* recharge */
		if(!this->event_recharge
		   && this->hit < this->max_hit) {
			pedantic("SpriteRecharge: %s beginning charging cycle %d/%d.\n", this->name, this->hit, this->max_hit);
			Event(&this->event_recharge, this->ms_recharge_hit, 0, FN_CONSUMER, &ship_recharge, this);
		}
	} else {
		this->hit = 0; /* killed */
	}
}

/** {hit} will be filled with {x} hit, {y} max hit. */
void ShipGetHit(const struct Ship *const this, struct Vec2u *const hit) {
	if(!hit) return;
	if(!this) {
		hit->x = hit->y = 0;
	} else {
		hit->x = this->hit, hit->y = this->max_hit;
	}
}

const struct Event *ShipGetEventRecharge(const struct Ship *const this) {
	if(!this) return 0;
	return this->event_recharge;
}








/** Accessor. */
int WmdGetDamage(const struct Wmd *const this) {
	if(!this) return 0;
	return this->class->damage;
}

/** Gets a SpaceZone that it goes to, if it exists. */
const struct AutoSpaceZone *GateGetTo(const struct Gate *const this) {
	if(!this) return 0;
	return this->to;
}

/** See \see{GateFind}.
 @implements <Gate>Predicate */
static int find_gate(struct Gate *const this, void *const vgate) {
	struct Gate *const gate = vgate;
	return this != gate;
}
/** Linear search for {gates} that go to a specific place. */
struct Gate *GateFind(struct AutoSpaceZone *const to) {
	pedantic("GateIncoming: SpaceZone to: #%p %s.\n", to, to->name);
	GateSetSetParam(gates, to);
	return GateSetShortCircuit(gates, &find_gate);
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
static void gate_travel(struct Gate *const gate, struct Ship *ship) {
	struct Vec2f diff, gate_norm;
	float proj;
	if(!gate || !ship) return;
	diff.x = ship->sprite.r.x - gate->sprite.data.r.x;
	diff.y = ship->sprite.r.y - gate->sprite.r.y;
	/* unneccesary?
	 vx = ship_vx - gate_vx;
	 vy = ship_vy - gate_vy;*/
	gate_norm_x = cosf(gate->theta);
	gate_norm_y = sinf(gate->theta);
	proj = x * gate_norm_x + y * gate_norm_y; /* proj is the new h */
	if(ship->sp.ship.horizon > 0 && proj < 0) {
		debug("gate_travel: %s crossed into the event horizon of %s.\n",
			SpriteToString(ship), SpriteToString(gate));
		if(ship == GameGetPlayer()) {
			/* trasport to zone immediately */
			Event(0, 0, 0, FN_CONSUMER, &ZoneChange, gate);
		} else {
			/* disappear */
			/* fixme: test! */
			SpriteDestroy(ship);
		}
	}/* else*/
	/* fixme: unreliable */
	ship->sp.ship.horizon = proj;
}

#endif
