/** Orthogonal components header file.

 @title		Ortho
 @author	Neil
 @version	3.3; 2017-06
 @since		3.3; 2017-06 */

#ifndef ORTHO_H
#define ORTHO_H

#include <assert.h>
#include <math.h>	/* fmodf */

/* M_PI is a widely accepted gnu standard, not C<=99 -- who knew? */
#ifndef M_PI_F
#define M_PI_F (3.141592653589793238462643383279502884197169399375105820974944f)
#endif
#ifndef M_2PI_F
#define M_2PI_F (6.283185307179586476925286766559005768394338798750211641949889f)
#endif

/* Break space in pixels up into bins. A {ortho_size} sprite is the maximum
 theoretical sprite size without clipping and collision artifacts, but the
 larger you set this, the farther it has to go to determine whether there's a
 collision, for every sprite. There are 2 parameters that control everything. */
#define ORTHO_BIN_LOG_SPACE (8) /* log_2 bin size */
#define ORTHO_BIN_LOG_SIZE (6) /* log_2 number of bins */
#define ORTHO_BIN_SIZE (1 << ORTHO_BIN_LOG_SIZE) /* size of array of bins */
static const unsigned ortho_bin_size       = ORTHO_BIN_SIZE;
static const unsigned ortho_bin_space      = 1 << ORTHO_BIN_LOG_SPACE;
static const unsigned ortho_bin_half_space = 1 << (ORTHO_BIN_LOG_SPACE - 1);
/* signed maximum value on either side of zero */
static const float ortho_de_sitter
	= (float)(ORTHO_BIN_SIZE * (1 << (ORTHO_BIN_LOG_SPACE - 1)));

/** Position, rotation, and position in the future. */
struct Ortho { float x, y, theta, /* temp */ x1, y1; };
/** 2-vector of floats. */
struct Ortho2f { float x, y; };
/** 3-vector of floats, usually normalised. */
struct Ortho3f { float r, g, b; };

static void Ortho_init(struct Ortho *const this) {
	assert(this);
	this->x  = this->y  = 0.0f;
	this->theta = 0.0f;
}

static void Ortho_assign(struct Ortho *const this,
	const struct Ortho *const that) {
	assert(this);
	assert(that);
	this->x     = that->x;
	this->y     = that->y;
	this->theta = that->theta;
}

static void Ortho_clip_position(struct Ortho *const this) {
	assert(this);
	if(this->x < -ortho_de_sitter) this->x = -ortho_de_sitter;
	else if(this->x > ortho_de_sitter) this->x = ortho_de_sitter;
	if(this->y < -ortho_de_sitter) this->y = -ortho_de_sitter;
	else if(this->y > ortho_de_sitter) this->y = ortho_de_sitter;
	/* branch cut -pi, pi */
	this->theta = fmodf(this->theta, M_PI_F);
}

/** Maps a location component into a bin. */
static unsigned Ortho_location_to_bin(const float x, const float y) {
	int bin_x = (int)x / ortho_bin_space + ortho_bin_half_space;
	int bin_y = (int)y / ortho_bin_space + ortho_bin_half_space;
	if(bin_x < 0) bin_x = 0;
	else if((unsigned)bin_x >= ortho_bin_size) bin_x = ortho_bin_size - 1;
	if(bin_y < 0) bin_y = 0;
	else if((unsigned)bin_y >= ortho_bin_size) bin_y = ortho_bin_size - 1;
	return (unsigned)((bin_y << ORTHO_BIN_LOG_SIZE) + bin_x);
}

static void ortho_unused_coda(void);
static void ortho_unused(void) {
	Ortho_init(0);
	Ortho_assign(0, 0);
	Ortho_clip_position(0);
	Ortho_location_to_bin(0.0f, 0.0f);
	ortho_unused_coda();
}
static void ortho_unused_coda(void) {
	ortho_unused();
}

#endif /* ORTHO_H */
