/* Copyright 2000, 2014 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt

 OpenGL module; includes graphics and input.

 @author Neil
 @version 2
 @since 2013 */

#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* fprintf */
#include <time.h>   /* srand(clock()) */
#include <math.h>   /* for shader() */
#include <stdarg.h> /* for Console */
#include "Math.h"
#include "OpenGlew.h" /* see OpenGlew.h for, eg, Windows, instructions */
#include "Game.h"
#include "Resources.h"
#include "Sprite.h"
#include "Light.h"
#include "Open.h"

/* hard coded resouce files; there should be the directory tools/ where you can
 compile utilities that can make these files; run "make" and this should be
 automated */
#include "../bin/Lighting_vs.h"
#include "../bin/Lighting_fs.h"

/* constants */
static const char *programme  = "Void";
static const char *year       = "2000, 2014";
static const int versionMajor = 3;
static const int versionMinor = 2;

static const struct Vec2i screen_init = { 640, 480 };
static const int          time_step   = 20; /* ms */
/* [0 (low = flat), infity (high = conical)] when shading;
 it's sort of like -- . . . /-\ . . . /\ */
static const float conical_shadows    = 2.5f;

static const int key_delay            = 300; /* ms */

/* Key subclass */

struct Key {
	int state;
	int down;
	int integral;
	int time;
};

/* Timer subclass */

#define TIMER_LOG_FRAMES 2

struct Timer {
	int time; /* ms */
	int frame;
	int frameTime[1 << TIMER_LOG_FRAMES];
};

const static int max_frame_time = sizeof(((struct Timer *)0)->frameTime) / sizeof(int);

/* Screen subclass */

struct Screen {
	struct Vec2i dim;
	struct Vec2i saved;
	struct Vec2i savedPos;
	struct Vec2f max;
	int          full;
};

/* Console subclass */

struct Console {
	char buffer[1024];
};

const static int console_buffer_size = sizeof(((struct Console *)0)->buffer) / sizeof(char);

/* class */

struct Open {
	struct Timer   graphics;
	GLenum         lastGlError;
	struct Game    *game;
	struct Key     key[k_max];
	struct Screen  screen;
	GLuint         shader;
	GLuint         lighting, background;
	struct Vec2f   inverseBackgroundSize;
	GLuint         vboSquare[3];
	GLuint         vboParticles;
	GLuint         vboParticleColour;
	struct Console console;
};

const static int console_size = sizeof(((struct Open *)0)->console) / sizeof(char);

/* global! */
struct Open *open = 0;

/* private */
static int isGLError(const char *title);
static void usage(const char *argvz);
static void draw(void);
static void update(int);
static void display(void);
static void background(const char *bg);
static void shader(void);
static void shader_(void);

static void Key(struct Key *key);
static enum Keys glut_to_keys(int k);
static void key_down(unsigned char k, int x, int y);
static void key_up(unsigned char k, int x, int y);
static void key_down_special(int k, int x, int y);
static void key_up_special(int k, int x, int y);

static void Timer(struct Timer *timer);
static void advance_time(struct Timer *timer);

static void Screen(struct Screen *screen, const int width, const int height);
static void resize(int width, int height);

static void Console(struct Console *console);
static void rasterise_text(void);

/** private (entry point) */
int main(int argc, char **argv) {
	usage(argv[0]);
	srand((unsigned)clock());
	/* negotiate with dynamic library, calculate some address */
	glutInit(&argc, argv);
	/* constructor */
	if(!Open(screen_init.x, screen_init.y, programme)) return EXIT_FAILURE;
	/* loop never returns */
	/* #ifndef GLUT_DISABLE_ATEXIT_HACK */
	if(atexit(&Open_)) perror("~Open");
	glutMainLoop();
	return EXIT_SUCCESS;
}

/** constructor */
struct Open *Open(const int width, const int height, const char *title) {
#if 0
	const float square_geometry[] = {
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f
	};
	const float square_texture[] = {
		-1.0f, -1.0f,
		1.0f,  -1.0f,
		1.0f,  1.0f,
		-1.0f, 1.0f
	};
	const char square_indices[] = {
		0, 1, 2, 3
	};
#endif
	int i;

	if(open) return open;
	if(width <= 0 || height <= 0 || !title) {
		fprintf(stderr, "Open: error initialising.\n");
		return 0;
	}
	if(!(open = malloc(sizeof(struct Open)))) {
		perror("Open constructor");
		Open_();
		return 0;
	}
	Timer(&open->graphics);
	open->lastGlError = GL_NO_ERROR;
	open->game        = 0;
	Screen(&open->screen, width, height);
	for(i = 0; i < k_max; i++) Key(&open->key[i]);
	open->shader     = 0;
	open->lighting   = 0;
	open->background = 0;
	open->inverseBackgroundSize.x = open->inverseBackgroundSize.y = 0;
	open->vboSquare[0] = open->vboSquare[1] = open->vboSquare[2] = 0;
	open->vboParticles = open->vboParticleColour = 0;
	Console(&open->console);
	fprintf(stderr, "Open: new, #%p.\n", (void *)open);

