#define SQRT1_2 0.7071067811865475244008443621048490392848359376884740

// these are from the vbo
attribute vec2 attrib_vertex;
attribute vec2 attrib_texture;
// these are set in the programme
uniform float size, angle;
uniform vec2 object, camera;
uniform vec2 projection;
// pass these to fragment shader
varying mat2 pass_rotation;
varying vec2 pass_texture;
varying vec2 pass_view;

void main() {
	// rotate the sprite
	float c = cos(angle), s = sin(angle);
	mat2 inv_rotate = mat2(c, s, -s, c);
	// texture unmolested, inv_inv_rotate to correct the normals
	pass_rotation = mat2(c, -s, s, c);
	pass_texture = attrib_texture;
	// needed for point lights
	pass_view = object + inv_rotate * attrib_vertex * size;
	// not numerically stable :[
	gl_Position = vec4((pass_view - camera) * projection, 0.0, 1.0);
}
