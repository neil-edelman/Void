/** Copyright 2000, 2013 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt.

 Lighting effects! Unfortunately, UBOs, which allow structs, were introduced
 in GLSL3.1; using 1.1 for compatibility, and only have 1.2. Ergo, this is a
 little bit messy. Maybe in 10 years I'll do a change.

 Lights depend on external unsigned integers to keep track. Don't modify the
 light with a function not in this and don't free the unsigned without calling
 Light_.

 @title		Light
 @author	Neil
 @version	3.3, 2016-01
 @since		1.0, 2000
 @fixme		Order the lights by distance/brighness and take the head of the
 list, and ignore all that pass a certain threshold. This will scale better. */

#include <stdio.h>  /* fprintf */
#include <string.h> /* memcpy */
#include "../Print.h"
#include "../general/Orcish.h"
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
static char            label[MAX_LIGHTS][16];

unsigned id_to_light(const int id);
int light_to_id(const unsigned light);
char *to_string(const unsigned light);

/* public */

/** Constructor.
 @param id_ptr	The pointer where this light lives.
 @param i		The intensity, i >= 0.
 @param r,g,b	The colours, [0, 1].
 @return		Boolean true on success; the id_ptr has (0, MAX_LIGHTS]. */
int Light(unsigned *const id_ptr, const float i, const float r, const float g, const float b) {
	unsigned light;

	if(!id_ptr) {
		warn("Light: light_ptr is null; where do you want the light to go?\n");
		return 0;
	}
	/* Naah, this can be un-initialised
	 if(*id_ptr) warn("Light: overriding %u on notify.\n", *id_ptr);*/
	if(r < 0.0f || g < 0.0f || b < 0.0f || r > 1.0f || g > 1.0f || b > 1.0f) {
		warn("Light: invalid colour, [%f, %f, %f].\n", r, g, b);
		return 0;
	}
	if(lights_size >= lights_capacity) {
		warn("Light: reached limit of %u/%u lights.\n", lights_size, lights_capacity);
		return 0;
	}
	light = lights_size++;
	position[light].x = 0.0f;
	position[light].y = 0.0f;
	colour[light].r   = i * r;
	colour[light].g   = i * g;
	colour[light].b   = i * b;
	notify[light]     = id_ptr;
	Orcish(label[light], sizeof label[light]);
	*id_ptr           = light_to_id(light);
	pedantic("Light: created %s.\n", to_string(light));
	return -1;
}

/** Destructor of light, will update with new particle at light.
 @param index	The light that's being destroyed. */
void Light_(unsigned *id_ptr) {
	unsigned light, replace;
	int id;
	unsigned characters;
	char buffer[128];

	if(!id_ptr || !(id = *id_ptr)) return;
	if((light = id_to_light(id)) >= lights_size) {
		warn("~Light: %u/%u out-of-bounds.\n", id, lights_size);
		return;
	}

	/* notify that we're deleting */
	if(notify[light]) *notify[light] = 0;

	/* store the string for debug info */
	characters = snprintf(buffer, sizeof buffer, "%s", to_string(light));

	/* place the lights_size item into this one, decrese # */
	if(light < (replace = lights_size - 1)) {
		/* change */
		position[light].x = position[replace].x;
		position[light].y = position[replace].y;
		colour[light].r   = colour[replace].r;
		colour[light].g   = colour[replace].g;
		colour[light].b   = colour[replace].b;
		notify[light]     = notify[replace];
		strncpy(label[light], label[replace], sizeof label[light]);
		/*label[light]      = label[replace];*/
		/* notify that we're changing */
		if(notify[light]) *notify[light] = light_to_id(light);

		if(characters < sizeof buffer) snprintf(buffer + characters, sizeof buffer - characters, "; replaced by %s", to_string(replace));
	}
	pedantic("~Light: erase %s.\n", buffer);
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
		warn("LightSetPosition: %u/%u not in range.\n", id, lights_size);
		return;
	}
	position[light].x = x;
	position[light].y = y;
}

/** This is important because Sprites change places, as well. */
void LightSetNotify(unsigned *const id_ptr) {
	int id;
	unsigned light;

	if(!id_ptr || !(id = *id_ptr)) return;
	light = id_to_light(id);
	if(light >= lights_size) {
		warn("LightSetNotify: %u/%u not in range.\n", id, lights_size);
		return;
	}
	notify[light] = id_ptr;
}

