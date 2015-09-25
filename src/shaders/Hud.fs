/* it was so much easier with bytes
r = 0xF0 -> 0x50, 240 -> 80
g = 0x00 -> 0xA0,   0 -> 160
b = 0x32 -> 0xC0,  50 -> 192
40 -> 0.156862745098039
 2 -> 0.007843137254902
80 -> 0.313725490196078 inv 3.1875 */

uniform sampler2D sampler;
uniform ivec2 shield;
varying vec2 tex;

void main() {
	/* fraction of shield [0,1] */
	float frac = float(shield.x) / float(shield.y);
	/* the unmolested image; [0,1], discard all by the red channel */
	float data = texture2D(sampler, tex).r;
	/* clip the transformed data */
	float look = clamp((data - frac + 0.156862745098039) * 3.1875, 0.0, 1.0);
	/* apply [simulated] palette */
	vec3 indexed;
	indexed.r = mix(0.941176470588235, 0.313725490196078, look);
	indexed.g = mix(0.000000000000000, 0.627450980392157, look);
	indexed.b = mix(0.196078431372549, 0.752941176470588, look);

	gl_FragColor = vec4(indexed, 0.5);
}
