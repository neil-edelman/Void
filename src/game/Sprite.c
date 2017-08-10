/** 2017 Neil Edelman, distributed under the terms of the GNU General
 Public License 3, see copying.txt, or
 \url{ https://opensource.org/licenses/GPL-3.0 }.

 Testing the bins. A {Bin} is like a hash bucket, but instead of hashing, it's
 determined by where in space you are. This allows you to do stuff like drawing
 and AI for onscreen bins and treat the faraway bins with statistical mechanics.
 Which allows you to have a lot of sprites -> ships, weapons -> more epic.

 @title		Bin
 @author	Neil
 @std		C89/90
 @version	3.4, 2017-05 generics
 @since		3.3, 2016-01
			3.2, 2015-06 */

#include <stdlib.h>	/* rand */
#include <stdio.h>  /* fprintf */
#include "../../build/Auto.h" /* for AutoImage, AutoShipClass, etc */
#include "../general/OrthoMath.h" /* for measurements and types */
#include "../general/Orcish.h"
#include "../system/Key.h" /* keys for input */
#include "../system/Draw.h" /* DrawGetCamera, DrawSetScreen */
#include "../system/Timer.h" /* for expiring */
#include "Light.h"	/* for glowing Sprites */
#include "Sprite.h"

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

#if 0
/* dependancy from Lore
 extern const struct Gate gate; */

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
#endif

/* Updated frame time, passed to {SpriteUpdate} but used in nearly
 everything. */
static float dt_ms;



/* Sprites */

/** {Sprite} virtual table. */
struct SpriteVt;
/* {Sprite} temporary on-screen values.
 struct SpriteTemp; ?? is it worth it? */
/** Collisions between two sprites, cleared at every frame. */
struct Collision;
/** Define {Sprite}. */
struct Sprite {
	const struct SpriteVt *vt; /* virtual table pointer */
	const struct AutoImage *image, *normals; /* what the sprite is */
	struct Collision *collision_set; /* temporary, \in collisions */
	unsigned bin; /* which bin is it in, set by {x_5} */
	struct Ortho3f x, v; /* where it is and where it is going */
	struct Vec2f dx, x_5; /* temporary values */
	float bounding, bounding1; /* bounding1 is temporary */
};
/** @implements <Bin>Comparator
 @fixme Actually, we can get rid of this. Unused? That was a lot of work for
 nothing. */
static int Sprite_x_cmp(const struct Sprite *a, const struct Sprite *b) {
	return (b->x_5.x < a->x_5.x) - (a->x_5.x < b->x_5.x);
}
/** @implements <Bin>Comparator */
static int Sprite_y_cmp(const struct Sprite *a, const struct Sprite *b) {
	return (b->x_5.x < a->x_5.x) - (a->x_5.x < b->x_5.x);
}
/* declare */
static void Sprite_to_string(const struct Sprite *this, char (*const a)[12]);
/* Define {BinList} and {BinListNode}. */
#define LIST_NAME Sprite
#define LIST_TYPE struct Sprite
#define LIST_UA_NAME X
#define LIST_UA_COMPARATOR &Sprite_x_cmp
#define LIST_UB_NAME Y
#define LIST_UB_COMPARATOR &Sprite_y_cmp
#define LIST_TO_STRING &Sprite_to_string
#include "../general/List.h"
/** Every sprite has one {bin} based on their position. \see{OrthoMath.h}. */
static struct SpriteList bins[BIN_BIN_FG_SIZE];
/** Fills a sprite with data. This is kind of an abstract class: it never
 appears outside of another struct.
 @param auto: An auto-sprite prototype, contained in the precompiled {Auto.h}
 resources, that specifies graphics.
 @param vt: Virtual table.
 @param x: Where should the sprite go. Leave null to place it in a uniform
 distribution across space.
 @fixme I don't like that. I don't like random velocity. */
static void Sprite_filler(struct SpriteListNode *const this,
	const struct AutoSprite *const as, const struct SpriteVt *const vt,
	const struct Ortho3f *x) {
	struct Sprite *const s = &this->data;
	assert(this && as && vt);
	s->vt      = vt;
	s->image   = as->image;
	s->normals = as->normals;
	s->collision_set = 0;
	if(x) {
		Ortho3f_assign(&s->x, x);
	} else {
		Ortho3f_filler_fg(&s->x);
	}
	s->dx.x = s->dx.y = 0.0f;
	s->x_5.x = s->x.x, s->x_5.y = s->x.y;
	Ortho3f_filler_zero(&s->v);
	s->bounding = s->bounding1 = (as->image->width >= as->image->height ?
		as->image->width : as->image->height) / 2.0f; /* fixme: Crude. */
	Vec2f_to_bin(&s->x_5, &s->bin);
	SpriteListPush(bins + s->bin, this);
}
/** Stale pointers from {Set}'s {realloc} are taken care of by this callback.
 One must set it with {*SetSetMigrate}. It's kind of important.
 @implements Migrate */
static void Sprite_migrate(const struct Migrate *const migrate) {
	size_t i;
	for(i = 0; i < BIN_BIN_FG_SIZE; i++) {
		SpriteListMigrate(bins + i, migrate);
	}
}





/* Bins. */

/* Temporary pointers a subset of {bins}. */
#define SET_NAME Bin
#define SET_TYPE struct SpriteList *
#include "../general/Set.h" /* defines BinSet, BinSetNode */
static struct BinSet *draw_bins, *update_bins, *sprite_bins;
static struct Rectangle4i grow4; /* for clipping */

/** New bins calculates which bins are at all visible and which we should
 update, (around the visible,) every frame.
 @order The screen area. */
