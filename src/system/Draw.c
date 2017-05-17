/** Copyright 2000, 2014 Neil Edelman, distributed under the terms of the GNU
 General Public License, see copying.txt.

 This is an idempotent class dealing with the interface to OpenGL.

 @title		Draw
 @author	Neil
 @version	3.2, 2015-05
 @since		1.0, 2000 */

#include <stdlib.h> /* free */
#include <assert.h> /* assert */
#include "../../build/Auto.h"
#include "../Print.h"
#include "../game/Sprite.h"
#include "../game/Far.h"
#include "../game/Light.h"
#include "../game/Game.h"       /* shield display */
#include "../../external/lodepng.h"  /* texture() */
#include "../../external/nanojpeg.h" /* texture() */
#include "Glew.h"
#include "Draw.h"
#include "Window.h"

/* Auto-generated, hard coded resouce files from Vsfs2h; run "make"
 and this should be automated.
 @fixme Ignore errors about ISO C90 string length 509. */
#include "../../build/shaders/Background_vsfs.h"
#include "../../build/shaders/Hud_vsfs.h"
#include "../../build/shaders/Far_vsfs.h"
#include "../../build/shaders/Lighting_vsfs.h"
#include "../../build/shaders/Phong_vsfs.h"

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

#if 0
/* for textures; assert GlTex: GL_TEXTUREx <-> TexArray: x;
 fixme: do more complex texture mapping management once we have textures
 "The constants obey TEXTUREi = TEXTURE0+i (i is in the range 0 to k - 1,
 where k is the value of MAX_COMBINED_TEXTURE_IMAGE_UNITS)." */
enum GlTex {
	TexClassTexture(TEX_CLASS_NORMAL) = GL_TEXTURE0,
	TexClassTexture(TEX_CLASS_BACKGROUND) = GL_TEXTURE1,
	TexClassTexture(TEX_CLASS_SPRITE) = GL_TEXTURE2
};
enum TexArray {
	TEX_CLASS_NORMAL = 0,
	TEX_CLASS_BACKGROUND = 1,
	TEX_CLASS_SPRITE = 2
};
#endif

/* for shader attribute assigment */
enum VertexAttribs { G_POSITION, G_TEXTURE };
/*enum SpotAttribs { S_POSITION, S_COLOUR };*/

/* corresponds to VertexAttribs on hardware;
 vbo is used ubiquitously for static geometry, uploaded into video memory;
 fixme: it's just a shift, scale; we don't need half of them
 (the G_POSITIONs, since they're already being transformed); twice the stuff
 could be shown on the screen! */
static struct Vertex {
	GLfloat x, y;
	GLfloat s, t;
} vbo[] = {
	{  1.0f,  1.0f,  1.0f,  1.0f }, /* background; on update resize() changes */
	{  1.0f, -1.0f,  1.0f,  0.0f },
	{ -1.0f,  1.0f,  0.0f,  1.0f },
	{ -1.0f, -1.0f,  0.0f,  0.0f },
	{ 0.5f,  0.5f, 1.0f, 1.0f },    /* sprite */
	{ 0.5f, -0.5f, 1.0f, 0.0f },
	{ -0.5f, 0.5f, 0.0f, 1.0f },
	{ -0.5f,-0.5f, 0.0f, 0.0f }
};
static const void *vertex_pos_offset = 0;
static const int  vertex_pos_size    = 2;
static const void *vertex_tex_offset = (void *)(sizeof(float) * 2);
static const int  vertex_tex_size    = 2;
static const int vbo_bg_first     = 0, vbo_bg_count     = 4;
static const int vbo_sprite_first = 4, vbo_sprite_count = 4;

/* corresponds to SpotAttribs on hardware */
/*static struct Spot {
	GLfloat x, y;
	GLfloat r, g, b;
} spots[512];
static const int spot_capacity = sizeof(spots) / sizeof(struct Spot);
static int       spot_size;
static const void *spot_pos_offset    = 0;
static const int  spot_pos_size       = 2;
static const void *spot_colour_offset = (void *)(sizeof(float) * 2);
static const int  spot_colour_size    = 3;*/

