/** Copyright 2000, 2014 Neil Edelman, distributed under the terms of the GNU
 General Public License, see copying.txt.

 This is an idempotent class dealing with the interface to OpenGL.

 @title		Draw
 @author	Neil
 @version	3.2, 2015-05
 @since		1.0, 2000 */

#include <stdlib.h> /* free */
#include <assert.h> /* assert */
#include <math.h>   /* sqrtf fminf fmodf atan2f */
#include "../../build/Auto.h"
#include "../Print.h"
#include "../game/Sprites.h"
#include "../game/Fars.h"
#include "../game/Light.h"
#include "../game/Game.h"       /* shield display */
#include "../../external/lodepng.h"  /* texture() */
#include "../../external/nanojpeg.h" /* texture() */
#include "Glew.h"
#include "Draw.h"
#include "Window.h"

#include "../system/Timer.h" /* TimerGetTime */

/* Auto-generated, hard coded resouce files from Vsfs2h; run "make"
 and this should be automated.
 @fixme Ignore errors about ISO C90 string length 509. */
#include "../../build/shaders/Background_vsfs.h"
#include "../../build/shaders/Hud_vsfs.h"
#include "../../build/shaders/Far_vsfs.h"
#include "../../build/shaders/Lighting_vsfs.h"
#include "../../build/shaders/Info_vsfs.h"
#include "../../build/shaders/Lambert_vsfs.h"

#define M_2PI 6.283185307179586476925286766559005768394338798750211641949889
#define M_1_2PI 0.159154943091895335768883763372514362034459645740456448747667
#ifndef M_SQRT1_2
#define M_SQRT1_2 \
	0.707106781186547524400844362104849039284835937688474036588339868995366239
#endif

extern struct AutoImage auto_images[];
extern const int max_auto_images;

/* if is started, we don't and can't start it again */
static int is_started;

/** Texture addresses on the graphics card, when you want different textures
 simultaneously. Limit of {MAX_COMBINED_TEXTURE_IMAGE_UNITS}, which may be
 small, (like 2?) */
enum TexClass {
	TEX_CLASS_SPRITE,
	TEX_CLASS_NORMAL,
	TEX_CLASS_BACKGROUND
};
static GLuint TexClassTexture(const enum TexClass class) {
	assert(class < GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS);
	return class + GL_TEXTURE0;
}

/** Shader attribute assignment. {VBO_ATTRIB_CENTERED} is the normalised centre
 co-ordinates in which the sprite goes from [-1, 1]; {VBO_ATTRIB_TEXTURE} maps
 that to [0, 1] for texture mapping the sprite. */
enum { /* vec2 */ VBO_ATTRIB_VERTEX, /* vec2 */ VBO_ATTRIB_TEXTURE };
/** Corresponds to {(VBO_ATTRIB_VERTEX, VBO_ATTRIB_TEXTURE)}. */
static const struct {
	GLint size;
	GLenum type;
	const GLvoid *offset;
} vbo_attrib[] = {
	{ 2, GL_FLOAT, 0 },
	{ 2, GL_FLOAT, (GLvoid *)(sizeof(GLfloat) * 2) /* offset */ }
};

/** {struct} corresponding to the above. {vbo} is used ubiquitously for static
 geometry, uploaded into video memory. Fixme: not fully variablised!? */
static struct Vertex {
	GLfloat x, y;
	GLfloat s, t;
} vbo[] = {
	{  1.0f,  1.0f,  1.0f,  1.0f }, /* background; on update resize() changes */
	{  1.0f, -1.0f,  1.0f,  0.0f },
	{ -1.0f,  1.0f,  0.0f,  1.0f },
	{ -1.0f, -1.0f,  0.0f,  0.0f },
	{ 0.5f,  0.5f, 1.0f, 1.0f },    /* generic square, for sprites, etc */
	{ 0.5f, -0.5f, 1.0f, 0.0f },
	{ -0.5f, 0.5f, 0.0f, 1.0f },
	{ -0.5f,-0.5f, 0.0f, 0.0f }
};
/* Corresponds to the values in {vbo}. */
static const struct {
	GLint first;
	GLsizei count;
} vbo_info_bg = { 0, 4 }, vbo_info_square = { 4, 4 };

