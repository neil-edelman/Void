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

#define PRINT_PEDANTIC

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
static unsigned        *notify[MAX_LIGHTS];

/* public */

/** Constructor.
 @param light_ptr	The pointer where this light lives.
 @param i			The intensity, i > 0.
 @param r,g,b		The colours, [0, 1].
 @return			Boolean true on success; the light_ptr has the value. */
int Light(unsigned *const light_ptr, const float i, const float r, const float g, const float b) {
	unsigned light;

	if(!light_ptr) {
		Warn("Light: light_ptr is null; where do you want the light to go?\n");
		return 0;
	}
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
	notify[light]     = light_ptr;
	*light_ptr         = light;
	Pedantic("Light: created from pool, %s #%p.\n", LightToString(light), light_ptr);
	return -1;
}

/** Destructor of light, will update with new particle at light.
 @param index	The light that's being destroyed. */
void Light_(unsigned *light_ptr) {
	static char *buffer[64];
	unsigned light, replace;

	if(!light_ptr) return;
	if((light = *light_ptr) >= lights_size) {
		Debug("~Light: %u/%u out-of-bounds.\n", light + 1, lights_size);
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
		Pedantic("#%p = %u\n", notify[light], light);
		if(notify[light]) *notify[light] = light;
		snprintf((char *)buffer, sizeof buffer, "; %s is now %s", LightToString(replace), LightToString(light));
	}
	Pedantic("~Light: erase %s%s.\n", LightToString(light), buffer);
	lights_size = replace;
	*light_ptr = light = 0;
}

/** Sets the size to zero, clearing all lights. Fixme: Does not notify
 anything; it probably doesn't matter, but this is also useless */
void LightClear(void) {
	Pedantic("Light::clear: clearing Lights.\n");
	lights_size = 0;
}

/** @return		The number of lights active. */
int LightGetArraySize(void) { return lights_size; }

/** @return		The positions of the lights. */
struct Vec2f *LightGetPositionArray(void) { return position; }

/** @return		The colours of the lights. */
struct Colour3f *LightGetColourArray(void) { return colour; }

/**
 @param index	Index.
 @param x
 @param y		*/
void LightSetPosition(const unsigned light, const float x, const float y) {
	if(light >= lights_size) {
		Warn("Light::setPosition: %u/%u not in range.\n");
		return;
	}
	position[light].x = x;
	position[light].y = y;
}

/** This is important because Sprites change places, as well. */
void LightSetNotify(const unsigned light, unsigned *const light_ptr) {
	if(light >= lights_size) {
		Warn("Light::setPosition: %u/%u not in range.\n");
		return;
	}
	notify[light] = light_ptr;
}

/** Volatile-ish: can only print 4 Lights at once. */
char *LightToString(const unsigned light) {
	static int b;
	static char buffer[4][64];

	b &= 3;
	if(light >= lights_size) {
		snprintf(buffer[b], sizeof buffer[b], "%s", "not a light");
	} else {
		/*snprintf(buffer[b], sizeof buffer[b], "Lgh%d(%.1f,%.1f,%.1f)[%.1f,%.1f]", light + 1, colour[light].r, colour[light].g, colour[light].b, position[light].x, position[light].y);*/
		snprintf(buffer[b], sizeof buffer[b], "Lgh%d[%.1f,%.1f]", light + 1, position[light].x, position[light].y);
	}
	return buffer[b++];
}

void LightList(void) {
	unsigned i;
	Info("Lights: { ");
	for(i = 0; i < lights_size; i++) {
		Info("%s%s", LightToString(i), i == lights_size - 1 ? " }\n" : ", ");
	}
}
