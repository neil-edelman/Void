/* 1.10 default */
#define MAX_LIGHTS 64

uniform vec2 sprite_position;

varying vec2 fragment_position;

void main() {
	/* translation + rotation; fixme: 'GLSL common mistakes;' I'm making it; seems to work */
	gl_TexCoord[0].st = gl_MultiTexCoord0.st;
	/* translation + rotation - rotation (specified by TextureMatrix); fixme: without the shader, every sprite is at 0 rotation */
	gl_TexCoord[1] = gl_TextureMatrix[0] * gl_MultiTexCoord0;
	/* pixel location in world space */
	//fragment_position = sprite_position + (gl_NormalMatrix * gl_Vertex.xyz).xy; /* this seems like it shouldn't work */
	//fragment_position = sprite_position; /* this approximates the sprite to be at one spot */
	mat2 modelMatrix = mat2(
		gl_ModelViewMatrix[0].x, gl_ModelViewMatrix[0].y,
		gl_ModelViewMatrix[1].x, gl_ModelViewMatrix[1].y
	);
	fragment_position = sprite_position + modelMatrix * gl_Vertex.xy;
	gl_Position = ftransform();
}
