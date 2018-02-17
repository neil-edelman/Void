/** Mostly orthogonal math components header file.

 @title		Ortho.h
 @author	Neil
 @version	3.3; 2017-06
 @since		3.3; 2017-06 */

#ifndef ORTHO_H
#define ORTHO_H

#include <assert.h>
#include <math.h>	/* fmodf */
#include <stdlib.h>	/* RAND_MAX */

/* M_PI is a widely accepted gnu standard, not C<=99 -- who knew? */
#ifndef M_PI_F
#define M_PI_F \
	(3.141592653589793238462643383279502884197169399375105820974944f)
#endif
#ifndef M_2PI_F
#define M_2PI_F \
	(6.283185307179586476925286766559005768394338798750211641949889f)
#endif
#ifndef M_1_2PI_F
#define M_1_2PI_F \
	(0.159154943091895335768883763372514362034459645740456448747667344058896f)
#endif

/** {Ortho3f} must be able to be reinterpreted as {Vec3f}. */
struct Vec2i { int x, y; };
struct Vec2f { float x, y; };
struct Ortho3f { float x, y, theta; };
struct Colour3f { float r, g, b; };
struct Rectangle4i { int x_min, x_max, y_min, y_max; };
struct Rectangle4f { float x_min, x_max, y_min, y_max; };

/** Branch cut to the principal branch {(-Pi,Pi]} for numerical stability. We
 should really use fixed normalised {ints} for much more even accuracy and
 roll-over, but {OpenGl} doesn't like that.
 @fixme {floorf} may be slow.
 https://stackoverflow.com/questions/824118/why-is-floor-so-slow */
static void branch_cut_pi_pi(float *const theta_ptr) {
	assert(theta_ptr);
	*theta_ptr -= M_2PI_F * floorf((*theta_ptr + M_PI_F) * M_1_2PI_F);
}

/** Degree is good for artists, but we use radians in the branch cut
 {(-Pi, Pi]}. */
static void deg_to_rad(float *const theta_ptr) {
	assert(theta_ptr);
	*theta_ptr *= M_2PI_F / 360.0f;
	branch_cut_pi_pi(theta_ptr);
}

/** Generic uniform {(+/-max)} with {rand}. */
static float random_pm_max(const float max) {
	return 2.0f * max * rand() / RAND_MAX - max; /* fixme: Pedantic! */
}
/** Zeros {this}. */
static void ortho3f_init(struct Ortho3f *const this) {
	assert(this);
	this->x = this->y = this->theta = 0.0f;
}
/** Assigns {that} to {this}. */
static void ortho3f_assign(struct Ortho3f *const this,
	const struct Ortho3f *const that) {
	assert(this && that);
	this->x     = that->x;
	this->y     = that->y;
	this->theta = that->theta;
}
/** {this} += {a}. */
static void ortho3f_sum(struct Ortho3f *const this,
	const struct Ortho3f *const a) {
	assert(this && a);
	this->x     += a->x;
	this->y     += a->y;
	this->theta += a->theta;
}
/** {this} = {a} - {b}. */
static void ortho3f_sub(struct Ortho3f *const this,
	const struct Ortho3f *const a, const struct Ortho3f *const b) {
	assert(this && a && b);
	this->x     = a->x - b->x;
	this->y     = a->y - b->y;
	this->theta = a->theta - b->theta;
}
/** Assigns zero. */
static void rectangle4i_init(struct Rectangle4i *const this) {
	assert(this);
	this->x_min = this->x_max = this->y_min = this->y_max = 0;
}
/** Assigns zero. */
static void rectangle4f_init(struct Rectangle4f *const this) {
	assert(this);
	this->x_min = this->x_max = this->y_min = this->y_max = 0.0f;
}
/** Assigns {that} to {this}. */
static void rectangle4i_assign(struct Rectangle4i *const this,
	const struct Rectangle4i *const that) {
	assert(this);
	assert(that);
	this->x_min = that->x_min;
	this->x_max = that->x_max;
	this->y_min = that->y_min;
	this->y_max = that->y_max;
}
/** Assigns {that} to {this}. */
static void rectangle4f_assign(struct Rectangle4f *const this,
	const struct Rectangle4f *const that) {
	assert(this);
	assert(that);
	this->x_min = that->x_min;
	this->x_max = that->x_max;
	this->y_min = that->y_min;
	this->y_max = that->y_max;
}
/** Scales the rectangle. */
static void rectangle4f_scale(struct Rectangle4f *const this,
	const float scale) {
	assert(this);
	this->x_min *= scale;
	this->x_max *= scale;
	this->y_min *= scale;
	this->y_max *= scale;
}
/** Expands the rectangle; doesn't do checks for inversion. */
static void rectangle4f_expand(struct Rectangle4f *const this,
	const float add) {
	assert(this);
	this->x_min -= add;
	this->x_max += add;
	this->y_min -= add;
	this->y_max += add;
}

static void orthomath_unused_coda(void);
static void orthomath_unused(void) {
	branch_cut_pi_pi(0);
	deg_to_rad(0);
	random_pm_max(0);
	ortho3f_init(0);
	ortho3f_assign(0, 0);
	ortho3f_sum(0, 0);
	ortho3f_sub(0, 0, 0);
	rectangle4i_init(0);
	rectangle4f_init(0);
	rectangle4i_assign(0, 0);
	rectangle4f_assign(0, 0);
	rectangle4f_scale(0, 0.0f);
	rectangle4f_expand(0, 0.0f);
	orthomath_unused_coda();
}
static void orthomath_unused_coda(void) {
	orthomath_unused();
}

#endif /* ORTHO_H */
