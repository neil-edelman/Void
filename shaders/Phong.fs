#define M_PI 3.1415926535897932384626433832795
#define M_2PI 6.283185307179586476925286766559005768394338798750211641949889
#define M_1_PI 0.318309886183790671537767526745028724

// must be the same as in Light.c
#define MAX_LIGHTS 64
// black stuff is hard to see
#define AMBIENT 0.08
// for avoiding singularity
#define EPSILON 0.1

uniform sampler2D bmp_sprite, bmp_normal;
uniform int   lights;
uniform vec2  light_position[MAX_LIGHTS];
uniform vec3  light_colour[MAX_LIGHTS];
uniform float directional_angle;
uniform vec3  directional_colour;
// passed these from vertex shader
varying vec2 pass_texture, pass_light;
varying vec2 pass_position;

float light_i(int i, float in_sprite, vec4 light);

void main() {
	vec4 texel  = texture2D(bmp_sprite, pass_texture);
	vec4 normal = texture2D(bmp_normal, pass_light);
	vec3 shade = vec3(AMBIENT);
/*
	// one directional light
	to_light  = directional_angle;
	delta     = abs(mod(in_sprite - to_light + M_PI, M_2PI) - M_PI);
	delta     = M_1_PI * (M_PI - delta * 2.0);
	shade += max(0.0, mix(-0.1, delta, light.r)) * directional_colour;
	// point lights
	for(int i = 0; i < MAX_LIGHTS; i++) {
		if(i >= lights) {
			break;
		} else {
			shade += light_colour[i] * light_i(i, in_sprite, light);
		}
	}
*/
	// final colour
	gl_FragColor = vec4(shade, 1.0) * texel;
}

float light_i(int i, float in_sprite, vec4 light) {
	vec2 relative  = light_position[i] - pass_position;
	float dist     = length(relative) + EPSILON;
	// the angle to the light, in the sprite, and the difference
	float to_light = atan(relative.y, relative.x);
	float delta    = abs(mod(in_sprite - to_light + M_PI, M_2PI) - M_PI);
	// the final contrubution from light i
	return(mix(1.0, M_PI - delta, light.r) / dist);
}
