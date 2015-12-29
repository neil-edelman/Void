/** Copyright 2015 Neil Edelman, distributed under the terms of the GNU General
 Public License, see copying.txt */

#include <stdlib.h> /* malloc free */
#include <math.h>   /* sinf, cosf */
#include <string.h> /* memcpy */
#include "../Print.h"
#include "Ship.h"
#include "Sprite.h"
#include "Wmd.h"
#include "Game.h"
#include "../general/Map.h"
#include "../system/Draw.h"
#include "../system/Timer.h"
#include "../system/Key.h"

/* auto-generated from media dir; hope it's right; used in constructor */
#include "../../bin/Lore.h"

#define M_2PI 6.28318530717958647692528676655900576839433879875021164194988918461563281257241799725606965068423413596429617302656461329
#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288419716939937510582097494459230781640628620899862803482534211706798214808651328230664
#endif

/* Ship has license and is controlled by a player or an ai. Would be nice if
 Ship could inherit from Sprite (or actually from Debris which inherits from
 Sprite.)

 @author	Neil
 @version	3.2, 2015-06
 @since		3.2, 2015-06 */

static const float epsilon = 0.001f;

/* fixme */
static const float shot_recharge_ms = 300.0f;

/* ai */

static const float ai_turn        = 0.2f;     /* rad/ms */
static const float ai_too_close   = 3200.0f;  /* pixel^(1/2) */
static const float ai_too_far     = 32000.0f; /* pixel^(1/2) */
static const int   ai_speed       = 15;       /* pixel^2 / ms */
static const float ai_turn_sloppy = 0.4f;     /* rad */
static const float ai_turn_constant = 10.0f;

struct Ship {
	struct Sprite          *sprite;
	const struct ShipClass *class;
	float                  mass;    /* t (Mg) */
	float                  omega;   /* radians/s */
	float                  ms_recharge_wmd; /* ms -- the time when the weapon will be recharged (fixme) */
	int                    ms_recharge_hit, ms_max_recharge_hit; /* ms \alpha 1/W */
	int                    hit, max_hit; /* J */
	float                  max_speed2;
	float                  acceleration;
	float                  turn;
	float                  turn_limit;
	float                  friction_rate;
	enum Behaviour         behaviour;
	struct Ship            **notify; /* this is NOT sprite notify, further up the tree */
} ships[512];
static const int ships_capacity = sizeof(ships) / sizeof(struct Ship);
static int       ships_size;

/* private protos */
static void fire(struct Ship *s, const int colour);
void do_ai(struct Ship *const ai, const float dt_s);

/* public */

/** Get a new ships from the pool of unused.
 @param notify		Notifies on changing the ship pointer.
 @param ship_class	Make a copy of this ship class.
 @param behaviour	What should it do for input?
 @return		A ship or null. */
struct Ship *Ship(struct Ship **notify, const struct ShipClass *ship_class, const enum Behaviour behaviour) {
	struct Ship *ship;

	if(!ship_class) return 0;
	if(ships_size >= ships_capacity) {
		Debug("Ship: couldn't be created; reached maximum of %u.\n", ships_capacity);
		return 0;
	}
	ship = &ships[ships_size];
	/* superclass */
	if(!(ship->sprite = Sprite(S_SHIP, ship_class->image))) return 0;
	SpriteSetContainer(ship, &ship->sprite);
	ship->class      = ship_class;
	ship->mass       = (float)ship_class->mass; /* make int! */
	ship->omega      = 0.0f;
	ship->ms_recharge_wmd = 0;
	ship->ms_recharge_hit = 0;
	ship->ms_max_recharge_hit = ship_class->ms_recharge;
	ship->max_hit = ship->hit = ship_class->shield;
	ship->max_speed2 = ship_class->speed2;
	ship->acceleration = ship_class->acceleration;
	ship->turn       = ship_class->turn;
	ship->turn_limit = ship_class->turn * 500.0f; /* derived */
	ship->friction_rate = ship_class->turn * 1000.0f; /* derived */
	ship->behaviour  = behaviour;
	ship->notify     = notify;
	ships_size++;
	Pedantic("Ship: created from pool, Shp%u->Spr%u.\n", ShipGetId(ship), SpriteGetId(ship->sprite));

	if(notify && *notify) Debug("Ship: warning, new notify is not void.\n");
	if(notify) *notify = ship;
	return ship;
}

/** Erase a ships from the pool (array of static ships.)
 @param ships_ptr	A pointer to the ships; gets set null on success. */
