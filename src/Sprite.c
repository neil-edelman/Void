/* Copyright 2000, 2013 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt */

#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* fprintf */
#include <math.h>
#include "Math.h"
#include "Open.h"
#include "Sprite.h"

/* troll physics */
const static float bounceRatio     = 0.7f;
const static float rotationDamping = 0.8f;
const static float rotationLimit   = 0.0035f;
const static float speedLimit      = 0.03f;
const static float speedZeroFrames = 16.0f;
const static float speedMin        = 0.00003f;

/* from Open.c */
extern struct Open *open;
extern unsigned char (*isTexture)(unsigned);

/* FIXME: "storing 32-bit float result in memory" */

struct Sprite {
	struct Vec2f p, v; /* position, velocity */
	float t, w;        /* \theta, \omega */
	struct Vec2f half;
	int texture;
	int stopFrames; /* fixme: msStop instead */
	int isStopped;
};

const static float rand_norm = 1.0f / RAND_MAX;

/* public */

/** constructor */
struct Sprite *Sprite(const int tex, const int width, const int height) {
	struct Sprite *sprite;

	if(!open /*|| !isTexture(tex)*/) {
		fprintf(stderr, "Sprite: invalid (texture?)\n");
		return 0;
	}
	if(!(sprite = malloc(sizeof(struct Sprite)))) {
		perror("Sprite constructor");
		Sprite_(&sprite);
		return 0;
	}
	sprite->p.x = sprite->p.y = 0;
	sprite->v.x = sprite->v.y = 0;
	sprite->t   = sprite->w   = 0;
	sprite->half.x            = 0.5f * width;
	sprite->half.y            = 0.5f * height;
	sprite->texture           = tex;
	sprite->stopFrames        = 0;
	sprite->isStopped         = -1;
	fprintf(stderr, "Sprite: new, #%p texture %u (%dx%d.)\n", (void *)sprite,
			sprite->texture, width, height);

	return sprite;
}

/** destructor */
void Sprite_(struct Sprite **sprite_ptr) {
	struct Sprite *sprite;

	if(!sprite_ptr || !(sprite = *sprite_ptr)) return;
	fprintf(stderr, "~Sprite: erase, #%p.\n", (void *)sprite);
	free(sprite);
	*sprite_ptr = sprite = 0;
}

int SpriteGetTexture(const struct Sprite *sprite) {
	if(!sprite) return 0;
	return sprite->texture;
}

/** accessor */
/*const struct Vec2f *SpriteGetVelocity();*/
struct Vec2f SpriteFuturePosition(const struct Sprite *sprite, const float ms) {
	struct Vec2f p = { 0, 0 };
	if(!sprite) return p;
	p.x = sprite->p.x + ms * sprite->v.x;
	p.y = sprite->p.y + ms * sprite->v.y;
	return p;
}
float SpriteFutureAngle(const struct Sprite *sprite, const float ms) {
	if(!sprite) return 0;
	return sprite->t + ms * sprite->w;
}
struct Vec2f SpriteGetPosition(const struct Sprite *sprite) {
	struct Vec2f p = { 0, 0 };
	if(!sprite) return p;
	p.x = sprite->p.x;
	p.y = sprite->p.y;
	return p;
}
float SpriteGetAngle(const struct Sprite *sprite) {
	if(!sprite) return 0;
	return sprite->t;
}
/* fixme: hahaha make my own matrices */
float SpriteGetDegrees(const struct Sprite *sprite) {
	if(!sprite) return 0;
	return sprite->t * (float)M_180_PI;
}
struct Vec2f SpriteGetVelocity(const struct Sprite *sprite) {
	struct Vec2f v = { 0, 0 };
	if(!sprite) return v;
	v.x = sprite->v.x;
	v.y = sprite->v.y;
	return v;
}
struct Vec2f SpriteGetHalf(const struct Sprite *sprite) {
	struct Vec2f s = { 0, 0 };
	if(!sprite) return s;
	s.x = sprite->half.x;
	s.y = sprite->half.y;
	return s;
}

/** set */
void SpriteImpact(struct Sprite *sprite, const struct Vec2f *impact) {
	if(!sprite || !impact) return;
	sprite->v.x += impact->x;
	sprite->v.y += impact->y;
}