/* private prototypes */
static int texture(struct AutoImage *image); /* decompresses */
static int light_compute_texture(void); /* creates image */
static void display(void); /* callback for odisplay */
static void resize(int width, int height); /* callback  */

/* globals */
static GLuint vbo_geom, light_tex, background_tex, shield_tex;
static struct Vec2f camera, camera_extent;

/** Gets all the graphics stuff started.
 @return All good to draw? */
int Draw(void) {
	const float sunshine[] = { 1.0f * 3.0f, 1.0f * 3.0f, 1.0f * 3.0f };
	int i;

	if(is_started) return -1;

	if(!WindowStarted()) {
		warn("Draw: window not started.\n");
		return 0;
	}

	glutDisplayFunc(&display);
	glutReshapeFunc(&resize);

	/* http://www.opengl.org/sdk/docs/man2/xhtml/glBlendFunc.xml */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glPointSize(5.0f);
	glEnable(GL_POINT_SMOOTH);

	/* we don't need z-buffer depth things */
	/*glDepthFunc(GL_ALWAYS);*/
	glDisable(GL_DEPTH_TEST);

	/* vbo; glVertexAttribPointer(index attrib, size, type, normaised, stride, offset) */
	glGenBuffers(1, (GLuint *)&vbo_geom);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_geom);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vbo), vbo, GL_STATIC_DRAW);
	/* fixme: the texture should be the same as the vetices, half the data */
	/* fixme: done per-frame, because apparently OpenGl does not keep track of the bindings per-buffer */
	glEnableVertexAttribArray(VBO_ATTRIB_VERTEX);
	glVertexAttribPointer(VBO_ATTRIB_VERTEX, vbo_attrib[VBO_ATTRIB_VERTEX].size,
		vbo_attrib[VBO_ATTRIB_VERTEX].type, GL_FALSE, sizeof(struct Vertex),
		vbo_attrib[VBO_ATTRIB_VERTEX].offset);
    glEnableVertexAttribArray(VBO_ATTRIB_TEXTURE);
    glVertexAttribPointer(VBO_ATTRIB_TEXTURE,
		vbo_attrib[VBO_ATTRIB_TEXTURE].size,
		vbo_attrib[VBO_ATTRIB_TEXTURE].type, GL_FALSE, sizeof(struct Vertex),
		vbo_attrib[VBO_ATTRIB_TEXTURE].offset);
	debug("Draw: created vertex buffer, Vbo%u.\n", vbo_geom);

	/* textures */
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
	/* lighting */
	glActiveTexture(TexClassTexture(TEX_CLASS_NORMAL));
	if(!(light_tex = light_compute_texture()))
		warn("Draw: failed computing light texture.\n");
	/* textures stored in imgs */
	for(i = 0; i < max_auto_images; i++) texture(&auto_images[i]);

	/* shader initialisation */
	if(!auto_Background(VBO_ATTRIB_VERTEX, VBO_ATTRIB_TEXTURE)) return Draw_(), 0;
	/* these are constant; fixme: could they be declared as such? */
	glUniform1i(auto_Background_shader.sampler, TEX_CLASS_BACKGROUND);
	if(!auto_Far(VBO_ATTRIB_VERTEX, VBO_ATTRIB_TEXTURE)) return Draw_(), 0;
	glUniform1i(auto_Far_shader.bmp_sprite, TEX_CLASS_SPRITE);
	glUniform1i(auto_Far_shader.bmp_normal, TEX_CLASS_NORMAL);
	glUniform3f(auto_Far_shader.sun_direction, -0.2f, -0.2f, 0.1f);
	glUniform3fv(auto_Far_shader.sun_colour, 1, sunshine);
	glUniform1i(auto_Far_shader.foreshortening, 0.25f);
	if(!auto_Hud(VBO_ATTRIB_VERTEX, VBO_ATTRIB_TEXTURE)) return Draw_(), 0;
	glUniform1i(auto_Hud_shader.sampler, TEX_CLASS_SPRITE);
	if(!auto_Lighting(VBO_ATTRIB_VERTEX, VBO_ATTRIB_TEXTURE)) return Draw_(),0;
	glUniform1i(auto_Lighting_shader.sampler, TEX_CLASS_SPRITE);
	glUniform1i(auto_Lighting_shader.sampler_light, TEX_CLASS_NORMAL);
	glUniform1f(auto_Lighting_shader.directional_angle, -2.0f);
	glUniform3fv(auto_Lighting_shader.directional_colour, 1, sunshine);
	if(!auto_Info(VBO_ATTRIB_VERTEX, VBO_ATTRIB_TEXTURE)) return Draw_(), 0;
	glUniform1i(auto_Info_shader.bmp_sprite, TEX_CLASS_SPRITE);
	if(!auto_Lambert(VBO_ATTRIB_VERTEX, VBO_ATTRIB_TEXTURE)) return Draw_(), 0;
	glUniform1i(auto_Lambert_shader.bmp_sprite, TEX_CLASS_SPRITE);
	glUniform1i(auto_Lambert_shader.bmp_normal, TEX_CLASS_NORMAL);
	glUniform3f(auto_Lambert_shader.sun_direction, -0.2f, -0.2f, 0.1f);
	glUniform3fv(auto_Lambert_shader.sun_colour, 1, sunshine);

	WindowIsGlError("Draw");

	is_started = -1;
	return -1;
}

