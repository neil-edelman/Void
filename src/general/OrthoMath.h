/** Orthogonal math components header file.

 @title		OrthoMath.h
 @author	Neil
 @version	3.3; 2017-06
 @since		3.3; 2017-06 */

#ifndef ORTHOMATH_H
#define ORTHOMATH_H

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

#define ONE_S_TO_ONE_MS_F (0.001f)

/* changed? update shader Far.vs! should be 0.00007:14285.714; space is
 big! too slow to get anywhere, so fudge it; basically, this makes space much
 smaller: ratio of fg:bg distance. */
#define FG_BG_FORSHORTENING_F (0.2f)
#define BG_FG_ONE_FORESHORTENING_F (5.0f)
/* 1 guarantees that an object is detected within BIN_FG_SPACE; less will make
 it n^2 faster, but could miss something. */
#define SHORTCUT_COLLISION (0.5f)

/* 16384px de sitter (8192px on each quadrant,) divided between positive and
 negative for greater floating point accuracy */
#define BIN_LOG_ENTIRE 14
#define BIN_ENTIRE (1 << BIN_LOG_ENTIRE)
#define BIN_HALF_ENTIRE (1 << (BIN_LOG_ENTIRE - 1))
/* 256px bins in the foreground, (ships, etc.) */
#define BIN_FG_LOG_SPACE 8
#define BIN_FG_SPACE (1 << BIN_FG_LOG_SPACE)
#define BIN_FG_HALF_SPACE (1 << (BIN_FG_LOG_SPACE - 1))
#define BIN_FG_LOG_SIZE (BIN_LOG_ENTIRE - BIN_FG_LOG_SPACE)
#define BIN_FG_SIZE (1 << BIN_FG_LOG_SIZE)
#define BIN_BIN_FG_SIZE (BIN_FG_SIZE * BIN_FG_SIZE)
/* 1024px bins in the background, (planets, etc.) */
#define BIN_BG_LOG_SPACE 10
#define BIN_BG_SPACE (1 << BIN_BG_LOG_SPACE)
#define BIN_BG_HALF_SPACE (1 << (BIN_BG_LOG_SPACE - 1))
#define BIN_BG_LOG_SIZE (BIN_LOG_ENTIRE - BIN_BG_LOG_SPACE)
#define BIN_BG_SIZE (1 << BIN_FG_LOG_SIZE)
#define BIN_BIN_BG_SIZE (BIN_BG_SIZE * BIN_BG_SIZE)

/** {Ortho3f} must be able to be reinterpreted as {Vec3f}. */
/*struct Vec2u { unsigned x, y; };*/
struct Vec2i { int x, y; };
struct Vec2f { float x, y; };
struct Ortho3f { float x, y, theta; };
struct Colour3f { float r, g, b; };
struct Rectangle4i { int x_min, x_max, y_min, y_max; };
struct Rectangle4f { float x_min, x_max, y_min, y_max; };






/** Doesn't check for overflow.
 @return Maps a {bin2u} to a {bin}.
 @fixme Unused? */
/*static unsigned bin2u_to_bin(const struct Vec2u bin2) {
 assert(bin2.x < BIN_FG_SIZE);
 assert(bin2.y < BIN_FG_SIZE);
 return (bin2.y << BIN_FG_LOG_SIZE) + bin2.x;
 }*/
/** Doesn't check for overflow.
 @return Maps a {bin2i} to a {bin}. */
static unsigned bin2i_to_bin(const struct Vec2i bin2) {
	assert(bin2.x >= 0);
	assert(bin2.y >= 0);
	assert(bin2.x < BIN_FG_SIZE);
	assert(bin2.y < BIN_FG_SIZE);
	return (bin2.y << BIN_FG_LOG_SIZE) + bin2.x;
}
/** @return Maps a {bin} to the lower left corner of the pixel
 c\:o-ordinates. */
