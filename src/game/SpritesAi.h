/** 2017 Neil Edelman, distributed under the terms of the GNU General
 Public License 3, see copying.txt, or
 \url{ https://opensource.org/licenses/GPL-3.0 }.

 This is included in \see{Sprites.c}. AI/Human stuff.

 @title		SpritesAi
 @author	Neil
 @std		C89/90
 @version	2017-11 Broke off from Sprites. */

static const float ai_too_close = 3200.0f; /* pixel^(1/2) */
static const float ai_too_far = 32000.0f; /* pixel^(1/2) */
static const float ai_turn = 0.2f; /* rad/ms */
static const float ai_turn_constant = 10.0f;
static const float ai_turn_sloppy = 0.4f; /* rad */
static const int ai_speed = 15; /* pixel^2 / ms */

/** This is used by AI and humans.
 @fixme Simplistic, shoot on frame. */
static void ship_input(struct Ship *const this, const int ms_turning,
	const int ms_acceleration, const int ms_shoot) {
	assert(this);
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
	if(ms_shoot && TimerIsTime(this->ms_recharge_wmd) && this->wmd) {
		SpritesWmd(this->wmd, this);
		this->ms_recharge_wmd = TimerGetGameTime() + this->wmd->ms_recharge;
	}
}
/** @implements <Ship>Predicate */
static int ship_update_human(struct Ship *const this) {
	ship_input(this, -PollGetRight(), PollGetUp(), PollGetShoot());
	return 1;
}

#if 0
/** @implements <Ship>Predicate */
static int ship_update_ai(struct Ship *const this) {
	assert(this);
	this->sprite.data.v.theta = 0.0002f;
	return 1;
}
#else
static void ship_update_ai(struct Ship *const this) {
	const struct Ship *const p = get_player();
	struct Vec2f d;
	float d_2, theta, t;
	int ms_turning = 0, ms_acceleration = 0, ms_shoot = 0;
	if(!p) return; /* @fixme The player is the only reason for being! */
	d.x = p->sprite.data.x.x - this->sprite.data.x.x,
	d.y = p->sprite.data.x.y - this->sprite.data.x.y;
	d_2   = d.x * d.x + d.y * d.y;
	theta = atan2f(d.y, d.x);
	/* {t} is the error of where wants vs where it's at. */
	t = theta - this->sprite.data.x.theta;
	branch_cut_pi_pi(&t);
	/* too close; ai only does one thing at once, or else it would be hard */
	if(d_2 < ai_too_close) { if(t < 0) t += M_PI_F; else t -= M_PI_F; }
	/* turn */
	if(t < -ai_turn || t > ai_turn) {
		ms_turning = (int)(t * ai_turn_constant);
		/*if(turning * dt_s fixme */
		/*if(t < 0) {
		 t += (float)M_PI_F;
		 } else {
		 t -= (float)M_PI_F;
		 }*/
	} else if(d_2 > ai_too_close && d_2 < ai_too_far) {
		ms_shoot = 10.0f;
	} else if(t > -ai_turn_sloppy && t < ai_turn_sloppy) {
		ms_acceleration = ai_speed;
	}
	ship_input(this, ms_turning, ms_acceleration, ms_shoot);
}
#endif
