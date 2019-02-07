/** Copyright 2000, 2014 Neil Edelman, distributed under the terms of the GNU
 General Public License, see copying.txt.

 Interface to OpenGL.

 @title		Draw
 @author	Neil
 @version	2015-05
 @since		2000 */

#include <stdio.h>  /* *printf */
#include <stdlib.h> /* free */
#include <assert.h> /* assert */
#include <math.h>   /* sqrtf fminf fmodf atan2f */
#include "../../build/Auto.h" /* for images */
#include "../Ortho.h"
#include "../game/Items.h" /* in display */
#include "../game/Fars.h" /* in display */
#include "../../external/lodepng.h"  /* in texture */
#include "../../external/nanojpeg.h" /* in texture */
#include "../Window.h" /* WindowIsGlError */
#include "Draw.h"
/* Auto-generated, hard coded resouce files from Vsfs2h; run "make"
 and this should be automated.
 @fixme Errors about ISO C90 string length 509. */
#include "../../build/shaders/Background_vsfs.h"
#include "../../build/shaders/Lambert_vsfs.h"
#include "../../build/shaders/Far_vsfs.h"
#include "../../build/shaders/Info_vsfs.h"
#include "../../build/shaders/Hud_vsfs.h"

#define M_2PI 6.283185307179586476925286766559005768394338798750211641949889
#define M_1_2PI 0.159154943091895335768883763372514362034459645740456448747667

extern struct AutoImage auto_images[];
extern const int max_auto_images;

/** Texture addresses on the graphics card, when you want different textures
 simultaneously. Limit of {MAX_COMBINED_TEXTURE_IMAGE_UNITS}, which may be
 small, (like 2? then it will break.) */
enum TexClass {
	TEX_CLASS_SPRITE,
	TEX_CLASS_NORMAL,
	TEX_CLASS_BACKGROUND,
	TEX_CLASS_NO
};
static GLuint TexClassTexture(const enum TexClass class) {
	assert(class < GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS);
	return class + GL_TEXTURE0;
}

/** {vertices} is used ubiquitously for static geometry, uploaded into video
 memory. It's pretty simple because all of the sprites have a square geometry.
 The important part is what texture map we put on them. First we define the
 data, and then the meaning of the data. */
struct {
	GLfloat x, y;
	GLfloat s, t;
} vertices[] = {
	/* Background; {resize} changes on update; this is the only thing
	 that's not constant. */
	{  1.0f,  1.0f,  1.0f,  1.0f },
	{  1.0f, -1.0f,  1.0f,  0.0f },
	{ -1.0f,  1.0f,  0.0f,  1.0f },
	{ -1.0f, -1.0f,  0.0f,  0.0f },
	/* Generic square, for sprites, etc. Should be constant, but is not. */
	{ 0.5f,  0.5f, 1.0f, 1.0f },
	{ 0.5f, -0.5f, 1.0f, 0.0f },
	{ -0.5f, 0.5f, 0.0f, 1.0f },
	{ -0.5f,-0.5f, 0.0f, 0.0f }
};
/* Used in {vertex_attribute} as an index. */
enum {
	/* vec2 */ VBO_ATTRIB_VERTEX,
	/* vec2 */ VBO_ATTRIB_TEXTURE
};
/** Shader attribute assignment. There are two two-vectors corresponding to the
 two VBO enums, and hence four entries per vertex in {vertices}. */
static const struct {
	GLint size;
	GLenum type;
	const GLvoid *offset;
} vertex_attribute[] = {
	/* VBO_ATTRIB_VERTEX */  { 2, GL_FLOAT, 0 },
	/* VBO_ATTRIB_TEXTURE */ { 2, GL_FLOAT, (GLvoid *)(sizeof(GLfloat) * 2) }
};
/* Corresponds to the indices in the {vertices} array. */
static const struct {
	GLint first;
	GLsizei count;
} vertex_index_background = { 0, 4 }, vertex_index_square = { 4, 4 };

