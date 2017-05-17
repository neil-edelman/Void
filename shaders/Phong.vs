#define SQRT1_2 0.7071067811865475244008443621048490392848359376884740

// these are from the vbo
attribute vec2 attrib_position;
attribute vec2 attrib_texture;
// these are set in the programme
uniform float size, angle;
uniform vec2 position, camera;
uniform vec2 inv_screen;
// pass these to fragment shader
varying vec2 pass_texture, pass_light;
varying vec2 pass_position;

void main() {
	// we have everything, just have to arrage it
	float c = cos(angle), s = sin(angle);
	float ac = size * c, as = size * s;
	float qc = SQRT1_2 * c, qs = SQRT1_2 * s;
	float w = inv_screen.x, h = inv_screen.y;

	// adjust for the camera (not numerically stable!)
	vec2 p = position - camera;

	mat2 vertex2screen = mat2(ac*w, as*h, -as*w, ac*h);
	vec2 screen_pos    = vec2(p.x*w, p.y*h);
	vec2 light_pos     = vec2(0.5 * (-qc + qs) + 0.5, 0.5 * (-qs - qc) + 0.5);
	mat2 rotate        = mat2(c, s, -s, c);

	/* pass varyings to fragment shader */

	// texture / lighting lookup
	pass_texture  = attrib_texture;
	pass_light    = light_pos      + rotate * attrib_texture * SQRT1_2;
	// accurate world position in pixels and in [-1, 1] screen rendering
	pass_position = position       + rotate * attrib_position * size;
	gl_Position  = vec4(screen_pos + vertex2screen * attrib_position, 0.0, 1.0);
}