void Ship_(struct Ship **ship_ptr) {
	struct Ship *ship;
	int index;

	if(!ship_ptr || !(ship = *ship_ptr)) return;
	index = ship - ships;
	if(index < 0 || index >= ships_size) {
		Debug("~Ship: Shp%u not in range %u.\n", ShipGetId(ship), ships_size);
		return;
	}
	Pedantic("~Ship: returning to pool, Shp%u->Spr%u.\n", ShipGetId(ship), SpriteGetId(ship->sprite));

	/* superclass Sprite */
	Sprite_(&ship->sprite);
	/* we have been deleted, for things that care (eg ship controls) */
	if(ship->notify) *ship->notify = 0;

	/* move the terminal ship to replace this one */
	if(index < --ships_size) {
		memcpy(ship, &ships[ships_size], sizeof(struct Ship));
		SpriteSetContainer(ship, &ship->sprite);
		if(ship->notify) *ship->notify = ship;
		Pedantic("~Ship: Shp%u has become Shp%u.\n", ships_size + 1, ShipGetId(ship));
	}

	*ship_ptr = ship = 0;
}

/** Sets the orientation with respect to the screen, pixels and (0, 0) is at
 the centre.
 @param sprite	Which sprite to set.
 @param x		x (pixels)
 @param y		y (pixels)
 @param theta	\theta (radians) */
void ShipSetOrientation(struct Ship *ship, const float x, const float y, const float theta) {
	if(!ship || !ship->sprite) return;
	SpriteSetOrientation(ship->sprite, x, y, theta);
}

/** If you need to get a position. Note: this will crash if you do not provide
 valid pointers. */
int ShipGetOrientation(const struct Ship *ship, float *const x_ptr, float *const y_ptr, float *const theta_ptr) {
	if(!ship || !ship->sprite) return 0;
	SpriteGetOrientation(ship->sprite, x_ptr, y_ptr, theta_ptr);
	return -1;
}

/** Fixme: have limits according to which ship.
 Note that turning and acceleration are proportional to the time, dt_s.
 @param sprite			Which sprite to set.
 @param turning			How many <em>ms</em> was this pressed? left = positive
 @param acceleration	How many <em>ms</em> was this pressed?
 @param dt_s			How many seconds has it been? */
void ShipSetVelocities(struct Ship *ship, const int turning, const int acceleration, const float dt_s) {
	float x, y, theta, vx, vy;

	if(!ship) return;
	SpriteGetOrientation(ship->sprite, &x, &y, &theta); /* fixme: don't need x, y */
	if(acceleration != 0) {
		float a = ship->acceleration * acceleration, speed2;
		SpriteGetVelocity(ship->sprite, &vx, &vy);
		vx += cosf(theta) * a;
		vy += sinf(theta) * a;
		/*ship->vx    += cosf(theta) * a;
		ship->vy    += sinf(theta) * a;
		speed2 = ship->vx * ship->vx + ship->vy * ship->vy;*/
		if((speed2 = vx * vx + vy * vy) > ship->max_speed2) {
			float correction = ship->max_speed2 / speed2; /* fixme: is it the sqrt? */
			vx *= correction;
			vy *= correction;
		}
		SpriteSetVelocity(ship->sprite, vx, vy);
	}
	if(turning) {
		ship->omega += ship->turn * turning;
		if(ship->omega < -ship->turn_limit) {
			ship->omega = -ship->turn_limit;
		} else if(ship->omega > ship->turn_limit) {
			ship->omega = ship->turn_limit;
		}
	} else {
		float damping = ship->friction_rate * dt_s;
		if(ship->omega > damping) {
			ship->omega -= damping;
		} else if(ship->omega < -damping) {
			ship->omega += damping;
		} else {
			ship->omega = 0;
		}
	}
	theta += ship->omega * dt_s;
	SpriteSetTheta(ship->sprite, theta);
}

/* unused */
float ShipGetOmega(const struct Ship *ship) {
	if(!ship) return 0.0f;
	return ship->omega;
}

void ShipGetVelocity(const struct Ship *ship, float *vx_ptr, float *vy_ptr) {
	if(!ship) return;
	SpriteGetVelocity(ship->sprite, vx_ptr, vy_ptr);
}

float ShipGetMass(const struct Ship *ship) {
	if(!ship) return 1.0f;
	return ship->mass;
}

float ShipGetBounding(const struct Ship *ship) {
	if(!ship) return 1.0;
	return SpriteGetBounding(ship->sprite);
}

struct Sprite *ShipGetSprite(const struct Ship *ship) {
	if(!ship) return 0;
	return ship->sprite;
}

int ShipGetHit(const struct Ship *const ship) {
	if(!ship) return 0;
	return ship->hit;
}

int ShipGetMaxHit(const struct Ship *const ship) {
	if(!ship) return 0;
	return ship->max_hit;
}