/* Internal draw things. */
static struct {
	/* Used to render idempotent. */
	int is_started;
	/* Sprites used in debugging; initialised in {Draw}. */
	const struct AutoImage *icon_light;
	/* Pointers to a GPU-buffers. */
	struct { GLuint vertices; } arrays;
	/* Pointers to GPU textures. */
	struct { GLuint light, background, shield; } textures;
	/* A separate frame-buffer is used to bake text. */
	struct { GLuint text; } framebuffers;
	/* The camera. */
	struct { struct Vec2f x, extent; } camera;
} draw;


/** Callback for {glutDisplayFunc}; this is where all of the drawing happens.
 It sets up the shaders, then calls whatever draw functions use those
 shaders. */
static void display(void) {
	unsigned lights;

	/* @fixme https://www.khronos.org/opengl/wiki/Common_Mistakes
	 "The buffers should always be cleared. On much older hardware, there was
	 a technique to get away without clearing the scene, but on even semi-recent
	 hardware, this will actually make things slower. So always do the clear."
	 I call bullshit. Time it.
	 glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	 */

	/* Use sprites; triangle strips, two to form a square, vertex buffer,
	 [-0.5, 0.5 : -0.5, 0.5] */
	glBindBuffer(GL_ARRAY_BUFFER, draw.arrays.vertices);

	/* @fixme Don't use painters' algorithm; stencil test! */

	/* background (desktop):
	 turn off transperency
	 background vbo
	 update glUniform1i(GLint location, GLint v0)
	 update glUniformMatrix4fv(location, count, transpose, *value)
	 glDrawArrays(type flag, offset, no) */
	if(draw.textures.background) {
		/* use background shader */
		glUseProgram(auto_Background_shader.compiled);
		/* turn transperency off */
		glDisable(GL_BLEND);
		/* why?? */
		glActiveTexture(TexClassTexture(TEX_CLASS_BACKGROUND));
		/* fixme: of course it's a background, set once */
		/*glUniform1i(background_sampler_location, TEX_CLASS_BACKGROUND);*/
		/*glUniformMatrix4fv(tex_map_matrix_location, 1, GL_FALSE, background_matrix);*/
		glDrawArrays(GL_TRIANGLE_STRIP, vertex_index_background.first, vertex_index_background.count);
	}

	glEnable(GL_BLEND);

	/* Draw far objects. */
	glUseProgram(auto_Far_shader.compiled);
	glUniform2f(auto_Far_shader.camera, draw.camera.x.x, draw.camera.x.y);
	FarsDraw();

	/* Set up lights, draw sprites in foreground. */
	glUseProgram(auto_Lambert_shader.compiled);
	glUniform2f(auto_Lambert_shader.camera, draw.camera.x.x, draw.camera.x.y);
	glUniform1i(auto_Lambert_shader.points, lights = (unsigned)ItemsLightGetSize());
	if(lights) {
		struct Vec2f *parray = ItemsLightPositions();
		unsigned i;
		glUniform2fv(auto_Lambert_shader.point_position, lights,
					 (GLfloat *)parray);
		glUniform3fv(auto_Lambert_shader.point_colour, lights,
					 (GLfloat *)ItemsLightGetColours());
		/* Debug. */
		for(i = 0; i < lights; i++) Info(parray + i, draw.icon_light);
	}
	ItemsDraw();

	/* Display info on top without lighting. */
	glUseProgram(auto_Info_shader.compiled);
	glUniform2f(auto_Info_shader.camera, draw.camera.x.x, draw.camera.x.y);
	ItemsInfo();

	/* Overlay hud. @fixme */
	if(draw.textures.shield) {
		struct Ship *player;
		const struct Ortho3f *x;
		const struct Vec2f *hit;
		if((player = ItemsGetPlayerShip())
		   && (x = ItemGetPosition((struct Item *)player))
		   && (hit = ShipGetHit(player))) {
			glUseProgram(auto_Hud_shader.compiled);
			glBindTexture(GL_TEXTURE_2D, draw.textures.shield);
			glUniform2f(auto_Hud_shader.camera,draw.camera.x.x,draw.camera.x.y);
			glUniform2f(auto_Hud_shader.size, 256.0f, 8.0f);
			glUniform2f(auto_Hud_shader.position, x->x, x->y + 64.0f);
			glUniform2i(auto_Hud_shader.shield, hit->x, hit->y);
			glDrawArrays(GL_TRIANGLE_STRIP, vertex_index_square.first, vertex_index_square.count);
		}
	}

	/* We want to do this:
	 glActiveTexture(TexClassTexture(TEX_CLASS_SPRITE));
	 glBindTexture(GL_TEXTURE_2D, text_name);
	 current_texture = text_name;
	 glUniform1f(auto_Info_shader.size, 100);
	 glUniform2f(auto_Info_shader.object, 0.0f, 0.0f);
	 glDrawArrays(GL_TRIANGLE_STRIP,vbo_info_square.first,vbo_info_square.count);*/

	/* Disable, swap. */
	glUseProgram(0);
	/* @fixme glFinish and have logic stepped up? what happens when the timer
	 calls the fuction more times? does it build up in a queue? */
	/*glFinish(); <- does nothing */
	/* <-- glut glFlush implied. */
	glutSwapBuffers();
	/* glut --> */
}

