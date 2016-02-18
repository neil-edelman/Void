/* Copyright 2015 Neil Edelman, distributed under the terms of the GNU General
 Public License, see copying.txt */

/** Sprites have a (world) position, a rotation, and a bitmap. They are sorted
 by bitmap and drawn by the gpu in ../system/Draw based on the lighting. These
 are static; they have sprites_capacity maximum sprites. Sprites detect
 collisions. Sprites can be part of ships, asteroids, all sorts of gameplay
 elements, but this doesn't know that. However, background stuff is not
 sprites; ie, not collision detected, not lit. There are several types.
 <p>
 SP_DEBRIS: is everything that doesn't have license, but moves around on a
 linear path, can be damaged, killed, and moved. Astroids and stuff.
 <p>
 SP_SHIP: has license and is controlled by a player or an ai.
 <p>
 SP_WMD: cannons, bombs, and other cool stuff that has sprites and can hurt.
 <p>
 SP_ETHEREAL: are in the forground but poll collision detection instead of
 interacting; they do something in the game. For example, gates are devices
 that can magically transport you faster than light, or powerups.
 <p>
 Sprites can change address. If you want to hold an address, use
 Sprite::setUpdate() (only one address is updated.)
 @author	Neil
 @version	3.3, 2016-01
 @since		3.2, 2015-06 */

#include <stdarg.h> /* va_* */
#include <math.h>   /* sqrtf, atan2f, cosf, sinf */
#include <string.h> /* memset */
#include <stdio.h>	/* snprintf */
#include "../../bin/Auto.h"
#include "../Print.h"
#include "../general/Sorting.h"
#include "../system/Timer.h"
#include "../system/Draw.h"
#include "../system/Key.h"
#include "Light.h"
#include "Zone.h"
#include "Game.h"
#include "Event.h"
#include "Sprite.h"

/* M_PI is a widely accepted gnu standard, not C>=99 -- who knew? */
#ifndef M_PI
#define M_PI 3.141592653589793238462643383279502884197169399375105820974944
#endif
#ifndef M_2PI
#define M_2PI 6.283185307179586476925286766559005768394338798750211641949889
#endif

/* from Lore */
extern const struct Gate gate;

/* hmm, 256 is a lot of pixel space for the front layer, should be enough?
 the larger you set this, the farther it has to go to determine whether there's
 a collision, for every sprite! however, this means that the maximum value for
 a sprite is 256x256 to be guaranteed to be without clipping and collision
 artifacts */
static const int half_max_size = 128;
static const float epsilon = 0.0005f;
extern const float de_sitter;

static const float minimum_mass        = 0.01f;
static const float small_asteroid_mass = 10.0f;

static const float deb_hit_per_mass         = 5.0f;
static const float deb_maximum_speed_2      = 20.0f; /* 2000? */
static const float deb_explosion_elasticity = 0.5f; /* < 1 */

static const float deg_sec_to_rad_ms  = (float)(M_PI / (180.0 * 1000.0));
static const float px_s_to_px_ms      = 0.001f;
static const float px_s_to_px2_ms2    = 0.000001f;
static const float deg_to_rad         = (float)(M_PI / 180.0);

static const float shp_ai_turn        = 0.2f;     /* rad/ms */
static const float shp_ai_too_close   = 3200.0f;  /* pixel^(1/2) */
static const float shp_ai_too_far     = 32000.0f; /* pixel^(1/2) */
static const int   shp_ai_speed       = 15;       /* pixel^2 / ms */
static const float shp_ai_turn_sloppy = 0.4f;     /* rad */
static const float shp_ai_turn_constant = 10.0f;
static const int   shp_ms_sheild_uncertainty = 50;

/* fixme: 1.415? */
static const float wmd_distance_mod         = 1.3f; /* to clear ship */

static struct Sprite {
	float    x,  y;
	float    x1, y1;	/* temp; the spot where you want to go */
	float    theta, omega;
	float    vx,  vy;
	float    vx1, vy1;	/* temp; the velocity after colliding */
	float    bounding;
	float    mass;      /* t (ie Mg) */
	unsigned size;		/* the (x, y) size; they are the same */
	int      texture;	/* gpu texture */
	/* if we store a Sprite, and the Sprite changes addresses, this will
	 automatically change the pointer; should be passed a null on setNotify */
	struct Sprite **notify;

	enum SpType  sp_type;
	union {
		struct {
			int           hit;
			/*void          (*on_kill)(void); minerals? */
		} debris;
		struct {
			const struct AutoShipClass *class;
			unsigned       ms_recharge_wmd; /* ms */
			unsigned       ms_recharge_hit; /* ms */
			struct Event   *event_recharge;
			/*unsigned       ms_recharge_event;*/ /* ms */
			int            hit, max_hit; /* GJ */
			float          max_speed2;
			float          acceleration;
			float          turn; /* deg/s -> rad/ms */
			float          turn_acceleration;
			float          horizon;
			enum Behaviour behaviour;
		} ship;
		struct {
			struct Sprite  *from;
			struct AutoWmdType *wmd_type;
			unsigned       expires;
			int            light;
		} wmd;
		struct {
			void       (*callback)(struct Sprite *const, struct Sprite *const);
			const struct AutoSpaceZone *to;
			int        is_picked_up;
		} ethereal;
	} sp;

	int           is_selected;   /* temp; per object per frame */
	int           no_collisions; /* temp; per frame */
	float         x_min, x_max, y_min, y_max; /* temp; bounding box */
	float         t0_dt_collide; /* temp; time to collide over frame time */

	/* sort by axes in doubly-linked list */
	struct Sprite *prev_x, *next_x, *prev_y, *next_y;
} sprites[8192], *first_x, *first_y, *first_x_window, *first_y_window, *window_iterator, *iterator = sprites;
static const int sprites_capacity = sizeof(sprites) / sizeof(struct Sprite);
static int       sprites_size;

static int sprites_considered, sprites_onscreen; /* for stats */

/* private prototypes */

/* branch cut (-Pi,Pi] */
static void branch_cut_pi_pi(float *theta_ptr);
static struct Sprite *iterate(void);
/*static void sort(void);*/
static void sort_notify(struct Sprite *s);
static void collide(struct Sprite *s);
static int collide_circles(const float a_x, const float a_y,
						   const float b_x, const float b_y,
						   const float c_x, const float c_y,
						   const float d_x, const float d_y,
						   const float r, float *t0_ptr);
static void elastic_bounce(struct Sprite *a, struct Sprite *b, const float t0);
static void push(struct Sprite *a, const float angle, const float impulse);
static int compare_x(const struct Sprite *a, const struct Sprite *b);
static int compare_y(const struct Sprite *a, const struct Sprite *b);
static struct Sprite **address_prev_x(struct Sprite *const a);
static struct Sprite **address_next_x(struct Sprite *const a);
static struct Sprite **address_prev_y(struct Sprite *const a);
static struct Sprite **address_next_y(struct Sprite *const a);
static void shp_shp(struct Sprite *, struct Sprite *, const float);
static void deb_deb(struct Sprite *, struct Sprite *, const float);
static void shp_deb(struct Sprite *, struct Sprite *, const float);
static void deb_shp(struct Sprite *, struct Sprite *, const float);
static void wmd_deb(struct Sprite *, struct Sprite *, const float);
static void deb_wmd(struct Sprite *, struct Sprite *, const float);
static void wmd_shp(struct Sprite *, struct Sprite *, const float);
static void shp_wmd(struct Sprite *, struct Sprite *, const float);
static void shp_eth(struct Sprite *, struct Sprite *, const float);
static void eth_shp(struct Sprite *, struct Sprite *, const float);
static char *decode_sprite_type(const enum SpType sptype);
static void gate_travel(struct Sprite *const gate, struct Sprite *ship);
static void do_ai(struct Sprite *const s, const int dt_ms);
static void sprite_poll(void);
/*static*/ void ship_recharge(struct Sprite *const a);

