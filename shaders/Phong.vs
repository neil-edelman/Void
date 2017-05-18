#define SQRT1_2 0.7071067811865475244008443621048490392848359376884740

// these are from the vbo
attribute vec2 attrib_centred;
attribute vec2 attrib_texture;
// these are set in the programme
uniform float size, angle;
uniform vec2 object, camera;
uniform vec2 inv_screen;
// pass these to fragment shader
varying vec2 pass_texture, pass_conserve;
varying vec2 pass_position;
varying float c, s;

void main() {
	// translate and rotate the sprite
	vec2 object_camera = object - camera;
	c = cos(angle), s = sin(angle);
	mat2 rotate = mat2(c, -s, s, c), inv_rotate = mat2(c, s, -s, c);

	float qc = SQRT1_2 * c, qs = SQRT1_2 * s;
	vec2 light_pos = vec2(0.5 * (-qc + qs) + 0.5, 0.5 * (-qs - qc) + 0.5);
	pass_conserve = light_pos       + rotate * attrib_texture * SQRT1_2;
	// accurate world position in pixels and in [-1, 1] screen rendering
	pass_position = object + rotate * attrib_centred * size;

	// pass the texture unmolested
	pass_texture = attrib_texture;
	gl_Position = vec4((object_camera + inv_rotate * attrib_centred * size) * inv_screen, 0.0, 1.0);
}