/** Destructor. */
void Draw_(void) {
	unsigned tex;
	int i;

	/* erase the shaders */
	auto_Lambert_();
	auto_Info_();
	auto_Lighting_();
	auto_Hud_();
	auto_Far_();
	auto_Background_();
	/* erase the textures */
	for(i = max_auto_images - 1; i; i--) {
		if(!(tex = auto_images[i].texture)) continue;
		debug("~Draw: erase texture, Tex%u.\n", tex);
		glDeleteTextures(1, &tex);
		auto_images[i].texture = 0;
	}
	if(light_tex) {
		debug("~Draw: erase lighting texture, Tex%u.\n", light_tex);
		glDeleteTextures(1, &light_tex);
		light_tex = 0;
	}
	if(vbo_geom && glIsBuffer(vbo_geom)) {
		debug("~Draw: erase Vbo%u.\n", vbo_geom);
		glDeleteBuffers(1, &vbo_geom);
		vbo_geom = 0;
	}
	/* get all the errors that we didn't catch */
	WindowIsGlError("~Draw");

	is_started = 0;
}

/** Sets the camera location.
 @param x: (x, y) in pixels. */
void DrawSetCamera(const struct Vec2f *const x) {
	if(!x) return;
	camera.x = x->x, camera.y = x->y;
}

/** Gets the visible part of the screen. */
void DrawGetScreen(struct Rectangle4f *const rect) {
	if(!rect) return;
	rect->x_min = camera.x - camera_extent.x;
	rect->x_max = camera.x + camera_extent.x;
	rect->y_min = camera.y - camera_extent.y;
	rect->y_max = camera.y + camera_extent.y;
}

/** Sets background to the image with key key.
 @fixme allows you to set not {TexClassTexture(TEX_CLASS_BACKGROUND)} textures,
 which probably don't work, maybe? (oh, they do.) */
void DrawSetBackground(const char *const key) {
	struct AutoImage *image;

	/* clear the backgruoud; fixme: test, it isn't used at all */
	if(!key) {
		background_tex = 0;
		glActiveTexture(TexClassTexture(TEX_CLASS_BACKGROUND));
		glBindTexture(GL_TEXTURE_2D, 0);
		debug("DrawSetBackground: image desktop cleared.\n");
		return;
	}
	if(!(image = AutoImageSearch(key))) {
		warn("DrawSetBackground: image \"%s\" not found.\n", key);
		return;
	}
	/* background_tex is a global; the witdh/height of the image can be found with background_tex */
	background_tex = image->texture;
	glActiveTexture(TexClassTexture(TEX_CLASS_BACKGROUND));
	glBindTexture(GL_TEXTURE_2D, background_tex);
	debug("DrawSetBackground: image \"%s,\" (Tex%u,) set as desktop.\n",
		image->name, background_tex);
}

