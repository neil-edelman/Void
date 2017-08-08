// these are from the vbo
attribute vec2 attrib_vertex;
attribute vec2 attrib_texture;
// these are set in the programme
uniform float size;
uniform vec2 object, camera;
uniform vec2 projection;
// pass these to fragment shader
varying vec2 pass_texture;

void main() {
	pass_texture = attrib_texture;
	gl_Position = vec4(projection * (object - camera + attrib_vertex * size), 0.0, 1.0);
}
