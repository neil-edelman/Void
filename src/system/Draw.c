/* Copyright 2000, 2014 Neil Edelman, distributed under the terms of the GNU
 General Public License, see copying.txt */

#include <stdlib.h> /* malloc free */
#include <math.h>   /* cis */
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

/* auto-generated, hard coded resouce files; there should be the directory
 tools/ where you can compile utilities that can make these files; run "make"
 and this should be automated; ignore errors about ISO C90 string length 509  */
#include "../../build/shaders/Background_vs.h"
#include "../../build/shaders/Background_fs.h"
#include "../../build/shaders/Hud_vs.h"
#include "../../build/shaders/Hud_fs.h"
#include "../../build/shaders/Far_vs.h"
#include "../../build/shaders/Far_fs.h"
#include "../../build/shaders/Lighting_vs.h"
#include "../../build/shaders/Lighting_fs.h"

#define M_2PI 6.283185307179586476925286766559005768394338798750211641949889
#define M_1_2PI 0.159154943091895335768883763372514362034459645740456448747667
#ifndef M_SQRT1_2
#define M_SQRT1_2 0.707106781186547524400844362104849039284835937688474036588339868995366239
#endif

/** This is an idempotent class dealing with the interface to OpenGL.
 @author	Neil
 @version	3.2, 2015-05
 @since		1.0, 2000 */

extern struct AutoImage auto_images[];
extern const int max_auto_images;

/* if is started, we don't and can't start it again */
static int is_started;

/* for textures; assert GlTex: GL_TEXTUREx <-> TexArray: x;
 fixme: do more complex texture mapping management once we have textures
 "The constants obey TEXTUREi = TEXTURE0+i (i is in the range 0 to k - 1,
 where k is the value of MAX_COMBINED_TEXTURE_IMAGE_UNITS)." */
enum GlTex {
	GT_LIGHT = GL_TEXTURE0,
	GT_BACKGROUND = GL_TEXTURE1,
	GT_SPRITES = GL_TEXTURE2
};
enum TexArray {
	T_LIGHT = 0,
	T_BACKGROUND = 1,
	T_SPRITES = 2
};

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
static GLuint link_shader(const char *vert_vs, const char *frag_fs, void (*attrib)(const GLuint)); /* repeated */
static void tex_map_attrib(const GLuint shader); /* callback for internal */
static int texture(struct AutoImage *image); /* decompresses */
static int light_compute_texture(void); /* creates image */
static void display(void); /* callback for odisplay */
static void resize(int width, int height); /* callback  */

/* so many globals! */
static int     screen_width = 300, screen_height = 200;
static GLuint  vbo_geom, /*spot_geom,*/ light_tex, background_tex, shield_tex;

static GLuint  background_shader;
static GLint   background_scale_location;

static GLuint  hud_shader;
static GLint   hud_size_location, hud_shield_location, hud_position_location, hud_camera_location, hud_two_screen_location;

static GLuint  far_shader;
static GLint   far_size_location, far_angle_location, far_position_location, far_camera_location, /*far_texture_location,*/ far_two_screen_location;
static GLint   far_dirang_location, far_dirclr_location;

static GLuint  light_shader;
static GLint   light_size_location, light_angle_location, light_position_location, light_camera_location, /*light_texture_location,*/ light_two_screen_location;
static GLint   light_lights_location, light_lightpos_location, light_lightclr_location;
static GLint   light_dirang_location, light_dirclr_location;
static GLfloat two_width, two_height;
static float   camera_x, camera_y;

/** Gets all the graphics stuff started.
 @return		All good to draw? */
