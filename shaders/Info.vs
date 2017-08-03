// these are from the vbo
attribute vec2 attrib_centred;
attribute vec2 attrib_texture;
// these are set in the programme
uniform float size;
uniform vec2 object, camera;
uniform vec2 inv_screen;
// pass these to fragment shader
varying vec2 pass_texture;

void main() {
	// translate and rotate the sprite
	vec2 object_camera = object - camera;
	// texture unmolested
	pass_texture = attrib_texture;
	// not numerically stable :[
	gl_Position = vec4((object + attrib_centred * size) - camera, 0.0, 1.0);
}