/* fixme: this assumes SP_DEBRIS = 0, ..., SP_ETHEREAL = 3 */
static void (*const collision_matrix[4][4])(struct Sprite *, struct Sprite *, const float) = {
	{ &deb_deb, &shp_deb, &wmd_deb, 0 },
	{ &deb_shp, &shp_shp, &wmd_shp, &eth_shp },
	{ &deb_wmd, &shp_wmd, 0,        0 },
	{ 0,        &shp_eth, 0,        0 }
};
static const int collision_matrix_size = sizeof(collision_matrix[0]) / sizeof(void *);

/* public */

/** Get a new sprite from the pool of unused.
 Sprite(SP_DEBRIS, const struct Image *image, const int x, y, const float theta,
  const unsigned mass);
 Sprite(SP_SHIP, const int x, y, const float theta, const struct ShipClass *const class,
  const enum Behaviour behaviour, const struct Ship **notify);
 Sprite(SP_WMD, struct Sprite *const from, struct WmdType *const wmd_type);
 Sprite(SP_ETHEREAL, const struct Image *image, const int x, y, const float theta);
 <p>
 FIXME: const float x, y!
 @param sp_type		Type of sprite.
 @param image		(SP_DEBRIS|SP_ETHEREAL) (const struct Image *) Image.
 @param x, y, theta	(SP_DEBRIS|SP_SHIP|SP_ETHEREAL) Orientation.
 @param mass		(SP_DEBRIS) (const int) Mass.
 @param class		(SP_SHIP) (const struct ShipClass *const)
 @param behaviour	(SP_SHIP) (const enum Behaviour)
 @param notify		(SP_SHIP) (const struct Ship **) For other reference; can
					be, and usally is, 0.
 @param from		(SP_WMD) (struct Sprite *const) Which Ship?
 @param wmd_type	(SP_WMD) (struct WmdType *const) Which WmdType?
 @return			Created Sprite or null.
 @fixme				SpType contained in TypeOfObject instead of Image? work on
					this a lot more. */
struct Sprite *Sprite(const enum SpType sp_type, ...) {
	va_list args;
	struct AutoImage *image;
	struct AutoShipClass *class;
	struct AutoWmdType *wtype;
	struct Sprite *s, *from;
	float lenght, one_lenght, cs, sn;

	if(sprites_size >= sprites_capacity) {
		Warn("Sprite: %s couldn't be created; reached maximum of %u.\n", decode_sprite_type(sp_type), sprites_capacity);
		return 0;
	}

	s = &sprites[sprites_size++];

	s->x = s->x1 = 0.0f;
	s->y = s->y1 = 0.0f;
	s->theta  = 0.0f;
	s->omega  = 0.0f;
	s->vx = s->vy = s->vy1 = s->vy1 = 0.0f;
	s->mass   = 1.0f;
	s->notify = 0;

	/* polymorphism */
	s->sp_type = sp_type;
	va_start(args, sp_type);
	switch(sp_type) {
		case SP_DEBRIS:
			image            = va_arg(args, struct AutoImage *const);
			s->x = s->x1     = va_arg(args, const int);
			s->y = s->y1     = va_arg(args, const int);
			s->theta         = va_arg(args, const double);
			s->mass          = va_arg(args, const double);
			s->sp.debris.hit = (int)(s->mass * deb_hit_per_mass);
			break;
		case SP_SHIP:
			s->x = s->x1     = va_arg(args, const int);
			s->y = s->y1     = va_arg(args, const int);
			s->theta         = va_arg(args, const double);
			s->sp.ship.class = class = va_arg(args, struct AutoShipClass *const);
			image                      = (struct AutoImage *)class->image; /*fixme*/
			s->mass                    = class->mass;
			s->sp.ship.ms_recharge_wmd = TimerGetGameTime();
			s->sp.ship.ms_recharge_hit = class->ms_recharge;
			s->sp.ship.event_recharge  = 0; /* full shields, not recharging */
			s->sp.ship.hit = s->sp.ship.max_hit = class->shield;
			s->sp.ship.max_speed2      = class->speed * class->speed * px_s_to_px2_ms2;
			/* fixme:units! should be t/s^2 -> t/ms^2 */
			s->sp.ship.acceleration    = class->acceleration;
			s->sp.ship.turn            = class->turn * deg_sec_to_rad_ms;
			/* fixme: have it explicity settable */
			s->sp.ship.turn_acceleration = class->turn * deg_sec_to_rad_ms * 0.01f /*<-fixme*/;
			s->sp.ship.horizon         = 0.0f; /* fixme: horizon is broken w/o a way to reset */
			s->sp.ship.behaviour       = va_arg(args, const enum Behaviour);
			break;
		case SP_WMD:
			/* fixme: 'from' could change! tie it with, I don't know, ship[]
			 . . . not used now */
			s->sp.wmd.from     = from  = va_arg(args, struct Sprite *const);
			s->sp.wmd.wmd_type = wtype = va_arg(args, struct AutoWmdType *const);
			/* fixme: both those should be non-null! */
			s->mass            = wtype->impact_mass;
			image              = (struct AutoImage *)wtype->image;
			s->sp.wmd.expires  = TimerGetGameTime() + wtype->ms_range;
			lenght = sqrtf(wtype->r*wtype->r + wtype->g*wtype->g + wtype->b*wtype->b);
			one_lenght = lenght > epsilon ? 1.0f / lenght : 1.0;
			Light(&s->sp.wmd.light, lenght, wtype->r*one_lenght, wtype->g*one_lenght, wtype->b*one_lenght);
			/* set the wmd's position as a function of the ship's position */
			cs = cosf(s->sp.wmd.from->theta);
			sn = sinf(s->sp.wmd.from->theta);
			s->x = s->x1 = s->sp.wmd.from->x + cs*from->bounding*wmd_distance_mod;
			s->y = s->y1 = s->sp.wmd.from->y + sn*from->bounding*wmd_distance_mod;
			s->theta = s->sp.wmd.from->theta;
			s->vx = s->vx1 = s->sp.wmd.from->vx + cs*wtype->speed*px_s_to_px_ms;
			s->vy = s->vy1 = s->sp.wmd.from->vy + sn*wtype->speed*px_s_to_px_ms;
			break;
		case SP_ETHEREAL:
			image             = va_arg(args, struct AutoImage *);
			s->x = s->x1      = va_arg(args, const int);
			s->y = s->y1      = va_arg(args, const int);
			s->theta          = va_arg(args, const double);
			/* one has to set these up in a different fn */
			s->sp.ethereal.callback      = 0;
			s->sp.ethereal.to            = 0;
			s->sp.ethereal.is_picked_up  = 0;
			break;
		default:
			Warn("Sprite: bad type outside of enumeration %d.\n", sp_type);
			sprites_size--;
			return 0;
	}
	va_end(args);

	/* could be null was passed, eg, Sprite(ImageSearch("not there")); check!
	this is after va_end because most things have resources that define image */
	if(!image) {
		Warn("Sprite: couldn't create %s because image is null.\n", SpriteToString(s));
		sprites_size--;
		return 0;
	}

	/* ensure pricipal branch */
	branch_cut_pi_pi(&s->theta);
	if(s->x < -de_sitter || s->x > de_sitter
	   || s->y < -de_sitter || s->y > de_sitter) {
		Warn("Sprite: %s is beyond de Sitter universe, zeroed.\n", SpriteToString(s));
		s->x = s->y = 0.0f;
	}
	/* fixme: have a more sutble way; ie, examine the image? */
	s->bounding = image->width * 0.5f;
	s->size     = image->width;
	s->texture  = image->texture;

	s->is_selected   = 0;
	s->no_collisions = 0;

	/* enforce minimum mass */
	if(s->mass < minimum_mass) {
		Warn("Sprite: %s set to minimum mass %.2f.\n", SpriteToString(s), minimum_mass);
		s->mass = minimum_mass;
	}

