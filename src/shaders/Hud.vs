/* hud */

attribute vec2 vbo_position;
attribute vec2 vbo_texture;
varying vec2 tex;
uniform vec2 size;
uniform vec2 position, camera;
uniform vec2 two_screen;

void main() {
	// screen transformation in pixels
	vec2 p = position - camera;
	// add vbo position; add useless holographic c\"oordinates
	gl_Position = vec4((p + size * vbo_position) * two_screen, 0.0, 1.0);
	// save the texture position
	tex = vbo_texture;
}
