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
static void Light_(struct Light *const light) {
	struct Lights *const lights = &sprites->lights;
	size_t no, r;
	assert(sprites && light);
	no = light - lights->light_table;
	printf("light: %lu\n", no);
	if(no >= lights->size) {printf("~light: %lu not in range of lights, %lu.\n",
		(unsigned long)no, (unsigned long)lights->size); return; }
	/* Take the last light and replace this one. */
	if(no < (r = lights->size - 1)) {
		memcpy(light, lights->light_table + r, sizeof *light);
		memcpy(lights->x_table + no, lights->x_table + r,
			sizeof *lights->x_table);
		memcpy(lights->colour_table + no, lights->colour_table + r,
			sizeof *lights->colour_table);
		assert(light->sprite && light->sprite->light == lights->light_table +r);
		light->sprite->light = lights->light_table + no;
	}
	sprites->lights.size--;
}
/** @return True if the light was created.
 @fixme Have some random picks to replace if the capacity is full. */
static int Light(struct Sprite *const sprite,
	const float r, const float g, const float b) {
	struct Lights *const lights = &sprites->lights;
	struct Light *this;
	struct Vec2f *x;
	struct Colour3f *colour;
	size_t l;
	assert(sprites && sprite && r >= 0.0f && g >= 0.0f && b >= 0.0f);
	if(sprite->light) return 0; /* Already associated with a light. */
	if(lights->size >= MAX_LIGHTS)
		return fprintf(stderr, "light: capacity.\n"), 0;
	l = lights->size++;
	this = lights->light_table + l;
	this->sprite = sprite, sprite->light = this;
	x = lights->x_table + l;
	colour = lights->colour_table + l;
	x->x = 0.0f, x->y = 0.0f;
	colour->r = r, colour->g = g, colour->b = b;
	return 1;
}
/** Delete all lights. */
static void Light_clear(void) {
	struct Lights *const lights = &sprites->lights;
	size_t i;
	assert(sprites);
	for(i = 0; i < lights->size; i++) {
		assert(lights->light_table[i].sprite);
		lights->light_table[i].sprite->light = 0;
	}
	lights->size = 0;
}
size_t SpritesLightGetSize(void) {
	if(!sprites) return 0;
	return sprites->lights.size;
}
struct Vec2f *SpritesLightPositions(void) {
	struct Light *lights, *light;
	struct Vec2f *xs, *x;
	size_t i, size;
	if(!sprites) return 0;
	size = sprites->lights.size;
	lights = sprites->lights.light_table;
	xs = sprites->lights.x_table;
	for(i = 0; i < size; i++) {
		x = xs + i;
		light = lights + i;
		x->x = light->sprite->x.x;
		x->y = light->sprite->x.y;
	}
	return xs;
}
struct Colour3f *SpritesLightGetColours() {
	if(!sprites) return 0;
	return sprites->lights.colour_table;
}