	/* stick sprite onto the head of the lists and then sort it */
	s->prev_x        = 0;
	s->next_x        = first_x;
	s->prev_y        = 0;
	s->next_y        = first_y;
	if(first_x) first_x->prev_x = s;
	if(first_y) first_y->prev_y = s;
	first_x = first_y = s;
	sort_notify(s);

	Pedantic("Sprite: created %s %u.\n", SpriteToString(s), SpriteGetHit(s));

	/* FIXME! */
	KeyRegister('s',  &sprite_poll);

	return s;
}

/** This calls Sprite and further sets it up as a Gate from gate. */
struct Sprite *SpriteGate(const struct AutoGate *gate) {
	struct Sprite *s;

	if(!gate) return 0;

	if(!(s = Sprite(SP_ETHEREAL, AutoImageSearch("Gate.png"), gate->x, gate->y, gate->theta * deg_to_rad))) return 0;
	s->sp.ethereal.callback = &gate_travel;
	s->sp.ethereal.to       = gate->to;
	Pedantic("Sprite(Gate): created from Sprite, %s.\n", SpriteToString(s));

	return s;
}

/** Erase a sprite from the pool (array of static sprites.)
 @param sprite_ptr	A pointer to the sprite; gets set null on success. */
void Sprite_(struct Sprite **sprite_ptr) {
	struct Sprite *sprite, *replace;
	int index;
	unsigned characters;
	char buffer[128];

	if(!sprite_ptr || !(sprite = *sprite_ptr)) return;
	index = sprite - sprites;
	if(index < 0 || index >= sprites_size) {
		Warn("~Sprite: %s, %u not in range %u.\n", SpriteToString(sprite), index + 1, sprites_size);
		return;
	}

	/* store the string for debug info */
	characters = snprintf(buffer, sizeof buffer, "%s", SpriteToString(sprite));

	/* update notify */
	if(sprite->notify) *sprite->notify = 0;

	/* gid rid of the resources associated with each type of sprite */
	switch(sprite->sp_type) {
		case SP_SHIP:
			Pedantic("~Sprite: first deleting Event from %s.\n", SpriteToString(sprite));
			Event_(&sprite->sp.ship.event_recharge);
			break;
		case SP_WMD:
			Pedantic("~Sprite: first deleting Light from %s.\n", SpriteToString(sprite));
			Light_(&sprite->sp.wmd.light);
			break;
		case SP_DEBRIS:
		case SP_ETHEREAL:
			break;
	}

	/* deal with deleting it while iterating */
	if(sprite <= iterator) iterator--;

	/* take it out of the lists */
	if(first_x_window == sprite) first_x_window = sprite->next_x;
	if(first_x        == sprite) first_x        = sprite->next_x;
	if(first_y_window == sprite) first_y_window = sprite->next_y;
	if(first_y        == sprite) first_y        = sprite->next_y;
	if(sprite->prev_x) sprite->prev_x->next_x = sprite->next_x;
	else               first_x                = sprite->next_x;
	if(sprite->next_x) sprite->next_x->prev_x = sprite->prev_x;
	if(sprite->prev_y) sprite->prev_y->next_y = sprite->next_y;
	else               first_y                = sprite->next_y;
	if(sprite->next_y) sprite->next_y->prev_y = sprite->prev_y;

	/* move the terminal sprite to replace this one */
	if(index < --sprites_size) {

		replace = &sprites[sprites_size];
		memcpy(sprite, replace, sizeof(struct Sprite));

		/* update the neighbours */
		if(sprite->prev_x) sprite->prev_x->next_x = sprite;
		else               first_x                = sprite;
		if(sprite->next_x) sprite->next_x->prev_x = sprite;
		if(sprite->prev_y) sprite->prev_y->next_y = sprite;
		else               first_y                = sprite;
		if(sprite->next_y) sprite->next_y->prev_y = sprite;

		/* may need to update pointers to list; iterator doesn't use
		 linked-list, rather, uses array; dealt with above */
		if(replace == first_x)        first_x        = sprite;
		if(replace == first_x_window) first_x_window = sprite;
		if(replace == first_y)        first_y        = sprite;
		if(replace == first_y_window) first_y_window = sprite;
		if(replace == window_iterator)window_iterator= sprite;

		/* update notify */
		if(sprite->notify) *sprite->notify = sprite;

		/* move the resouces associated from replace to sprite */
		switch(sprite->sp_type) {
			case SP_SHIP:
				Debug("!\t~Sprite: %s replacing Event assocated %s.\n", SpriteToString(sprite), EventToString(sprite->sp.ship.event_recharge));
				EventReplaceArguments(sprite->sp.ship.event_recharge, sprite);
				break;
			case SP_WMD:
				LightSetNotify(&sprite->sp.wmd.light);
				break;
			default:
				break;
		}

		if(characters < sizeof buffer) snprintf(buffer + characters, sizeof buffer - characters, "; replaced by %s", SpriteToString(sprite));
	}

	Pedantic("~Sprite: erase %s.\n", buffer);
	*sprite_ptr = sprite = 0;

}

/** @return		The global variable sprites_considered, which is updated every
				frame with the marked Sprites. */
int SpriteGetConsidered(void) { return sprites_considered; }

/** @return		The global variable sprites_onscreen, equal-to or smaller then
				considered. */
int SpriteGetOnscreen(void)   { return sprites_onscreen; }

/** Allows you to get a single sprite if you know it's pointer.
 @param sprite	The sprite (this one can not be valid, in which case the others
 are not modified.)
 @param x_ptr	x
 @param y_ptr	y
 @param t_ptr	\theta */
void SpriteGetPosition(const struct Sprite *const sprite, float *x_ptr, float *const y_ptr) {
	if(!sprite) return;
	*x_ptr       = sprite->x;
	*y_ptr       = sprite->y;
}

/** Sets the orientation with respect to the screen, pixels and (0, 0) is at
 the centre.
 @param sprite	Which sprite to set.
 @param x		x
 @param y		y */
void SpriteSetPosition(struct Sprite *const sprite, const float x, const float y) {
	if(!sprite) return;
	sprite->x     = x;
	sprite->y     = y;
	inotify((void **)&first_x, (void *)sprite, (int (*)(const void *, const void *))&compare_x, (void **(*)(void *const))&address_prev_x, (void **(*)(void *const))&address_next_x);
	inotify((void **)&first_y, (void *)sprite, (int (*)(const void *, const void *))&compare_y, (void **(*)(void *const))&address_prev_y, (void **(*)(void *const))&address_next_y);
}

float SpriteGetTheta(const struct Sprite *const sprite) {
	if(!sprite) return 0.0f;
	return sprite->theta;
}

void SpriteSetTheta(struct Sprite *const sprite, const float theta) {
	if(!sprite) return;
	sprite->theta = theta;
	branch_cut_pi_pi(&sprite->theta);
}

void SpriteAddTheta(struct Sprite *sprite, const float theta) {
	if(!sprite) return;
	sprite->theta += theta;
	branch_cut_pi_pi(&sprite->theta);
}

void SpriteGetVelocity(const struct Sprite *const sprite, float *vx_ptr, float *vy_ptr) {
	if(!sprite) return;
	*vx_ptr      = sprite->vx;
	*vy_ptr      = sprite->vy;
}

void SpriteSetVelocity(struct Sprite *const sprite, const float vx, const float vy) {
	if(!sprite) return;
	sprite->vx = vx;
	sprite->vy = vy;
}

float SpriteGetOmega(const struct Sprite *const sprite) {
	if(!sprite) return 0.0f;
	return sprite->omega;
}

void SpriteSetOmega(struct Sprite *const sprite, const float omega) {
	if(!sprite) return;
	sprite->omega = omega;
}

/** @return		The bounding radius. */
float SpriteGetBounding(const struct Sprite *const sprite) {
	if(!sprite) return 1.0f;
	return sprite->bounding;
}

float SpriteGetMass(const struct Sprite *const s) {
	if(!s) return 0.0f;
	return s->mass;
}

unsigned SpriteGetSize(const struct Sprite *const s) {
	if(!s) return 0;
	return s->size;
}