/** Callback for glutReshapeFunc.
 @fixme Not guaranteed to have a background! this will crash.
 @param width, height: The size of the client area. */
static void resize(int width, int height) {
	struct Vec2f two_screen;
	int w_tex, h_tex;
	float w_w_tex, h_h_tex;

	/*fprintf(stderr, "resize: %dx%d.\n", width, height);*/
	if(width <= 0 || height <= 0) return;
	glViewport(0, 0, width, height);
	draw.camera.extent.x = width  / 2.0f;
	draw.camera.extent.y = height / 2.0f;

	/* Resize the background. */
	/* glActiveTexture(TEX_CLASS_BACKGROUND); this does nothing? */
	glBindTexture(GL_TEXTURE_2D, draw.textures.background);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH,  &w_tex);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h_tex);
	/*fprintf(stderr, "w %d h %d\n", w_tex, h_tex);*/
	w_w_tex = (float)width  / w_tex;
	h_h_tex = (float)height / h_tex;

	/* Update the background texture vbo on the card with global data here. */
	vertices[0].s = vertices[1].s =  w_w_tex;
	vertices[2].s = vertices[3].s = -w_w_tex;
	vertices[0].t = vertices[2].t =  h_h_tex;
	vertices[1].t = vertices[3].t = -h_h_tex;
	glBufferSubData(GL_ARRAY_BUFFER, vertex_index_background.first,
		vertex_index_background.count * (GLsizei)sizeof *vertices,
		vertices + vertex_index_background.first);

	/* Update the shaders; one must call this if ones shader is dependent on
	 screen-size. For Background, the image may not cover the whole drawing
	 area, so we may need a constant scaling; if it is so, the image will have
	 to be linearly interpolated for quality. */
	glUseProgram(auto_Background_shader.compiled);
	glBindTexture(GL_TEXTURE_2D, draw.textures.background);
	if(w_w_tex > 1.0f || h_h_tex > 1.0f) {
		const float scale = 1.0f / ((w_w_tex > h_h_tex) ? w_w_tex : h_h_tex);
		glUniform1f(auto_Background_shader.scale, scale);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	} else {
		glUniform1f(auto_Background_shader.scale, 1.0f);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	two_screen.x = 2.0f / width;
	two_screen.y = 2.0f / height;
	glUseProgram(auto_Lambert_shader.compiled);
	glUniform2f(auto_Lambert_shader.projection, two_screen.x, two_screen.y);
	glUseProgram(auto_Far_shader.compiled);
	glUniform2f(auto_Far_shader.projection, two_screen.x, two_screen.y);
	glUseProgram(auto_Info_shader.compiled);
	glUniform2f(auto_Info_shader.projection, two_screen.x, two_screen.y);
	glUseProgram(auto_Hud_shader.compiled);
	glUniform2f(auto_Hud_shader.two_screen, two_screen.x, two_screen.y);
}

