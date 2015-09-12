/* background image */

uniform sampler2D sampler;
varying vec2 tex;

void main() {
	gl_FragColor = texture2D(sampler, tex);
}