	/* create the window */
	glutInitDisplayMode(GLUT_DOUBLE);
	glutInitWindowSize(width, height);
	glutCreateWindow(title);
	fprintf(stderr, "Open: opened GLUT window; vendor %s; version %s; renderer %s; shading language version %s; combined texture image units %d.\n",
		   glGetString(GL_VENDOR), glGetString(GL_VERSION),
		   glGetString(GL_RENDERER), glGetString(GL_SHADING_LANGUAGE_VERSION),
		   GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS /* > 80 */);

	/* set callbacks */
	glutDisplayFunc(&display);
	glutReshapeFunc(&resize);
	glutKeyboardFunc(&key_down);
	glutKeyboardUpFunc(&key_up);
	glutSpecialFunc(&key_down_special);
	glutSpecialUpFunc(&key_up_special);
	/* glutMouseFunc(&mouse); */
	/* glutIdleFunc(0); disable */
	glutTimerFunc(time_step, &update, 0);

	/* enable textures */
	glEnable(GL_TEXTURE_2D);
	/* we're using glsl 1.10, so noperspecive is missing; not really that much
	 of an issue, but perspective-correct polygons when you don't have
	 perspective is just a waste */
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

	/* enable transparency;
	 http://www.opengl.org/sdk/docs/man2/xhtml/glBlendFunc.xml */
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	/* big points */
	glPointSize(5);
	glEnable(GL_POINT_SMOOTH);

	/* call other functions that make this too long */
	if(!OpenGlew()) {
		fprintf(stderr, "Open: can't use shaders! will start up in OpenGL 1.0 mode.\n");
	} else {
		shader();
	}