/** @param str: Resource name to set the shield indicator. */
void DrawSetShield(const char *const key) {
	struct AutoImage *image;
	if(!(image = AutoImageSearch(key))) {
		warn("DrawSetShield: image \"%s\" not found.\n", key);
		return;
	}
	shield_tex = image->texture;
	glActiveTexture(TexClassTexture(TEX_CLASS_SPRITE));
	glBindTexture(GL_TEXTURE_2D, shield_tex);
	debug("DrawSetShield: image \"%s,\" (Tex%u,) set as shield.\n",
		image->name, shield_tex);
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

	/* uncompress the image! */
	switch(image->data_format) {
		case IF_PNG:
			/* fixme: this will force it to be 32-bit RGBA even if it doesn't
			 have an alpha channel, extremely wasteful */
			if((error = lodepng_decode32(&pic, &width, &height, image->data,
				image->data_size))) {
				warn("texture: lodepng error %u: %s\n", error,
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
			warn("texture: unknown image format.\n");
	}
	if(!is_alloc) {
		warn("texture: allocation failed.\n");
		return 0;
	}
	if(width != image->width || height != image->height || depth != image->depth) {
		warn("texture: dimension mismatch %u:%ux%u vs %u:%ux%u.\n",
			image->width, image->height, image->depth, width, height, depth);
		is_bad = -1;
	}
	/* select image format */
	switch(depth) {
		case 1:
			/* we use exclusively for shaders, so I don't think this matters */
			/* GL_LUMINANCE; <- "core context depriciated," I was using that */
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
			warn("texture: not a recognised depth, %d.\n", depth);
			is_bad = -1;
	}
	/* invert to go with OpenGL's strict adherence to standards vs traditions */
	{
		unsigned bytes_line = width * depth;
		unsigned top, bottom, i;
		unsigned char *pb_t, *pb_b;
		for(top = 0, bottom = height - 1; top < bottom; top++, bottom--) {
			pb_t = pic + top    * bytes_line;
			pb_b = pic + bottom * bytes_line;
			for(i = 0; i < bytes_line; i++, pb_t++, pb_b++) {
				*pb_t ^= *pb_b;
				*pb_b ^= *pb_t;
				*pb_t ^= *pb_b;
			}
		}
	}
	/* load the uncompressed image into a texture */
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
		pedantic("texture: %u:\n", image->texture);
		if(image->width <= 80) image_print(image, pic);
		else pedantic(" . . . too big to show.\n");
#endif
	}
	/* free the pic */
	switch(image->data_format) {
		case IF_PNG:
			free(pic);
			break;
		case IF_JPEG:
			njDone();
			break;
		default:
			break;
	}
	debug("texture: created %dx%dx%d texture, Tex%u.\n",
		width, height, depth, tex);

	WindowIsGlError("texture");

	return tex ? -1 : 0;
}

/* Creates a hardcoded lighting texture with the Red the radius and Green the
 angle.
 @return The texture or zero. */
static int light_compute_texture(void) {
	int   i, j;
	float x, y;
	float *buffer; /*[512][512][2]; MSVC does not have space on the stack*/
	/*const int buffer_ysize = sizeof(buffer)  / sizeof(*buffer);
	const int buffer_xsize = sizeof(*buffer) / sizeof(**buffer);*/
	const int buffer_size = 1024/*512*/; /* width/height */
	const float buffer_norm = (float)M_SQRT1_2 * 4.0f /
		sqrtf(2.0f * buffer_size * buffer_size);
	unsigned name;

	if(!(buffer = malloc(sizeof(float) * buffer_size * buffer_size << 1))) return 0;
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
	debug("texture: created %dx%dx%d hardcoded lighting texture, Tex%u.\n",
		buffer_size, buffer_size, 2, name);

	WindowIsGlError("light_compute_texture");

	return name;
}

int draw_is_print_sprites;
static unsigned old_texture;

/** Only used as a callback from \see{display} while OpenGL is using Lambert.
 @implements LambertOutput */
static void display_lambert(const struct Ortho3f *const x,
	const struct AutoImage *const tex, const struct AutoImage *const nor) {
	assert(x && tex && nor);
	if(old_texture != tex->texture) {
		glActiveTexture(TexClassTexture(TEX_CLASS_NORMAL));
		glBindTexture(GL_TEXTURE_2D, nor->texture);
		glActiveTexture(TexClassTexture(TEX_CLASS_SPRITE));
		glBindTexture(GL_TEXTURE_2D, tex->texture);
		old_texture = tex->texture;
		glUniform1f(auto_Lambert_shader.size, tex->width);
	}
	glUniform1f(auto_Lambert_shader.angle, x->theta);
	glUniform2f(auto_Lambert_shader.object, x->x, x->y);
	glDrawArrays(GL_TRIANGLE_STRIP,vbo_info_square.first,vbo_info_square.count);
}

/** Only used as a callback from \see{display} while OpenGL is using Far.
 @implements FarOutput */
static void far_lambert(const struct Ortho3f *const x,
	const struct AutoImage *const tex, const struct AutoImage *const nor) {
	assert(x && tex && nor);
	if(old_texture != tex->texture) {
		glActiveTexture(TexClassTexture(TEX_CLASS_NORMAL));
		glBindTexture(GL_TEXTURE_2D, nor->texture);
		glActiveTexture(TexClassTexture(TEX_CLASS_SPRITE));
		glBindTexture(GL_TEXTURE_2D, tex->texture);
		old_texture = tex->texture;
		glUniform1f(auto_Far_shader.size, tex->width);
	}
	glUniform1f(auto_Far_shader.angle, x->theta);
	glUniform2f(auto_Far_shader.object, x->x, x->y);
	glDrawArrays(GL_TRIANGLE_STRIP,vbo_info_square.first,vbo_info_square.count);
}

/** Only used as a callback from \see{display} while OpenGL is using Sprite.
 @implements SpriteOutput */
static void display_info(const struct Vec2f *const x,
	const struct AutoImage *const tex) {
	assert(x && tex);
	if(old_texture != tex->texture) {
		glActiveTexture(TexClassTexture(TEX_CLASS_SPRITE));
		glBindTexture(GL_TEXTURE_2D, tex->texture);
		old_texture = tex->texture;
		glUniform1f(auto_Info_shader.size, tex->width);
	}
	glUniform2f(auto_Info_shader.object, x->x, x->y);
	glDrawArrays(GL_TRIANGLE_STRIP,vbo_info_square.first,vbo_info_square.count);
}

/** Callback for glutDisplayFunc; this is where all of the drawing happens. */
static void display(void) {
	int lights;
	/* for SpriteIterate */
	/*struct Ortho3f x;
	unsigned old_tex = 0, tex, size;*/

	/* glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	 <- https://www.khronos.org/opengl/wiki/Common_Mistakes
	 "The buffers should always be cleared. On much older hardware, there was
	 a technique to get away without clearing the scene, but on even semi-recent
	 hardware, this will actually make things slower. So always do the clear."
	 I'm not using those features, thought! and the screen cover is complete, I
	 don't see how doing extra work for nothing is going to make it faster.
	 "A technique," haha, it was pretty ubiquitous; only newbies cleared the
	 screen. */

	/* use sprites; triangle strips, two to form a square, vertex buffer,
	 [-0.5, 0.5 : -0.5, 0.5] */
	glBindBuffer(GL_ARRAY_BUFFER, vbo_geom);

	/* fixme: don't use painters' algorithm; stencil test! */

	/* background (desktop):
	 turn off transperency
	 background vbo
	 update glUniform1i(GLint location, GLint v0)
	 update glUniformMatrix4fv(location, count, transpose, *value)
	 glDrawArrays(type flag, offset, no) */
	if(background_tex) {
		/* use background shader */
		glUseProgram(auto_Background_shader.compiled);
		/* turn transperency off */
		glDisable(GL_BLEND);
		/* why?? */
		glActiveTexture(TexClassTexture(TEX_CLASS_BACKGROUND));
		/* fixme: of course it's a background, set once */
		/*glUniform1i(background_sampler_location, TEX_CLASS_BACKGROUND);*/
		/*glUniformMatrix4fv(tex_map_matrix_location, 1, GL_FALSE, background_matrix);*/
		glDrawArrays(GL_TRIANGLE_STRIP, vbo_info_bg.first, vbo_info_bg.count);
	}
	glEnable(GL_BLEND);

	/* Draw far objects. */
	glUseProgram(auto_Far_shader.compiled);
	glUniform2f(auto_Far_shader.camera, camera.x, camera.y);
	/*SpritesDrawFar(&far_lambert);*/

	/* Set up lights, draw sprites in foreground. */
	glUseProgram(auto_Lambert_shader.compiled);
	glUniform2f(auto_Lambert_shader.camera, camera.x, camera.y);
	glUniform1i(auto_Lambert_shader.points, lights = LightGetArraySize());
	if(lights) {
		glUniform2fv(auto_Lambert_shader.point_position, lights,
			(GLfloat *)LightGetPositionArray());
		glUniform3fv(auto_Lambert_shader.point_colour, lights,
			(GLfloat *)LightGetColourArray());
	}
	SpritesDrawForeground(&display_lambert);

	/* Display info on top. */
	glUseProgram(auto_Info_shader.compiled);
	glUniform2f(auto_Info_shader.camera, camera.x, camera.y);
	/*SpritesDrawInfo(&display_info);*/

	/* Reset texture for next frame. */
	old_texture = 0;

	if(draw_is_print_sprites) draw_is_print_sprites = 0;

#if 0
	/* overlay hud */
	if(shield_tex && (player = GameGetPlayer())) {
		struct Vec2f x;
		struct Vec2u hit;
		ShipGetPosition(player, &x);
		glUseProgram(auto_Hud_shader.compiled);
		glBindTexture(GL_TEXTURE_2D, shield_tex);
		glUniform2f(auto_Hud_shader.camera, camera.x, camera.y);
		glUniform2f(auto_Hud_shader.size, 256.0f, 8.0f);
		glUniform2f(auto_Hud_shader.position, x.x, x.y + 64.0f);
		ShipGetHit(player, &hit);
		glUniform2i(auto_Hud_shader.shield, hit.x, hit.y);
		glDrawArrays(GL_TRIANGLE_STRIP, vbo_info_square.first, vbo_info_square.count);
	}
#endif

	/* Disable, swap. */
	glUseProgram(0);
	glutSwapBuffers();

	/*WindowIsGlError("display");*/
}

/** Callback for glutReshapeFunc.
 @fixme not guaranteed to have a background! this will crash.
 @param width, height: The size of the client area. */
static void resize(int width, int height) {
	struct Vec2f two_screen;
	int w_tex, h_tex;
	float w_w_tex, h_h_tex;

	/*debug("resize: %dx%d.\n", width, height);*/
	if(width <= 0 || height <= 0) return;
	glViewport(0, 0, width, height);
	camera_extent.x = width / 2.0f, camera_extent.y = height / 2.0f; /* global*/

	/* resize the background */
	/* glActiveTexture(TEX_CLASS_BACKGROUND); this does nothing? */
	glBindTexture(GL_TEXTURE_2D, background_tex);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH,  &w_tex);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h_tex);
	/*debug("w %d h %d\n", w_tex, h_tex);*/
	w_w_tex = (float)width  / w_tex;
	h_h_tex = (float)height / h_tex;

	/* update the background texture vbo on the card with global data in here */
	vbo[0].s = vbo[1].s =  w_w_tex;
	vbo[2].s = vbo[3].s = -w_w_tex;
	vbo[0].t = vbo[2].t =  h_h_tex;
	vbo[1].t = vbo[3].t = -h_h_tex;
	glBufferSubData(GL_ARRAY_BUFFER, vbo_info_bg.first,
		vbo_info_bg.count * (GLsizei)sizeof(struct Vertex), vbo + vbo_info_bg.first);

	/* the image may not cover the whole drawing area, so we may need a constant
	 scaling; if it is so, the image will have to be linearly interpolated for
	 quality */
	glUseProgram(auto_Background_shader.compiled);
	glBindTexture(GL_TEXTURE_2D, background_tex);
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

	/* update the shaders; YOU MUST CALL THIS IF THE PROGRAMME HAS ANY
	 DEPENDENCE ON SCREEN SIZE */
	/* update the inverse screen on the card */
	two_screen.x = 2.0f / width;
	two_screen.y = 2.0f / height;
	/* fixme */
	glUseProgram(auto_Hud_shader.compiled);
	glUniform2f(auto_Hud_shader.two_screen, two_screen.x, two_screen.y);
	glUseProgram(auto_Far_shader.compiled);
	glUniform2f(auto_Far_shader.projection, two_screen.x, two_screen.y);
	/* fixme! */
	glUseProgram(auto_Lighting_shader.compiled);
	glUniform2f(auto_Lighting_shader.two_screen, two_screen.x, two_screen.y);
	glUseProgram(auto_Info_shader.compiled);
	glUniform2f(auto_Info_shader.projection, two_screen.x, two_screen.y);
	glUseProgram(auto_Lambert_shader.compiled);
	glUniform2f(auto_Lambert_shader.projection, two_screen.x, two_screen.y);
}
