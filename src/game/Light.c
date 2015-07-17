/* Copyright 2000, 2013 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt */

#include <stdio.h>  /* fprintf */
#include <string.h> /* memcpy */
#include "Light.h"

struct Vec2f { float x, y; };
struct Colour3f { float r, g, b; };

/** Lighting effects! Unfortunately, UBOs, which allow structs, were introduced
 in GLSL3.1; using 1.1 for compatibility, and only have 1.2. Ergo, this is a
 little bit messy. Maybe in 10 years I'll do a change.
 <p>
 Fixme: the other way, viz, having a pointer to change is probably better and
 more straightforward. (Fixed)
 @author	Neil
 @version	3.0, 05-2015
 @since		1.0, 2000 */

/* must be the same as in Lighting.fs */
#define MAX_LIGHTS (64)

/* private */
/*static int find_new_hash(void);*/

static int             no_lights;
static struct Vec2f    position[MAX_LIGHTS];
static struct Colour3f colour[MAX_LIGHTS];
static int             *notify[MAX_LIGHTS];
/*
static int             hash[MAX_LIGHTS];
static int             next_hash;
static int             hash_table_p1[MAX_LIGHTS * 2];
const static int max_hash = sizeof(hash_table_p1) / sizeof(int); *//* must be a power of two */

/* This is what I want to do!
static struct Light {
	float           intensity;
	struct Vec2f    position;
	struct Colour3f colour;
} light[64];
const static int max_lights = sizeof(light) / sizeof(struct Light);*/

/* public */

/** Constructor.
 @param i	The intensity, <tt>i > 0</tt>.
 @param r
 @param g
 @param b	The colours, <tt>0 >= x >= 1</tt>.
 @return	An index that Light_ will destroy or zero on failure. */
int Light(const float i, const float r, const float g, const float b) {
	/*int h;*/

	if(r < 0.0f || g < 0.0f || b < 0.0f || r > 1.0f || g > 1.0f || b > 1.0f) {
		fprintf(stderr, "Light: invalid colour, [%f, %f, %f].\n", r, g, b);
		return 0;
	}
	if(no_lights >= MAX_LIGHTS) {
		fprintf(stderr, "Light: reached limit of %d/%d lights.\n", no_lights, MAX_LIGHTS);
		return 0;
	}
	/*h = find_new_hash();*/
	position[no_lights].x = 0.0f;
	position[no_lights].y = 0.0f;
	colour[no_lights].r   = i * r;
	colour[no_lights].g   = i * g;
	colour[no_lights].b   = i * b;
	notify[no_lights]     = 0;
	fprintf(stderr, "Light: created from pool, colour (%f,%f,%f), Lgt%u.\n", colour[no_lights].r, colour[no_lights].g, colour[no_lights].b, no_lights + 1);
	/*hash[no_lights]       = h;
	hash_table_p1[h]      = no_lights + 1;
	fprintf(stderr, "Light: new, colour (%f,%f,%f) at (%f,%f), light %u, #%u.\n", colour[no_lights].r, colour[no_lights].g, colour[no_lights].b, 0.0f, 0.0f, no_lights, h + 1);
	no_lights++;

	return h + 1;*/
	return ++no_lights;
}

/** Destructor of no, will update with new particle at no.
 @param index	The light that's being destroyed. */
void Light_(const int h_p1) {
	/*const int h = h_p1 - 1;
	int no;

	if(h < 0 || h >= max_hash) return;
	if((no = hash_table_p1[h] - 1) == -1) {
		fprintf(stderr, "~Light: #%u is not a light.\n", h_p1);
		return;
	}
	hash_table_p1[h] = 0;*/
	const int no = h_p1 - 1;
	if(h_p1 == 0) return;
	if(no < 0 || no >= no_lights) {
		fprintf(stderr, "~Light: Lgt%u, out-of-bounds (index %u.)\n", h_p1, no);
		return;
	}
	fprintf(stderr, "~Light: erase, turning off light (index %u,) Lgt%u.\n", no, h_p1);

	/* place the no_lights item into this one, decrese #; nobody will know */
	if(no < --no_lights) {
		/*int rep = no_lights, replace_hash;*/
		position[no].x = position[no_lights].x;
		position[no].y = position[no_lights].y;
		colour[no].r   = colour[no_lights].r;
		colour[no].g   = colour[no_lights].g;
		colour[no].b   = colour[no_lights].b;
		notify[no]     = notify[no_lights];
		if(notify[no]) *notify[no] = no + 1;
		/*replace_hash   = hash[no_lights];
		hash_table_p1[replace_hash] = no + 1;*/
		fprintf(stderr, "~Light: Lgt%u is now Lgt%u.\n", no_lights + 1, no + 1);
	}
}

/** Lights can change number; this will notify a location of the new pointer.
 @param no_p1		The Sprite number.
 @param no_p1_ptr	Notify this. */
void LightSetContainer(int *const no_p1_ptr) {
	int no_p1, no;
	if(!no_p1_ptr || !(no_p1 = *no_p1_ptr) || (no = no_p1 - 1) < 0 || no >= MAX_LIGHTS) return;
	notify[no] = no_p1_ptr;
}

/** @return		The number of lights active. */
int LightGetNo(void) {
	return no_lights;
}

/** @return		The positions of the lights. */
struct Vec2f *LightGetPositions(void) {
	return position;
}

/**
 @param index	Index.
 @param x
 @param y		*/
void LightSetPosition(const int h_p1, const float x, const float y) {
	/*const int h = h_p1 - 1;
	int no;*/
	const int no = h_p1 - 1;

	/*if(h < 0 || h >= max_hash || (no = hash_table_p1[h] - 1) == -1) return;*/
	if(no < 0 || no >= MAX_LIGHTS) return;
	position[no].x = x;
	position[no].y = y;
}

/** @return		The colours of the lights. */
struct Colour3f *LightGetColours(void) {
	return colour;
}

/* fixme: clear */

#if 0
/** Find an index that's 0. */
static int find_new_hash(void) {
	/* assert: max_indeces must be >= MAX_LIGHTS and a power of two */
	while(hash_table_p1[next_hash] != 0) {
		fprintf(stderr, "Light::find_new_hash: collision.\n");
		next_hash = (next_hash + 1) & (max_hash - 1);
	}
	return next_hash++;
}
#endif