void SpriteSetAcceleration(struct Sprite *sprite, const float a) {
	float t, speed2, correction;
	if(!sprite) return;
	t = sprite->t /** M_PI / 180.0*/;
	sprite->v.x += a * cosf(t); /* fixme?: expesive, lut */
	sprite->v.y += a * sinf(t);
	/* enforce the speed limit, allow stopping */
	speed2 = sprite->v.x * sprite->v.x + sprite->v.y * sprite->v.y;
	if(sprite->isStopped) {
		if(speed2 >= speedMin) sprite->isStopped = 0;
	} else {
		if(speed2 > speedLimit) {
			correction = speedLimit / speed2;
			sprite->v.x *= correction;
			sprite->v.y *= correction;
			/* fprintf(stderr, "Slow down!\n"); <- spam! */
		} else if(speed2 < speedMin) {
			/* should be ms not frames but don't have access, it's not important,
			 I just hate moving always even when you're stopped */
			if(++sprite->stopFrames >= speedZeroFrames) {
				sprite->v.x = 0;
				sprite->v.y = 0;
				sprite->isStopped = -1;
				fprintf(stderr, "Stopped.\n"); /* fixme: never happens now */
			}
		} else {
			sprite->stopFrames = 0;
		}
	}
}

void SpriteSetOmega(struct Sprite *sprite, const float w) {
	if(!sprite) return;
	sprite->w += w;
	if(sprite->w < -rotationLimit) {
		sprite->w = -rotationLimit;
		/* printf("Right turn limit.\n"); <- spam! */
	} else if(sprite->w > rotationLimit) {
		sprite->w = rotationLimit;
		/* printf("Left turn limit.\n"); <- spam! */
	}
}

void SpriteUpdate(struct Sprite *sprite, const int ms) {
	const struct Vec2f *screen = OpenGetScreen();
	if(!sprite) return;
	/* update */
	sprite->p.x += sprite->v.x * ms;
	sprite->p.y += sprite->v.y * ms;
	sprite->t   += sprite->w   * ms;
	/* keep angles b/t [-M_PI - M_PI) for greater precision */
	if(sprite->t      < -(float)M_PI) sprite->t += (float)M_2PI;
	else if(sprite->t >= (float)M_PI) sprite->t -= (float)M_2PI;
	/* this should be framerate independant; it's too costly, and the user
	 won't notice 25/26 ms: sprite->w *= pow(rotationDamping, ms); */
	/* fixme: framerate dep! */
	sprite->w *= rotationDamping;
	/* bounce off the sides */
	if(sprite->p.x < -screen->x) {
		/*fprintf(stderr, "Bounce! (%f)\n", -screen->x);*/
		sprite->p.x = -screen->x;
		sprite->v.x = -sprite->v.x * bounceRatio;
	} else if(sprite->p.x > screen->x) {
		/*fprintf(stderr, "Bounce! (%f)\n", screen->x);*/
		sprite->p.x = screen->x;
		sprite->v.x = -sprite->v.x * bounceRatio;
	}
	if(sprite->p.y < -screen->y) {
		/*fprintf(stderr, "Bounce! (%f)\n", -screen->y);*/
		sprite->p.y = -screen->y;
		sprite->v.y = -sprite->v.y * bounceRatio;
	} else if(sprite->p.y > screen->y) {
		/*fprintf(stderr, "Bounce! (%f)\n", screen->y);*/
		sprite->p.y = screen->y;
		sprite->v.y = -sprite->v.y * bounceRatio;
	}
}

void SpriteRandomise(struct Sprite *sprite) {
	const struct Vec2f *screen = OpenGetScreen();
	if(!sprite) return;
	sprite->p.x = (rand() * 2.0f * rand_norm - 1.0f) * screen->x;
	sprite->p.y = (rand() * 2.0f * rand_norm - 1.0f) * screen->y;
	sprite->t   = rand() * (float)M_2PI * rand_norm;
}

void SpritePush(struct Sprite *sprite, float push) {
	if(!sprite) return;
	sprite->v.x = (rand() * 2.0f * rand_norm - 1.0f) * push;
	sprite->v.y = (rand() * 2.0f * rand_norm - 1.0f) * push;
}

/* private */