/** Sets a spot in memory which points to the, possibly changing, sprite. */
void SpriteSetNotify(struct Sprite **const s_ptr) {
	struct Sprite *s;
	
	if(!s_ptr || !(s = *s_ptr)) return;
	if(s->notify) Warn("Sprite::setNotify: %s overriding a previous notification.\n", SpriteToString(s));
	s->notify = s_ptr;
}

/** Gets the Sprite type that was assigned at the beginning. */
enum SpType SpriteGetType(const struct Sprite *const sprite) {
	if(!sprite) return 0;
	return sprite->sp_type;
}

/** Volatile-ish: can only print 4 Sprites at once. */
char *SpriteToString(const struct Sprite *const s) {
	static int b;
	static char buffer[4][64];
	int last_b;

	if(!s) {
		snprintf(buffer[b], sizeof buffer[b], "%s", "null sprite");
	} else {
		/*snprintf(buffer[b], sizeof buffer[b], "%sSpr%u[%.1f,%.1f:%.1f]%.2ft", decode_sprite_type(s->sp_type), (int)(s - sprites) + 1, s->x, s->y, s->theta, s->mass);*/
		/*snprintf(buffer[b], sizeof buffer[b], "%sSpr%u[Lgh%d]", decode_sprite_type(s->sp_type), (int)(s - sprites) + 1, s->sp_type == SP_WMD ? s->sp.wmd.light : 0);*/
		snprintf(buffer[b], sizeof buffer[b], "%sSpr%u[%s]", decode_sprite_type(s->sp_type), (int)(s - sprites) + 1, s->sp_type == SP_SHIP && s->sp.ship.event_recharge ? EventToString(s->sp.ship.event_recharge) : "");
	};
	last_b = b;
	b = (b + 1) & 3;

	return buffer[last_b];
}

/** Gets a SpaceZone that it goes to, if it exists. */
const struct AutoSpaceZone *SpriteGetTo(const struct Sprite *const s) {
	if(!s || s->sp_type != SP_ETHEREAL) return 0;
	return s->sp.ethereal.to;
}

/** fixme: all these set an int to zero, do not use polymorphism */

int SpriteGetDamage(const struct Sprite *const s) {
	if(!s || s->sp_type != SP_WMD) return 0;
	return s->sp.wmd.wmd_type->damage;
}

int SpriteGetHit(const struct Sprite *const s) {
	if(!s) return 0;
	switch(s->sp_type) {
		case SP_DEBRIS:		return s->sp.debris.hit;
		case SP_SHIP:		return s->sp.ship.hit;
		case SP_WMD:		return 1;
		case SP_ETHEREAL:	return s->sp.ethereal.is_picked_up ? 0 : 1;
	}
	return 0;
}

int SpriteGetMaxHit(const struct Sprite *const s) {
	if(!s || s->sp_type != SP_SHIP) return 0;
	return s->sp.ship.max_hit;
}

struct Event *SpriteGetEventRecharge(const struct Sprite *const s) {
	if(!s || s->sp_type != SP_SHIP) return 0;
	return s->sp.ship.event_recharge;
}

const struct AutoWmdType *SpriteGetWeapon(const struct Sprite *const s) {
	if(!s || s->sp_type != SP_SHIP) return 0;
	return s->sp.ship.class->weapon;
}

/** This mostly does damage, (recharge is negative.) This also sets an Event for
 recharge. */
void SpriteRecharge(struct Sprite *const s, const int recharge) {
	if(!s) return;
	switch(s->sp_type) {
		case SP_DEBRIS:
			if(-recharge < s->sp.debris.hit) {
				s->sp.debris.hit += recharge;
			} else {
				s->sp.debris.hit = 0;
			}
			break;
		case SP_SHIP:
			if(recharge + s->sp.ship.hit >= s->sp.ship.max_hit) {
				/* cap off */
				s->sp.ship.hit = s->sp.ship.max_hit;
			} else if(-recharge < s->sp.ship.hit) {
				s->sp.ship.hit += (unsigned)recharge;
				/* rechage */
				if(!s->sp.ship.event_recharge
				   && s->sp.ship.hit < s->sp.ship.max_hit) {
					Event(&s->sp.ship.event_recharge, s->sp.ship.ms_recharge_hit, 0, FN_CONSUMER, &ship_recharge, s);
					Debug("Sprite::recharge: %s beginning charging cycle %d/%d with %s #%p.\n", SpriteToString(s), s->sp.ship.hit, s->sp.ship.max_hit, EventToString(s->sp.ship.event_recharge), s);
				}
			} else {
				/* killed */
				s->sp.ship.hit = 0;
			}
			break;
		case SP_WMD:
			if(recharge < 0) s->sp.wmd.expires = 0;
			break;
		case SP_ETHEREAL:
			if(recharge < 0) s->sp.ethereal.is_picked_up = -1;
			break;
	}
}

int SpriteIsDestroyed(const struct Sprite *const s) {
	if(!s) return -1;
	switch(s->sp_type) {
		case SP_DEBRIS:		return s->sp.debris.hit <= 0;
		case SP_SHIP:		return s->sp.ship.hit <= 0;
		case SP_WMD:		return TimerIsTime(s->sp.wmd.expires);
		case SP_ETHEREAL:	return s->sp.ethereal.is_picked_up;
	}
	return -1;
}

void SpriteDestroy(struct Sprite *const s) {
	if(!s) return;
	switch(s->sp_type) {
		case SP_DEBRIS:		s->sp.debris.hit = 0; break;
		case SP_SHIP:		s->sp.ship.hit = 0; break;
		case SP_WMD:		s->sp.wmd.expires = 0; break;
		case SP_ETHEREAL:	s->sp.ethereal.is_picked_up = -1; break;
	}
}

/** Enforces the maximum speed by breaking the debris into smaller chunks; the
 process is not elastic, so it loses energy (speed.) */
/*void DebrisEnforce(struct Debris *deb) {
	float vx, vy;
	float speed_2;
	
	if(!deb || !deb->sprite) return;
	
	SpriteGetVelocity(deb->sprite, &vx, &vy);
	if((speed_2 = vx * vx + vy * vy) > maximum_speed_2) {
		Debug("Debris::enforce: maximum %.3f\\,(pixels/s)^2, Deb%u is moving %.3f\\,(pixels/s)^2.\n", maximum_speed_2, DebrisGetId(deb), speed_2);
		DebrisExplode(deb);
	}
}*/

/** Spawns smaller Debris (fixme: stub.) */
void SpriteDebris(const struct Sprite *const s) {
	struct AutoImage *small_image;
	struct Sprite *sub;

	if(!s || !(small_image = AutoImageSearch("AsteroidSmall.png")) || s->size <= small_image->width) return;

	Pedantic("Sprite::debris: %s is exploding at (%.3f, %.3f).\n", SpriteToString(s), s->x, s->y);

	/* break into pieces -- new debris */
	sub = Sprite(SP_DEBRIS, small_image, (int)s->x, (int)s->y, s->theta, small_asteroid_mass);
	SpriteSetVelocity(sub, deb_explosion_elasticity * s->vx, deb_explosion_elasticity * s->vy);
}

/** Gets input to a Ship.
 @param sprite			Which sprite to set.
 @param turning			How many <em>ms</em> was this pressed? left = positive
 @param acceleration	How many <em>ms</em> was this pressed?
 @param dt_s			How many seconds has it been? */
