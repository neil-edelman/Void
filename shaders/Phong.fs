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
uniform vec2 sun_direction;
uniform vec3 sun_colour;
uniform int  points;
uniform vec2 point_position[MAX_LIGHTS];
uniform vec3 point_colour[MAX_LIGHTS];
// passed these from vertex shader
varying mat2 pass_rotation;
varying vec2 pass_texture;

void main() {
	vec4 texel  = texture2D(bmp_sprite, pass_texture);
	vec3 normal = texture2D(bmp_normal, pass_texture).xyz;
	normal.xy = (normal.xy - 0.5) * 2.0;
	normal.xy *= pass_rotation;
	normal.xy = (normal.xy + 1.0) * 0.5;
	//vec3 shade = vec3(AMBIENT);
	//shade += sun_colour * normal.z;
	// \\cite{lambert1892photometrie}
	//shade += sun_colour * max(0.0, dot(normal, sun_direction)) * 100.0;
/*
	for(int i = 0; i < MAX_LIGHTS; i++) {
		if(i >= points) {
			break;
		} else {
			vec3 colour = point_colour[i];
			vec2 incoming = point_position[i] - pass_position;
			shade += colour * max(0.0, dot(normal.xy, normalize(incoming))) / length(incoming);
		}
	}
*/
	// final colour
	//gl_FragColor = vec4(shade, 1.0) * texel;
	gl_FragColor = vec4(normal, texel.w);
}
