/* Copyright 2000, 2013 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt */

#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* fprintf */
#include "Math.h"
#include "Open.h"
#include "Sprite.h" /* for LightCollide() */
#include "Light.h"

/* from Open.c */
extern struct Open *open;
extern void (*vertex)(float, float);
extern void (*colour)(float, float, float, float);

/* private */

#define MAX_ONSCREEN 64 /* has to be the same as in the shader */
#define MAX_LIGHTS   64 /* >= MAX_ONSCREEN */

static const float particle_alpha = 0.8f;

/* lights everywhere */

struct Light {
	struct Vec2f v;
	struct Colour3f colour;
	int damage;  /* 0, intangable */
	int expires; /* 0, no expiry */
	char name;
	/* fixme: callback to have homing */
	/* fixme: place in order to have lighting! */
	/*struct Light *xNext;*/
};

int noLights = 0; /* counting all the lights (that are active) */
static struct Light light[MAX_LIGHTS];
const static int maxLights = sizeof(light) / sizeof(struct Light);

/* lights on-screen are a subset of lights everywhere that have contiguous
 array for shaders */

int                    l_no;              /* on-screen lights */
struct Colour3f        l_lux[MAX_LIGHTS];
struct Vec2f           l_pos[MAX_LIGHTS]; /* relative position */

/* public */

/** constructor */
int Light(const struct Vec2f x, const struct Vec2f v, const struct Colour3f lux, const int damage, const int expires) {
	/*const struct Vec2f *screen = OpenGetScreen();*/
	int no;
	struct Light *p;
	static char name = 'A';

	if(!open) {
		fprintf(stderr, "Light: graphics not up.\n");
		return -1;
	}
	if(noLights >= maxLights) {
		fprintf(stderr, "Light: reached limit of %d/%d lights.\n", noLights, maxLights);
		return -1;
	}
	/*fprintf(stderr, "Light: reached limit of %d/%d lights.\n", l_no, MAX_LIGHTS);*/
	/*	if(x.x < -screen->x || x.x > screen->x || x.y < -screen->y || x.y > screen->y) {
		fprintf(stderr, "Light: not created offscreen.\n");
		return -1;
	}*/
	no = l_no++;
	p  = &light[no];
	l_pos[no].x = x.x;
	l_pos[no].y = x.y;
	p->v.x = v.x;
	p->v.y = v.y;
	l_lux[no].r = lux.r;
	l_lux[no].g = lux.g;
	l_lux[no].b = lux.b;
	/*l_lux[no].a = particle_alpha;*/
	p->damage  = damage;
	p->expires = expires;
	p->name    = name++;
	if(name == 'z') name = 'A';
	/*fprintf(stderr, "Light: new colour (%f,%f,%f) at (%f,%f), #%d.\n",
			lux.r, lux.g, lux.b, x.x, x.y, no); <- spam! */
	/*fprintf(stderr, "Light: new '%c', #%d.\n", p->name, no);
	{
		int i;
		for(i = 0; i < l_no; i++) fprintf(stderr, "%c, ", particle[i].name);
		fprintf(stderr, "0.\n");
	}*/

	return no;
}

/** destructor of no, will update with new particle at no */
void Light_(const int no) {
	if(no < 0 || no >= l_no) return;
	/* fprintf(stderr, "~Light: erase, #%d.\n", no); <- spam! */
	/*fprintf(stderr, "~Light: erase '%c', #%d.\n", particle[no].name, no);*/
	/* place the noLights item into this one, decrese #; nobody will know */
	if(no < l_no - 1) {
		struct Light *p, *z;
		p = &light[no];
		z = &light[l_no - 1];
		l_pos[no].x = l_pos[l_no - 1].x;
		l_pos[no].y = l_pos[l_no - 1].y;
		p->v.x = z->v.x;
		p->v.y = z->v.y;
		l_lux[no].r = l_lux[l_no - 1].r;
		l_lux[no].g = l_lux[l_no - 1].g;
		l_lux[no].b = l_lux[l_no - 1].b;
		/*l_lux[no].a = l_lux[l_no - 1].a;*/
		p->damage  = z->damage;
		p->expires = z->expires;
		p->name    = z->name;
	}
	l_no--;
	/*{
		int i;
		for(i = 0; i < noLights; i++) fprintf(stderr, "%c, ", particle[i].name);
		fprintf(stderr, "0.\n");
	}*/
}

/** get address of things for shaders and VBOs in Open.c */
int LightGetNo(void) { return l_no; }
const struct Colour3f *LightGetColours(void) { return l_lux; }
const struct Vec2f *LightGetPositions(void) { return l_pos; }
int LightGetMax(void) { return MAX_LIGHTS; }

void LightUpdate(const int ms) {
	const struct Vec2f *screen = OpenGetScreen();
	int i;
	struct Light *p;
	for(i = 0, p = light; i < l_no; ) {
		/* expire */
		if(p->expires && (p->expires -= ms) <= 0) {
			Light_(i);
			continue;
		}
		l_pos[i].x += p->v.x * ms;
		l_pos[i].y += p->v.y * ms;
		if(l_pos[i].x < -screen->x
		   || l_pos[i].x > screen->x
		   || l_pos[i].y < -screen->y
		   || l_pos[i].y > screen->y) {
			Light_(i);
			continue; /* will update with new particle */
		}
		/* fixme: do collision detection if damage > 0 */
		i++;
		p++;
	}
}

/* do collision detection with shots on screen; return how much damage
 and out *push */
/* fixme: improve this by checking the entire path and not just the end */
/* fixme: check further if the part of the ship hit is opaque */
/* fixme: make a doexplosion func to just push everything around out and
 seed an explosion graphic */
int LightCollide(const struct Sprite *e, struct Vec2f *push) {
	int i, damage = 0;
	struct Light *p;
	struct Vec2f pos, size;

	if(!push) return 0;
	push->x = 0;
	push->y = 0;
	if(!e) return 0;
	pos  = SpriteGetPosition(e);
	size = SpriteGetHalf(e);
	for(i = 0, p = light; i < l_no; ) {
		if(p->damage
		   && l_pos[i].x >= pos.x - size.x
		   && l_pos[i].x <= pos.x + size.x
		   && l_pos[i].y >= pos.y - size.y
		   && l_pos[i].y <= pos.y + size.y) {
			damage += p->damage;
			push->x += p->v.x;
			push->y += p->v.y;
			Light_(i);
			continue; /* will update with new particle */
		}
		i++;
		p++;
	}
	return damage;
}