static void new_bins(void) {
	struct Rectangle4f rect;
	struct Rectangle4i bin4/*, grow4 <- now static */;
	struct Vec2i bin2i;
	struct SpriteList **bin;
	BinSetClear(draw_bins), BinSetClear(update_bins);
	DrawGetScreen(&rect);
	Rectangle4f_to_bin4(&rect, &bin4);
	/* the updating region extends past the drawing region */
	Rectangle4i_assign(&grow4, &bin4);
	if(grow4.x_min > 0)               grow4.x_min--;
	if(grow4.x_max + 1 < BIN_FG_SIZE) grow4.x_max++;
	if(grow4.y_min > 0)               grow4.y_min--;
	if(grow4.y_max + 1 < BIN_FG_SIZE) grow4.y_max++;
	/* draw in the centre */
	for(bin2i.y = bin4.y_max; bin2i.y >= bin4.y_min; bin2i.y--) {
		for(bin2i.x = bin4.x_min; bin2i.x <= bin4.x_max; bin2i.x++) {
			if(!(bin = BinSetNew(draw_bins))) { perror("draw_bins"); return; }
			*bin = bins + bin2i_to_bin(bin2i);
		}
	}
	/* update around the outside of the screen */
	if((bin2i.y = grow4.y_min) < bin4.y_min) {
		for(bin2i.x = grow4.x_min; bin2i.x <= grow4.x_max; bin2i.x++) {
			if(!(bin = BinSetNew(update_bins))) { perror("update_bins");return;}
			*bin = bins + bin2i_to_bin(bin2i);
		}
	}
	if((bin2i.y = grow4.y_max) > bin4.y_max) {
		for(bin2i.x = grow4.x_min; bin2i.x <= grow4.x_max; bin2i.x++) {
			if(!(bin = BinSetNew(update_bins))) { perror("update_bins");return;}
			*bin = bins + bin2i_to_bin(bin2i);
		}
	}
	if((bin2i.x = grow4.x_min) < bin4.x_min) {
		for(bin2i.y = bin4.y_min; bin2i.y <= bin4.y_max; bin2i.y++) {
			if(!(bin = BinSetNew(update_bins))) { perror("update_bins");return;}
			*bin = bins + bin2i_to_bin(bin2i);
		}
	}
	if((bin2i.x = grow4.x_max) > bin4.x_max) {
		for(bin2i.y = bin4.y_min; bin2i.y <= bin4.y_max; bin2i.y++) {
			if(!(bin = BinSetNew(update_bins))) { perror("update_bins");return;}
			*bin = bins + bin2i_to_bin(bin2i);
		}
	}
}

/** Called from \see{collide}. */
static void sprite_new_bins(const struct Sprite *const this) {
	struct Rectangle4f extent;
	struct Rectangle4i bin4;
	struct Vec2i bin2i;
	struct SpriteList **bin;
	float extent_grow = this->bounding1 + SHORTCUT_COLLISION * BIN_FG_SPACE;
	assert(this && sprite_bins);
	BinSetClear(sprite_bins);
	extent.x_min = this->x_5.x - extent_grow;
	extent.x_max = this->x_5.x + extent_grow;
	extent.y_min = this->x_5.y - extent_grow;
	extent.y_max = this->x_5.y + extent_grow;
	Rectangle4f_to_bin4(&extent, &bin4);
	/* draw in the centre */
	/*printf("sprite_new_bins(%f, %f)\n", this->x.x, this->x.y);*/
	/* from \see{new_bins}, clip the sprite bins to the update bins */
	if(bin4.x_min < grow4.x_min) bin4.x_min = grow4.x_min;
	if(bin4.x_max > grow4.x_max) bin4.x_max = grow4.x_max;
	if(bin4.y_min < grow4.y_min) bin4.y_min = grow4.y_min;
	if(bin4.y_max > grow4.y_max) bin4.y_max = grow4.y_max;
	/* enumerate bins for the sprite */
	for(bin2i.y = bin4.y_max; bin2i.y >= bin4.y_min; bin2i.y--) {
		for(bin2i.x = bin4.x_min; bin2i.x <= bin4.x_max; bin2i.x++) {
			struct Vec2i bin_pos;
			/*printf("sprite bin2i(%u, %u)\n", bin2i.x, bin2i.y);*/
			if(!(bin = BinSetNew(sprite_bins))) { perror("bins"); return; }
			*bin = bins + bin2i_to_bin(bin2i);
			bin_to_Vec2i(bin2i_to_bin(bin2i), &bin_pos);
			/*fprintf(gnu_glob, "set arrow from %f,%f to %d,%d lw 0.2 "
				"lc rgb \"#CCEEEE\" front;\n",
				this->x_5.x, this->x_5.y, bin_pos.x, bin_pos.y);*/
		}
	}
}

/** @implements <SpriteList *, SpriteAction *>DiAction */
static void act_bins(struct SpriteList **const pthis, void *const pact_void) {
	struct SpriteList *const this = *pthis;
	const SpriteAction *const pact = pact_void, act = *pact;
	assert(pthis && this && act);
	SpriteListYForEach(this, act);
}
/** {{act} \in { Draw }}. */
static void for_each_draw(SpriteAction act) {
	BinSetBiForEach(draw_bins, &act_bins, &act);
}
/** {{act} \in { Update, Draw }}. */
static void for_each_update(SpriteAction act) {
	BinSetBiForEach(update_bins, &act_bins, &act);
	for_each_draw(act);
}





/* Collisions between sprites to apply later. This is a pool that sprites can
 use. */

struct Collision {
	struct Collision *next;
	struct Vec2f v;
	float t;
};
#define SET_NAME Collision
#define SET_TYPE struct Collision
#include "../general/Set.h" /* CollisionSet, CollisionSetNode */
static struct CollisionSet *collisions;
/** @implements <Sprite, Migrate>DiAction */
static void Sprite_migrate_collisions(struct Sprite *const this,
	void *const migrate_void) {
	const struct Migrate *const migrate = migrate_void;
	struct Collision *c;
	char a[12];
	assert(this && migrate);
	Sprite_to_string(this, &a);
	if(!this->collision_set) return;
	CollisionMigrate(migrate, &this->collision_set);
	/*printf(" %s: [%p, %p]->%lu: ", a,
	 migrate->begin, migrate->end, (unsigned long)migrate->delta);*/
	/*c = this->collision_set, printf("(collision (%.1f, %.1f):%.1f)", c->v.x, c->v.y, c->t);*/
	for(c = this->collision_set; c->next; c = c->next) {
		CollisionMigrate(migrate, &c->next);
		/*printf("(next (%.1f, %.1f):%.1f)", c->v.x, c->v.y, c->t);*/
	}
	/*printf(".\n");*/
	/*this->collision_set = 0;*/
}

