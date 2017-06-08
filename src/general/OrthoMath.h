/** Orthogonal math components header file.

 @title		OrthoMath.h
 @author	Neil
 @version	3.3; 2017-06
 @since		3.3; 2017-06 */

#ifndef ORTHOMATH_H
#define ORTHOMATH_H

#include <assert.h>
#include <math.h>	/* fmodf */

/* M_PI is a widely accepted gnu standard, not C<=99 -- who knew? */
#ifndef M_PI_F
#define M_PI_F (3.141592653589793238462643383279502884197169399375105820974944f)
#endif
#ifndef M_2PI_F
#define M_2PI_F \
	(6.283185307179586476925286766559005768394338798750211641949889f)
#endif

/** {Ortho3f} must be able to be reinterpreted as {Vec3f}. */
struct Ortho3f { float x, y, theta; };
struct Vec2f { float x, y; };
struct Vec2i { int x, y; };
struct Vec2u { unsigned x, y; };
struct Colour3f { float r, g, b; };
struct Rectangle4f { float x_min, x_max, y_min, y_max; };

/* Break space in pixels up into bins. A {ortho_size} sprite is the maximum
 theoretical sprite size without clipping artifacts. It is also the range of a
 sprite in one frame without collision artifacts. The larger you set this, the
 farther it has to go to determine whether there's a collision, for every
 sprite. There are 2 parameters that control everything. */
#define BIN_LOG_SPACE (8) /* log_2 bin space in pixels */
#define BIN_LOG_SIZE (6) /* log_2 number of bins across/vertically */
#define BIN_SIZE (1 << BIN_LOG_SIZE) /* size of array of bins (64) */
#define BIN_BIN_SIZE (BIN_SIZE * BIN_SIZE) /* size of 2d array (4096) */
static const unsigned bin_size       = BIN_SIZE;
static const unsigned bin_space      = 1 << BIN_LOG_SPACE; /* 256 pixels */
static const unsigned bin_half_space = 1 << (BIN_LOG_SPACE - 1);
/* signed maximum value on either side of zero */
static const float de_sitter
	= (float)(BIN_SIZE * (1 << (BIN_LOG_SPACE - 1)));

static void Ortho3f_init(struct Ortho3f *const this) {
	assert(this);
	this->x  = this->y  = 0.0f;
	this->theta = 0.0f;
}

static void Ortho3f_assign(struct Ortho3f *const this,
	const struct Ortho3f *const that) {
	assert(this);
	assert(that);
	this->x     = that->x;
	this->y     = that->y;
	this->theta = that->theta;
}

/*static void Ortho5f_assign3f(struct Ortho5f *const this,
	const struct Ortho3f *const that) {
	Ortho3f_assign3f((struct Ortho3f *)this, that);
	this->x1    = that->x;
	this->y1    = that->y;
}*/

static void Ortho3f_clip_position(struct Ortho3f *const this) {
	assert(this);
	if(this->x < -de_sitter) this->x = -de_sitter;
	else if(this->x > de_sitter) this->x = de_sitter;
	if(this->y < -de_sitter) this->y = -de_sitter;
	else if(this->y > de_sitter) this->y = de_sitter;
	/* branch cut -pi, pi */
	this->theta = fmodf(this->theta, M_PI_F);
}

static void Rectangle4f_init(struct Rectangle4f *const this) {
	assert(this);
	this->x_min = this->x_max = this->y_min = this->y_max = 0;
}

/** Maps a {bin2} to a {bin}. Doesn't check for overflow.
 @return Success. */
static unsigned bin2_to_bin(const struct Vec2u bin2) {
	assert(bin2.x < bin_size);
	assert(bin2.y < bin_size);
	return (bin2.y << BIN_LOG_SIZE) + bin2.x;
}

/** Maps a location component into a bin. */
static unsigned location_to_bin(const struct Vec2f x) {
	struct Vec2i bin2 = { (int)x.x / bin_space + bin_half_space,
		(int)x.y / bin_space + bin_half_space };
	struct Vec2u bin2u;
	if(bin2.x < 0) bin2u.x = 0;
	else if((unsigned)bin2.x >= bin_size) bin2u.x = bin_size - 1;
	else bin2u.x = bin2.x;
	if(bin2.y < 0) bin2u.y = 0;
	else if((unsigned)bin2.y >= bin_size) bin2u.y = bin_size - 1;
	else bin2u.y = bin2.y;
	return bin2_to_bin(bin2u);
}

/** Maps a recangle with centre {center} and radius {extent}, to an area in bin
 space, {neg} -- {pos}. */
static void rectangle_to_bin(const struct Vec2f centre,
	const struct Vec2f extent,
	struct Vec2u *const neg, struct Vec2u *const pos) {
	int temp;
	assert(neg);
	assert(pos);
	temp = ((int)(centre.x - extent.x) >> BIN_LOG_SPACE) +bin_half_space;
	if(temp < 0) temp = 0;
	else if(((unsigned)(temp)) >= bin_size) temp = (int)bin_size - 1;
	neg->x = (unsigned)temp;
	temp = ((int)(centre.y - extent.y) >> BIN_LOG_SPACE) + bin_half_space;
	if(temp < 0) temp = 0;
	else if((unsigned)temp >= bin_size) temp = (int)bin_size - 1;
	neg->y = (unsigned)temp;
	temp = ((int)(centre.x + extent.x + 1) >> BIN_LOG_SPACE) + bin_half_space;
	if(temp < 0) temp = 0;
	else if((unsigned)temp >= bin_size) temp = (int)bin_size - 1;
	pos->x = (unsigned)temp;
	temp = ((int)(centre.y + extent.y + 1) >> BIN_LOG_SPACE) + bin_half_space;
	if(temp < 0) temp = 0;
	else if((unsigned)temp >= bin_size) temp = (int)bin_size - 1;
	pos->y = (unsigned)temp;
}

static void orthomath_unused_coda(void);
static void orthomath_unused(void) {
	struct Vec2f v = { 0.0f, 0.0f };
	Ortho3f_init(0);
	Ortho3f_assign(0, 0);
	/*Ortho5f_assign3f(0, 0);*/
	Ortho3f_clip_position(0);
	Rectangle4f_init(0);
	location_to_bin(v);
	rectangle_to_bin(v, v, 0, 0);
	orthomath_unused_coda();
}
static void orthomath_unused_coda(void) {
	orthomath_unused();
}

#endif /* ORTHOMATH_H */