/** Creates a texture from an image; sets the image texture unit.
 @fixme This should go in Auto?
 @param image: The Image as seen in Lores.h.
 @return Success. */
static int texture(struct AutoImage *image) {
	unsigned width, height, depth, error;
	unsigned char *pic;
	int is_alloc = 0, is_bad = 0;
	unsigned format = 0, internal = 0;
	unsigned tex = 0;
	if(!image || image->texture) return 0;
	/* Uncompress the image. */
	switch(image->data_format) {
		case IF_PNG:
			/* @fixme This will force it to be 32-bit RGBA even if it doesn't
			 have an alpha channel, potentially wasteful. */
			if((error = lodepng_decode32(&pic, &width, &height, image->data,
										 image->data_size))) {
				fprintf(stderr, "texture: lodepng error %u: %s\n", error,
						lodepng_error_text(error));
				break;
			}
			is_alloc = -1;
			depth    = 4;
			break;
		case IF_JPEG:
			if(njDecode(image->data, (int)image->data_size)) break;
			is_alloc = -1;
			pic = njGetImage();
			if(!njIsColor()) is_bad = -1;
			width  = njGetWidth();
			height = njGetHeight();
			depth  = 3;
			break;
		case IF_UNKNOWN:
		default:
			fprintf(stderr, "texture: unknown image format.\n");
	}
	if(!is_alloc) {
		fprintf(stderr, "texture: allocation failed.\n");
		return 0;
	}
	if(width != image->width || height != image->height || depth!=image->depth){
		fprintf(stderr, "texture: dimension mismatch %u:%ux%u vs %u:%ux%u.\n",
				image->width, image->height, image->depth, width, height, depth);
		is_bad = -1;
	}
	/* select image format */
	switch(depth) {
		case 1:
			/* We use exclusively for shaders, so I don't think this matters. */
			/* GL_LUMINANCE; <- "core context depriciated," I was using that. */
			internal = GL_RGB;
			format   = GL_RED;
			break;
		case 3:
			/* non-transparent */
			internal = GL_RGB8;
			format   = GL_RGB;
			break;
		case 4:
			internal = GL_RGBA8;
			format   = GL_RGBA;
			break;
		default:
			fprintf(stderr, "texture: not a recognised depth, %d.\n", depth);
			is_bad = -1;
	}
	/* Invert to go with OpenGL's strict adherence to standards vs traditions.*/
	{
		unsigned bytes_line = width * depth;
		unsigned top, bottom, i;
		unsigned char *pb_t, *pb_b;
		for(top = 0, bottom = height - 1; top < bottom; top++, bottom--) {
			pb_t = pic + top    * bytes_line;
			pb_b = pic + bottom * bytes_line;
			for(i = 0; i < bytes_line; i++, pb_t++, pb_b++)
				*pb_t ^= *pb_b, *pb_b ^= *pb_t, *pb_t ^= *pb_b;
		}
	}
	/* Load the uncompressed image into a texture. */
	if(!is_bad) {
		glGenTextures(1, (unsigned *)&tex);
		glActiveTexture((unsigned)(image->depth == 3 ? TexClassTexture(TEX_CLASS_BACKGROUND) : TexClassTexture(TEX_CLASS_SPRITE)));
		glBindTexture(GL_TEXTURE_2D, tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		/* linear because fractional positioning; may be changed with
		 backgrounds in resize */
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		/* no mipmap */
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
		/* void glTexImage2D(target, level, internalFormat, width, height,
		 border, format, type, *data); */
		glTexImage2D(GL_TEXTURE_2D, 0, internal, (int)width, (int)height, 0,
					 format, GL_UNSIGNED_BYTE, pic);
		image->texture = tex;
		/* debug */
#ifdef PRINT_PEDANTIC
		printf("texture: %u:\n", image->texture);
		if(image->width <= 80) image_print(image, pic);
		else printf(" . . . too big to show.\n");
#endif
	}
	/* Free the pic. */
	switch(image->data_format) {
		case IF_PNG: free(pic); break;
		case IF_JPEG: njDone(); break;
		default: break;
	}
	fprintf(stderr, "texture: created %dx%dx%d texture, Tex%u.\n",
			width, height, depth, tex);
	WindowIsGlError("texture");
	return tex ? 1 : 0;
}

#if 0
/** New texture with {str}.
 \url{http://www.opengl-tutorial.org/intermediate-tutorials/tutorial-14-render-to-texture/},
 \url{http://www.songho.ca/opengl/gl_fbo.html#example}.
 @return Texture name or zero. */
static int text_render(const char *const str) {
	GLuint framebuffer_name, texture_name;
	const GLenum draw_buffers[] = { GL_COLOR_ATTACHMENT0 };
	if(!str) return 0;
	/*  */
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_name);
	

	glGenTextures(1, &texture_name);
	glBindTexture(GL_TEXTURE_2D, texture_name);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1024, 768, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	/*glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture_name, 0);*/
	glDrawBuffers(sizeof draw_buffers / sizeof *draw_buffers, draw_buffers);
	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		return 0;
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_name);
	glViewport(0, 0, 1024, 768);
	glColor4f(0.0f, 0.0f, 0.0f, 0.0f);
	glRasterPos3f(30.0f, 20.0f, 0.0f);
	while(*str) glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *str);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &framebuffer_name);

	return texture_name;
}
#endif

