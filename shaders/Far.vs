#define SQRT1_2 0.7071067811865475244008443621048490392848359376884740
#define FORESHORTENING 0.2

attribute vec2 vbo_position;
attribute vec2 vbo_texture;
varying vec2 tex, tex_light;
varying vec2 var_position;
uniform float size, angle;
uniform vec2 position, camera;
uniform vec2 two_screen;

void main() {

	// we have everything, just have to arrage it
	float c = cos(angle), s = sin(angle);
	float ac = size * c, as = size * s;
	float qc = SQRT1_2 * c, qs = SQRT1_2 * s;
	float w = two_screen.x, h = two_screen.y;

	// adjust for the camera (not numerically stable!)
	vec2 p = position - camera * FORESHORTENING;

	mat2 vertex2screen = mat2(ac*w, as*h, -as*w, ac*h);
	vec2 screen_pos    = vec2(p.x*w, p.y*h);
	vec2 light_pos     = vec2(0.5 * (-qc + qs) + 0.5, 0.5 * (-qs - qc) + 0.5);
	mat2 rotate        = mat2(c, s, -s, c);

	/* pass varyings to fragment shader */

	// texture / lighting lookup
	tex          = vbo_texture;
	tex_light    = light_pos        + rotate * vbo_texture * SQRT1_2;
	// accurate world position in pixels and in [-1, 1] screen rendering
	var_position = position         + rotate * vbo_position * size;
	gl_Position  = vec4(screen_pos  + vertex2screen * vbo_position, 0.0, 1.0);

}
