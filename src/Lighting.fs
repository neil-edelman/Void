/* 1.10 defualt */
#define M_PI 3.1415926535897932384626433832795 /* pi */
#define M_1_PI 0.318309886183790671537767526745028724 /* 1/pi */
#define M_2_PI 0.636619772367581343075535053490057448 /* 2/pi */
#define M_2PI 6.283185307179586476925286766559005768394338798750211641949889 /* 2pi */
#define M_1_2PI 0.159154943091895335768883763372514362034459645740456448747667 /* 1/(2pi) */

#define MAX_LIGHTS 64
#define AMBIENT 0.08

varying vec2 fragment_position;

uniform sampler2D sprite;
uniform sampler2D lighting;
uniform int  l_no;
uniform vec3 l_lux[MAX_LIGHTS];
uniform vec2 l_pos[MAX_LIGHTS];

void main() {
	vec4 texel = texture2D(sprite, gl_TexCoord[0].st);
	/* light.r = f(radius) [0-1); light.g = angle [0-1) */
	vec4 light = texture2D(lighting, gl_TexCoord[1].st);
	/* light colour, start ambient */
	vec3 shade = vec3(AMBIENT);
	for(int i = 0; i < MAX_LIGHTS; i++) {
		if(i < l_no) {
			vec2 relative = l_pos[i] - fragment_position;
			float dist = length(relative) + 0.1;
			/* the angle to the light, in the sprite, and the difference */
			float tolight = atan(relative.y, relative.x);
			float insprite = light.g * M_2PI;
			float delta = abs(mod(insprite - tolight + M_PI, M_2PI) - M_PI);
			/* the final contrubution from light i */
			shade += l_lux[i] * mix(1.0, M_PI - delta, light.r) / dist;
		} else {
			break;
		}
	}
	gl_FragColor = vec4(shade, 1.0) * texel;
}
