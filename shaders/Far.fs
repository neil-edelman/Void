// copied from Lambert, but ambient and only sun

// black stuff is hard to see
#define AMBIENT 0.08

// passed these from C
uniform sampler2D bmp_sprite, bmp_normal;
uniform vec3 sun_direction;
uniform vec3 sun_colour;
// passed these from vertex shader
varying mat2 pass_rotation;
varying vec2 pass_texture;
varying vec2 pass_view;

void main() {
	// texture map
	vec4 texel = texture2D(bmp_sprite, pass_texture);
	// normal vectors are encoded as colours in excess-0.5, (viz. PNG -127, PNG does not use 255?)
	vec3 normal = (texture2D(bmp_normal, pass_texture).rgb - 0.5) * 2.0;
	// the texture is fixed, need to correct the inverse rotation by applying the opposite
	normal.xy *= pass_rotation;

	// \\cite{lambert1892photometrie} -- sun directional light is modulated by length
	vec3 shade = sun_colour * max(AMBIENT, dot(normal, normalize(sun_direction))) * length(sun_direction);
	// with the lighting
	gl_FragColor = vec4(shade * texel.xyz, texel.w);
}