/** @implements <Bin, Migrate>BiAction */
static void Bin_migrate_collisions(struct SpriteList **const pthis,
	void *const migrate_void) {
	struct SpriteList *const this = *pthis;
	assert(pthis && this && migrate_void);
	SpriteListYBiForEach(this, &Sprite_migrate_collisions, migrate_void);
}
/** @implements Migrate */
static void Collision_migrate(const struct Migrate *const migrate) {
	printf("Collision migrate [%p, %p]->%lu.\n", migrate->begin,
		migrate->end, (unsigned long)migrate->delta);
	BinSetBiForEach(update_bins, &Bin_migrate_collisions, (void *const)migrate);
	BinSetBiForEach(draw_bins, &Bin_migrate_collisions, (void *const)migrate);
}





/* Sprite transfers for delayed moving sprites while SpriteListForEach. */

struct Transfer {
	struct Sprite *sprite;
	unsigned to_bin;
};

#define SET_NAME Transfer
#define SET_TYPE struct Transfer
#include "../general/Set.h" /* defines BinSet, BinSetNode */
static struct TransferSet *transfers;

static void transfer_sprite(struct Transfer *const this) {
	/**/char a[12];
	assert(this && this->sprite && this->to_bin < BIN_BIN_FG_SIZE);
	/**/Sprite_to_string(this->sprite, &a);
	printf("%s#%p: Bin%u -> Bin%u.\n", a, (void *)this->sprite,
		this->sprite->bin, this->to_bin);
	SpriteListRemove(bins + this->sprite->bin, this->sprite);
	SpriteListPush(bins + this->to_bin, (struct SpriteListNode *)this->sprite);
	this->sprite->bin = this->to_bin;
}





/* Define function types. */
typedef float (*SpriteFloatAccessor)(const struct Sprite *const);
/** Define {SpriteVt}. */
struct SpriteVt {
	SpriteAction update, delete;
	SpriteToString to_string;
	SpriteFloatAccessor get_mass;
};
/* Vt is undefined until this point, and we need this as a function to pass to
 a generic {Sprite}. Define it now.
 @implements <Sprite>ToString */
static void Sprite_to_string(const struct Sprite *this, char (*const a)[12]) {
	this->vt->to_string(this, a);
}



/* Declare {ShipVt}. */
struct ShipVt;

/* Define {ShipSet} and {ShipSetNode}, a subclass of {Sprite}. */
struct Ship {
	struct SpriteListNode sprite;
	struct ShipVt *vt;
	float mass;
	float shield, ms_recharge, max_speed2, acceleration, turn;
	char name[16];
	const struct AutoWmdType *wmd;
	unsigned ms_recharge_wmd;
};
#define SET_NAME Ship
#define SET_TYPE struct Ship
#include "../general/Set.h"
static struct ShipSet *ships;
/** @implements <Ship>ToString */
static void Ship_to_string(const struct Ship *this, char (*const a)[12]) {
	sprintf(*a, "%.11s", this->name);
}
/** @implements <Ship>Action */
static void Ship_delete(struct Ship *const this) {
	char a[12];
	assert(this);
	Ship_to_string(this, &a);
	printf("Ship_delete %s#%p.\n", a, (void *)&this->sprite.data);
	SpriteListRemove(bins + this->sprite.data.bin, &this->sprite.data);
	ShipSetRemove(ships, this);
}
/** @implements <Ship>FloatAccessor */
static float Ship_get_mass(const struct Ship *const this) {
	/* Mass is used for collisions, you don't want zero-mass objects. */
	assert(this && this->mass >= minimum_mass);
	return this->mass;
}

static void shoot(struct Ship *const this);
/** @implements <Ship>Action */
static void Ship_dumb_ai(struct Ship *const this) {
	assert(this);
	this->sprite.data.v.theta = 0.0002f;
}
/** As well as {Ship_human}.
 @implements <Ship>Action */
static void Ship_human(struct Ship *const this) { UNUSED(this); }
/** Called at beginning of frame with one ship, {player}.
 @implements <Ship>Action */
static void Ship_player(struct Ship *const this) {
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
			- turn_damping_1st_order * (dt_ms - 25.0f);
	}
	if(ms_shoot) shoot(this);
}
/* Fill in the member functions of this implementation. */
static const struct SpriteVt ship_ai_vt = {
	(SpriteAction)&Ship_dumb_ai,
	(SpriteAction)&Ship_delete,
	(SpriteToString)&Ship_to_string,
	(SpriteFloatAccessor)&Ship_get_mass
};
static const struct SpriteVt ship_human_vt = {
	(SpriteAction)&Ship_human,
	(SpriteAction)&Ship_delete,
	(SpriteToString)&Ship_to_string,
	(SpriteFloatAccessor)&Ship_get_mass
};
/** Extends {Sprite_filler}. */
struct Ship *Ship(const struct AutoShipClass *const class,
	const struct Ortho3f *const x, const enum AiType ai) {
	struct Ship *ship;
	const struct SpriteVt *vt;
	assert(class && class->sprite
		&& class->sprite->image && class->sprite->normals);
	switch(ai) {
		case AI_DUMB:  vt = &ship_ai_vt;    break;
		case AI_HUMAN: vt = &ship_human_vt; break;
		default: assert(printf("Out of range.", 0));
	}
	if(!(ship = ShipSetNew(ships))) return 0;
	Sprite_filler(&ship->sprite, class->sprite, vt, x);
	ship->vt = 0;
	ship->mass = class->mass;
	ship->shield = class->shield;
	ship->ms_recharge = class->ms_recharge;
	ship->max_speed2 = class->speed * class->speed;
	ship->acceleration = class->acceleration;
	ship->turn = class->turn /* \deg/s */
		* 0.001f /* s/ms */ * M_2PI_F / 360.0f /* \rad/\deg */;
	Orcish(ship->name, sizeof ship->name);
	ship->wmd = class->weapon;
	ship->ms_recharge_wmd = 0;
	return ship;
}