static void bin_to_bin2i(const unsigned bin, struct Vec2i *const bin2) {
	assert(bin < BIN_BIN_FG_SIZE);
	assert(bin2);
	bin2->x = bin & (BIN_FG_SIZE - 1);
	bin2->y = bin >> BIN_FG_LOG_SIZE;
}
/** Maps a {Vec2f} into a {bin}. */
static void Vec2f_to_fg_bin(const struct Vec2f *const x, unsigned *const bin) {
	struct Vec2i b;
	assert(x);
	assert(bin);
	b.x = ((int)x->x + BIN_HALF_ENTIRE) >> BIN_FG_LOG_SPACE;
	if(b.x < 0) b.x = 0; else if(b.x >= BIN_FG_SIZE) b.x = BIN_FG_SIZE - 1;
	b.y = ((int)x->y + BIN_HALF_ENTIRE) >> BIN_FG_LOG_SPACE;
	if(b.y < 0) b.y = 0; else if(b.y >= BIN_FG_SIZE) b.y = BIN_FG_SIZE - 1;
	*bin = (b.y << BIN_FG_LOG_SIZE) + b.x;
}
/** Maps a {Vec2f} into a bg {bin}. */
static void Vec2f_to_bg_bin(const struct Vec2f *const x, unsigned *const bin) {
	struct Vec2i b;
	assert(x);
	assert(bin);
	b.x = ((int)x->x + BIN_HALF_ENTIRE) >> BIN_BG_LOG_SPACE;
	if(b.x < 0) b.x = 0; else if(b.x >= BIN_BG_SIZE) b.x = BIN_BG_SIZE - 1;
	b.y = ((int)x->y + BIN_HALF_ENTIRE) >> BIN_BG_LOG_SPACE;
	if(b.y < 0) b.y = 0; else if(b.y >= BIN_BG_SIZE) b.y = BIN_BG_SIZE - 1;
	*bin = (b.y << BIN_BG_LOG_SIZE) + b.x;
}
/** Maps the lower-left corner of a {Bin} to a {Vec2i}. */
static void bin_to_Vec2i(const unsigned bin, struct Vec2i *const x) {
	assert(bin < BIN_BIN_FG_SIZE);
	assert(x);
	x->x = ((bin & (BIN_FG_SIZE - 1)) << BIN_FG_LOG_SPACE) - BIN_HALF_ENTIRE;
	x->y = ((bin >> BIN_FG_LOG_SIZE) << BIN_FG_LOG_SPACE) - BIN_HALF_ENTIRE;
}

/** Assigns a random position. */
static void Vec2f_filler_fg(struct Vec2f *const this) {
	assert(this);
	this->x = 1.0f * rand() / RAND_MAX * BIN_ENTIRE - BIN_HALF_ENTIRE;
	this->y = 1.0f * rand() / RAND_MAX * BIN_ENTIRE - BIN_HALF_ENTIRE;
}
/** Assigns zero to {Ortho3f}. */
static void Ortho3f_init(struct Ortho3f *const this) {
	assert(this);
	this->x  = this->y  = 0.0f;
	this->theta = 0.0f;
}
/** Assigns {that} to {this}. */
static void Ortho3f_assign(struct Ortho3f *const this,
	const struct Ortho3f *const that) {
	assert(this);
	assert(that);
	this->x     = that->x;
	this->y     = that->y;
	this->theta = that->theta;
}
/** Assigns a random {Ortho3f} orientation. */
static void Ortho3f_filler_fg(struct Ortho3f *const this) {
	assert(this);
	Vec2f_filler_fg((struct Vec2f *)this);
	this->theta = M_2PI_F * rand() / RAND_MAX - M_PI_F;
}
/** Assigns zero to an {Ortho3f}. */
static void Ortho3f_filler_zero(struct Ortho3f *const this) {
	assert(this);
	this->x = this->y = this->theta = 0.0f;
}



#if 0
/* de_sitter = BIN_HALF_ENTIRE */
static void Ortho3f_clip_position(struct Ortho3f *const this) {
	assert(this);
	if(this->x < -de_sitter) this->x = -de_sitter;
	else if(this->x > de_sitter) this->x = de_sitter;
	if(this->y < -de_sitter) this->y = -de_sitter;
	else if(this->y > de_sitter) this->y = de_sitter;
	/* branch cut -pi, pi */
	this->theta = fmodf(this->theta, M_PI_F);
}
#endif

