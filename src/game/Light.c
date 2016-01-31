/* Copyright 2000, 2013 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt */

/** Lighting effects! Unfortunately, UBOs, which allow structs, were introduced
 in GLSL3.1; using 1.1 for compatibility, and only have 1.2. Ergo, this is a
 little bit messy. Maybe in 10 years I'll do a change.
 <p>
 Lights depend on external unsigned integers to keep track. Don't modify the
 light with a function not in this and don't free the unsigned without calling
 Light_.
 <p>
 Fixme: order the lights by distance/brighness and take the head of the list,
 and ignore all that pass a certain threshold. This will scale way better.
 @author	Neil
 @version	3.3, 2016-01
 @since		1.0, 2000 */

#include <stdio.h>  /* fprintf */
#include <string.h> /* memcpy */
#include "../Print.h"
#include "Light.h"

//////////// REMOVE
#include "Sprite.h"

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
 @param i		The intensity, i >= 0.
 @param r,g,b	The colours, [0, 1].
 @return		Boolean true on success; the id_ptr has (0, MAX_LIGHTS]. */
int Light(int *const id_ptr, const float i, const float r, const float g, const float b, const struct Sprite *const s) {
	unsigned light;

	if(!id_ptr) {
		Warn("Light: light_ptr is null; where do you want the light to go?\n");
		return 0;
	}
	/* Naah, this can be un-initialised
	 if(*id_ptr) Warn("Light: overriding %u on notify.\n", *id_ptr);*/
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
	Pedantic("Light: created %s.\n", to_string(light));
	Debug("\t%u.notify = id#%p(%d) create (lights %u) for %s\n", light, notify[light], *notify[light], lights_size, s ? SpriteToString(s) : "<not a sprite>");
	return -1;
}

/** Destructor of light, will update with new particle at light.
 @param index	The light that's being destroyed. */
void Light_(int *id_ptr) {
	unsigned light, replace;
	int id;
	unsigned characters;
	char buffer[128];

	if(!id_ptr || !(id = *id_ptr)) return;
	if((light = id_to_light(id)) >= lights_size) {
		Debug("~Light: %u/%u out-of-bounds.\n", id, lights_size);
		Debug("\t****************NOOOOOOOOO%c\n", 'O');
		return;
	}

	Debug("\t%u.notify = id#%p(%d) delete (lights %u) -> id#%p(0)\n", light, notify[light], *notify[light], lights_size, notify[light]);
	/* notify that we're deleting */
	if(notify[light]) *notify[light] = 0;

	/* store the string for debug info */
	characters = snprintf(buffer, sizeof buffer, "%s", to_string(light));

	/* place the lights_size item into this one, decrese # */
	if(light < (replace = lights_size - 1)) {
		Debug("\t%u.notify = id#%p(%d) replace (lights %u->%u)\n", replace, notify[replace], *notify[replace], lights_size, replace);
		/* change */
		position[light].x = position[replace].x;
		position[light].y = position[replace].y;
		colour[light].r   = colour[replace].r;
		colour[light].g   = colour[replace].g;
		colour[light].b   = colour[replace].b;
		notify[light]     = notify[replace];
		/* notify that we're changing */
		if(notify[light]) *notify[light] = light_to_id(light);
		Debug("\t%u.notify = id#%p(%d) replaced and updated (lights %u)\n", light, notify[light], *notify[light], replace);

		if(characters < sizeof buffer) snprintf(buffer + characters, sizeof buffer - characters, "; replaced by %s", to_string(replace));
	}
	Pedantic("~Light: erase %s.\n", buffer);
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

	if(!id) return;

	if(light >= lights_size) {
		Warn("Light::setPosition: %u/%u not in range.\n", id, lights_size);
		return;
	}
	position[light].x = x;
	position[light].y = y;
}

/** This is important because Sprites change places, as well. */
void LightSetNotify(int *const id_ptr) {
	int id;
	unsigned light;

	if(!id_ptr || !(id = *id_ptr)) return;
	light = id_to_light(id);
	if(light >= lights_size) {
		Warn("Light::setNotify: %u/%u not in range.\n", id, lights_size);
		return;
	}
	Debug("\t%u.notify = id#%p(%d) -> %u.notify = id#%p(%d) (lights %u)\n", light, notify[light], *notify[light], light, id_ptr, id, lights_size);
	notify[light] = id_ptr;
}

/** Volatile-ish: can only print 4 Lights at once. */
char *LightToString(const int id) {
	if(!id) return "null light";
	return to_string(id_to_light(id));
}

void LightList(void) {
	unsigned i;
	Info("Lights: { ");
	for(i = 0; i < lights_size; i++) {
		Info("%s%s", to_string(i), i == lights_size - 1 ? " " : ", ");
	}
	Info("}\n");
}

unsigned id_to_light(const int id) { return id - 1; }

int light_to_id(const unsigned light) { return light + 1; }

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

#include "../general/Random.h"
#include "../../bin/Auto.h"

static int lgt_bubbles[64];
static struct Sprite *spr_bubbles[64];
static int bubble = 0;

void BubblePush(void) {
	float x = RandomUniformFloat(300.0f);
	float y = RandomUniformFloat(300.0f);
	if(bubble >= 64) return;
	LightList();
	Debug("Creating Bubble %u\n", bubble + 1);
	Light(&lgt_bubbles[bubble], 32.0f, 1.0f, 1.0f, 1.0f, 0);
	LightSetPosition(lgt_bubbles[bubble], x, y);
	spr_bubbles[bubble] = Sprite(SP_ETHEREAL, AutoImageSearch("AsteroidSmall.png"), (int)x, (int)y, 0.0f);
	bubble++;
	LightList();
}

void BubblePop(void) {
	if(bubble <= 0) return;
	LightList();
	Debug("Deleting Bubble %u\n", bubble);
	bubble--;
	Light_(&lgt_bubbles[bubble]);
	Sprite_(&spr_bubbles[bubble]);
	LightList();
}