/* Define {DebrisSet} and {DebrisSetNode}, a subclass of {Sprite}. */
struct Debris {
	struct SpriteListNode sprite;
	float mass;
};
#define SET_NAME Debris
#define SET_TYPE struct Debris
#include "../general/Set.h"
static struct DebrisSet *debris;
/** @implements <Debris>ToString */
static void Debris_to_string(const struct Debris *this, char (*const a)[12]) {
	sprintf(*a, "Debris%u", (unsigned)DebrisSetGetIndex(debris, this) % 100000);
}
/** @implements <Debris>Action */
static void Debris_delete(struct Debris *const this) {
	char a[12];
	assert(this);
	Debris_to_string(this, &a);
	printf("Debris_delete %s#%p.\n", a, (void *)&this->sprite.data);
	SpriteListRemove(bins + this->sprite.data.bin, &this->sprite.data);
	DebrisSetRemove(debris, this);
}
/** @implements <Debris>FloatAccessor */
static float Debris_get_mass(const struct Debris *const this) {
	assert(this && this->mass > 0.0f);
	return this->mass;
}
/** Does nothing; just debris.
 @implements <Debris>Action */
static void Debris_update(struct Debris *const this) {
	UNUSED(this);
}
/* Fill in the member functions of this implementation. */
static const struct SpriteVt debris_vt = {
	(SpriteAction)&Debris_update,
	(SpriteAction)&Debris_delete,
	(SpriteToString)&Debris_to_string,
	(SpriteFloatAccessor)&Debris_get_mass
};
/** Extends {Sprite_filler}. */
struct Debris *Debris(const struct AutoDebris *const class,
	const struct Ortho3f *const x) {
	struct Debris *d;
	assert(class && class->sprite
		&& class->sprite->image && class->sprite->normals);
	if(!(d = DebrisSetNew(debris))) return 0;
	Sprite_filler(&d->sprite, class->sprite, &debris_vt, x);
	d->mass = class->mass;
	return d;
}



/* Define {WmdSet} and {WmdSetNode}, a subclass of {Sprite}. */
struct Wmd {
	struct SpriteListNode sprite;
	const struct AutoWmdType *class;
	const struct Sprite *from;
	float mass;
	unsigned expires;
	unsigned light;
};
#define SET_NAME Wmd
#define SET_TYPE struct Wmd
#include "../general/Set.h"
static struct WmdSet *wmds;
/** @implements <Wmd>ToString */
static void Wmd_to_string(const struct Wmd *this, char (*const a)[12]) {
	sprintf(*a, "Wmd%u", (unsigned)WmdSetGetIndex(wmds, this) % 100000000);
}
/** @implements <Wmd>Action */
static void Wmd_delete(struct Wmd *const this) {
	char a[12];
	assert(this);
	Wmd_to_string(this, &a);
	printf("Wmd_delete %s#%p.\n", a, (void *)&this->sprite.data);
	Light_(&this->light);
	SpriteListRemove(bins + this->sprite.data.bin, &this->sprite.data);
	WmdSetRemove(wmds, this);
}
/** @implements <Debris>FloatAccessor */
static float Wmd_get_mass(const struct Wmd *const this) {
	assert(this);
	return 1.0f;
}
/** @implements <Wmd>Action
 @fixme Replace delete with more dramatic death. */
static void Wmd_update(struct Wmd *const this) {
	if(TimerIsTime(this->expires)) /*wmd_death*//*Wmd_delete(this)*/; /* this!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */
}
/* Fill in the member functions of this implementation. */
static const struct SpriteVt wmd_vt = {
	(SpriteAction)&Wmd_update,
	(SpriteAction)&Wmd_delete,
	(SpriteToString)&Wmd_to_string,
	(SpriteFloatAccessor)&Wmd_get_mass
};
/** Extends {Sprite_filler}. */
struct Wmd *Wmd(const struct AutoWmdType *const class,
	const struct Ship *const from) {
	struct Wmd *w;
	struct Ortho3f x;
	struct Vec2f dir;
	assert(class && class->sprite
		&& class->sprite->image && class->sprite->normals && from);
	dir.x = cosf(from->sprite.data.x.theta);
	dir.y = sinf(from->sprite.data.x.theta);
	x.x = from->sprite.data.x.x
		+ dir.x * from->sprite.data.bounding * wmd_distance_mod;
	x.y = from->sprite.data.x.y
		+ dir.y * from->sprite.data.bounding * wmd_distance_mod;
	x.theta = from->sprite.data.x.theta;
	if(!(w = WmdSetNew(wmds)))
		{ printf("Wmd: %s.\n", WmdSetGetError(wmds)); return 0; }
	Sprite_filler(&w->sprite, class->sprite, &wmd_vt, &x);
	/* Speed is in [px/s], want it [px/ms]. */
	w->sprite.data.v.x = from->sprite.data.v.x + dir.x * class->speed * 0.001f;
	w->sprite.data.v.y = from->sprite.data.v.y + dir.y * class->speed * 0.001f;
	w->from = &from->sprite.data;
	w->mass = class->impact_mass;
	w->expires = TimerGetGameTime() + class->ms_range;
	{
		const float length = sqrtf(class->r * class->r + class->g * class->g
			+ class->b * class->b);
		const float one_length = length > epsilon ? 1.0f / length : 1.0f;
		Light(&w->light, length, class->r * one_length, class->g * one_length,
			class->b * one_length);
	}
	return w;
}
/** This is used by AI and humans.
 @fixme Simplistic, shoot on frame. */
static void shoot(struct Ship *const this) {
	assert(this && this->wmd);
	if(!TimerIsTime(this->ms_recharge_wmd)) return;
	printf("Phew!\n");
	Wmd(this->wmd, this);
	this->ms_recharge_wmd = TimerGetGameTime() + this->wmd->ms_recharge;
}