int ShipIsDestroyed(const struct Ship *s) {
	if(!s) return -1;
	return s->hit <= 0 ? -1 : 0;
}

int ShipGetId(const struct Ship *s) {
	if(!s) return 0;
	return (int)(s - ships) + 1;
}

/** This is general, mostly-ai. This will be called from Game::update, so no
 need to have Sprites.
 FIXME! instead of an enum, have a set of functions, colletively called AI,
 that define the behavior, which can be null in the case of a player */
void ShipUpdate(const float dt_s) {
	struct Ship *ship;

	for(ship = ships; ship < ships + ships_size; ship++) {
		/*printf("Shp%u.\n", ShipGetId(ship));*/
		switch(ship->behaviour) {
			case B_STUPID:
				/* they are really stupid */
				do_ai(ship, dt_s);
				break;
			case B_HUMAN:
				/* do nothing; Game::update has game.player, which it takes
				 care of; this would be like, network code for multiple human-
				 controlled pieces */
				break;
			case B_NONE:
				break;
		}
	}
}

/** Called from {@code Game::update}. */
void ShipShoot(struct Ship *ship, const int colour) {
	const int time_ms = TimerGetGameTime();
	if(!ship || time_ms < ship->ms_recharge_wmd) return;
	Wmd(ship->sprite, colour);
	ship->ms_recharge_wmd = time_ms + shot_recharge_ms;
}

/** Called from {@code Sprite::update} via shp_wmd.
 @return	True if the ship should survive. */
void ShipHit(struct Ship *ship, const int damage) {
	if(!ship) return;
	if(ship->hit > damage) {
		ship->hit -= damage;
		Debug("Ship::hit: Shp%u hit %d, now %d.\n", ShipGetId(ship), damage, ship->hit);
	} else {
		ship->hit = 0;
		Debug("Ship::hit: Shp%u destroyed.\n", ShipGetId(ship));
	}
}

/* private */

/** Called from {@code Ship::update} (viz, not; nonsense bug.) */
static void fire(struct Ship *ship, const int colour) { /* get s */
	const int time_ms = TimerGetGameTime();
	if(!ship || time_ms < ship->ms_recharge_wmd) return;
	Wmd(ship->sprite, colour);
	ship->ms_recharge_wmd = time_ms + shot_recharge_ms;
}

void do_ai(struct Ship *const ai, const float dt_s) {
	const struct Ship *player = GameGetPlayer();
	float a_x, a_y, a_theta;
	float b_x, b_y, b_theta;
	float c_x, c_y;
	float d_2, theta, t;
	int turning = 0, acceleration = 0;

	if(!player) return;
	SpriteGetOrientation(ai->sprite,     &a_x, &a_y, &a_theta);
	SpriteGetOrientation(player->sprite, &b_x, &b_y, &b_theta);
	c_x = b_x - a_x;
	c_y = b_y - a_y;
	d_2   = c_x * c_x + c_y * c_y;
	theta = atan2f(c_y, c_x);
	/* the ai ship aims where it thinks the player will be when it's shot gets
	 there, approx - too hard to beat! */
	/*target.x = ship[SHIP_PLAYER].p.x - Sin[ship[SHIP_PLAYER].r] * ship[SHIP_PLAYER].inertia.x * (disttoenemy / WEPSPEED) - ship[1].inertia.x;
	 target.y = ship[SHIP_PLAYER].p.y + Cos[ship[SHIP_PLAYER].r] * ship[SHIP_PLAYER].inertia.y * (disttoenemy / WEPSPEED) - ship[1].inertia.y;
	 disttoenemy = hypot(target.x - ship[SHIP_CPU].p.x, target.y - ship[SHIP_CPU].p.y);*/
	/* t is the error of where wants vs where it's at */
	t = theta - a_theta;
	if(t >= (float)M_PI) { /* keep it in the branch cut */
		t -= (float)M_2PI;
	} else if(t < -M_PI) {
		t += (float)M_2PI;
	}
	/* too close; ai only does one thing at once, or else it would be hard */
	if(d_2 < ai_too_close) {
		if(t < 0) t += (float)M_PI;
		else      t -= (float)M_PI;
	}
	/* turn */
	if(t < -ai_turn || t > ai_turn) {
		turning = (int)(t * ai_turn_constant);
		/*if(turning * dt_s fixme */
		/*if(t < 0) {
			t += (float)M_PI;
		} else {
			t -= (float)M_PI;
		}*/
	} else if(d_2 > ai_too_close && d_2 < ai_too_far) {
		fire(ai, 0);
	} else if(t > -ai_turn_sloppy && t < ai_turn_sloppy) {
		acceleration = ai_speed;
	}
	ShipSetVelocities(ai, turning, acceleration, dt_s);
}