void SpriteInput(struct Sprite *s, const int turning, const int acceleration, const int dt_ms) {
	if(!s || s->sp_type != SP_SHIP) return;
	if(acceleration != 0) {
		float vx, vy;
		float a = s->sp.ship.acceleration * acceleration, speed2;
		SpriteGetVelocity(s, &vx, &vy);
		vx += cosf(s->theta) * a;
		vy += sinf(s->theta) * a;
		if((speed2 = vx * vx + vy * vy) > s->sp.ship.max_speed2) {
			float correction = sqrtf(s->sp.ship.max_speed2 / speed2);
			vx *= correction;
			vy *= correction;
		}
		SpriteSetVelocity(s, vx, vy);
	}
	if(turning) {
		s->omega += s->sp.ship.turn_acceleration * turning * dt_ms;
		if(s->omega < -s->sp.ship.turn) {
			s->omega = -s->sp.ship.turn;
		} else if(s->omega > s->sp.ship.turn) {
			s->omega = s->sp.ship.turn;
		}
	} else {
		const float damping = s->sp.ship.turn_acceleration * dt_ms;
		if(s->omega > damping) {
			s->omega -= damping;
		} else if(s->omega < -damping) {
			s->omega += damping;
		} else {
			s->omega = 0;
		}
	}
	/* fixme: this should be gone! instead have it for all sprites and get rid
	 of dt_ms */
	SpriteAddTheta(s, s->omega * dt_ms);
}

void SpriteShoot(struct Sprite *const s) {
	struct Sprite *wmd;

	if(!s || s->sp_type != SP_SHIP || !TimerIsTime(s->sp.ship.ms_recharge_wmd)) return;
	wmd = Sprite(SP_WMD, s, s->sp.ship.class->weapon);
	s->sp.ship.ms_recharge_wmd = TimerGetGameTime() + s->sp.ship.class->weapon->ms_recharge;
	Pedantic("Sprite::shoot: %s shot %s\n", SpriteToString(s), SpriteToString(wmd));
}

/** Linear search for Sprites that are gates that go to to. */
struct Sprite *SpriteOutgoingGate(const struct AutoSpaceZone *to) {
	struct Sprite *s;

	/*Debug("Outgoing: SpaceZone to: #%p %s.\n", to, to->name);*/
	while((s = iterate())) {
		/*Debug("Outgoing: Sprite: #%p %s.\n", (void *)s, SpriteToString(s));
		if(s->sp_type == SP_ETHEREAL) Debug("Outgoing: Ethreal to: #%p %s.\n", s->sp.ethereal.to, s->sp.ethereal.to->name);*/
		if(s->sp_type != SP_ETHEREAL || s->sp.ethereal.to != to) continue;
		iterator = sprites; /* reset */
		break;
	}
	return s;
}

/** Removes all of the Sprites for which predicate is true; if predicate is
 null, than it removes all sprites.
 <p>
 Fixme: Is really slow copying memory in large scenes where delete nearly all
 sprite; viz, the thing you want to do. */
void SpriteRemoveIf(int (*const predicate)(struct Sprite *const)) {
	struct Sprite *s;

	/* we may be doing a double-iteration
	 (ie SpriteUpdate->iterate->collide->shp_eth->callback->SpriteRemoveIf),
	 so push the iterator; fixed! pushed in the event queue with 0ms, don't
	 need to wory about it anymore */
	while((s = iterate())) {
		Pedantic("Sprite::removeIf: consdering %s.\n", SpriteToString(s));
		if(!predicate || predicate(s)) Sprite_(&s);
	}
}

extern int draw_is_print_sprites;

/** Returns true while there are more sprites in the window, sets the values.
 The pointers need to all be there or else there will surely be a segfault.
 <p>
 Called in ../system/Draw to draw spites.
 @param x_ptr		x
 @param y_ptr		y
 @param t_ptr		\theta
 @param texture_ptr	OpenGl texture unit.
 @param size_ptr	Size of the texture.
 @return			True if the values have been set. */
int SpriteIterate(float *x_ptr, float *y_ptr, float *theta_ptr, int *texture_ptr, int *size_ptr) {
	static int x_min_window, x_max_window, y_min_window, y_max_window;
	static int x_min, x_max, y_min, y_max;
	static int is_reset = -1; /* oy, static */
	struct Sprite *s, *feeler;
	float camera_x, camera_y;

	if(draw_is_print_sprites) {
		Debug("%s: {", "iterate");
		while((s = iterate())) Debug(" %s", SpriteToString(s));
		Debug(" }\n");
	}

	/* go to the first spot in the window */
	if(is_reset) {
		unsigned w, h, screen_width, screen_height;
		DrawGetScreen(&screen_width, &screen_height);
		w = (screen_width  >> 1) + (screen_width & 1);
		h = (screen_height >> 1) + (screen_height & 1);
		/* determine the window */
		DrawGetCamera(&camera_x, &camera_y);
		x_min_window = (int)camera_x - w;
		x_max_window = (int)camera_x + w;
		y_min_window = (int)camera_y - h;
		y_max_window = (int)camera_y + h;
		/* also should include sprites that are out of the window, but part of
		 them is in */
		x_min = x_min_window - half_max_size;
		x_max = x_max_window + half_max_size;
		y_min = y_min_window - half_max_size;
		y_max = y_max_window + half_max_size;
		if(draw_is_print_sprites) Debug("window+(%d:%d,%d,%d)\n", x_min, x_max, y_min, y_max);
		/* no sprite anywhere? */
		if(!first_x_window && !(first_x_window = first_x)) return 0;
		/* place the first_x_window on the first x in the window */
		if(first_x_window->x < x_min) {
			do {
				if(!(first_x_window = first_x_window->next_x)) return 0;
			} while(first_x_window->x < x_min);
		} else {
			while((feeler = first_x_window->prev_x)
				  && (x_min <= feeler->x)) first_x_window = feeler;
		}
		/*Debug("first_x_window (%f,%f) %d\n", first_x_window->x, first_x_window->y, first_x_window->texture);*/
		/* mark x; O(n) :[ */
		for(s = first_x_window; s && s->x <= x_max; s = s->next_x) {
			if(draw_is_print_sprites) Debug("marking %s\n", SpriteToString(s));
			s->is_selected = -1;
		}
		/* now repeat for y; no sprite anywhere (this shouldn't happen!) */
		if(!first_y_window && !(first_y_window = first_y)) return 0;
		/* place the first_y_window on the first y in the window */
		if(first_y_window->y < y_min) {
			do {
				if(!(first_y_window = first_y_window->next_y)) return 0;
			} while(first_y_window->y < y_min);
		} else {
			while((feeler = first_y_window->prev_y)
				  && (y_min <= feeler->y)) first_y_window = feeler;
		}
		/* select the first y; fixme: should probably go the other way around,
		 less to mark since the y-extent is probably more; or maybe it's good
		 to mark more? */
		window_iterator = first_y_window;
		is_reset = 0;
		sprites_considered = 0;
		sprites_onscreen   = 0;
		if(draw_is_print_sprites) Debug("first_y_window = window_iterator(_0) = %s\n", SpriteToString(window_iterator));
	}

	/* consider y */
	while(window_iterator && window_iterator->y <= y_max) {
		if(draw_is_print_sprites) Debug("considering %s\n", SpriteToString(window_iterator));
		sprites_considered++;
		if(window_iterator->is_selected) {
			int extent = (window_iterator->size >> 1) + 1;
			/* tighter bounds -- slow, but worth it; fixme: optimise for b-t */
			if(   window_iterator->x > x_min_window - extent
			   && window_iterator->x < x_max_window + extent
			   && window_iterator->y > y_min_window - extent
			   && window_iterator->y < y_max_window + extent) {
				sprites_onscreen++;
				/*Debug("Sprite (%.3f, %.3f : %.3f) Tex%d size %d.\n", window_iterator->x, window_iterator->y, window_iterator->theta, window_iterator->texture, window_iterator->size);*/
				*x_ptr       = window_iterator->x;
				*y_ptr       = window_iterator->y;
				*theta_ptr   = window_iterator->theta;
				*texture_ptr = window_iterator->texture;
				*size_ptr    = window_iterator->size;
				window_iterator = window_iterator->next_y;
				return -1;
			} else if(draw_is_print_sprites) {
				Debug("Tighter bounds rejected Spr%u(%f,%f)\n", SpriteToString(window_iterator));
			}
		} else if(draw_is_print_sprites) {
			Debug("not marked %s\n", SpriteToString(window_iterator));
		}
		window_iterator = window_iterator->next_y;
	}

	/* reset for next time */
	for(s = first_x_window; s && s->x <= x_max; s = s->next_x)
		s->is_selected = 0;
	is_reset = -1;
	return 0;
}