/* Define {GateSet} and {GateSetNode}, a subclass of {Sprite}. */
struct Gate {
	struct SpriteListNode sprite;
	const struct AutoSpaceZone *to;
};
#define SET_NAME Gate
#define SET_TYPE struct Gate
#include "../general/Set.h"
static struct GateSet *gates;
/** @implements <Debris>ToString */
static void Gate_to_string(const struct Gate *this, char (*const a)[12]) {
	sprintf(*a, "%.7sGate", this->to->name);
}
/** @implements <Debris>Action */
static void Gate_update(struct Gate *const this) {
	UNUSED(this);
}
/** @implements <Debris>Action */
static void Gate_delete(struct Gate *const this) {
	char a[12];
	assert(this);
	Gate_to_string(this, &a);
	printf("Gate_delete %s#%p.\n", a, (void *)&this->sprite.data);
	SpriteListRemove(bins + this->sprite.data.bin, &this->sprite.data);
	GateSetRemove(gates, this);
}
/** @implements <Debris>FloatAccessor */
static float Gate_get_mass(const struct Gate *const this) {
	assert(this);
	return 1e36f; /* No moving. I don't think this is ever called. */
}
/* Fill in the member functions of this implementation. */
static const struct SpriteVt gate_vt = {
	(SpriteAction)&Gate_update,
	(SpriteAction)&Gate_delete,
	(SpriteToString)&Gate_to_string,
	(SpriteFloatAccessor)&Gate_get_mass
};
/** Extends {Sprite_filler}. */
struct Gate *Gate(const struct AutoGate *const class) {
	const struct AutoSprite *const gate_sprite = AutoSpriteSearch("Gate");
	struct Gate *g;
	struct Ortho3f x;
	assert(class && gate_sprite && gate_sprite->image && gate_sprite->normals);
	x.x     = class->x;
	x.y     = class->y;
	x.theta = class->theta;
	if(!(g = GateSetNew(gates))) return 0;
	Sprite_filler(&g->sprite, gate_sprite, &gate_vt, &x);
	g->to = class->to;
	return g;
}









/** Calculates temporary values, {dx}, {x_5}, and {bounding1}. Half-way through
 the frame, {x_5}, is used to divide the sprites into bins; {transfers} is used
 to stick sprites that have overflowed their bin, and should be dealt with
 next.
 @implements <Sprite>Action */
static void extrapolate(struct Sprite *const this) {
	unsigned bin;
	assert(this);
	/* Do whatever the sprite parent class does. */
	this->vt->update(this);
	/* Kinematics. */
	this->dx.x = this->v.x * dt_ms;
	this->dx.y = this->v.y * dt_ms;
	/* Determines into which bin it should be. */
	this->x_5.x = this->x.x + 0.5f * this->dx.x;
	this->x_5.y = this->x.y + 0.5f * this->dx.y;
	/* Expanded bounding circle that takes the velocity into account. */
	this->bounding1 = this->bounding + 0.5f * sqrtf(this->dx.x * this->dx.x
		+ this->dx.y * this->dx.y);
	/* Wandered out of the bin? Mark it as such; you don't want to move it
	 right away, because this is called in sequence. */
	Vec2f_to_bin(&this->x_5, &bin);
	if(bin != this->bin) {
		struct Transfer *t;
		if(!(t = TransferSetNew(transfers))) { fprintf(stderr,
			"Transfer error: %s.\n", TransferSetGetError(transfers)); return; }
		t->sprite = this;
		t->to_bin = bin;
	}
}

/* branch cut (-Pi,Pi]? */

/** Called from \see{elastic_bounce}. */
static void apply_bounce_later(struct Sprite *const this,
	const struct Vec2f *const v, const float t) {
	struct Collision *const c = CollisionSetNew(collisions);
	char a[12];
	assert(this && v && t >= 0.0f && t <= dt_ms);
	if(!c) { fprintf(stderr, "Collision set: %s.\n",
		CollisionSetGetError(collisions)); return; }
	Sprite_to_string(this, &a);
	c->next = this->collision_set, this->collision_set = c;
	c->v.x = v->x, c->v.y = v->y;
	c->t = t;
	/*printf("^^^ Sprite %s BOUNCE -> ", a);
	for(i = this->collision_set; i; i = i->next) {
		printf("(%.1f, %.1f):%.1f, ", i->v.x, i->v.y, i->t);
	}
	printf("DONE\n");*/
}

/** Elastic collision between circles; use this when we've determined that {u}
 collides with {v} at {t0_dt}. Called explicitly from collision handlers.
 Alters {u} and {v}. Also, it may alter (fudge) the positions a little to avoid
 interpenetration.
 @param t0_dt: The added time that the collision occurs in {ms}.
 @fixme Changing to generics and now it doesn't work properly; probably a typo
 somewhere migrating it over. */
