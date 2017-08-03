// passed these from C
uniform sampler2D bmp_sprite;
// passed these from vertex shader
varying vec2 pass_texture;

void main() {
	// texture map
	vec4 texel = texture2D(bmp_sprite, pass_texture);
	// with the lighting
	gl_FragColor = vec4(texel.xyz, texel.w);
}