/** This is where most of the work gets done. Called every frame, O(n). Also,
 this calls appropriate handlers for subclasses.
 @param dt_ms	Milliseconds since the last frame. */
void SpriteUpdate(const int dt_ms) {
	struct Sprite *s;

	/* sorts the sprites; they're (hopefully) almost sorted already from last
	 frame, just freshens with insertion sort, O(n + m) where m is the
	 related to the dynamicness of the scene.
	 Nah, I changed it so they're always in order; probably change it back; it's
	 slightly more correct this way, but slower? */
	/*sort();*/

	/* pre-compute bounding boxes of where the sprite wants to go this frame,
	 containing the projected circle along a vector x -> x + v*dt */
	while((s = iterate())) {
		/* where they should be if there's no collision */
		s->x1 = s->x + s->vx * dt_ms;
		s->y1 = s->y + s->vy * dt_ms;
		/*if(s == GameGetPlayer()) {
			Info("Player (%.1f, %.1f) + %d*(%.1f, %.1f) = (%.1f, %.1f)\n", s->x, s->y, dt_ms, s->vx, s->vy, s->x1, s->y1);
		}*/
		/* extend the bounding box along the circle's trajectory */
		s->x_min = ((s->x <= s->x1) ? s->x  : s->x1) - s->bounding;
		s->x_max = ((s->x <= s->x1) ? s->x1 : s->x)  + s->bounding;
		s->y_min = ((s->y <= s->y1) ? s->y  : s->y1) - s->bounding;
		s->y_max = ((s->y <= s->y1) ? s->y1 : s->y)  + s->bounding;
	}

	/* dynamic; move the sprites around */
	while((s = iterate())) {

		/* this sets no_collision and if no_collision > 0 sets v[xy]1 */
		collide(s);

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
		}

		/* apply De Sitter spacetime */
		if(s->x      < -de_sitter) s->x =  de_sitter - 20.0f;
		else if(s->x >  de_sitter) s->x = -de_sitter + 20.0f;
		if(s->y      < -de_sitter) s->y =  de_sitter - 20.0f;
		else if(s->y >  de_sitter) s->y = -de_sitter + 20.0f;

		/* keep it sorted (fixme: no?) */
		sort_notify(s);
	}

	/* do some stuff that is unique to each sprite type */
	while((s = iterate())) {
		switch(s->sp_type) {
			/* fixme: have more drama then just deleting */
			case SP_DEBRIS:
				if(0 < s->sp.debris.hit) break;
				SpriteDebris(s);
				Sprite_(&s);
				break;
			case SP_SHIP:
				switch(s->sp.ship.behaviour) {
					case B_STUPID:
						/* they are really stupid */
						do_ai(s, dt_ms);
						break;
					case B_HUMAN:
						/* do nothing; Game::update has game.player, which it takes
						 care of; this would be like, network code for multiple human-
						 controlled pieces */
						break;
					case B_NONE:
						break;
				}
				if(0 >= s->sp.ship.hit) {
					Info("Sprite::update: %s destroyed.\n", SpriteToString(s));
					Sprite_(&s);
					break;
				}
				/* left over from polling */
				/*if(s->sp.ship.hit < s->sp.ship.max_hit) {
					while(TimerIsTime(s->sp.ship.ms_recharge_event)) {
						SpriteRecharge(s, 1);
						if(s->sp.ship.hit >= s->sp.ship.max_hit) break;
						s->sp.ship.ms_recharge_event += s->sp.ship.ms_recharge_hit;
						Debug("Sprite::update: %s recharging.\n", SpriteToString(s));
					}
				}*/
				break;
			case SP_WMD:
				if(TimerIsTime(s->sp.wmd.expires)) { Sprite_(&s); break; }
				/* update light */
				LightSetPosition(s->sp.wmd.light, s->x, s->y);
				break;
			case SP_ETHEREAL:
				if(!s->sp.ethereal.is_picked_up) break;
				Sprite_(&s);
				break;
			default:
				Warn("Sprite::update: unknown type.\n");
		}
		/* s is possibly invalid! don't do anything here */
	}
}

void SpriteList(void) {
	struct Sprite *s;
	int is_first = -1;
	Info("Sprites: {");
	while((s = iterate())) {
		Info("%s%s", is_first ? " " : ", ", SpriteToString(s));
		is_first = 0;
	}
	Info(" }\n");
}

/* private */

/* branch cut (-Pi,Pi] */
static void branch_cut_pi_pi(float *theta_ptr) {
	*theta_ptr -= M_2PI * floorf((*theta_ptr + M_PI) / M_2PI);
}

/** This is a private iteration which uses the same variable as Sprite::iterate.
 This actually returns a Sprite. Supports deleting Sprites (see @see{Sprite_}.)
 This iterates over the whole Sprite list, not only the ones in the window. */
static struct Sprite *iterate(void) {
	if(iterator >= sprites + sprites_size) {
		iterator = sprites;
		return 0;
	}
	return iterator++;
}

/** Sorts the sprites; they're (hopefully) almost sorted already from last
 frame, just freshens with insertion sort, O(n + m) where m is the
 related to the dynamicness of the scene
 @return	The number of equvalent-swaps (doesn't do this anymore.) */
/*static void sort(void) {
	isort((void **)&first_x,
		  (int (*)(const void *, const void *))&compare_x,
		  (void **(*)(void *const))&address_prev_x,
		  (void **(*)(void *const))&address_next_x);
	isort((void **)&first_y,
		  (int (*)(const void *, const void *))&compare_y,
		  (void **(*)(void *const))&address_prev_y,
		  (void **(*)(void *const))&address_next_y);
}*/

/** Keep it sorted when there is one element out-of-place. */
static void sort_notify(struct Sprite *s) {
	inotify((void **)&first_x,
			(void *)s,
			(int (*)(const void *, const void *))&compare_x,
			(void **(*)(void *const))&address_prev_x,
			(void **(*)(void *const))&address_next_x);
	inotify((void **)&first_y,
			(void *)s,
			(int (*)(const void *, const void *))&compare_y,
			(void **(*)(void *const))&address_prev_y,
			(void **(*)(void *const))&address_next_y);
}

/** First it uses the Hahn–Banach separation theorem (viz, hyperplane
 separation theorem) and the ordered list of projections on the x and y axis
 that we have been keeping, to build up a list of possible colliders based on
 their bounding circles. Calls {@see collide_circles}, and if it passed,
 {@see elastic_bounce}. Calls the items in the collision matrix.
 @return	False if the sprite has been destroyed. */
void collide(struct Sprite *a) {
	struct Sprite *b, *b_adj_y;
	void (*response)(struct Sprite *, struct Sprite *, const float);
	float explore_x_min, explore_x_max, explore_y_min, explore_y_max;
	float t0;

	if(!a) return;

	/* extend the bounds to check all of this space for collisions */
	explore_x_min = a->x_min - half_max_size;
	explore_x_max = a->x_max + half_max_size;
	explore_y_min = a->y_min - half_max_size;
	explore_y_max = a->y_max + half_max_size;

	/* mark x */
	for(b = a->prev_x; b && b->x >= explore_x_min; b = b->prev_x) {
		if(a < b && a->x_min < b->x_max) b->is_selected = -1;
	}
	for(b = a->next_x; b && b->x <= explore_x_max; b = b->next_x) {
		if(a < b && a->x_max > b->x_min) b->is_selected = -1;
	}

	/* consider y and maybe add it to the list of colliders */
	for(b = a->prev_y; b && b->y >= explore_y_min; b = b_adj_y) {
		b_adj_y = b->prev_y; /* b can be deleted; make a copy */
		if(b->is_selected
		   && a->y_min < b->y_max
		   && (response = collision_matrix[b->sp_type][a->sp_type])
		   && collide_circles(a->x, a->y, a->x1, a->y1, b->x, b->y, b->x1,
							  b->y1, a->bounding + b->bounding, &t0))
			response(a, b, t0);
			/*Debug("Collision %s--%s\n", SpriteToString(a), SpriteToString(b));*/
	}
	for(b = a->next_y; b && b->y <= explore_y_max; b = b_adj_y) {
		b_adj_y = b->next_y;
		if(b->is_selected
		   && a->y_max > b->y_min
		   && (response = collision_matrix[b->sp_type][a->sp_type])
		   && collide_circles(a->x, a->y, a->x1, a->y1, b->x, b->y, b->x1,
							  b->y1, a->bounding + b->bounding, &t0))
			response(a, b, t0);
			/*Debug("Collision %s--%s\n", SpriteToString(a), SpriteToString(b));*/
	}

	/* reset for next time; fixme: ugly */
	for(b = a->prev_x; b && b->x >= explore_x_min; b = b->prev_x) {
		b->is_selected = 0;
	}
	for(b = a->next_x; b && b->x <= explore_x_max; b = b->next_x) {
		b->is_selected = 0;
	}
}

