// copied from Lambert, but added forshortening

// these are from the vbo
attribute vec2 attrib_vertex;
attribute vec2 attrib_texture;
// these are set in the programme
uniform float size, angle, foreshortening;
uniform vec2 object, camera;
uniform vec2 projection;
// pass these to fragment shader
varying mat2 pass_rotation; // needed for correct normals
varying vec2 pass_texture;

void main() {
	float c = cos(angle), s = sin(angle);
	mat2 inv_rotate = mat2(c, s, -s, c);
	pass_rotation = mat2(c, -s, s, c);
	pass_texture = attrib_texture;
	vec2 pass_view = object + inv_rotate * attrib_vertex * size;
	gl_Position = vec4(projection * (pass_view - camera * foreshortening), 0.0, 1.0);
}