static void elastic_bounce(struct Sprite *const a, struct Sprite *const b,
	const float t0_dt) {
	struct Vec2f ac, bc, d, a_nrm, a_tan, b_nrm, b_tan, v;
	float nrm;
	const float a_m = a->vt->get_mass(a), b_m = b->vt->get_mass(b),
		diff_m = a_m - b_m, invsum_m = 1.0f / (a_m + b_m);
	/* fixme: float stored in memory? */

	/* Extrapolate position of collision. */
	ac.x = a->x.x + a->v.x * t0_dt, ac.y = a->x.y + a->v.y * t0_dt;
	bc.x = b->x.x + b->v.x * t0_dt, bc.y = b->x.y + b->v.y * t0_dt;
	/* At point of impact. */
	d.x = bc.x - ac.x, d.y = bc.y - ac.y;
	nrm = sqrtf(d.x * d.x + d.y * d.y);
	/* Degeneracy pressure; you absolutely do not want objects to get stuck
	 orbiting each other. */
	if(nrm < a->bounding + b->bounding) {
		const float push = (a->bounding + b->bounding - nrm) * 0.5f;
		/*pedantic("elastic_bounce: \\pushing sprites %f distance apart\n", push);*/
		a->x.x -= d.x * push, a->x.y -= d.y * push;
		b->x.x += d.x * push, b->x.y += d.y * push;
	}
	/* Invert to apply to delta to get normalised components. */
	nrm = (nrm < epsilon) ? 1.0f / epsilon : 1.0f / nrm;
	d.x *= nrm; d.y *= nrm;
	/* {a}'s velocity, normal direction */
	nrm = a->v.x * d.x + a->v.y * d.y;
	a_nrm.x = nrm * d.x, a_nrm.y = nrm * d.y;
	/* {a}'s velocity, tangent direction */
	a_tan.x = a->v.x - a_nrm.x, a_tan.y = a->v.y - a_nrm.y;
	/* b's velocity, normal direction */
	nrm = b->v.x * d.x + b->v.y * d.y;
	b_nrm.x = nrm * d.x, b_nrm.y = nrm * d.y;
	/* b's velocity, tangent direction */
	b_tan.x = b->v.y - b_nrm.x, b_nrm.y = b->v.y - b_nrm.y;
	/* elastic collision */
	v.x = a_tan.x +  (a_nrm.x*diff_m + 2*b_m*b_nrm.x) * invsum_m;
	v.y = a_tan.y +  (a_nrm.y*diff_m + 2*b_m*b_nrm.y) * invsum_m;
	assert(!isnan(v.x) && !isnan(v.y));
	apply_bounce_later(a, &v, t0_dt);
	v.x = b_tan.x + (-b_nrm.x*diff_m + 2*a_m*a_nrm.x) * invsum_m;
	v.y = b_tan.y + (-b_nrm.y*diff_m + 2*a_m*a_nrm.y) * invsum_m;
	assert(!isnan(v.x) && !isnan(v.y));
	apply_bounce_later(b, &v, t0_dt);
}

/** Called from \see{sprite_sprite_collide} to give the next level of detail
 with time.
 @param t0_ptr: If true, sets the time of collision past the current frame in
 {ms}.
 @return If {u} and {v} collided. */
static int collide_circles(const struct Sprite *const a,
	const struct Sprite *const b, float *t0_ptr) {
	/* t \in [0,1]
	             u = a.dx
	             v = b.dx
	if(v-u ~= 0) t = doesn't matter, parallel-ish
	          p(t) = a0 + ut
	          q(t) = b0 + vt
	 distance(t)   = |q(t) - p(t)|
	 distance^2(t) = (q(t) - p(t))^2
	               = ((b0+vt) - (a0+ut))^2
	               = ((b0-a0) + (v-u)t)^2
	               = (v-u)*(v-u)t^2 + 2(b0-a0)*(v-u)t + (b0-a0)*(b0-a0)
	             0 = 2(v-u)*(v-u)t_min + 2(b0-a0)*(v-u)
	         t_min = -(b0-a0)*(v-u)/(v-u)*(v-u)
	 this is an optimisation, if t \notin [0,frame] then pick the closest; if
	 distance^2(t_min) < r^2 then we have a collision, which happened at t0,
	           r^2 = (v-u)*(v-u)t0^2 + 2(b0-a0)*(v-u)t0 + (b0-a0)*(b0-a0)
	            t0 = [-2(b0-a0)*(v-u) - sqrt((2(b0-a0)*(v-u))^2
	                 - 4((v-u)*(v-u))((b0-a0)*(b0-a0) - r^2))] / 2(v-u)*(v-u)
	            t0 = [-(b0-a0)*(v-u) - sqrt(((b0-a0)*(v-u))^2
	                 - ((v-u)*(v-u))((b0-a0)*(b0-a0) - r^2))] / (v-u)*(v-u) */
	/* fixme: float stored in memory? can this be more performant? with
	 doubles? eww overkill */
	struct Vec2f v, z, dist;
	float t, v2, vz, z2, dist_2, det;
	const float r = a->bounding + b->bounding;
	assert(a && b && t0_ptr);
	v.x = a->v.x - b->v.x, v.y = a->v.y - b->v.y;
	z.x = a->x.x - b->x.x, z.y = a->x.y - b->x.y;
	v2 = v.x * v.x + v.y * v.y;
	vz = v.x * z.x + v.y * z.y;
	/* Time of closest approach. */
	if(v2 < epsilon) {
		t = 0.0f;
	} else {
		t = -vz / v2;
		if(     t < 0.0f)  t = 0.0f;
		else if(t > dt_ms) t = dt_ms;
	}
	/* Distance(t_min). */
	dist.x = z.x + v.x * t, dist.y = z.y + v.y * t;
	dist_2 = dist.x * dist.x + dist.y * dist.y;
	if(dist_2 >= r * r) return 0;
	/* The first root or zero is when the collision happens. */
	z2 = z.x * z.x + z.y * z.y;
	det = (vz * vz - v2 * (z2 - r * r));
	t = (det <= 0.0f) ? 0.0f : (-vz - sqrtf(det)) / v2;
	if(t < 0.0f) t = 0.0f; /* bounded, dist < r^2 */
	*t0_ptr = t;
	return 1;
}

/** This first applies the most course-grained collision detection in pure
 two-dimensional space using {x_5} and {bounding1}. If passed, calls
 \see{collide_circles} to introduce time.
 @implements <Sprite, Sprite>BiAction */