/** Given two seqments, u: a->b and v: c->d, and their combined radius, r,
 returns false if they do not collide, or true if they did, and in that case,
 t0_ptr will be assigned a time of collision.
 <p>
 This is called from {@see collide} as a more detailed check.
 @param a_x
 @param a_y
 @param b_x
 @param b_y		Point u moves from a to b.
 @param c_x
 @param c_y
 @param d_x
 @param d_y		Point v moves from c to d.
 @param r		The distance to test.
 @param t0_ptr	If collision is true, sets this to the intersecting time in
				[0, 1]. If it is 0, (u, v) is at (a, c), and 1, (c, d).
 @return		If they collided. */
static int collide_circles(const float a_x, const float a_y,
						   const float b_x, const float b_y,
						   const float c_x, const float c_y,
						   const float d_x, const float d_y,
						   const float r, float *t0_ptr) {
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
	const float u_x   = b_x - a_x, u_y  = b_y - a_y;
	const float v_x   = d_x - c_x, v_y  = d_y - c_y;
	const float vu_x  = v_x - u_x, vu_y = v_y - u_y; /* vu = v - u */
	const float vu_2  = vu_x * vu_x + vu_y * vu_y;
	const float ca_x  = c_x - a_x, ca_y = c_y - a_y;
	const float ca_vu = ca_x * vu_x + ca_y * vu_y;
	float       t, dist_x, dist_y, dist_2, ca_2, det;
	/* fixme: float stored in memory? can this be more performant? */

	/* time (t_min) of closest approach; we want the first contact */
	if(vu_2 < epsilon) {
		t = 0.0f;
	} else {
		t = -ca_vu / vu_2;
		if(     t < 0.0f) t = 0.0f;
		else if(t > 1.0f) t = 1.0f;
	}

	/* distance(t_min) of closest approach */
	dist_x = ca_x + vu_x * t;
	dist_y = ca_y + vu_y * t;
	dist_2 = dist_x * dist_x + dist_y * dist_y;

	/* not colliding */
	if(dist_2 >= r * r) return 0;

	/* collision time, t_0 */
	ca_2  = ca_x * ca_x + ca_y * ca_y;
	det   = (ca_vu * ca_vu - vu_2 * (ca_2 - r * r));
	t = (det <= 0.0f) ? 0.0f : (-ca_vu - sqrtf(det)) / vu_2;
	if(t < 0.0f) t = 0.0f; /* it can not be > 1 because dist < r^2 */
	*t0_ptr = t;

	return -1;
}

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
static void elastic_bounce(struct Sprite *a, struct Sprite *b, const float t0_dt) {
	/* interpolate position of collision */
	const float a_x = a->x * t0_dt + a->x1 * (1.0f - t0_dt);
	const float a_y = a->y * t0_dt + a->y1 * (1.0f - t0_dt);
	const float b_x = b->x * t0_dt + b->x1 * (1.0f - t0_dt);
	const float b_y = b->y * t0_dt + b->y1 * (1.0f - t0_dt);
	/* delta */
	const float d_x = b_x - a_x;
	const float d_y = b_y - a_y;
	/* normal at point of impact; fixme: iffy infinities */
	const float n_d2 = d_x * d_x + d_y * d_y;
	const float n_n  = (n_d2 < epsilon) ? 1.0f / epsilon : 1.0f / sqrtf(n_d2);
	const float n_x  = d_x * n_n;
	const float n_y  = d_y * n_n;
	/* a's velocity, normal direction */
	const float a_nrm_s = a->vx * n_x + a->vy * n_y;
	const float a_nrm_x = a_nrm_s * n_x;
	const float a_nrm_y = a_nrm_s * n_y;
	/* a's velocity, tangent direction */
	const float a_tan_x = a->vx - a_nrm_x;
	const float a_tan_y = a->vy - a_nrm_y;
	/* b's velocity, normal direction */
	const float b_nrm_s = b->vx * n_x + b->vy * n_y;
	const float b_nrm_x = b_nrm_s * n_x;
	const float b_nrm_y = b_nrm_s * n_y;
	/* b's velocity, tangent direction */
	const float b_tan_x = b->vx - b_nrm_x;
	const float b_tan_y = b->vy - b_nrm_y;
	/* mass (assume all mass is positive!) */
	const float a_m = a->mass, b_m = b->mass;
	const float diff_m = a_m - b_m, invsum_m = 1.0f / (a_m + b_m);
	/* elastic collision */
	const float a_vx = a_tan_x + (a_nrm_x*diff_m + 2*b_m*b_nrm_x) * invsum_m;
	const float a_vy = a_tan_y + (a_nrm_y*diff_m + 2*b_m*b_nrm_y) * invsum_m;
	const float b_vx = (-b_nrm_x*diff_m + 2*a_m*a_nrm_x) * invsum_m + b_tan_x;
	const float b_vy = (-b_nrm_y*diff_m + 2*a_m*a_nrm_y) * invsum_m + b_tan_y;
	/* check for sanity */
	const float bounding = a->bounding + b->bounding;
	/* fixme: float stored in memory? */

	Pedantic("elasitic_bounce: colision between %s--%s norm_d %f; sum_r %f, %f--%ft\n", SpriteToString(a), SpriteToString(b), sqrtf(n_d2), bounding, a_m, b_m);

	/* interpenetation; happens about half the time because of IEEE754 numerics,
	 which could be on one side or the other; also, sprites that just appear,
	 multiple collisions interfering, and gremlins; you absolutely do not want
	 objects to get stuck orbiting each other (fixme: this happens) */
	if(n_d2 < bounding * bounding) {
		const float push = (bounding - sqrtf(n_d2)) * 0.5f;
		Pedantic(" \\pushing sprites %f distance apart\n", push);
		a->x -= n_x * push;
		a->y -= n_y * push;
		b->x += n_x * push;
		b->y += n_y * push;
	}

	if(!a->no_collisions) {
		/* first collision */
		a->no_collisions = 1;
		a->t0_dt_collide = t0_dt;
		a->vx1           = a_vx;
		a->vy1           = a_vy;
	} else {
		/* there are multiple collisions, rebound from the 1st one and add */
		if(t0_dt < a->t0_dt_collide) a->t0_dt_collide = t0_dt;
		a->vx1           += a_vx;
		a->vy1           += a_vy;
		a->no_collisions++;
		Pedantic(" \\%u collisions %s (%s.)\n", a->no_collisions, SpriteToString(a), SpriteToString(b));
	}
	if(!b->no_collisions) {
		/* first collision */
		b->no_collisions = 1;
		b->t0_dt_collide = t0_dt;
		b->vx1           = b_vx;
		b->vy1           = b_vy;
	} else {
		/* there are multiple collisions, rebound from the 1st one and add */
		if(t0_dt < b->t0_dt_collide) b->t0_dt_collide = t0_dt;
		b->vx1           += b_vx;
		b->vy1           += b_vy;
		b->no_collisions++;
		Pedantic(" \\%u collisions %s (%s.)\n", b->no_collisions, SpriteToString(b), SpriteToString(a));
	}

}

