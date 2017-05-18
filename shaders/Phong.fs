// must be the same as in Light.c
#define MAX_LIGHTS 64

// passed these from C
uniform sampler2D bmp_sprite, bmp_normal;
uniform vec3 sun_direction;
uniform vec3 sun_colour;
uniform int  points;
uniform vec2 point_position[MAX_LIGHTS];
uniform vec3 point_colour[MAX_LIGHTS];
// passed these from vertex shader
varying mat2 pass_rotation;
varying vec2 pass_texture;
varying vec2 pass_object;

void main() {
	// texture map
	vec4 texel = texture2D(bmp_sprite, pass_texture);
	// normal vectors are encoded as colours in excess-0.5, (viz. PNG -127, PNG does not use 255?)
	vec3 normal = (texture2D(bmp_normal, pass_texture).rgb - 0.5) * 2.0;
	// the texture is fixed, need to correct the inverse rotation by applying the opposite
	normal.xy *= pass_rotation;

	// \\cite{lambert1892photometrie} -- sun directional light is modulated by length
	vec3 shade = sun_colour * max(0.0, dot(normal, normalize(sun_direction))) * length(sun_direction);
	// point lights are modulated by inverse distance, and z is in null-space
	for(int i = 0; i < MAX_LIGHTS; i++) {
		if(i >= points) {
			break;
		} else {
			vec3 colour = point_colour[i];
			vec2 incoming = point_position[i] - pass_object;
			shade += colour * max(0.0, dot(normal.xy, normalize(incoming))) / length(incoming);
		}
	}
	// with the lighting
	gl_FragColor = vec4(shade * texel.xyz, texel.w);
}