/* Creates a hardcoded lighting texture with the Red the radius and Green the
 angle.
 @return The texture or zero. */
static int light_compute_texture(void) {
	int   i, j;
	float x, y;
	float *buffer;
	const int buffer_size = 1024; /* width/height */
	const float buffer_norm = (float)M_SQRT1_2 * 4.0f
	/ sqrtf(2.0f * buffer_size * buffer_size);
	unsigned name;

	if(!(buffer = malloc(sizeof(float) * buffer_size * buffer_size << 1)))
		return 0;
	for(j = 0; j < buffer_size; j++) {
		for(i = 0; i < buffer_size; i++) {
			x = (float)i - 0.5f*buffer_size + 0.5f;
			y = (float)j - 0.5f*buffer_size + 0.5f;
			buffer[((j*buffer_size + i) << 1) + 0] =
			fminf(sqrtf(x*x + y*y) * buffer_norm, 1.0f);
			/* NOTE: opengl clips [0, 1), even if it says different;
			 maybe it's GL_LINEAR? */
			buffer[((j*buffer_size + i) << 1) + 1] = fmodf(atan2f(y, x)
				+ (float)M_2PI, (float)M_2PI) * (float)M_1_2PI;
		}
	}
	glGenTextures(1, (unsigned *)&name);
	glBindTexture(GL_TEXTURE_2D, name);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	/* NOTE: GL_LINEAR is a lot prettier, but causes a crease when the
	 principal value goes from [0, 1), ie, [0, 2Pi) */
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	/* void glTexImage2D(target, level, internalFormat, width, height,
	 border, format, type, *data); */
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, buffer_size, buffer_size, 0,
				 GL_RG, GL_FLOAT, buffer);
	free(buffer);
	fprintf(stderr, "light_comute_texture: created %dx%dx%d hardcoded lighting "
		"texture, Tex%u.\n", buffer_size, buffer_size, 2, name);
	WindowIsGlError("light_compute_texture");
	return name;
}

/** Gets all the graphics stuff started. Must have a window.
 @return All good to draw? */
