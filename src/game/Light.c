/* Copyright 2000, 2013 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt */

/** Lighting effects! Unfortunately, UBOs, which allow structs, were introduced
 in GLSL3.1; using 1.1 for compatibility, and only have 1.2. Ergo, this is a
 little bit messy. Maybe in 10 years I'll do a change.
 <p>
 Lights depend on external unsigned integers to keep track. Don't modify the
 light with a function not in this and don't free the unsigned without calling
 Light_.
 @author	Neil
 @version	3.3, 2016-01
 @since		1.0, 2000 */

#include <stdio.h>  /* fprintf */
#include <string.h> /* memcpy */
#include "../Print.h"
#include "Light.h"

/* must be the same as in Lighting.fs */
#define MAX_LIGHTS (64)

static const unsigned lights_capacity = MAX_LIGHTS;
static unsigned       lights_size;

struct Vec2f { float x, y; };
struct Colour3f { float r, g, b; };

static struct Vec2f    position[MAX_LIGHTS];
static struct Colour3f colour[MAX_LIGHTS];
static int             *notify[MAX_LIGHTS];

unsigned id_to_light(const int id);
int light_to_id(const unsigned light);
char *to_string(const unsigned light);

/* public */

/** Constructor.
 @param id_ptr	The pointer where this light lives.
 @param i		The intensity, i > 0.
 @param r,g,b	The colours, [0, 1].
 @return		Boolean true on success; the id_ptr has (0, MAX_LIGHTS]. */
int Light(int *const id_ptr, const float i, const float r, const float g, const float b) {
	unsigned light;

	if(!id_ptr) {
		Warn("Light: light_ptr is null; where do you want the light to go?\n");
		return 0;
	}
	if(*id_ptr) Warn("Light: overriding %u on notify.\n", *id_ptr);
	if(r < 0.0f || g < 0.0f || b < 0.0f || r > 1.0f || g > 1.0f || b > 1.0f) {
		Warn("Light: invalid colour, [%f, %f, %f].\n", r, g, b);
		return 0;
	}
	if(lights_size >= lights_capacity) {
		Warn("Light: reached limit of %u/%u lights.\n", lights_size, lights_capacity);
		return 0;
	}
	light = lights_size++;
	position[light].x = 0.0f;
	position[light].y = 0.0f;
	colour[light].r   = i * r;
	colour[light].g   = i * g;
	colour[light].b   = i * b;
	notify[light]     = id_ptr;
	*id_ptr           = light_to_id(light);
	Pedantic("Light: created from pool, %s.\n", to_string(light));
	return -1;
}

/** Destructor of light, will update with new particle at light.
 @param index	The light that's being destroyed. */
void Light_(int *id_ptr) {
	static char *buffer[64];
	unsigned light, replace;
	int id;

	if(!id_ptr /*|| !(id = *id_ptr)*/) return; id = *id_ptr;
	if((light = id_to_light(id)) >= lights_size) {
		Debug("~Light: %u/%u out-of-bounds.\n", id, lights_size);
		return;
	}

	buffer[0] = '\0';

	/* place the lights_size item into this one, decrese # */
	if(light < (replace = lights_size - 1)) {
		position[light].x = position[replace].x;
		position[light].y = position[replace].y;
		colour[light].r   = colour[replace].r;
		colour[light].g   = colour[replace].g;
		colour[light].b   = colour[replace].b;
		notify[light]     = notify[replace];
		if(notify[light]) *notify[light] = light_to_id(light);
		snprintf((char *)buffer, sizeof buffer, "; %s is replacing", to_string(replace));
	}
	Pedantic("~Light: erase %s%s.\n", to_string(light), buffer);
	lights_size = replace;
	*id_ptr = 0;
}

/** @return		The number of lights active. */
unsigned LightGetArraySize(void) { return lights_size; }

/** @return		The positions of the lights. */
struct Vec2f *LightGetPositionArray(void) { return position; }

/** @return		The colours of the lights. */
struct Colour3f *LightGetColourArray(void) { return colour; }

/**
 @param index	Index.
 @param x
 @param y		*/
void LightSetPosition(const int id, const float x, const float y) {
	const unsigned light = id_to_light(id);

	if(light >= lights_size) {
		Warn("Light::setPosition: %u/%u not in range.\n", id, lights_size);
		return;
	}
	position[light].x = x;
	position[light].y = y;
}

/** This is important because Sprites change places, as well. */
void LightSetNotify(const int id, int *const id_ptr) {
	const unsigned light = id_to_light(id);

	if(light >= lights_size) {
		Warn("Light::setPosition: %u/%u not in range.\n", id, lights_size);
		return;
	}
	notify[light] = id_ptr;
}

/** Volatile-ish: can only print 4 Lights at once. */
char *LightToString(const int id) { return to_string(id_to_light(id)); }

void LightList(void) {
	unsigned i;
	Info("Lights: { ");
	for(i = 0; i < lights_size; i++) {
		Info("%s%s", to_string(i), i == lights_size - 1 ? " }\n" : ", ");
	}
}

unsigned id_to_light(const int id) { return id/* - 1*/; }

int light_to_id(const unsigned light) { return light/* + 1*/; }

char *to_string(const unsigned light) {
	static int b;
	static char buffer[4][64];
	
	b &= 3;
	if(light >= lights_size) {
		snprintf(buffer[b], sizeof buffer[b], "%s (Lgh%d)", "not a light", light_to_id(light));
	} else {
		/*snprintf(buffer[b], sizeof buffer[b], "Lgh%d(%.1f,%.1f,%.1f)[%.1f,%.1f]", id, colour[light].r, colour[light].g, colour[light].b, position[light].x, position[light].y);*/
		snprintf(buffer[b], sizeof buffer[b], "Lgh%d[%.1f,%.1f]", light_to_id(light), position[light].x, position[light].y);
	}
	return buffer[b++];
}
