/** 2017 Neil Edelman, distributed under the terms of the GNU General
 Public License 3, see copying.txt, or
 \url{ https://opensource.org/licenses/GPL-3.0 }.

 Lights are sent to the GPU for drawing. More accurately, lights are uploaded
 to the GPU every frame and every {Sprite} is affected by them in the shader.

 @title		SpritesLight
 @author	Neil
 @std		C89/90
 @version	2017-12 Joined from {Light.c}; sprites associated with lights.
 			2016-01 Lights are [awkward] objects.
 @since		2000 Brute force. */

/** Deletes the light. */
static void light_(struct Sprite *const light) {
	assert(sprites);
	if(!light) return;
	/// fixme: move the last one into this
	assert(sprites->lights.size);
	sprites->lights.size--;
}
/** @return True if the light was created.
 @fixme Have some random picks to replace if the capacity is full. */
static int light(struct Sprite *const sprite,
	const float r, const float g, const float b) {
	struct Lights *const lights = &sprites->lights;
	struct Vec2f *x;
	struct Colour3f *colour;
	size_t l;
	assert(sprites && sprite);
	if(sprite->light) return 0; /* Already associated with a light. */
	if(lights->size >= MAX_LIGHTS)
		return fprintf(stderr, "light: capacity.\n"), 0;
	assert(r >= 0.0f && g >= 0.0f && b >= 0.0f);
	l = lights->size++;
	lights->sprite[l] = sprite;
	sprite->light = sprite + l;
	x = lights->x + l;
	colour = lights->colour + l;
	x->x = 0.0f, x->y = 0.0f;
	colour->r = r, colour->g = g, colour->b = b;
	return 1;
}
/** Delete all lights. */
static void light_clear(void) {
	struct Lights *const lights = &sprites->lights;
	size_t i;
	assert(sprites);
	for(i = 0; i < lights->size; i++) {
		assert(lights->sprite + i);
		lights->sprite[i]->light = 0;
	}
	lights->size = 0;
}