int Draw(void) {
	const float sunshine[] = { 1.0f * 3.0f, 1.0f * 3.0f, 1.0f * 3.0f };
	int i;

	if(draw.is_started) return 1;

	draw.icon_light = AutoImageSearch("Idea16.png");
	assert(draw.icon_light);

	if(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS < TEX_CLASS_NO)
		return fprintf(stderr,
		"This graphics card supports %u combined textures; we need %u.\n",
		GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, TEX_CLASS_NO), 0;

	/*text_name = text_compute_texture();*/

	/* <-- glut */
	glutDisplayFunc(&display);
	glutReshapeFunc(&resize);
	/* glut --> */

	/* http://www.opengl.org/sdk/docs/man2/xhtml/glBlendFunc.xml */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glPointSize(5.0f);
	glEnable(GL_POINT_SMOOTH);

	/* we don't need z-buffer depth things */
	/*glDepthFunc(GL_ALWAYS);*/
	glDisable(GL_DEPTH_TEST);

	/* vbo; glVertexAttribPointer(index attrib, size, type, normaised, stride, offset) */
	glGenBuffers(1, (GLuint *)&draw.arrays.vertices);
	glBindBuffer(GL_ARRAY_BUFFER, draw.arrays.vertices);
	glBufferData(GL_ARRAY_BUFFER, sizeof vertices, vertices, GL_STATIC_DRAW);
	/* fixme: the texture should be the same as the vetices, half the data */
	/* fixme: done per-frame, because apparently OpenGl does not keep track of the bindings per-buffer */
	glEnableVertexAttribArray(VBO_ATTRIB_VERTEX);
	glVertexAttribPointer(VBO_ATTRIB_VERTEX,
		vertex_attribute[VBO_ATTRIB_VERTEX].size,
		vertex_attribute[VBO_ATTRIB_VERTEX].type, GL_FALSE, sizeof *vertices,
		vertex_attribute[VBO_ATTRIB_VERTEX].offset);
    glEnableVertexAttribArray(VBO_ATTRIB_TEXTURE);
    glVertexAttribPointer(VBO_ATTRIB_TEXTURE,
		vertex_attribute[VBO_ATTRIB_TEXTURE].size,
		vertex_attribute[VBO_ATTRIB_TEXTURE].type, GL_FALSE, sizeof *vertices,
		vertex_attribute[VBO_ATTRIB_TEXTURE].offset);
	fprintf(stderr, "Draw: created vertex buffer, Vbo%u.\n",
		draw.arrays.vertices);

	/* textures */
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
	/* lighting */
	glActiveTexture(TexClassTexture(TEX_CLASS_NORMAL));
	if(!(draw.textures.light = light_compute_texture()))
		fprintf(stderr, "Draw: failed computing light texture.\n");
	/* textures stored in imgs */
	for(i = 0; i < max_auto_images; i++) texture(&auto_images[i]);

	/* Text rendering. */
	glGenFramebuffers(1, &draw.framebuffers.text);

	/* Shader initialisation. */
	if(!auto_Background(VBO_ATTRIB_VERTEX, VBO_ATTRIB_TEXTURE)) return Draw_(), 0;
	/* These are constant. @fixme Could they be declared as such? */
	glUniform1i(auto_Background_shader.sampler, TEX_CLASS_BACKGROUND);
	if(!auto_Far(VBO_ATTRIB_VERTEX, VBO_ATTRIB_TEXTURE)) return Draw_(), 0;
	glUniform1i(auto_Far_shader.bmp_sprite, TEX_CLASS_SPRITE);
	glUniform1i(auto_Far_shader.bmp_normal, TEX_CLASS_NORMAL);
	glUniform3f(auto_Far_shader.sun_direction, -0.2f, -0.2f, 0.1f);
	glUniform3fv(auto_Far_shader.sun_colour, 1, sunshine);
	glUniform1i(auto_Far_shader.foreshortening, 0.25f);
	if(!auto_Hud(VBO_ATTRIB_VERTEX, VBO_ATTRIB_TEXTURE)) return Draw_(), 0;
	glUniform1i(auto_Hud_shader.sampler, TEX_CLASS_SPRITE);
	if(!auto_Info(VBO_ATTRIB_VERTEX, VBO_ATTRIB_TEXTURE)) return Draw_(), 0;
	glUniform1i(auto_Info_shader.bmp_sprite, TEX_CLASS_SPRITE);
	if(!auto_Lambert(VBO_ATTRIB_VERTEX, VBO_ATTRIB_TEXTURE)) return Draw_(), 0;
	glUniform1i(auto_Lambert_shader.bmp_sprite, TEX_CLASS_SPRITE);
	glUniform1i(auto_Lambert_shader.bmp_normal, TEX_CLASS_NORMAL);
	glUniform3f(auto_Lambert_shader.sun_direction, -0.2f, -0.2f, 0.1f);
	glUniform3fv(auto_Lambert_shader.sun_colour, 1, sunshine);

	WindowIsGlError("Draw");

	draw.is_started = 1;
	return -1;
}