/* this is for drawing the background with tex_map programme; 2 * 0.5 = 1,
 column -- fixme: not really necessary for tex_map_shader to have all these
 degrees-of-freedom; position on screen and size should be enough */
/*static const GLfloat background_matrix[] = {
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f
};*/

/* private prototypes */
static int texture(struct AutoImage *image); /* decompresses */
static int light_compute_texture(void); /* creates image */
static void display(void); /* callback for odisplay */
static void resize(int width, int height); /* callback  */

/* globals */
static int     screen_width = 300, screen_height = 200;
static GLuint  vbo_geom, /*spot_geom,*/ light_tex, background_tex, shield_tex;
static GLfloat two_width, two_height;
static float   camera_x, camera_y;

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
	glEnableVertexAttribArray(G_POSITION);
	glVertexAttribPointer(G_POSITION, vertex_pos_size, GL_FLOAT,
		GL_FALSE, sizeof(struct Vertex), vertex_pos_offset);
    glEnableVertexAttribArray(G_TEXTURE);
    glVertexAttribPointer(G_TEXTURE, vertex_tex_size, GL_FLOAT, GL_FALSE,
		sizeof(struct Vertex), vertex_tex_offset);
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
	if(!auto_Background(G_POSITION, G_TEXTURE)) return Draw_(), 0;
	/* these are constant; fixme: could they be declared as such? */
	glUniform1i(auto_Background_shader.sampler, TEX_CLASS_BACKGROUND);
	if(!auto_Far(G_POSITION, G_TEXTURE)) return Draw_(), 0;
	glUniform1i(auto_Far_shader.sampler, TEX_CLASS_SPRITE);
	glUniform1i(auto_Far_shader.sampler_light, TEX_CLASS_NORMAL);
	glUniform1f(auto_Far_shader.directional_angle, -2.0f);
	glUniform3fv(auto_Far_shader.directional_colour, 1, sunshine);
	if(!auto_Hud(G_POSITION, G_TEXTURE)) return Draw_(), 0;
	glUniform1i(auto_Hud_shader.sampler, TEX_CLASS_SPRITE);
	if(!auto_Lighting(G_POSITION, G_TEXTURE)) return Draw_(), 0;
	glUniform1i(auto_Lighting_shader.sampler, TEX_CLASS_SPRITE);
	glUniform1i(auto_Lighting_shader.sampler_light, TEX_CLASS_NORMAL);
	glUniform1f(auto_Lighting_shader.directional_angle, -2.0f);
	glUniform3fv(auto_Lighting_shader.directional_colour, 1, sunshine);
	if(!auto_Phong(G_POSITION, G_TEXTURE)) return Draw_(), 0;

	WindowIsGlError("Draw");

	is_started = -1;
	return -1;
}