int Draw(void) {
	int i;

	if(is_started) return -1;

	if(!WindowStarted()) {
		Debug("Draw: window not started.\n");
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
	Debug("Draw: created vertex buffer, Vbo%u.\n", vbo_geom);

	/* textures */
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
	/* lighting */
	glActiveTexture(GT_LIGHT);
	if(!(light_tex = light_compute_texture())) Debug("Draw: failed computing light texture.\n");
	/* textures stored in imgs */
	for(i = 0; i < max_auto_images; i++) texture(&auto_images[i]);

	/* shaders: simple texture for hud elements and stuff */
	if(!(background_shader = link_shader(Background_vs, Background_fs, &tex_map_attrib))) { Draw_(); return 0; }
	glUniform1i(glGetUniformLocation(background_shader, "sampler"),
		T_BACKGROUND); /* it's always the background */
	background_scale_location   = glGetUniformLocation(background_shader,
		"scale");

	/* shaders: simple texture for hud elements and stuff */
	if(!(hud_shader = link_shader(Hud_vs, Hud_fs, &tex_map_attrib))) { Draw_(); return 0; }
	hud_size_location      = glGetUniformLocation(hud_shader, "size");
	hud_shield_location    = glGetUniformLocation(hud_shader, "shield");
	hud_position_location  = glGetUniformLocation(hud_shader, "position");
	hud_camera_location    = glGetUniformLocation(hud_shader, "camera");
	hud_two_screen_location= glGetUniformLocation(hud_shader, "two_screen");
	glUniform1i(glGetUniformLocation(hud_shader, "sampler"), T_SPRITES);

	/* shader: objects that are far; lit, but not dynamically */
	if(!(far_shader = link_shader(Far_vs, Far_fs, &tex_map_attrib))) { Draw_(); return 0; }
	/* vs */
	far_angle_location     = glGetUniformLocation(far_shader, "angle");
	far_size_location      = glGetUniformLocation(far_shader, "size");
	far_position_location  = glGetUniformLocation(far_shader, "position");
	far_camera_location    = glGetUniformLocation(far_shader, "camera");
	far_two_screen_location= glGetUniformLocation(far_shader, "two_screen");
	/* fs */
	/*const->far_texture_location = glGetUniformLocation(far_shader, "sampler");*/
	glUniform1i(glGetUniformLocation(far_shader, "sampler"),       T_SPRITES);
	glUniform1i(glGetUniformLocation(far_shader, "sampler_light"), T_LIGHT);
	far_dirang_location  = glGetUniformLocation(far_shader, "directional_angle");
	far_dirclr_location  = glGetUniformLocation(far_shader, "directional_colour");
	/* this is the one that's doing the work */
	if(!(light_shader = link_shader(Lighting_vs, Lighting_fs, &tex_map_attrib))) { Draw_(); return 0; }
	/* vs */
	light_angle_location     = glGetUniformLocation(light_shader, "angle");
	light_size_location      = glGetUniformLocation(light_shader, "size");
	light_position_location  = glGetUniformLocation(light_shader, "position");
	light_camera_location    = glGetUniformLocation(light_shader, "camera");
	light_two_screen_location= glGetUniformLocation(light_shader, "two_screen");
	/* fs */
	/*light_texture_location = glGetUniformLocation(light_shader, "sampler"); <- if you want it to be variable */
	glUniform1i(glGetUniformLocation(light_shader, "sampler"),       T_SPRITES);
	glUniform1i(glGetUniformLocation(light_shader, "sampler_light"), T_LIGHT);
	light_lights_location  = glGetUniformLocation(light_shader, "lights");
	light_lightpos_location= glGetUniformLocation(light_shader, "light_position");
	light_lightclr_location= glGetUniformLocation(light_shader, "light_colour");
	light_dirang_location  = glGetUniformLocation(light_shader, "directional_angle");
	light_dirclr_location  = glGetUniformLocation(light_shader, "directional_colour");

	/* sunshine; fixme: have it change in different regions */
	{
		/* scientificly inaccurate:
		float sunshine[] = { 1.0f * 3.0f, 0.97f * 3.0f, 0.46f * 3.0f }; */
		float sunshine[] = { 1.0f * 3.0f, 1.0f * 3.0f, 1.0f * 3.0f };
		glUseProgram(far_shader);
		glUniform1f(far_dirang_location, /*0.0f*/-2.0f);
		glUniform3fv(far_dirclr_location, 1, sunshine);
		glUseProgram(light_shader);
		glUniform1f(light_dirang_location, /*0.0f*/-2.0f);
		glUniform3fv(light_dirclr_location, 1, sunshine);
	}

	/* particle spot buffer; oh man, this has to be done every time? */
	/*glGenBuffers(1, (GLuint *)&spot_geom);
	glBindBuffer(GL_ARRAY_BUFFER, spot_geom);
	glBufferData(GL_ARRAY_BUFFER, sizeof(spots), spots, GL_STREAM_DRAW);
	Debug("Draw: created spot particle buffer, Vbo%u.\n", spot_geom); */

	glUseProgram(0);

	WindowIsGlError("Draw");

	is_started = -1;
	return -1;
}

/** Destructor. */
void Draw_(void) {
	unsigned tex;
	int i;

	/*if(glIsBuffer(spot_geom)) {
		glDeleteBuffers(1, &spot_geom);
		Debug("~Draw: Deleted particle buffer.\n");
		spot_geom = 0;
	}*/
	if(light_shader) {
		Debug("~Draw: erase Sdr%u.\n", light_shader);
		glDeleteProgram(light_shader);
		light_shader = 0;
	}
	if(far_shader) {
		Debug("~Draw: erase Sdr%u.\n", far_shader);
		glDeleteProgram(far_shader);
		far_shader = 0;
	}
	if(hud_shader) {
		Debug("~Draw: erase Sdr%u.\n", hud_shader);
		glDeleteProgram(hud_shader);
		hud_shader = 0;
	}
	/* erase the textures */
	/*if(imgs) {
		while(MapIterate(imgs, (const void **)&name, (void **)&img)) {
			if(!img || !(tex = ImageGetTexture(img))) continue;
			Debug("~Draw: erase \"%s,\" Tex%u.\n", name, tex);
			glDeleteTextures(1, (unsigned *)&tex);
			ImageSetTexture(img, 0);
		}
	} <- static now! */
	/* textures stored in imgs */
	for(i = max_auto_images - 1; i; i--) {
		if((tex = auto_images[i].texture)) {
			Debug("~Draw: erase texture, Tex%u.\n", tex);
			glDeleteTextures(1, &tex);
			auto_images[i].texture = 0;
		}
	}
	if(light_tex) {
		Debug("~Draw: erase lighting texture, Tex%u.\n", light_tex);
		glDeleteTextures(1, &light_tex);
		light_tex = 0;
	}
	if(vbo_geom && glIsBuffer(vbo_geom)) {
		Debug("~Draw: erase Vbo%u.\n", vbo_geom);
		glDeleteBuffers(1, &vbo_geom);
		vbo_geom = 0;
	}
	/* get all the errors that we didn't catch */
	WindowIsGlError("~Draw");

	is_started = 0;
}

/** Sets the camera location.
 @param x
 @param y	(x, y) in pixels. */
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
 GT_BACKGROUND textures, which probably don't work, maybe? (oh, they do) */
void DrawSetBackground(const char *const str) {
	struct AutoImage *image;
	
	/* clear the backgruoud; fixme: test, it isn't used at all */
	if(!str) {
		background_tex = 0;
		glActiveTexture(GT_BACKGROUND);
		glBindTexture(GL_TEXTURE_2D, 0);
		Debug("Image desktop cleared.\n");
		return;
	}
	if(!(image = AutoImageSearch(str))) {
		Debug("Draw::setDesktop: image \"%s\" not found.\n", str);
		return;
	}
	/* background_tex is a global; the witdh/height of the image can be found with background_tex */
	background_tex = image->texture;
	glActiveTexture(GT_BACKGROUND);
	glBindTexture(GL_TEXTURE_2D, background_tex);
	Debug("Image \"%s,\" (Tex%u,) set as desktop.\n", image->name, background_tex);
}

void DrawSetShield(const char *const str) {
	struct AutoImage *image;
	if(!(image = AutoImageSearch(str))) {
		Debug("Draw::setShield: image \"%s\" not found.\n", str);
		return;
	}
	shield_tex = image->texture;
	glActiveTexture(GT_SPRITES);
	glBindTexture(GL_TEXTURE_2D, shield_tex);
	Debug("Image \"%s,\" (Tex%u,) set as shield.\n", image->name, shield_tex);
}

/** Compiles, links and verifies a shader.
 @param vert_vs		Vertex shader source.
 @param frag_fs		Fragment shader source.
 @param attrib(const GLuint)
					A function which is passed the uncompiled shader source and
					which is supposed to set attributes. Can be null.
 @return			A shader or 0 if it didn't link. */
static GLuint link_shader(const char *vert_vs, const char *frag_fs, void (*attrib)(const GLuint)) {
	GLchar info[128];
	GLuint vs = 0, fs = 0, shader = 0;
	GLint status;
	enum { S_NOERR = 0, S_VERT, S_FRAG, S_LINK, S_VALIDATE } error = S_NOERR;

	/* try */
	do {
		/* compile vs; shader, count, **string, *length */
		vs = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vs, 1, &vert_vs, 0);
		glCompileShader(vs);
		glGetShaderiv(vs, GL_COMPILE_STATUS, &status);
		if(!status) { error = S_VERT; break; }
		Debug("Draw::link_shader: compiled vertex shader, Sdr%u.\n", vs);

		/* compile fs */
		fs = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fs, 1, &frag_fs, 0);
		glCompileShader(fs);
		glGetShaderiv(fs, GL_COMPILE_STATUS, &status);
		if(!status) { error = S_FRAG; break; }
		Debug("Draw::link_shader: compiled fragment shader, Sdr%u.\n", fs);

		/* link */
		shader = glCreateProgram();
		glAttachShader(shader, vs);
		glAttachShader(shader, fs);
		/* callback */
		if(attrib) attrib(shader);
		glLinkProgram(shader);
		glGetProgramiv(shader, GL_LINK_STATUS, &status);
		if(!status) { error = S_LINK; break; }
		Debug("Draw::link_shader: linked shader programme, Sdr%u.\n", shader);

		/* validate */
		glValidateProgram(shader);
		glGetProgramiv(shader, GL_VALIDATE_STATUS, &status);
		if(!status) { error = S_VALIDATE; break; }
		Debug("Draw::link_shader: validated shader programme, Sdr%u.\n", shader);

	} while(0);

	/* catch */
	switch(error) {
		case S_NOERR:
			break;
		case S_VERT:
			glGetShaderInfoLog(vs, (GLsizei)sizeof(info), 0, info);
			Debug("Draw::link_shader: Sdr%u failed vertex shader compilation; OpenGL: %s", vs, info);
			break;
		case S_FRAG:
			glGetShaderInfoLog(fs, (GLsizei)sizeof(info), 0, info);
			Debug("Draw::link_shader: Sdr%u failed fragment shader compilation; OpenGL: %s", fs, info);
			break;
		case S_LINK:
			glGetProgramInfoLog(shader, (GLsizei)sizeof(info), 0, info);
			Debug("Draw::link_shader: Sdr%u failed shader linking; OpenGL: %s", shader, info);
			break;
		case S_VALIDATE: /* fixme: untested! */
			glGetProgramInfoLog(shader, (GLsizei)sizeof(info), 0, info);
			Debug("Draw::link_shader: Sdr%u failed shader validation; OpenGL: %s", shader, info);
			break;
	}

	/* don't need these anymore, whatever the result */
	if(fs) {
		glDeleteShader(fs);
		if(shader) glDetachShader(shader, fs);
		Debug("Draw::link_shader: erasing fragment shader, Sdr%u.\n", fs);
	}
	if(vs) {
		glDeleteShader(vs);
		if(shader) glDetachShader(shader, vs);
		Debug("Draw::link_shader: erasing vertex shader, Sdr%u.\n", vs);
	}

	/* resume catching */
	if(error) {
		if(shader) {
			glDeleteProgram(shader);
			Debug("Draw::link_shader: deleted shader programme, Sdr%u.\n", shader);
		}
		return 0;
	}

	/* immediately use the shader; we probably want to set some uniforms */
	glUseProgram(shader);

	/* catch any errors */
	WindowIsGlError("shader");

	return shader;
}

