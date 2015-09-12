#define M_PI 3.1415926535897932384626433832795
#define M_2PI 6.283185307179586476925286766559005768394338798750211641949889
#define M_1_PI 0.318309886183790671537767526745028724

// black stuff is hard to see
#define AMBIENT 0.08

uniform sampler2D sampler;
uniform sampler2D sampler_light;
uniform float     directional_angle;
uniform vec3      directional_colour;
varying vec2 tex, tex_light;
varying vec2 var_position;

void main() {
	float to_light, in_sprite, delta;
	vec4 texel = texture2D(sampler,       tex);
	vec4 light = texture2D(sampler_light, tex_light);
	vec3 shade = vec3(AMBIENT);

	// one directional light
	in_sprite = light.g * M_2PI;
	to_light  = directional_angle;
	delta     = abs(mod(in_sprite - to_light + M_PI, M_2PI) - M_PI);
	delta     = M_1_PI * (M_PI - delta * 2.0);
	shade += max(0.0, mix(-0.1, delta, light.r)) * directional_colour;
	// final colour
	gl_FragColor = vec4(shade, 1.0) * texel;
}
