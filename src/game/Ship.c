/** Copyright 2015 Neil Edelman, distributed under the terms of the GNU General
 Public License, see copying.txt */

#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* fprintf */
#include <math.h>   /* sinf, cosf */
#include <string.h> /* memcpy */
#include "Ship.h"
#include "Sprite.h"
#include "Wmd.h"
#include "Game.h"
#include "../General/Map.h"
#include "../System/Draw.h"
#include "../System/Timer.h"
#include "../System/Key.h"

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

/* ships */

const static int max_shields      = 320;    /* joule (tesla?) */
const static int ms_recharge      = 1024;   /* s */

const static float speed_limit2   = 2800.0f; /* (pixel / s)^2 */
const static float accel_rate     = 0.05f;   /* pixel / ms^2 */

const static float turn_rate      = 0.003f; /* rad / ms */
const static float rotation_limit = 1.5f;   /* rad / s */
const static float friction_rate  = 3.0f;   /* rad / s^2 */

const static float shot_recharge_ms = 500.0f;

/* ai */

const static float ai_turn        = 0.2f;     /* rad/ms */
const static float ai_too_close   = 3200.0f;  /* pixel^(1/2) */
const static float ai_too_far     = 32000.0f; /* pixel^(1/2) */
const static int   ai_speed       = 15;       /* pixel^2 / ms */
const static float ai_turn_sloppy = 0.4f;     /* rad */
const static float ai_turn_constant = 10.0f;

struct Ship {
	struct Sprite *sprite;
	float         omega; /* radians/s */
	float         mass;  /* t (Mg) */
	float         recharge_wmd;  /* ms */
	int           hit;   /* J */
	enum Behaviour behaviour;
	struct Ship   **notify; /* this is NOT sprite notify, further up the tree */
} ships[64];
static const int ships_capacity = sizeof(ships) / sizeof(struct Ship);
static int       ships_size;

/* private protos */
static void fire(struct Ship *s, const int colour);
void do_ai(struct Ship *const ai, const float dt_s);

/* public */

/** Get a new ships from the pool of unused.
 @return		A ship or null. */
struct Ship *Ship(struct Ship **notify, const int texture, const int size, const enum Behaviour behaviour) {
	struct Ship *ship;

	if(ships_size >= ships_capacity) {
		fprintf(stderr, "Ship: couldn't be created; reached maximum of %u.\n", ships_capacity);
		return 0;
	}
	ship = &ships[ships_size];
	/* superclass */
	if(!(ship->sprite = Sprite(S_SHIP, texture, size))) return 0;
	SpriteSetContainer(ship, &ship->sprite);
	ship->omega      = 0.0f;
	ship->mass       = 30.0f;
	ship->recharge_wmd = 0;
	ship->hit        = 300;
	ship->behaviour  = behaviour;
	ship->notify     = notify;
	ships_size++;
	fprintf(stderr, "Ship: created from pool, Shp%u->Spr%u.\n", ShipGetId(ship), SpriteGetId(ship->sprite));

	if(notify && *notify) fprintf(stderr, "Ship: warning, new notify is not void.\n");
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
		fprintf(stderr, "~Ship: Shp%u not in range %u.\n", ShipGetId(ship), ships_size);
		return;
	}
	fprintf(stderr, "~Ship: returning to pool, Shp%u->Spr%u.\n", ShipGetId(ship), SpriteGetId(ship->sprite));

	/* superclass Sprite */
	Sprite_(&ship->sprite);
	/* we have been deleted, for things that care (eg ship controls) */
	if(ship->notify) *ship->notify = 0;

	/* move the terminal ship to replace this one */
	if(index < --ships_size) {
		memcpy(ship, &ships[ships_size], sizeof(struct Ship));
		SpriteSetContainer(ship, &ship->sprite);
		if(ship->notify) *ship->notify = ship;
		fprintf(stderr, "~Ship: Shp%u has become Shp%u.\n", ships_size + 1, ShipGetId(ship));
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

/** If you need to get a position. */
int ShipGetOrientation(const struct Ship *ship, float *x_ptr, float *y_ptr, float *theta_ptr) {
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

	if(!ship || !ship->sprite) return;
	SpriteGetOrientation(ship->sprite, &x, &y, &theta); /* fixme: don't need x, y */
	if(acceleration != 0) {
		float a = accel_rate * acceleration, speed2;
		SpriteGetVelocity(ship->sprite, &vx, &vy);
		vx += cosf(theta) * a;
		vy += sinf(theta) * a;
		/*ship->vx    += cosf(theta) * a;
		ship->vy    += sinf(theta) * a;
		speed2 = ship->vx * ship->vx + ship->vy * ship->vy;*/
		if((speed2 = vx * vx + vy * vy) > speed_limit2) {
			float correction = speed_limit2 / speed2; /* fixme: is it the sqrt? */
			vx *= correction;
			vy *= correction;
		}
		SpriteSetVelocity(ship->sprite, vx, vy);
	}
	if(turning) {
		ship->omega += turn_rate * turning;
		if(ship->omega < -rotation_limit) {
			ship->omega = -rotation_limit;
		} else if(ship->omega > rotation_limit) {
			ship->omega = rotation_limit;
		}
	} else {
		float damping = friction_rate * dt_s;
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

struct Sprite *ShipGetSprite(const struct Ship *ship) {
	if(!ship) return 0;
	return ship->sprite;
}

int ShipGetId(const struct Ship *s) {
	if(!s) return 0;
	return (int)(s - ships) + 1;
}

/** This is general, mostly-ai. This will be called from Game::update, so no
 need to have Sprites. */
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
	const int time_ms = TimerLastTime();
	if(!ship || time_ms < ship->recharge_wmd) return;
	Wmd(ship->sprite, colour);
	ship->recharge_wmd = time_ms + shot_recharge_ms;
}

/** Called from {@code Sprite::update} via shp_wmd.
 @return	True if the ship should survive. */
int ShipHit(struct Ship *ship, const int damage) {
	if(!ship) return 0;
	if(ship->hit > damage) {
		ship->hit -= damage;
		fprintf(stderr, "Shit::hit: Shp%u hit %d, now %d.\n", ShipGetId(ship), damage, ship->hit);
		return -1;
	} else {
		ship->hit = 0;
		fprintf(stderr, "Ship::hit: Shp%u destroyed.\n", ShipGetId(ship));
		return 0;
	}
}

/* private */

/** Called from {@code Ship::update} (viz, not; nonsense bug.) */
static void fire(struct Ship *ship, const int colour) { /* get s */
	const int time_ms = TimerLastTime();
	if(!ship || time_ms < ship->recharge_wmd) return;
	Wmd(ship->sprite, colour);
	ship->recharge_wmd = time_ms + shot_recharge_ms;
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