/** Setup tex_map attributes callback.
 @param shader	The unlinked shader. */
static void tex_map_attrib(const GLuint shader) {
	/* programme, index attrib, name */
	glBindAttribLocation(shader, G_POSITION, "vbo_position");
	glBindAttribLocation(shader, G_TEXTURE,  "vbo_texture");
}

/** Creates a texture from an image; sets the image texture unit.
 @param image	The Image as seen in Lores.h.
 @return		Success. */
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
				Debug("Draw::texture: lodepng error %u: %s\n", error,
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
			Debug("Draw::texture: Unknown image format.\n");
	}
	if(!is_alloc) {
		Debug("Draw::texture: allocation failed.\n");
		return 0;
	}
	if(width != image->width || height != image->height || depth != image->depth) {
		Debug("Draw::texture: dimension mismatch %u:%ux%u vs %u:%ux%u.\n", image->width, image->height, image->depth, width, height, depth);
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
			Debug("Draw::texture: not a recognised depth, %d.\n", depth);
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
		glActiveTexture((unsigned)(image->depth == 3 ? GT_BACKGROUND : GT_SPRITES));
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
		Pedantic("Tex: %u.\n", image->texture);
		if(image->width <= 80) image_print(image, pic);
		else Pedantic("...too big to show.\n");
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
		/*case IF_BMP:
			Bitmap_(&bmp);*/
		default:
			break;
	}
	Debug("Draw::texture: created %dx%dx%d texture, Tex%u.\n", width, height, depth, tex);

	WindowIsGlError("texture");

	return tex ? -1 : 0;
}

