// passed these from C
uniform sampler2D bmp_sprite;
// passed these from vertex shader
varying vec2 pass_texture;

void main() {
	gl_FragColor = texture2D(bmp_sprite, pass_texture);
}