static void sprite_sprite_collide(struct Sprite *const this,
	void *const void_that) {
	struct Sprite *const that = void_that;
	struct Vec2f diff;
	float bounding1, t0;
	char a[12], b[12];
	assert(this && that);
	/* Break symmetry -- if two objects are near, we only need to report it
	 once. */
	if(this >= that) return;
	Sprite_to_string(this, &a);
	Sprite_to_string(that, &b);
	/* Calculate the distance between; the sum of {bounding1}, viz, {bounding}
	 projected on time for easy discard. */
	diff.x = this->x_5.x - that->x_5.x, diff.y = this->x_5.y - that->x_5.y;
	bounding1 = this->bounding1 + that->bounding1;
	if(diff.x * diff.x + diff.y * diff.y >= bounding1 * bounding1) return;
	if(!collide_circles(this, that, &t0)) {
		/* Not colliding. */
		/*fprintf(gnu_glob, "set arrow from %f,%f to %f,%f nohead lw 0.5 "
			"lc rgb \"#AA44DD\" front;\n", this->x.x, this->x.y, that->x.x,
			that->x.y);*/
		return;
	}
	/*fprintf(gnu_glob, "set arrow from %f,%f to %f,%f nohead lw 0.5 "
		"lc rgb \"red\" front;\n", this->x.x, this->x.y, that->x.x, that->x.y);*/
	/* fixme: collision matrix */
	elastic_bounce(this, that, t0);
	/*printf("%.1f ms: %s, %s collide at (%.1f, %.1f)\n", t0, a, b, this->x.x, this->x.y);*/
}
/** @implements <Bin, Sprite *>BiAction */
static void bin_sprite_collide(struct SpriteList **const pthis,
	void *const target_param) {
	struct SpriteList *const this = *pthis;
	struct Sprite *const target = target_param;
	struct Vec2i b2;
	const unsigned b = (unsigned)(this - bins);
	bin_to_bin2i(b, &b2);
	/*printf("bin_collide_sprite: bin %u (%u, %u) Sprite: (%f, %f).\n", b, b2.x,
	 b2.y, target->x.x, target->x.y);*/
	SpriteListYBiForEach(this, &sprite_sprite_collide, target);
}
/** The sprites have been ordered by their {x_5}. Now we can do collisions.
 This places a list of {Collide} in the sprite.
 @implements <Sprite>Action */
static void collide(struct Sprite *const this) {
	/*SpriteBiAction biact = &collision_detection_and_response;*/
	/*printf("--Sprite bins:\n");*/
	sprite_new_bins(this);
	/*printf("--Checking:\n");*/
	BinSetBiForEach(sprite_bins, &bin_sprite_collide, this);
}

/* Branch cut to the principal branch (-Pi,Pi] for numerical stability. We
 should really use normalised {ints}, so this would not be a problem, but
 {OpenGl} doesn't like that. */
static void branch_cut_pi_pi(float *theta_ptr) {
	assert(theta_ptr);
	*theta_ptr -= M_2PI_F * floorf((*theta_ptr + M_PI_F) / M_2PI_F);
}

/** Relies on \see{extrapolate}; all pre-computation is finalised in this step
 and values are advanced. Collisions are used up and need to be cleared after.
 @implements <Sprite>Action */
static void timestep(struct Sprite *const this) {
	struct Collision *c;
	float t0 = dt_ms;
	struct Vec2f v1 = { 0.0f, 0.0f }, d;
	assert(this);
	/* The earliest time to collision and sum the collisions together. */
	for(c = this->collision_set; c; c = c->next) {
		char a[12];
		d.x = this->x.x + this->v.x * c->t;
		d.y = this->x.y + this->v.y * c->t;
		/*fprintf(gnu_glob, "set arrow from %f,%f to %f,%f lw 0.5 "
		 "lc rgb \"#EE66AA\" front;\n", d.x, d.y,
		 d.x + c->v.x * (1.0f - c->t), d.y + c->v.y * (1.0f - c->t));*/
		this->vt->to_string(this, &a);
		printf("%s collides at %.1f and ends up going (%.1f, %.1f).\n", a,
			   c->t, c->v.x * 1000.0f, c->v.y * 1000.0f, (void *)c->next);
		if(c->t < t0) t0 = c->t;
		v1.x += c->v.x, v1.y += c->v.y;
		/* fixme: stability! */
	}
	this->x.x = this->x.x + this->v.x * t0 + v1.x * (1.0f - t0);
	this->x.y = this->x.y + this->v.y * t0 + v1.y * (1.0f - t0);
	if(this->collision_set) {
		this->collision_set = 0;
		this->v.x = v1.x, this->v.y = v1.y;
	}
	/* Angular velocity. */
	this->x.theta += this->v.theta /* omega */ * dt_ms;
	branch_cut_pi_pi(&this->x.theta);
}

/** Initailises all sprite buffers.
 @return Success. */
int Sprites(void) {
	unsigned i;
	if(!(draw_bins = BinSet())
		|| !(update_bins = BinSet())
		|| !(sprite_bins = BinSet())
		|| !(transfers = TransferSet())
		|| !(collisions = CollisionSet())
		|| !(ships = ShipSet())
		|| !(debris = DebrisSet())
		|| !(wmds = WmdSet())
		|| !(gates = GateSet())) return 0;
	CollisionSetSetMigrate(collisions, &Collision_migrate);
	ShipSetSetMigrate(ships, &Sprite_migrate);
	DebrisSetSetMigrate(debris, &Sprite_migrate);
	for(i = 0; i < BIN_BIN_FG_SIZE; i++) SpriteListClear(bins + i);
	return 1;
}

/** Erases all sprite buffers. */
void Sprites_(void) {
	unsigned i;
	for(i = 0; i < BIN_BIN_FG_SIZE; i++) SpriteListClear(bins + i);
	GateSet_(&gates);
	WmdSet_(&wmds);
	DebrisSet_(&debris);
	ShipSet_(&ships);
	CollisionSet_(&collisions);
	TransferSet_(&transfers);
	BinSet_(&update_bins);
	BinSet_(&draw_bins);
}

void SpritesUpdate(const int dt_ms_passed, struct Ship *const player) {
	/* The passed parameter goes into a global; we are using it a lot. */
	dt_ms = dt_ms_passed;
	/* Centre on the sprite that was passed, if it is available. */
	/* \see{SpriteUpdate} has a fixed camera position; apply input before the
	 update so we can predict what the camera is going to do; when collisions
	 occur, this causes the camera to jerk, but it's better than always
	 lagging? */
	if(player) {
		DrawSetCamera((struct Vec2f *)&player->sprite.data.x);
		Ship_player(player);
	}
	/* Each frame, calculate the bins that are visible and stuff. */
	new_bins();
	/* Calculate the future positions and transfer the sprites that have
	 escaped their bin. */
	for_each_update(&extrapolate);
	TransferSetForEach(transfers, &transfer_sprite);
	TransferSetClear(transfers);
	/* fixme: place things in to drawing area. */
	/* Collision relies on the values calculated in \see{extrapolate}. */
	/*for_each_update(&collide);*/
	/* Final time-step where new values are calculated. Takes data from
	 \see{extrapolate} and \see{collide}. */
	for_each_update(&timestep);
	CollisionSetClear(collisions);
}