/* Creates a hardcoded lighting texture with the Red the radius and Green the
 angle.
 @return	The texture or zero. */
static int light_compute_texture(void) {
	int   i, j;
	float x, y;
	float *buffer; /*[512][512][2]; MSVC does not have space on the stack*/
	/*const int buffer_ysize = sizeof(buffer)  / sizeof(*buffer);
	const int buffer_xsize = sizeof(*buffer) / sizeof(**buffer);*/
	const int buffer_size = 1024/*512*/; /* width/height */
	const float buffer_norm = (float)M_SQRT1_2 * 4.0f / sqrtf(2.0f * buffer_size * buffer_size);
	unsigned name;

	if(!(buffer = malloc(sizeof(float) * buffer_size * buffer_size << 1))) return 0;
	for(j = 0; j < buffer_size; j++) {
		for(i = 0; i < buffer_size; i++) {
			x = (float)i - 0.5f*buffer_size + 0.5f;
			y = (float)j - 0.5f*buffer_size + 0.5f;
			buffer[((j*buffer_size + i) << 1) + 0] = fminf(sqrtf(x*x + y*y) * buffer_norm, 1.0f);
			/* NOTE: opengl clips [0, 1), even if it says different;
			 maybe it's GL_LINEAR? */
			buffer[((j*buffer_size + i) << 1) + 1] = fmodf(atan2f(y, x) + (float)M_2PI, (float)M_2PI) * (float)M_1_2PI;
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
	Debug("Draw::texture: created %dx%dx%d hardcoded lighting texture, Tex%u.\n", buffer_size, buffer_size, 2, name);

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
		glUseProgram(background_shader);

		/* turn transperency off */
		glDisable(GL_BLEND);

		/* why?? */
		glActiveTexture(GT_BACKGROUND);

		/* fixme: of course it's a background, set once */
		/*glUniform1i(background_sampler_location, T_BACKGROUND);*/
		/*glUniformMatrix4fv(tex_map_matrix_location, 1, GL_FALSE, background_matrix);*/
		glDrawArrays(GL_TRIANGLE_STRIP, vbo_bg_first, vbo_bg_count);
	}
	glEnable(GL_BLEND);

	/* use simple tex_map_shader */
	/*glUseProgram(tex_map_shader);*/
	/* why? the glsl entirely specifies this */
	glActiveTexture(GT_SPRITES);

	/* turn on background lighting (sprites) */
	glUseProgram(far_shader);
	/*glUseProgram(light_shader);
	glUniform1i(light_lights_location, 0);*/

	/* background sprites */
	/*const->glUniform1i(far_texture_location, T_SPRITES); */
	glUniform2f(far_camera_location, camera_x, camera_y);
	while(FarIterate(&x, &y, &t, &tex, &size)) {
		if(old_texture != tex) {
			glBindTexture(GL_TEXTURE_2D, tex);
			old_texture = tex;
		}
		glUniform1f(far_size_location, (float)size);
		glUniform1f(far_angle_location, t);
		glUniform2f(far_position_location, x, y);
		glDrawArrays(GL_TRIANGLE_STRIP, vbo_sprite_first, vbo_sprite_count);
	}
	old_texture = 0;

	/* turn on lighting */
	glUseProgram(light_shader);
	glUniform1i(light_lights_location, lights = LightGetArraySize());
	if(lights) {
		glUniform2fv(light_lightpos_location, lights, (GLfloat *)LightGetPositionArray());
		glUniform3fv(light_lightclr_location, lights, (GLfloat *)LightGetColourArray());
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
	/*glUniform1i(light_texture_location, T_SPRITES); <- constant, now */
	glUniform2f(light_camera_location, camera_x, camera_y);
	if(draw_is_print_sprites) Debug("%s:\n", "Draw");
	while(SpriteIterate(&x, &y, &t, &tex, &size)) {
		if(draw_is_print_sprites) Debug("Tex %d Size %d (%.1f,%.1f:%.1f)\n", tex, size, x, y, t);
		/* draw a sprite; fixme: minimise texture transitions */
		if(old_texture != tex) {
			glBindTexture(GL_TEXTURE_2D, tex);
			old_texture = tex;
		}
		glUniform1f(light_size_location, (float)size);
		glUniform1f(light_angle_location, t);
		glUniform2f(light_position_location, x, y);
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
		glUseProgram(hud_shader);
		glBindTexture(GL_TEXTURE_2D, shield_tex);
		glUniform2f(hud_camera_location, camera_x, camera_y);
		glUniform2f(hud_size_location, 256.0f, 8.0f);
		glUniform2f(hud_position_location, x, y - t * 2.0f);
		glUniform2i(hud_shield_location, SpriteGetHit(player), SpriteGetMaxHit(player));
		glDrawArrays(GL_TRIANGLE_STRIP, vbo_sprite_first, vbo_sprite_count);
	}

	/* disable, swap */
	glUseProgram(0);
	glutSwapBuffers();

	WindowIsGlError("Draw::display");

}

/** Callback for glutReshapeFunc.
 <p>
 fixme: not gauranteed to have a background! this will crash.
 @param width
 @param height	The size of the client area. */
static void resize(int width, int height) {
	int w_tex, h_tex;
	float w_w_tex, h_h_tex;

	Debug("Draw::resize: %dx%d.\n", width, height);
	if(width <= 0 || height <= 0) return;
	glViewport(0, 0, width, height);
	screen_width  = width; /* global shared with drawing */
	screen_height = height;

	/* update the inverse screen on the card */
	two_width  = 2.0f / width;
	two_height = 2.0f / height;

	/* resize the background */
	/* glActiveTexture(T_BACKGROUND); this does nothing? */
	glBindTexture(GL_TEXTURE_2D, background_tex);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH,  &w_tex);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h_tex);
	/*Debug("w %d h %d\n", w_tex, h_tex);*/
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
	glUseProgram(background_shader);
	glBindTexture(GL_TEXTURE_2D, background_tex);
	if(w_w_tex > 1.0f || h_h_tex > 1.0f) {
		const float scale = 1.0f / ((w_w_tex > h_h_tex) ? w_w_tex : h_h_tex);
		glUniform1f(background_scale_location, scale);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	} else {
		glUniform1f(background_scale_location, 1.0f);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	/* far shader and light shader also need updates of 2/[width|height];
	 meh, we can probably do them in sw? */
	glUseProgram(hud_shader);
	glUniform2f(hud_two_screen_location, two_width, two_height);
	glUseProgram(far_shader);
	glUniform2f(far_two_screen_location, two_width, two_height);
	glUseProgram(light_shader);
	glUniform2f(light_two_screen_location, two_width, two_height);
}