/** Destructor. */
void Draw_(void) {
	unsigned tex;
	int i;
	/* Erase the shaders. */
	auto_Lambert_();
	auto_Info_();
	/*auto_Lighting_();*/
	auto_Hud_();
	auto_Far_();
	auto_Background_();
	/* Erase the text frame-buffer if it exists. */
	glDeleteFramebuffers(1, &draw.framebuffers.text);
	/* Erase the textures. @fixme glDeleteTexture is safe to use. */
	for(i = max_auto_images - 1; i; i--) {
		if(!(tex = auto_images[i].texture)) continue;
		fprintf(stderr, "~Draw: erase texture, Tex%u.\n", tex);
		glDeleteTextures(1, &tex);
		auto_images[i].texture = 0;
	}
	draw.textures.background = draw.textures.shield = 0;
	/* Erase generated texture. */
	if(draw.textures.light) {
		fprintf(stderr, "~Draw: erase lighting texture, Tex%u.\n",
				draw.textures.light);
		glDeleteTextures(1, &draw.textures.light);
		draw.textures.light = 0;
	}
	if(draw.arrays.vertices && glIsBuffer(draw.arrays.vertices)) {
		fprintf(stderr, "~Draw: erase Vbo%u.\n", draw.arrays.vertices);
		glDeleteBuffers(1, &draw.arrays.vertices);
		draw.arrays.vertices = 0;
	}
	/* Get all the errors that we didn't catch. */
	WindowIsGlError("~Draw");
	draw.is_started = 0;
}

/** Sets the camera location.
 @param x: (x, y) in pixels. */
void DrawSetCamera(const struct Vec2f *const x) {
	if(!x) return;
	draw.camera.x.x = x->x, draw.camera.x.y = x->y;
}

/** Gets the visible part of the screen. */
void DrawGetScreen(struct Rectangle4f *const rect) {
	if(!rect) return;
	rect->x_min = draw.camera.x.x - draw.camera.extent.x;
	rect->x_max = draw.camera.x.x + draw.camera.extent.x;
	rect->y_min = draw.camera.x.y - draw.camera.extent.y;
	rect->y_max = draw.camera.x.y + draw.camera.extent.y;
}



/* Texture functions. */

/** Sets background to the image with {key} key. */
void DrawSetBackground(const char *const key) {
	struct AutoImage *image;
	/* clear the backgruoud; @fixme Test, it isn't used at all */
	if(!key) {
		draw.textures.background = 0;
		glActiveTexture(TexClassTexture(TEX_CLASS_BACKGROUND));
		glBindTexture(GL_TEXTURE_2D, 0);
		fprintf(stderr, "DrawSetBackground: image desktop cleared.\n");
		return;
	}
	if(!(image = AutoImageSearch(key)))
		{ fprintf(stderr, "DrawSetBackground: image \"%s\" not found.\n", key);
		return; }
	/* background_tex is a global; the witdh/height of the image can be found with background_tex */
	draw.textures.background = image->texture;
	glActiveTexture(TexClassTexture(TEX_CLASS_BACKGROUND));
	glBindTexture(GL_TEXTURE_2D, draw.textures.background);
	fprintf(stderr, "DrawSetBackground: image \"%s,\" (Tex%u,) set as "
		"desktop.\n", image->name, draw.textures.background);
}

/** @param str: Resource name to set the shield indicator. */
void DrawSetShield(const char *const key) {
	struct AutoImage *image;
	if(!(image = AutoImageSearch(key))) { fprintf(stderr,
		"DrawSetShield: image \"%s\" not found.\n", key); return; }
	draw.textures.shield = image->texture;
	glActiveTexture(TexClassTexture(TEX_CLASS_SPRITE));
	glBindTexture(GL_TEXTURE_2D, draw.textures.shield);
	fprintf(stderr, "DrawSetShield: image \"%s,\" (Tex%u,) set as shield.\n",
		image->name, draw.textures.shield);
}

