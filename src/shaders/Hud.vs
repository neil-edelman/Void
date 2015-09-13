/* hud */

attribute vec2 vbo_position;
attribute vec2 vbo_texture;
varying vec2 tex;
uniform float size;
uniform vec2 position, camera;
uniform vec2 two_screen;

void main() {
	vec2 p = position - camera;
	vec2 screen_pos = vec2(p*two_screen);
	tex = vbo_texture * vec2(0.5, 0.5) + vec2(0.5, 0.5);
	gl_Position = vec4(screen_pos /*vbo_position*/, 0.0, 1.0);
}
