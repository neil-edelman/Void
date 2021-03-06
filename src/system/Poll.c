/** 2017 Neil Edelman, distributed under the terms of the GNU General
 Public License 3, see copying.txt, or
 \url{ https://opensource.org/licenses/GPL-3.0 }.

 {Poll} takes the serialised unsigned int array and creates a constant key map.
 Takes {Key.c} and makes it polling for axis information. This creates another
 layer that is passed to sprites.

 @title		Poll
 @author	Neil
 @std		C89/90
 @version	2017-10 Broke off from {Sprite}. */

#include <stdlib.h> /* malloc */
#include <stdio.h> /* perror */
#include "Key.h"
#include "Poll.h"

struct PollKey {
	unsigned press;
	unsigned ms;
};

struct PollAxis {
	unsigned decrease, increase;
	int ms;
};

/** The complete input pattern that we need to poll for. */
static struct Poll {
	struct PollAxis move_x, move_y;
	struct PollKey shoot;
} poll;

/** Constructor.
 @param pkeys: A pointer to a serialised key-map. */
void Poll(const unsigned (*const pkeys)[5]) {
	const unsigned *k;
	if(!pkeys || !(k = *pkeys))
		{ fprintf(stderr, "Poll: called without keys array.\n"); return; }
	poll.move_x.decrease = k[0];
	poll.move_x.increase = k[1];
	poll.move_x.ms = 0;
	poll.move_y.decrease = k[2];
	poll.move_y.increase = k[3];
	poll.move_y.ms = 0;
	poll.shoot.press = k[4];
	poll.shoot.ms = 0;
}

static void press(struct PollKey *k) {
	k->ms = KeyTime(k->press);
}

static void axis(struct PollAxis *a) {
	a->ms = KeyTime(a->increase) - KeyTime(a->decrease);
}

/** Should be called every frame to update the joystick-like keys. */
void PollUpdate(void) {
	axis(&poll.move_x);
	axis(&poll.move_y);
	press(&poll.shoot);
}

/** Accessor. */
int PollGetRight(void) { return poll.move_x.ms; }
/** Accessor. */
int PollGetUp(void) { return poll.move_y.ms; }
/** Accessor. */
int PollGetShoot(void) { return poll.shoot.ms; }

#if 0
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
#endif