/** Assigns zero. */
static void Rectangle4f_init(struct Rectangle4f *const this) {
	assert(this);
	this->x_min = this->x_max = this->y_min = this->y_max = 0;
}
/** Assigns {that} to {this}. */
static void Rectangle4i_assign(struct Rectangle4i *const this,
	const struct Rectangle4i *const that) {
	assert(this);
	assert(that);
	this->x_min = that->x_min;
	this->x_max = that->x_max;
	this->y_min = that->y_min;
	this->y_max = that->y_max;
}
/** Maps a recangle from pixel space, {pixel}, to bin2 space, {bin}. */
static void Rectangle4f_to_fg_bin4(const struct Rectangle4f *const pixel,
	struct Rectangle4i *const bin) {
	int temp;
	assert(pixel);
	assert(pixel->x_min <= pixel->x_max);
	assert(pixel->y_min <= pixel->y_max);
	assert(bin);
	temp = ((int)pixel->x_min + BIN_HALF_ENTIRE) >> BIN_FG_LOG_SPACE;
	if(temp < 0) temp = 0;
	else if((unsigned)temp >= BIN_FG_SIZE) temp = BIN_FG_SIZE - 1;
	bin->x_min = temp;
	/*printf("x_min %f -> %d\n", pixel->x_min, temp);*/
	temp = ((int)pixel->x_max + 1 + BIN_HALF_ENTIRE) >> BIN_FG_LOG_SPACE;
	if(temp < 0) temp = 0;
	else if((unsigned)temp >= BIN_FG_SIZE) temp = BIN_FG_SIZE - 1;
	bin->x_max = temp;
	temp = ((int)pixel->y_min + BIN_HALF_ENTIRE) >> BIN_FG_LOG_SPACE;
	if(temp < 0) temp = 0;
	else if((unsigned)temp >= BIN_FG_SIZE) temp = BIN_FG_SIZE - 1;
	bin->y_min = temp;
	temp = ((int)pixel->y_max + 1 + BIN_HALF_ENTIRE) >> BIN_FG_LOG_SPACE;
	if(temp < 0) temp = 0;
	else if((unsigned)temp >= BIN_FG_SIZE) temp = BIN_FG_SIZE - 1;
	bin->y_max = temp;
}
static void Rectangle4f_to_bg_bin4(const struct Rectangle4f *const pixel,
	struct Rectangle4i *const bin) {
	int temp;
	assert(pixel);
	assert(pixel->x_min <= pixel->x_max);
	assert(pixel->y_min <= pixel->y_max);
	assert(bin);
	temp = ((int)pixel->x_min + BIN_HALF_ENTIRE) >> BIN_BG_LOG_SPACE;
	if(temp < 0) temp = 0;
	else if((unsigned)temp >= BIN_BG_SIZE) temp = BIN_BG_SIZE - 1;
	bin->x_min = temp;
	temp = ((int)pixel->x_max + 1 + BIN_HALF_ENTIRE) >> BIN_BG_LOG_SPACE;
	if(temp < 0) temp = 0;
	else if((unsigned)temp >= BIN_BG_SIZE) temp = BIN_BG_SIZE - 1;
	bin->x_max = temp;
	temp = ((int)pixel->y_min + BIN_HALF_ENTIRE) >> BIN_BG_LOG_SPACE;
	if(temp < 0) temp = 0;
	else if((unsigned)temp >= BIN_BG_SIZE) temp = BIN_BG_SIZE - 1;
	bin->y_min = temp;
	temp = ((int)pixel->y_max + 1 + BIN_HALF_ENTIRE) >> BIN_BG_LOG_SPACE;
	if(temp < 0) temp = 0;
	else if((unsigned)temp >= BIN_BG_SIZE) temp = BIN_BG_SIZE - 1;
	bin->y_max = temp;
}

static void orthomath_unused_coda(void);
static void orthomath_unused(void) {
	struct Vec2i w = { 0, 0 };
	bin2i_to_bin(w);
	bin_to_bin2i(0, 0);
	Vec2f_to_fg_bin(0, 0);
	Vec2f_to_bg_bin(0, 0);
	bin_to_Vec2i(0, 0);

	Vec2f_filler_fg(0);
	Ortho3f_filler_fg(0);
	Ortho3f_init(0);
	Ortho3f_assign(0, 0);
	Ortho3f_filler_fg(0);
	Ortho3f_filler_zero(0);

	Rectangle4f_init(0);
	Rectangle4i_assign(0, 0);
	Rectangle4f_to_fg_bin4(0, 0);
	Rectangle4f_to_bg_bin4(0, 0);

	orthomath_unused_coda();
}
static void orthomath_unused_coda(void) {
	orthomath_unused();
}

#endif /* ORTHOMATH_H */