	/* set up VBO for particles with the Light.c */
	fprintf(stderr, "Open: enabling particle vertex buffer array object thing.\n");
	glEnableClientState(GL_VERTEX_ARRAY); /* OGL 1.1 */
#ifdef KEEP_ON_SW /* maybe, I don't really know */
	glVertexPointer(2, GL_FLOAT, 0, LightGetPositions());
#else
	glGenBuffers(1, &open->vboParticles);
	glBindBuffer(GL_ARRAY_BUFFER, open->vboParticles);
	glVertexPointer(2, GL_FLOAT, 0, 0); /* OGL 1.1 */
	glBufferData(GL_ARRAY_BUFFER, LightGetMax() * sizeof(struct Vec2f), LightGetPositions(), GL_STREAM_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
#endif
	/* colours */
	glEnableClientState(GL_COLOR_ARRAY); /* OGL 1.1 */
#ifndef KEEP_ON_SW /* no, colour values are . . . different? man, I hate hw */
	glColorPointer(3, GL_FLOAT, 0, LightGetColours());
#else
	glGenBuffers(1, &open->vboParticleColour);
	glBindBuffer(GL_ARRAY_BUFFER, open->vboParticleColour);
	glVertexPointer(3, GL_FLOAT, 0, 0); /* OGL 1.1 */
	glBufferData(GL_ARRAY_BUFFER, LightGetMax() * sizeof(struct Colour3f), LightGetColours(), GL_STREAM_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
#endif

#if 0
	/* fuck I just want four points to go on the graphics card, what a mess
	 like, it draws nothing and glutText gives errors; wtf I hate vaos; this is what lack
	 of forward planning gets you :[ */
	/* a square -- ogl3 */
	/*glGenVertexArrays(1, &open->vaoSquare);
	glBindVertexArray(open->vaoSquare);*/
	glGenBuffers(3, open->vboSquare);
	/* geometry */
	glBindBuffer(GL_ARRAY_BUFFER, open->vboSquare[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(square_geometry), square_geometry, GL_STATIC_DRAW);
	glVertexAttribPointer(0 /*index*/, 2 /*size*/, GL_FLOAT, GL_FALSE /*norm*/, 0 /*stride*/, 0 /*pointer*/);
	glEnableVertexAttribArray(0); /* <~ shader */
	/* texture */
	glBindBuffer(GL_ARRAY_BUFFER, open->vboSquare[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(square_texture), square_texture, GL_STATIC_DRAW);
	glVertexAttribPointer(1 /*index*/, 2 /*size*/, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(1);
	/* order */
	/*glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, open->vboSquare[3]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(square_indices), square_indices, GL_STATIC_DRAW);*/
	/* unbind */
	/*glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);*/

	/* fixme: this fucks up whenever I touch it */
	/*glGenBuffers(1, &open->vboSquare);
	glBindBuffer(GL_ARRAY_BUFFER, open->vboSquare);
	glVertexPointer(2, GL_FLOAT, 0, 0); *//* OGL 1.1 */
	/*glBufferData(GL_ARRAY_BUFFER, sizeof(square_data), square_data, GL_STATIC_DRAW);*/
	/*glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 2 * 4, &square_data, GL_STATIC_DRAW);*/
	/*glBindBuffer(GL_ARRAY_BUFFER, 0);*/
#endif

	/* get the game set up; must be called after creating an OpenGL context,
	 ie glutCreateWindow */
	if(!(open->game = Game())) {
		Open_();
		return 0;
	}
	/* after game */
	background("Ngc4038_4039_bmp");

	/* get the initialisation error code */
	/*fprintf(stderr, "Open: OpenGL initilisation status, \"%s.\"\n", gluErrorString(glGetError()));*/
	isGLError("Open");

	return open;
}

/** destructor */
void Open_(void) {
	if(!open) return;
	Game_(&open->game);
	glDisableClientState(GL_VERTEX_ARRAY/* | GL_COLOR_ARRAY*/);
	if(glIsBuffer(open->vboParticles)) {
		glDeleteBuffers(1, &open->vboParticles);
		fprintf(stderr, "~Open: Deleted particle buffer.\n");
	}
	/*if(glIsTexture(open->background)) {
		glDeleteTextures(1, &open->background);
		fprintf(stderr, "~Open: Deleted background texture.\n");
	}*/
	shader_();

	/*fprintf(stderr, "~Open: OpenGL final status \"%s;\" deleting #%p.\n", gluErrorString(glGetError()), (void *)open);*/
	fprintf(stderr, "~Open: deleting OpenGL #%p.\n", (void *)open);
	free(open);
	/* fixme: are you supposed to free textures? (YES!) */
	open = 0;

	isGLError("Open~");
}

void OpenDebug(void) {
	int i;
	int l_no;
	const struct Vec2f *l_pos;

	if(!open) return;
	l_no  = LightGetNo();
	l_pos = LightGetPositions();
	fprintf(stderr, "Lights {\n");
	for(i = 0; i < l_no; i++) {
		fprintf(stderr, "\t(%f, %f)\n", l_pos[i].x, l_pos[i].y);
	}
	fprintf(stderr, "}\n");
}

void OpenPrintMatrix(void) {
	float p[16], m[16];
	glGetFloatv(GL_PROJECTION_MATRIX, p);
	fprintf(stderr, "GL_PROJECTION\n");
	fprintf(stderr, "[ %8f %8f %8f %8f ]\n", p[0], p[4], p[ 8], p[12]);
	fprintf(stderr, "[ %8f %8f %8f %8f ]\n", p[1], p[5], p[ 9], p[13]);
	fprintf(stderr, "[ %8f %8f %8f %8f ]\n", p[2], p[6], p[10], p[14]);
	fprintf(stderr, "[ %8f %8f %8f %8f ]\n", p[3], p[7], p[11], p[15]);
	glGetFloatv(GL_MODELVIEW_MATRIX, m);
	fprintf(stderr, "GL_MODELVIEW\n");
	fprintf(stderr, "[ %2.2f %2.2f %2.2f %2.2f ]\n", m[0], m[4], m[ 8], m[12]);
	fprintf(stderr, "[ %2.2f %2.2f %2.2f %2.2f ]\n", m[1], m[5], m[ 9], m[13]);
	fprintf(stderr, "[ %2.2f %2.2f %2.2f %2.2f ]\n", m[2], m[6], m[10], m[14]);
	fprintf(stderr, "[ %2.2f %2.2f %2.2f %2.2f ]\n", m[3], m[7], m[11], m[15]);
}

/** "static" toggle fullscreen */
void OpenToggleFullScreen(void) {
	/* TODO: glutGameModeString, glutEnterGameMode looks interesting */
	if(!open) return;
	if(!open->screen.full) {
		fprintf(stderr, "Entering fullscreen.\n");
		open->screen.full = -1;
		open->screen.saved.x = open->screen.dim.x;
		open->screen.saved.y = open->screen.dim.y;
		open->screen.savedPos.x = glutGet(GLUT_WINDOW_X);
		open->screen.savedPos.y = glutGet(GLUT_WINDOW_Y);
		glutFullScreen();
	} else {
		fprintf(stderr, "Exiting fullscreen.\n");
		open->screen.full = 0;
		glutReshapeWindow(open->screen.saved.x, open->screen.saved.y);
		glutPositionWindow(open->screen.savedPos.x, open->screen.savedPos.y);
	}

	isGLError("Open::ToggleFullScreen");
}

/** bmp must be width * height * depth;
 returns a handle to pass to OpenTexture_() */
int OpenTexture(const int width, const int height, const int depth, const unsigned char *bmp) {
	int format = 0, internal = 0;
	int name;

	if(!open || width <= 0 || height <= 0 || !bmp) return 0;
	if(depth == 1) {
		/* we use exclusively for shaders, so I don't think this matters */
		internal = GL_RGB;
		/* format   = GL_LUMINANCE;
		 core context depriciated -- doesn't work on certain versions of OGL
		 maybe this fixes it? I don't know; older Windows versions don't like
		 this or something */
		/*format   = GL_R16;
		format   = GL_R8;*/
		format   = GL_RED;
	} else if(depth == 3) {
		internal = GL_RGB8;
		format   = GL_RGB;
	} else if(depth == 4) {
		internal = GL_RGBA8;
		format   = GL_RGBA;
	} else {
		fprintf(stderr, "Open::Texture: not a reconised depth, %d.\n", depth);
		return 0;
	}
	glGenTextures(1, (unsigned *)&name);
	glBindTexture(GL_TEXTURE_2D, name);
	/* FIXME: OpenGL 2.0? \/ */
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	/* void glTexImage2D(target, level, internalFormat, width, height,
	                     border, format, type, *data); */
	glTexImage2D(GL_TEXTURE_2D, 0, internal, width, height,
				 0, format, GL_UNSIGNED_BYTE, bmp);
	glBindTexture(GL_TEXTURE_2D, 0);
	fprintf(stderr, "Open::Texture: texture %dx%dx%d assigned %u.\n", width, height, depth, name);

	isGLError("Open::Texture");

	return name;
}

void OpenTexture_(int name) {
	if(!name) return;
	fprintf(stderr, "Open::Texture: deleting texture %u.\n", name);
	glDeleteTextures(1, (unsigned int *)&name);

	isGLError("Open::Texture~");
}

/* private */

static void usage(const char *argvz) {
	fprintf(stderr, "Usage: %s\n", argvz);
	fprintf(stderr, "To win, blow up everything that's not you.\n");
	fprintf(stderr, "Player one controls: left, right, up, down, space\n");
	/* fprintf(stderr, "Player two controls: a, d, w, s, space.\n"); */
	fprintf(stderr, "Fullscreen: F1.\n");
	fprintf(stderr, "Exit: Escape.\n\n");
	fprintf(stderr, "Version %d.%d.\n\n", versionMajor, versionMinor);
	fprintf(stderr, "%s Copyright %s Neil Edelman\n", programme, year);
	fprintf(stderr, "This program comes with ABSOLUTELY NO WARRANTY.\n");
	fprintf(stderr, "This is free software, and you are welcome to redistribute it\n");
	fprintf(stderr, "under certain conditions; see copying.txt.\n\n");
}

static int isGLError(const char *title) {
	GLenum err;
	if((err = glGetError()) != GL_NO_ERROR) {
		fprintf(stderr, "%s: OpenGL error: %s.\n", title, gluErrorString(err));
	}
	return (err != GL_NO_ERROR);
}

/* FIXME: vertex array for a square */
/*const static char square_indices[] = { 1, 2, 3, 4 };
const static float square_verts[] = { -8,-8, -8,8, 8,8, 8,-8 };*/

static void draw(void) {
	const int l_no = LightGetNo();
	float ang;
	struct Vec2f pos, cam, half;
	struct Sprite *e;

	/* reset the modelview */
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	/* draw background */
	glMatrixMode(GL_TEXTURE);
	glPushMatrix();
	glTranslatef(0.5, 0.5, 0);
	glScalef(open->screen.max.x * open->inverseBackgroundSize.x * 2.0f, open->screen.max.y * open->inverseBackgroundSize.y * 2.0f, 0);
	glTranslatef(-0.5, -0.5, 0);
	glBindTexture(GL_TEXTURE_2D, open->background);
	glBegin(GL_QUADS); /* fixme */
	glTexCoord2f(0.0f, 0.0f); glVertex2f(-(float)open->screen.max.x * 2.0f, -(float)open->screen.max.y * 2.0f);
	glTexCoord2f(1.0f, 0.0f); glVertex2f((float)open->screen.max.x * 2.0f, -(float)open->screen.max.y * 2.0f);
	glTexCoord2f(1.0f, 1.0f); glVertex2f((float)open->screen.max.x * 2.0f, (float)open->screen.max.y * 2.0f);
	glTexCoord2f(0.0f, 1.0f); glVertex2f(-(float)open->screen.max.x * 2.0f, (float)open->screen.max.y * 2.0f);
	glEnd();
	glPopMatrix();

	/* load gpu with cool things */
	if(open->shader) {
		glUseProgram(open->shader);
		glUniform1i(glGetUniformLocation(open->shader, "l_no"), l_no);
		glUniform3fv(glGetUniformLocation(open->shader, "l_lux"), l_no, (float *)LightGetColours());
		glUniform2fv(glGetUniformLocation(open->shader, "l_pos"), l_no, (float *)LightGetPositions());
	}

	/* follow the main player around */
	cam = GameGetCamera(open->game);
	glMatrixMode(GL_MODELVIEW);
	glTranslatef(cam.x, cam.y, 0);

	/* draw entities */
	while((e = GameEnumSprites(open->game))) {
		pos =  SpriteGetPosition(e);
		ang =  SpriteGetDegrees(e);
		half = SpriteGetHalf(e);

		/* shader differentiates b/w unrotated/rotated by the texture matrix */
		if(open->shader) {
			glMatrixMode(GL_TEXTURE);
			glPushMatrix();
			/* fixme: scale(2^0.5) for mismatached rotated/un-rotated! */
			glTranslatef(0.5, 0.5, 0);
			glRotatef(ang, 0, 0, 1);
			glTranslatef(-0.5, -0.5, 0);
			/* we have no way of getting the world space co-\:ordinate so we
			 help by giving it a hint; this allows the camera to move */
			glUniform2f(glGetUniformLocation(open->shader, "sprite_position"), pos.x, pos.y);
		}

		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glTranslatef(pos.x, pos.y, 0);
		glRotatef(ang, 0, 0, 1); /* fixme: do my own! */
		glScalef(half.x, half.y, 1.0f);
		glBindTexture(GL_TEXTURE_2D, SpriteGetTexture(e));
		/*glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);*/
		/*glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, open->vboSquare[3]);
		glDrawElements(GL_QUADS, 4, GL_UNSIGNED_BYTE, 0);*/
		/*glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);*/
		/* fixme: vertex array | vbo */
		glBegin(GL_QUADS); /* fixme! fuck */
		glTexCoord2f(0.0, 0.0); glVertex2f(-1, -1);
		glTexCoord2f(1.0, 0.0); glVertex2f(1, -1);
		glTexCoord2f(1.0, 1.0); glVertex2f(1, 1);
		glTexCoord2f(0.0, 1.0); glVertex2f(-1, 1);
		glEnd();
		glBindTexture(GL_TEXTURE_2D, 0);
		glPopMatrix();

		if(open->shader) {
			glMatrixMode(GL_TEXTURE);
			glPopMatrix();
		}
	}
	if(open->shader) glUseProgram(0);

	/* draw particles */
	glBindBuffer(GL_ARRAY_BUFFER, open->vboParticles);
	/* faster, but no such fn: glMapBufferRange(GL_ARRAY_BUFFER, 0, ParticleGetNo(), GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);*/
#ifndef KEEP_ON_SW
	glBufferSubData(GL_ARRAY_BUFFER, 0, LightGetNo() * sizeof(struct Vec2f), LightGetPositions());
	/*glBufferSubData(GL_ARRAY_BUFFER, 0, LightGetNo() * sizeof(struct Colour3f), LightGetColours());*/
#endif
	glDrawArrays(GL_POINTS, 0, LightGetNo());
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	/********* fixed. FIXME! vertex() crashes Windows ***********/
	/* glPointSize(3); glBegin(GL_POINTS); LightDraw(); glEnd(); glColor4f(1, 1, 1, 1); */

	rasterise_text();

}

/** callback for glutTimerFunc */
static void update(int value) {
	glutTimerFunc(time_step, update, 0);
	/* todo: have graphics and logic timers instead of trusting time_step */
	GameUpdate(open->game, time_step);
	glutPostRedisplay();
}

/** callback for glutDisplayFunc */
static void display(void) {
	/* GLenum err; */

	/* has a background -> glClear(GL_COLOR_BUFFER_BIT);*/
	advance_time(&open->graphics);
	/* draw */
	draw();
	/* swap */
	glutSwapBuffers();
	/* check for errors */
	isGLError("Open::display");
	/*if((err = glGetError()) != GL_NO_ERROR && err != open->lastGlError) {
		fprintf(stderr, "Open::display: OpenGL error \"%s.\"\n", gluErrorString(err));
		open->lastGlError = err;
	}*/
}

/** this is the background, called only in Open constructor
 fixme: OpenGL < 2.0 requires square textures; don't know what happens! */
static void background(const char *bg) {
	struct Bitmap *bmp;

	/* max texture >= 1024 */
	/*glGenTextures(1, (unsigned *)&name);*/
	if(!(bmp = ResourcesFindBitmap(GameGetResources(open->game), "Ngc4038_4039_bmp"))) {
		printf("Open: failed to set background to \"%s;\" no such sprite?\n", bg);
		return;
	}
	glBindTexture(GL_TEXTURE_2D, bmp->id);
	/* fixme: what happens if we have less than opengl 2.0 with the params?
	 I believe what should happen is it ignores it and it's still GL_REPEAT */
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	/* void glTexImage2D(target, level, internalFormat, width, height,
	 border, format, type, *data); */
	/*glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, Ngc4038_4039_bmp_width, Ngc4038_4039_bmp_height,
				 0, GL_RGB, GL_UNSIGNED_BYTE, Ngc4038_4039_bmp_bmp);*/
	open->background = bmp->id;
	open->inverseBackgroundSize.x = 2.0f / bmp->width;
	open->inverseBackgroundSize.y = 2.0f / bmp->height;
	fprintf(stderr, "Activated background texture %d, %d x %d.\n", bmp->id, bmp->width, bmp->height);

	isGLError("Open::background");
}

/** creates the shadows by compiling shader programme,
 called only in Open constructor */
static void shader(void) {
	GLuint vert, frag;
	GLint status;
	GLchar info[128];

	/* texture that have to be there for the shader to work */
	glActiveTexture(GL_TEXTURE1);
	{
		int i, j;
		float x, y;
		/* variable [ysize][xsize] of the lighting sprite, must be 2 'colors' */
		float buffer[64][64][2];
		const int buffer_ysize = sizeof(buffer)  / sizeof(*buffer);
		const int buffer_xsize = sizeof(*buffer) / sizeof(**buffer);
		const float buffer_norm = conical_shadows * 2.0f / sqrtf(buffer_xsize * buffer_xsize + buffer_ysize * buffer_ysize);
		int name;

		for(j = 0; j < buffer_ysize; j++) {
			for(i = 0; i < buffer_xsize; i++) {
				x = (float)i - 0.5f*buffer_xsize + 0.5f;
				y = (float)j - 0.5f*buffer_ysize + 0.5f;
				buffer[j][i][0] = fminf(sqrtf(x*x + y*y) * buffer_norm, 1.0f);
				/* NOTE: opengl clips [0, 1), even if it says different;
				 maybe it's GL_LINEAR? */
				buffer[j][i][1] = fmodf(atan2f(y, x) + M_2PI, M_2PI) * (float)M_1_2PI;
			}
		}
		glGenTextures(1, (unsigned *)&name);
		glBindTexture(GL_TEXTURE_2D, name);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		/* NOTE: GL_LINEAR is a lot prettier, but causes a crease when the
		 principal value goes from [0, 1) ([0, 2Pi)) */
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		/* void glTexImage2D(target, level, internalFormat, width, height,
		 border, format, type, *data); */
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, buffer_xsize, buffer_ysize,
					 0, GL_RG, GL_FLOAT, buffer);
		open->lighting = name;
		fprintf(stderr, "Activated lighting texture.\n");
	}
	glActiveTexture(GL_TEXTURE0);
	/* create shaders; the 0 at the end is null-terminated */
	vert = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vert, 1, &Lighting_vs, 0);
	frag = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(frag, 1, &Lighting_fs, 0);
	/* compile vertex shader */
	glCompileShader(vert);
	glGetShaderiv(vert, GL_COMPILE_STATUS, &status);
	if(!status) {
		glGetShaderInfoLog(vert, sizeof(info), 0, info);
		fprintf(stderr, "Failed compiling vertex shader:\n");
		if(*info) fprintf(stderr, "%s\n", info);
		glDeleteShader(vert);
		glDeleteShader(frag);
		shader_();
		return;
	} else {
		fprintf(stderr, "Successfully compiled vertex shader.\n");
	}
	/* compile fragment shader */
	glCompileShader(frag);
	glGetShaderiv(frag, GL_COMPILE_STATUS, &status);
	if(!status) {
		glGetShaderInfoLog(frag, sizeof(info), 0, info);
		fprintf(stderr, "Failed compiling fragment shader:\n");
		if(*info) fprintf(stderr, "%s\n", info);
		glDeleteShader(vert);
		glDeleteShader(frag);
		shader_();
		return;
	} else {
		fprintf(stderr, "Successfully compiled fragment shader.\n");
	}
	/* link */
	open->shader = glCreateProgram();
	glAttachShader(open->shader, vert);
	glAttachShader(open->shader, frag);
	glLinkProgram(open->shader);
	glDetachShader(open->shader, frag);
	glDetachShader(open->shader, vert);
	glDeleteShader(vert);
	glDeleteShader(frag);
	/* success? */
	glGetProgramiv(open->shader, GL_LINK_STATUS, &status);
	if(!status) {
		glGetProgramInfoLog(open->shader, sizeof(info), 0, info);
		fprintf(stderr, "Failed linking shader programme:\n");
		if(*info) fprintf(stderr, "%s\n", info);
		shader_();
		return;
	} else {
		fprintf(stderr, "Successfully linked shader programme.\n");
	}
	/* check for OpenGL's ability to execute this programme */
	glValidateProgram(open->shader);
	glGetProgramiv(open->shader, GL_VALIDATE_STATUS, &status);
	if(!status) {
		fprintf(stderr, "Failed validating shader programme.\n");
		shader_();
		return;
	} else {
		fprintf(stderr, "Successfully validated shader programme.\n");
	}
	/* constant values (the GL_TEXTUREx) */
	glUseProgram(open->shader);
	glUniform1i(glGetUniformLocation(open->shader, "sprite"), 0);
	glUniform1i(glGetUniformLocation(open->shader, "lighting"), 1);
	glUseProgram(0);

	isGLError("Open::shader");
}

static void shader_(void) {
	if(open->shader) {
		glDeleteProgram(open->shader);
		open->shader = 0;
		fprintf(stderr, "Unlinked fragment shader programme.\n");
	}
	if(glIsTexture(open->lighting)) {
		glDeleteTextures(1, &open->lighting);
		fprintf(stderr, "Deleted lighting texture.\n");
	}

	isGLError("Open::shader!");
}

/************************************************/

/** subclass Key */
static void Key(struct Key *key) {
	if(!key) return;
	/* all keys are turned off;
	 that's not necesarily true, but user expects this */
	key->state    = 0;
	key->down     = 0;
	key->integral = 0;
	key->time     = 0;
}

/** "static" check for how long the keys has been pressed, without repeat rate
 (destructive) */
int OpenKeyTime(const int key) {
	int time;
	struct Key *k;
	if(key < 0 || key >= k_max || !open) return 0;
	k = &open->key[key];
	if(k->state) {
		time    = open->graphics.time - k->down + k->integral;
		k->down = open->graphics.time;
	} else {
		time    = k->integral;
	}
	k->integral = 0;
	return time;
}

/** "static" key press, with repeat rate (destructive) */ 
int OpenKeyPress(const int key) {
	int time;
	struct Key *k;
	if(key < 0 || key >= k_max || !open) return 0;
	k = &open->key[key];
	if(k->state || k->integral) {
		k->integral = 0;
		time = glutGet(GLUT_ELAPSED_TIME);
		if(time > k->time + key_delay) {
			k->time = time;
			return -1;
		}
	}
	return 0;
}

/** GLUT_ to internal keys (are you sure that this is not standard?) */
static enum Keys glut_to_keys(int k) {
	switch(k) {
		case GLUT_KEY_LEFT:   return k_left;
		case GLUT_KEY_UP:     return k_up;
		case GLUT_KEY_RIGHT:  return k_right;
		case GLUT_KEY_DOWN:   return k_down;
		case GLUT_KEY_PAGE_UP:return k_pgup;
		case GLUT_KEY_PAGE_DOWN: return k_pgdn;
		case GLUT_KEY_HOME:   return k_home;
		case GLUT_KEY_END:    return k_end;
		case GLUT_KEY_INSERT: return k_ins;
		case GLUT_KEY_F1:     return k_f1;
		case GLUT_KEY_F2:     return k_f2;
		case GLUT_KEY_F3:     return k_f3;
		case GLUT_KEY_F4:     return k_f4;
		case GLUT_KEY_F5:     return k_f5;
		case GLUT_KEY_F6:     return k_f6;
		case GLUT_KEY_F7:     return k_f7;
		case GLUT_KEY_F8:     return k_f8;
		case GLUT_KEY_F9:     return k_f9;
		case GLUT_KEY_F10:    return k_f10;
		case GLUT_KEY_F11:    return k_f11;
		case GLUT_KEY_F12:    return k_f12;
		default: return k_unknown;
	}
}

/** callback for glutKeyboardFunc */
static void key_down(unsigned char k, int x, int y) {
	struct Key *key = &open->key[k];
	if(key->state) return;
	key->state  = -1;
	key->down = glutGet(GLUT_ELAPSED_TIME);
	/* fprintf(stderr, "Open::key_down: key %d hit at %d ms.\n", k, key->down); */
}

/** callback for glutKeyboardUpFunc */
static void key_up(unsigned char k, int x, int y) {
	struct Key *key = &open->key[k];
	if(!key->state) return;
	key->state = 0;
	key->integral += glutGet(GLUT_ELAPSED_TIME) - key->down;
	/* fprintf(stderr, "Open::key_up: key %d pressed %d ms at end of frame.\n", k, key->integral); */
}

/** callback for glutSpecialFunc */
static void key_down_special(int k, int x, int y) {
	struct Key *key = &open->key[glut_to_keys(k)];
	if(key->state) return;
	key->state  = -1;
	key->down = glutGet(GLUT_ELAPSED_TIME);
	/* fprintf(stderr, "Open::key_down_special: key %d hit at %d ms.\n", k, key->down); */
}

/** callback for glutSpecialUpFunc */
static void key_up_special(int k, int x, int y) {
	struct Key *key = &open->key[glut_to_keys(k)];
	if(!key->state) return;
	key->state = 0;
	key->integral += glutGet(GLUT_ELAPSED_TIME) - key->down;
	/* fprintf(stderr, "Open::key_up_special: key %d pressed %d ms at end of frame.\n", k, key->integral); */
}

/************************************************/

/** subclass Timer */
static void Timer(struct Timer *timer) {
	int i;

	timer->time        = glutGet(GLUT_ELAPSED_TIME);
	timer->frame       = 0;
	for(i = 0; i < max_frame_time; i++) timer->frameTime[i] = timer->time;
}

/** advances the timer every eg frame, logic step */
static void advance_time(struct Timer *timer) {
	timer->time  = glutGet(GLUT_ELAPSED_TIME);
	timer->frame = (timer->frame + 1) & (max_frame_time - 1);
	timer->frameTime[timer->frame] = timer->time;	
}

/************************************************/

/** subclass Screen */
static void Screen(struct Screen *screen, const int width, const int height) {
	screen->dim.x      = width;
	screen->dim.y      = height;
	screen->max.x      = 0.5f * screen->dim.x;
	screen->max.y      = 0.5f * screen->dim.y;	
	screen->saved.x    = screen->dim.x;
	screen->saved.y    = screen->dim.y;
	screen->savedPos.x = 0;
	screen->savedPos.y = 0;
	screen->full       = 0;
}

/** query screen hight/width or get default if no open; must never return null */
const struct Vec2f *OpenGetScreen(void) {
	if(!open) return 0;
	return &open->screen.max;
}

/** callback for glutReshapeFunc */
static void resize(int width, int height) {
	float half_width = 0.5f * width, half_height = 0.5f * height;
	if(width <= 0 || height <= 0) return;
	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluOrtho2D(-half_width, half_width, -half_height, half_height);
	if(!open) return;
	open->screen.dim.x = width;
	open->screen.dim.y = height;
	open->screen.max.x = half_width;
	open->screen.max.y = half_height;
	OpenPrintMatrix();

	isGLError("Open::resize");
}

/************************************************/

/** subclass Console */

static void Console(struct Console *console) {
	console->buffer[0] = 'f';
	console->buffer[1] = 'o';
	console->buffer[2] = 'o';
	console->buffer[3] = '\0';
}

/** printf for the window */
void OpenPrint(const char *fmt, ...) {
	va_list ap;

	if(!fmt || !open) return;
	/* print the chars into the buffer */
	va_start(ap, fmt);
	vsnprintf(open->console.buffer, console_buffer_size, fmt, ap);
	va_end(ap);
	printf("Open::Console: \"%s\" (%d.)\n", open->console.buffer, console_buffer_size);
}

/** called by display every frame */
static void rasterise_text(void) {
	char *a;

	glPushAttrib(GL_TRANSFORM_BIT | GL_VIEWPORT_BIT);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glTranslatef(-1, 0.9, 0);
	/*glViewport(0, 0, open->screen.dim.x, open->screen.dim.y);*/
	glColor4f(0, 1, 0, 0.7);
	glRasterPos2i(0, 0);
	
	/* WTF!!!! it's drawing it in the bottom?? fuck I can't take this anymore */

	/* draw overlay */
	/*glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glTranslatef(-(float)open->screen.max.x * 1.f, -(float)open->screen.max.y * 1.f, 0);*/
	for(a = open->console.buffer; *a; a++) {
		switch(*a) {
			case '\n':
				glRasterPos2i(0, -1);
				break;
			default:
				glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *a);
		}
	}
	glColor4f(1, 1, 1, 1);
	/*glPopMatrix();*/

	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glPopAttrib();

	isGLError("Open::Console::rasterise_text");
}
