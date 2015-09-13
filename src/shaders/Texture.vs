attribute vec2 vbo_position;
attribute vec2 vbo_texture;
varying vec2 tex;

void main() {
	tex = vbo_texture * vec2(0.5, 0.5) + vec2(0.5, 0.5);
	gl_Position = vec4(vbo_position, 0.0, 1.0);
}