/** @implements <Bin*, InfoOutput*>DiAction */
static void draw_bin_literally(struct SpriteList **const pthis, void *const pout_void) {
	struct SpriteList *const this = *pthis;
	const InfoOutput *const pout = pout_void, out = *pout;
	const struct AutoImage *hair = AutoImageSearch("Hair.png");
	struct Vec2i bin_in_space;
	struct Vec2f bin_float;
	assert(pthis && this && out && hair);
	bin_to_Vec2i((unsigned)(this - bins), &bin_in_space);
	bin_float.x = bin_in_space.x, bin_float.y = bin_in_space.y;
	out(&bin_float, hair);
}
/** Must call \see{SpriteUpdate} before this, because it updates everything.
 Use when the Info GPU shader is loaded. */
void SpritesDrawInfo(InfoOutput draw) {
	/*const struct Vec2f a = { 10.0f, 10.0f };*/
	/*const struct AutoImage *hair = AutoImageSearch("Hair.png");*/
	assert(draw);
	BinSetBiForEach(draw_bins, &draw_bin_literally, &draw);
	/*draw(&a, hair);*/
}

/** @implements <Sprite, LambertOutput*>DiAction */
static void draw_sprite(struct Sprite *const this, void *const pout_void) {
	const LambertOutput *const pout = pout_void, out = *pout;
	assert(this && out);
	out(&this->x, this->image, this->normals);
}
/** @implements <Bin*, LambertOutput*>DiAction */
static void draw_bin(struct SpriteList **const pthis, void *const pout_void) {
	struct SpriteList *const this = *pthis;
	assert(pthis && this);
	SpriteListYBiForEach(this, &draw_sprite, pout_void);
}
/** Must call \see{SpriteUpdate} before this, because it updates everything.
 Use when the Lambert GPU shader is loaded. */
void SpritesDrawLambert(LambertOutput draw) {
	BinSetBiForEach(draw_bins, &draw_bin, &draw);
}

/* This is a debug thing. */

/* This is for communication with {sprite_data}, {sprite_arrow}, and
 {sprite_velocity} for outputting a gnu plot. */
struct OutputData {
	FILE *fp;
	unsigned i, n;
};
/** @implements <Sprite, OutputData>DiAction */
static void sprite_count(struct Sprite *this, void *const void_out) {
	struct OutputData *const out = void_out;
	UNUSED(this);
	out->n++;
}
/** @implements <Sprite, OutputData>DiAction */
static void print_sprite_data(struct Sprite *this, void *const void_out) {
	struct OutputData *const out = void_out;
	fprintf(out->fp, "%f\t%f\t%f\t%f\t%f\t%f\t%f\n", this->x.x, this->x.y,
			this->bounding, (double)out->i++ / out->n, this->x_5.x, this->x_5.y,
			this->bounding1);
}
/** @implements <Sprite, OutputData>DiAction */
static void print_sprite_velocity(struct Sprite *this, void *const void_out) {
	struct OutputData *const out = void_out;
	fprintf(out->fp, "set arrow from %f,%f to %f,%f lw 1 lc rgb \"blue\" "
		"front;\n", this->x.x, this->x.y,
		this->x.x + this->v.x * dt_ms * 1000.0f,
		this->x.y + this->v.y * dt_ms * 1000.0f);
}
/** For communication with {gnu_draw_bins}. */
struct ColourData {
	FILE *fp;
	const char *colour;
	unsigned object;
};
/** Draws squares for highlighting bins.
 @implements <Bin, ColourData>DiAction */
static void gnu_shade_bins(struct SpriteList **this, void *const void_col) {
	struct ColourData *const col = void_col;
	const unsigned bin = (unsigned)(*this - bins);
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
			SpriteListYBiForEach(bins + i, &sprite_count, &out);
		for(i = 0; i < BIN_BIN_FG_SIZE; i++)
			SpriteListYBiForEach(bins + i, &print_sprite_data, &out);
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
		BinSetBiForEach(draw_bins, &gnu_shade_bins, &col);
		col.fp = gnu, col.colour = "#D3D3D3";
		BinSetBiForEach(update_bins, &gnu_shade_bins, &col);
		/* draw arrows from each of the sprites to their bins */
		out.fp = gnu, out.i = 0;
		for(i = 0; i < BIN_BIN_FG_SIZE; i++)
			SpriteListYBiForEach(bins + i, &print_sprite_velocity, &out);
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
























#if 0

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
	char a[12];
	assert(this);
	Sprite_to_string(this, &a);
	printf("Sprite_delete %s#%p.\n", a, (void *)this);
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
	char a[12];
	assert(this);
	Debris_to_string(this, &a);
	printf("Debris_delete %s#%p.\n", a, (void *)&this->sprite.data);
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
	char a[12];
	assert(this);
	Ship_to_string(this, &a);
	printf("Ship_delete %s#%p.\n", a, (void *)&this->sprite.data);
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


/** @implements <Gate>Action */
static void gate_out(struct Gate *const this) {
	assert(this);
	printf("Gate at (%f, %f) in %u goes to %s.\n", this->sprite.data.r.x,
		this->sprite.data.r.y, this->sprite.data.bin, this->to->name);
}
/** @implements <Gate>Action */
static void gate_delete(struct Gate *const this) {
	char a[12];
	assert(this);
	Gate_to_string(this, &a);
	printf("Gate_delete %s#%p.\n", a, (void *)&this->sprite.data);
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
	bin = wtfaaaaailocation_to_bin(*(struct Vec2f *)r);
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

#endif