/** Destructor. */
void Draw_(void) {
	unsigned tex;
	int i;

	/* erase the shaders */
	auto_Phong_();
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
 @param x, y: (x, y) in pixels. */
void DrawSetCamera(const float x, const float y) {
	if(!is_started) return;
	camera_x = x;
	camera_y = y;
}

/** Gets the camera position and stores it in (*x_ptr, *y_ptr). */
void DrawGetCamera(float *x_ptr, float *y_ptr) {
	/* just set it to last \/ if(!is_started) return;*/
	*x_ptr = camera_x;
	*y_ptr = camera_y;
}

/** Gets the width and height. */
void DrawGetScreen(unsigned *width_ptr, unsigned *height_ptr) {
	/*if(!is_started) return; no, easier to guarantee valid */
	if(width_ptr)  *width_ptr  = screen_width;
	if(height_ptr) *height_ptr = screen_height;
}

/** Sets background to the image with key key. Fixme: allows you to set not
 TexClassTexture(TEX_CLASS_BACKGROUND) textures, which probably don't work, maybe? (oh, they do) */
void DrawSetBackground(const char *const str) {
	struct AutoImage *image;
	
	/* clear the backgruoud; fixme: test, it isn't used at all */
	if(!str) {
		background_tex = 0;
		glActiveTexture(TexClassTexture(TEX_CLASS_BACKGROUND));
		glBindTexture(GL_TEXTURE_2D, 0);
		debug("DrawSetBackground: image desktop cleared.\n");
		return;
	}
	if(!(image = AutoImageSearch(str))) {
		warn("DrawSetBackground: image \"%s\" not found.\n", str);
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
void DrawSetShield(const char *const str) {
	struct AutoImage *image;
	if(!(image = AutoImageSearch(str))) {
		warn("DrawSetShield: image \"%s\" not found.\n", str);
		return;
	}
	shield_tex = image->texture;
	glActiveTexture(TexClassTexture(TEX_CLASS_SPRITE));
	glBindTexture(GL_TEXTURE_2D, shield_tex);
	debug("DrawSetShield: image \"%s,\" (Tex%u,) set as shield.\n",
		image->name, shield_tex);
}

/** Creates a texture from an image; sets the image texture unit.
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
			if((error = lodepng_decode32(&pic, &width, &height, image->data,
				image->data_size))) {
				warn("texture: lodepng error %u: %s\n", error,
					lodepng_error_text(error));
				break;
			}
			is_alloc = -1;
			depth    = 4; /* fixme: not all pngs have four? */
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

/** Callback for glutDisplayFunc; this is where all of the drawing happens. */
static void display(void) {
	struct Sprite *player;
	int lights;
	/* for SpriteIterate */
	float x, y, t;
	unsigned old_texture = 0, tex, size;

	/* glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	 <- "The buffers should always be cleared. On much older hardware, there was
	 a technique to get away without clearing the scene, but on even semi-recent
	 hardware, this will actually make things slower. So always do the clear."
	 I'm not using those features, though! and the screen cover is complete, I
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
		glDrawArrays(GL_TRIANGLE_STRIP, vbo_bg_first, vbo_bg_count);
	}
	glEnable(GL_BLEND);

	/* use simple tex_map_shader */
	/*glUseProgram(tex_map_shader);*/
	/* why? the glsl entirely specifies this */
	glActiveTexture(TexClassTexture(TEX_CLASS_SPRITE));

	/* turn on background lighting (sprites) */
	glUseProgram(auto_Far_shader.compiled);
	/*glUseProgram(light_shader);
	glUniform1i(light_lights_location, 0);*/

	/* background sprites */
	/*const->glUniform1i(far_texture_location, TEX_CLASS_SPRITE); */
	glUniform2f(auto_Far_shader.camera, camera_x, camera_y);
	while(FarIterate(&x, &y, &t, &tex, &size)) {
		if(old_texture != tex) {
			glBindTexture(GL_TEXTURE_2D, tex);
			old_texture = tex;
		}
		glUniform1f(auto_Far_shader.size, (float)size);
		glUniform1f(auto_Far_shader.angle, t);
		glUniform2f(auto_Far_shader.position, x, y);
		glDrawArrays(GL_TRIANGLE_STRIP, vbo_sprite_first, vbo_sprite_count);
	}
	old_texture = 0;

	/* turn on lighting */
	glUseProgram(auto_Lighting_shader.compiled);
	glUniform1i(auto_Lighting_shader.lights, lights = LightGetArraySize());
	if(lights) {
		glUniform2fv(auto_Lighting_shader.light_position, lights, (GLfloat *)LightGetPositionArray());
		glUniform3fv(auto_Lighting_shader.light_colour, lights, (GLfloat *)LightGetColourArray());
	}

	/* sprites:
	 turn on transperency
	 sprite vbo
	 for @ sprite:
	 -calculate matrix
	 -upload to card
	 -draw */
	/*glEnable(GL_BLEND);*/
	/* fixme: have different indices to textures; keep track with texture manager; have to worry about how many tex units there are */
	/*glUniform1i(light_texture_location, TEX_CLASS_SPRITE); <- constant, now */
	glUniform2f(auto_Lighting_shader.camera, camera_x, camera_y);
	if(draw_is_print_sprites) debug("display: dump:\n");
	while(SpriteIterate(&x, &y, &t, &tex, &size)) {
		if(draw_is_print_sprites) debug("\tTex%d Size %d (%.1f,%.1f:%.1f)\n", tex, size, x, y, t);
		/* draw a sprite; fixme: minimise texture transitions */
		if(old_texture != tex) {
			glBindTexture(GL_TEXTURE_2D, tex);
			old_texture = tex;
		}
		glUniform1f(auto_Lighting_shader.size, (float)size);
		glUniform1f(auto_Lighting_shader.angle, t);
		glUniform2f(auto_Lighting_shader.position, x, y);
		glDrawArrays(GL_TRIANGLE_STRIP, vbo_sprite_first, vbo_sprite_count);
	}
	if(draw_is_print_sprites) {
		draw_is_print_sprites = 0;
	}

	/* create spots */
	/*glBindBuffer(GL_ARRAY_BUFFER, spot_geom);
	glEnableVertexAttribArray(S_POSITION);
	glVertexAttribPointer(S_POSITION, spot_pos_size, GL_FLOAT, GL_FALSE, sizeof(struct Spot), spot_pos_offset);
	glEnableVertexAttribArray(S_COLOUR);
	glVertexAttribPointer(S_COLOUR, spot_colour_size, GL_FLOAT, GL_FALSE, sizeof(struct Spot), spot_colour_offset);
	glDrawArrays(GL_POINTS, 0, 1);
	glDisableVertexAttribArray(S_POSITION);
	glDisableVertexAttribArray(S_COLOUR);*/

	/* overlay hud */
	if(shield_tex && (player = GameGetPlayer())) {
		SpriteGetPosition(player, &x, &y);
		t = SpriteGetBounding(player);
		glUseProgram(auto_Hud_shader.compiled);
		glBindTexture(GL_TEXTURE_2D, shield_tex);
		glUniform2f(auto_Hud_shader.camera, camera_x, camera_y);
		glUniform2f(auto_Hud_shader.size, 256.0f, 8.0f);
		glUniform2f(auto_Hud_shader.position, x, y - t * 2.0f);
		glUniform2i(auto_Hud_shader.shield, SpriteGetHit(player), SpriteGetMaxHit(player));
		glDrawArrays(GL_TRIANGLE_STRIP, vbo_sprite_first, vbo_sprite_count);
	}

	/* disable, swap */
	glUseProgram(0);
	glutSwapBuffers();

	WindowIsGlError("display");
}

/** Callback for glutReshapeFunc.
 @fixme not guaranteed to have a background! this will crash.
 @param width, height: The size of the client area. */
static void resize(int width, int height) {
	int w_tex, h_tex;
	float w_w_tex, h_h_tex;

	debug("resize: %dx%d.\n", width, height);
	if(width <= 0 || height <= 0) return;
	glViewport(0, 0, width, height);
	screen_width  = width; /* global shared with drawing */
	screen_height = height;

	/* update the inverse screen on the card */
	two_width  = 2.0f / width;
	two_height = 2.0f / height;

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
	glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)vbo_bg_first,
		(GLsizeiptr)(vbo_bg_count * sizeof(struct Vertex)), vbo + vbo_bg_first);

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
		/* fixme: here? */
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	/* far shader and light shader also need updates of 2/[width|height];
	 meh, we can probably do them in sw? */
	glUseProgram(auto_Hud_shader.compiled);
	glUniform2f(auto_Hud_shader.two_screen, two_width, two_height);
	glUseProgram(auto_Far_shader.compiled);
	glUniform2f(auto_Far_shader.two_screen, two_width, two_height);
	glUseProgram(auto_Lighting_shader.compiled);
	glUniform2f(auto_Lighting_shader.two_screen, two_width, two_height);
}