/** Pushes a from angle, amount.
 @param a		The object you want to push.
 @param angle	Standard co\:ordainates, radians, angle.
 @param impulse	tonne pixels / ms */
static void push(struct Sprite *a, const float angle, const float impulse) {
	const float deltav = a->mass ? impulse / a->mass : 1.0f; /* pixel / s */
	a->vx += cosf(angle) * deltav;
	a->vy += sinf(angle) * deltav;
}

/* for isort */

static int compare_x(const struct Sprite *a, const struct Sprite *b) {
	return a->x > b->x;
}

static int compare_y(const struct Sprite *a, const struct Sprite *b) {
	return a->y > b->y;
}

static struct Sprite **address_prev_x(struct Sprite *const a) {
	return &a->prev_x;
}

static struct Sprite **address_next_x(struct Sprite *const a) {
	return &a->next_x;
}

static struct Sprite **address_prev_y(struct Sprite *const a) {
	return &a->prev_y;
}

static struct Sprite **address_next_y(struct Sprite *const a) {
	return &a->next_y;
}

/* type collisions; can not modify list of Sprites as it is in the middle of
 x/ylist or delete! */

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
	Pedantic("wmd_deb: %s -- %s\n", SpriteToString(w), SpriteToString(d));
	/* avoid inifinite destruction loop */
	if(SpriteIsDestroyed(w) || SpriteIsDestroyed(d)) return;
	push(d, atan2f(d->y - w->y, d->x - w->x), w->mass);
	SpriteRecharge(d, -SpriteGetDamage(w));
	SpriteDestroy(w);
}

static void deb_wmd(struct Sprite *d, struct Sprite *w, const float d0) {
	wmd_deb(w, d, d0);
}

static void wmd_shp(struct Sprite *w, struct Sprite *s, const float d0) {
	/* avoid inifinite destruction loop */
	if(SpriteIsDestroyed(w) || SpriteIsDestroyed(s)) return;
	push(s, atan2f(s->y - w->y, s->x - w->x), w->mass);
	SpriteRecharge(s, -SpriteGetDamage(w));
	SpriteDestroy(w);
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
}

static void eth_shp(struct Sprite *e, struct Sprite *s, const float d0) {
	shp_eth(s, e, d0);
}

/** For debugging. */
static char *decode_sprite_type(const enum SpType sp_type) {
	switch(sp_type) {
		case SP_DEBRIS:		return "<Debris>";
		case SP_SHIP:		return "<Ship>";
		case SP_WMD:		return "<Wmd>";
		case SP_ETHEREAL:	return "<Ethereal>";
		default:			return "<not a sprite type>";
	}
}

/** can be a callback for an Ethereal, whenever it collides with a Ship.
 IT CAN'T MODIFY THE LIST */
static void gate_travel(struct Sprite *const gate, struct Sprite *ship) {
	/* this doesn't help!!! */
	float x, y, /*vx, vy,*/ gate_norm_x, gate_norm_y, proj/*, h*/;

	if(!gate || gate->sp_type != SP_ETHEREAL
	   || !ship || ship->sp_type != SP_SHIP) return; /* will never be true */
	x = ship->x - gate->x;
	y = ship->y - gate->y;
	/* unneccesary?
	 vx = ship_vx - gate_vx;
	 vy = ship_vy - gate_vy;*/
	gate_norm_x = cosf(gate->theta);
	gate_norm_y = sinf(gate->theta);
	proj = x * gate_norm_x + y * gate_norm_y; /* proj is the new h */
	if(ship->sp.ship.horizon > 0 && proj < 0) {
		Debug("gate_travel: %s crossed into the event horizon of %s.\n", SpriteToString(ship), SpriteToString(gate));
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

static void do_ai(struct Sprite *const a, const int dt_ms) {
	const struct Sprite *const b = GameGetPlayer();
	float c_x, c_y;
	float d_2, theta, t;
	int turning = 0, acceleration = 0;
	
	if(!b) return; /* fixme */
	c_x = b->x - a->x;
	c_y = b->y - a->y;
	d_2   = c_x * c_x + c_y * c_y;
	theta = atan2f(c_y, c_x);
	/* the ai ship aims where it thinks the player will be when it's shot gets
	 there, approx - too hard to beat! */
	/*target.x = ship[SHIP_PLAYER].p.x - Sin[ship[SHIP_PLAYER].r] * ship[SHIP_PLAYER].inertia.x * (disttoenemy / WEPSPEED) - ship[1].inertia.x;
	 target.y = ship[SHIP_PLAYER].p.y + Cos[ship[SHIP_PLAYER].r] * ship[SHIP_PLAYER].inertia.y * (disttoenemy / WEPSPEED) - ship[1].inertia.y;
	 disttoenemy = hypot(target.x - ship[SHIP_CPU].p.x, target.y - ship[SHIP_CPU].p.y);*/
	/* t is the error of where wants vs where it's at */
	t = theta - a->theta;
	if(t >= (float)M_PI) { /* keep it in the branch cut */
		t -= (float)M_2PI;
	} else if(t < -M_PI) {
		t += (float)M_2PI;
	}
	/* too close; ai only does one thing at once, or else it would be hard */
	if(d_2 < shp_ai_too_close) {
		if(t < 0) t += (float)M_PI;
		else      t -= (float)M_PI;
	}
	/* turn */
	if(t < -shp_ai_turn || t > shp_ai_turn) {
		turning = (int)(t * shp_ai_turn_constant);
		/*if(turning * dt_s fixme */
		/*if(t < 0) {
		 t += (float)M_PI;
		 } else {
		 t -= (float)M_PI;
		 }*/
	} else if(d_2 > shp_ai_too_close && d_2 < shp_ai_too_far) {
		SpriteShoot(a);
	} else if(t > -shp_ai_turn_sloppy && t < shp_ai_turn_sloppy) {
		acceleration = shp_ai_speed;
	}
	SpriteInput(a, turning, acceleration, dt_ms);
}

static void sprite_poll(void) {
	struct Sprite *s;

	Info("Sprites by array index:\n");
	while((s = iterate())) Info("%s\n", SpriteToString(s));
	Info("Sprites by x:\n");
	for(s = first_x; s; s = s->next_x) Info("%s\n", SpriteToString(s));
	Info("Sprites by y:\n");
	for(s = first_y; s; s = s->next_y) Info("%s\n", SpriteToString(s));
	draw_is_print_sprites = -1;
}

/** can be used as an Event */
/*static!*/void ship_recharge(struct Sprite *const a) {
	if(!a || SpriteGetType(a) != SP_SHIP) {
		Warn("ship_recharge: called on %s.\n", SpriteToString(a));
		return;
	}
	Debug("ship_recharge %s %uGJ/%uGJ\n", SpriteToString(a), a->sp.ship.hit, a->sp.ship.max_hit);
	if(a->sp.ship.hit >= a->sp.ship.max_hit) return;
	a->sp.ship.hit++;
	/* this checks if an Event is associated to the sprite, we momentarily don't
	 have an event, even though we are getting one soon! alternately, we could
	 call Sprite::recharge and not stick the event down there.
	 SpriteRecharge(a, 1);*/
	if(a->sp.ship.hit >= a->sp.ship.max_hit) {
		Debug("ship_recharge: %s shields full %uGJ/%uGJ.\n", SpriteToString(a), a->sp.ship.hit, a->sp.ship.max_hit);
		return;
	}
	/*if(a->sp.ship.event_recharge) {
		of course, it was SpriteRecharge
		Warn("***************%s!!!\n", SpriteToString(a));
		return;
	}*/
	Event(&a->sp.ship.event_recharge, a->sp.ship.ms_recharge_hit, shp_ms_sheild_uncertainty, FN_CONSUMER, &ship_recharge, a);
}