#if 0
/** Doesn't work. */
static int text_compute_texture(void) {
	GLuint framebuffer_name = 0, rendered_texture;
	GLenum draw_buffers[] = { GL_COLOR_ATTACHMENT0 };
	/* http://www.opengl-tutorial.org/intermediate-tutorials/tutorial-14-render-to-texture/ */
	glGenFramebuffers(1, &framebuffer_name);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_name);
	glGenTextures(1, &rendered_texture);
	glBindTexture(GL_TEXTURE_2D, rendered_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 100, 100, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rendered_texture, 0);
	glDrawBuffers(sizeof draw_buffers / sizeof *draw_buffers, draw_buffers);
	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		return 0;
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_name);
	glViewport(0, 0, 100, 100);
	glUseProgram(auto_Info_shader.compiled);
	glUniform2f(auto_Info_shader.camera, 0.0f, 0.0f);
	glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 'X');
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return framebuffer_name;
}
#endif



/* This is the shader texture display. Each one goes with a certian shader, so
 the OpenGL state must be set to that shader before calling. */

/* Current texture; don't want to switch textures to the same one. */
static unsigned current_texture;

/** Only used as a callback from \see{display} while OpenGL is using Lambert.
 For \see{SpritesDraw}.
 @implements DrawOutput */
void DrawDisplayLambert(const struct Ortho3f *const x,
	const struct AutoImage *const tex, const struct AutoImage *const nor) {
	assert(x && tex && nor);
	if(current_texture != tex->texture) {
		glActiveTexture(TexClassTexture(TEX_CLASS_NORMAL));
		glBindTexture(GL_TEXTURE_2D, nor->texture);
		glActiveTexture(TexClassTexture(TEX_CLASS_SPRITE));
		glBindTexture(GL_TEXTURE_2D, tex->texture);
		current_texture = tex->texture;
		glUniform1f(auto_Lambert_shader.size, tex->width);
	}
	glUniform1f(auto_Lambert_shader.angle, x->theta);
	glUniform2f(auto_Lambert_shader.object, x->x, x->y);
	glDrawArrays(GL_TRIANGLE_STRIP, vertex_index_square.first,
		vertex_index_square.count);
}

/** Only used as a callback from \see{display} while OpenGL is using Far. For
 \see{FarsDraw}.
 @implements DrawOutput */
void DrawDisplayFar(const struct Ortho3f *const x,
	const struct AutoImage *const tex, const struct AutoImage *const nor) {
	assert(x && tex && nor);
	if(current_texture != tex->texture) {
		glActiveTexture(TexClassTexture(TEX_CLASS_NORMAL));
		glBindTexture(GL_TEXTURE_2D, nor->texture);
		glActiveTexture(TexClassTexture(TEX_CLASS_SPRITE));
		glBindTexture(GL_TEXTURE_2D, tex->texture);
		current_texture = tex->texture;
		glUniform1f(auto_Far_shader.size, tex->width);
	}
	glUniform1f(auto_Far_shader.angle, x->theta);
	glUniform2f(auto_Far_shader.object, x->x, x->y);
	glDrawArrays(GL_TRIANGLE_STRIP, vertex_index_square.first,
		vertex_index_square.count);
}

/** Only used as a callback from \see{display} while OpenGL is using Info. */
void DrawDisplayInfo(const struct Vec2f *const x,
	const struct AutoImage *const tex) {
	assert(x && tex);
	if(current_texture != tex->texture) {
		glActiveTexture(TexClassTexture(TEX_CLASS_SPRITE));
		glBindTexture(GL_TEXTURE_2D, tex->texture);
		current_texture = tex->texture;
		glUniform1f(auto_Info_shader.size, tex->width);
	}
	glUniform2f(auto_Info_shader.object, x->x, x->y);
	glDrawArrays(GL_TRIANGLE_STRIP, vertex_index_square.first,
		vertex_index_square.count);
}
