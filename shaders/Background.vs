/* background image */

attribute vec2  vbo_position;
attribute vec2  vbo_texture;
uniform float scale;
varying vec2 tex;

void main() {
	tex = vbo_texture * vec2(0.5, 0.5) * scale + vec2(0.5, 0.5);
	gl_Position = vec4(vbo_position, 0.0, 1.0);
}