/** Volatile-ish: can only print 4 Lights at once. */
const char *LightToString(const int id) {
	if(!id) return "null light";
	return to_string(id_to_light(id));
}

void LightList(void) {
	unsigned i;
	info("Lights: { ");
	for(i = 0; i < lights_size; i++) {
		info("%s%s", to_string(i), i == lights_size - 1 ? " " : ", ");
	}
	info("}\n");
}

unsigned id_to_light(const int id) { return id - 1; }

int light_to_id(const unsigned light) { return light + 1; }

char *to_string(const unsigned light) {
	static int b;
	static char buffer[4][64];
	
	b &= 3;
	if(light >= lights_size) {
		snprintf(buffer[b], sizeof buffer[b], "not a light (#%d)", light_to_id(light));
	} else {
		/*snprintf(buffer[b], sizeof buffer[b], "Lgh%d(%.1f,%.1f,%.1f)[%.1f,%.1f]", id, colour[light].r, colour[light].g, colour[light].b, position[light].x, position[light].y);*/
		/*snprintf(buffer[b], sizeof buffer[b], "Lgh%d[%.1f,%.1f]", light_to_id(light), position[light].x, position[light].y);*/
		/*snprintf(buffer[b], sizeof buffer[b], "Lgh%d[#%p]", light_to_id(light), (void *)notify[light]);*/
		snprintf(buffer[b], sizeof buffer[b], "%s[#%d no#%d]", label[light], light_to_id(light), notify[light] ? *notify[light] : 0);
	}
	return buffer[b++];
}

/*#include "../general/Random.h"
#include "../../bin/Auto.h"

static int lgt_bubbles[64];
static struct Sprite *spr_bubbles[64];
static int tail_bubble = 0, head_bubble = 0;

void BubblePush(void) {
	float x = RandomUniformFloat(300.0f);
	float y = RandomUniformFloat(300.0f);
	if(((head_bubble + 1) & 63) == tail_bubble) {
		warn("%u-%u Bubble capacity reached.\n", tail_bubble, head_bubble);
		return;
	}
	debug("%u-%u Creating Bubble %u\n", tail_bubble, head_bubble, head_bubble);
	Light(&lgt_bubbles[head_bubble], 32.0f, 1.0f, 1.0f, 1.0f, 0);
	LightSetPosition(lgt_bubbles[head_bubble], x, y);
	spr_bubbles[head_bubble] = Sprite(SP_ETHEREAL, AutoImageSearch("AsteroidSmall.png"), (int)x, (int)y, 0.0f);
	SpriteSetNotify(&spr_bubbles[head_bubble]);
	head_bubble = (head_bubble + 1) & 63;
	SpriteList();
	LightList();
}

void BubblePop(void) {
	if(head_bubble == tail_bubble) {
		warn("%u-%u No Bubbles to delete.\n", tail_bubble, head_bubble);
		return;
	}
	debug("%u-%u Deleting Bubble %u\n", tail_bubble, head_bubble, tail_bubble);
	Light_(&lgt_bubbles[tail_bubble]);
	Sprite_(&spr_bubbles[tail_bubble]);
	tail_bubble = (tail_bubble + 1) & 63;
	SpriteList();
	LightList();
}*/

/*static int lgt_bubbles[64];
static struct Sprite *spr_bubbles[64];
static int bubble = 0;

void BubblePush(void) {
	float x = RandomUniformFloat(300.0f);
	float y = RandomUniformFloat(300.0f);
	if(bubble >= 64) {
		warn("Bubble capacity reached %u.\n", bubble);
		return;
	}
	debug("Creating Bubble %u.\n", bubble);
	Light(&lgt_bubbles[bubble], 32.0f, 1.0f, 1.0f, 1.0f, 0);
	LightSetPosition(lgt_bubbles[bubble], x, y);
	spr_bubbles[bubble] = Sprite(SP_ETHEREAL, AutoImageSearch("AsteroidSmall.png"), (int)x, (int)y, 0.0f);
	SpriteSetNotify(&spr_bubbles[bubble]);
	bubble++;
	SpriteList();
	LightList();
}

void BubblePop(void) {
	if(bubble >= 0) {
		warn("No Bubbles to delete %u.\n", bubble);
		return;
	}
	debug("Deleting Bubble %u\n", bubble);
	Light_(&lgt_bubbles[bubble]);
	Sprite_(&spr_bubbles[bubble]);
	bubble--;
	SpriteList();
	LightList();
}
*/
