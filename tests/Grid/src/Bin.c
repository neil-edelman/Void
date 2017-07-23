/** 2017 Neil Edelman, distributed under the terms of the GNU General
 Public License 3, see copying.txt, or
 \url{ https://opensource.org/licenses/GPL-3.0 }.

 Testing the bins. A {Bin} is like a hash bucket, but instead of hashing, it's
 determined by where in space you are. This allows you to do stuff like drawing
 and AI for onscreen bins and treat the faraway bins with statistical mechanics.
 Which allows you to have a lot of sprites -> ships, weapons -> more epic.

 @file		Bin
 @author	Neil
 @std		C89/90
 @fixme		Change diameter to radius. */

#include <stdlib.h>	/* rand */
#include <stdio.h>  /* fprintf */
#include <time.h>	/* clock */
#include <assert.h>	/* assert */
#include <math.h>	/* sqrtf (important, or else it will be wrong) */
#include "Orcish.h"

/* M_PI is a widely accepted gnu standard, not C<=99 -- who knew? */
#ifndef M_PI_F
#define M_PI_F (3.141592653589793238462643383279502884197169399375105820974944f)
#endif
#ifndef M_2PI_F
#define M_2PI_F \
(6.283185307179586476925286766559005768394338798750211641949889f)
#endif

const unsigned sprite_no = 2000;

/* 16384px de sitter (8192px on each quadrant,) divided between positive and
 negative for greater floating point accuracy */
#define BIN_LOG_ENTIRE 14
#define BIN_ENTIRE (1 << BIN_LOG_ENTIRE)
#define BIN_HALF_ENTIRE (1 << (BIN_LOG_ENTIRE - 1))
/* 256px bins in the foreground, (ships, etc.) */
#define BIN_FG_LOG_SPACE 8
#define BIN_FG_SPACE (1 << BIN_FG_LOG_SPACE)
#define BIN_FG_LOG_SIZE (BIN_LOG_ENTIRE - BIN_FG_LOG_SPACE)
#define BIN_FG_SIZE (1 << BIN_FG_LOG_SIZE)
#define BIN_BIN_FG_SIZE (BIN_FG_SIZE * BIN_FG_SIZE)
/* 1024px bins in the background, (planets, etc.) */
#define BIN_BG_LOG_SPACE 10
#define BIN_BG_LOG_SIZE (BIN_LOG_ENTIRE - BIN_BG_LOG_SPACE)
#define BIN_BG_SIZE (1 << BIN_FG_LOG_SIZE)
#define BIN_BIN_BG_SIZE (BIN_BG_SIZE * BIN_BG_SIZE)

/* changed? update shader Far.vs! should be 0.00007:14285.714; space is
 big! too slow to get anywhere, so fudge it; basically, this makes space much
 smaller: ratio of fg:bg distance. */
static const float foreshortening = 0.2f, one_foreshortening = 5.0f;
/* 1 guarentees that an object is detected within BIN_FG_SPACE; less will make
 it n^2 faster, but could miss something. */
static const float shortcut_collision = 0.5f;





/* Math things. */

struct Vec2f { float x, y; };
struct Vec2i { int x, y; };
struct Ortho3f { float x, y, theta; };
struct Rectangle4f { float x_min, x_max, y_min, y_max; };
struct Rectangle4i { int x_min, x_max, y_min, y_max; };

static struct Vec2f camera = { 0.0f, 0.0f },
	camera_extent = { 640.0f, 480.0f /*320.0f, 240.0f*/ };

/** Sets the camera location.
 @param x: (x, y) in pixels. */
static void DrawSetCamera(const struct Vec2f x) { camera.x = x.x, camera.y = x.y; }

/** Gets the visible part of the screen. */
static void DrawGetScreen(struct Rectangle4f *const rect) {
	if(!rect) return;
	rect->x_min = camera.x - camera_extent.x;
	rect->x_max = camera.x + camera_extent.x;
	rect->y_min = camera.y - camera_extent.y;
	rect->y_max = camera.y + camera_extent.y;
}

static void Vec2f_filler_fg(struct Vec2f *const this) {
	this->x = 1.0f * rand() / RAND_MAX * BIN_ENTIRE - BIN_HALF_ENTIRE;
	this->y = 1.0f * rand() / RAND_MAX * BIN_ENTIRE - BIN_HALF_ENTIRE;
}

static void Ortho3f_filler_fg(struct Ortho3f *const this) {
	Vec2f_filler_fg((struct Vec2f *)this);
	this->theta = M_2PI_F * rand() / RAND_MAX - M_PI_F;
}
static void Ortho3f_filler_v(struct Ortho3f *const this) {
	this->x = 10.0f * rand() / RAND_MAX - 5.0f;
	this->y = 10.0f * rand() / RAND_MAX - 5.0f;
	this->theta = 0.01f * rand() / RAND_MAX - 0.005f;
}
/** Maps a recangle from pixel space, {pixel}, to bin2 space, {bin}. */
static void Rectangle4f_to_bin4(const struct Rectangle4f *const pixel,
	struct Rectangle4i *const bin) {
	int temp;
	assert(pixel);
	assert(pixel->x_min <= pixel->x_max);
	assert(pixel->y_min <= pixel->y_max);
	assert(bin);
	/*printf("camera(%f, %f) extent(%f %f) %f -- %f, %f -- %f\n", camera.x, camera.y, camera_extent.x, camera_extent.y, pixel->x_min, pixel->x_max, pixel->y_min, pixel->y_max);
	printf("x_min %f + HALF %u >> LOG %u\n", pixel->x_min, BIN_HALF_ENTIRE, BIN_FG_LOG_SPACE);*/
	temp = ((int)pixel->x_min + BIN_HALF_ENTIRE) >> BIN_FG_LOG_SPACE;
	if(temp < 0) temp = 0;
	else if((unsigned)temp >= BIN_FG_SIZE) temp = BIN_FG_SIZE - 1;
	bin->x_min = temp;
	/*printf("x_min %f -> %d\n", pixel->x_min, temp);*/
	temp = ((int)pixel->x_max + 1 + BIN_HALF_ENTIRE) >> BIN_FG_LOG_SPACE;
	if(temp < 0) temp = 0;
	else if((unsigned)temp >= BIN_FG_SIZE) temp = BIN_FG_SIZE - 1;
	bin->x_max = temp;
	temp = ((int)pixel->y_min + BIN_HALF_ENTIRE) >> BIN_FG_LOG_SPACE;
	if(temp < 0) temp = 0;
	else if((unsigned)temp >= BIN_FG_SIZE) temp = BIN_FG_SIZE - 1;
	bin->y_min = temp;
	temp = ((int)pixel->y_max + 1 + BIN_HALF_ENTIRE) >> BIN_FG_LOG_SPACE;
	if(temp < 0) temp = 0;
	else if((unsigned)temp >= BIN_FG_SIZE) temp = BIN_FG_SIZE - 1;
	bin->y_max = temp;
	/*printf("y_max %f + HALF %u >> LOG %u\n", pixel->y_max, BIN_HALF_ENTIRE, BIN_FG_LOG_SPACE);*/
}

static void Rectangle4i_assign(struct Rectangle4i *const this,
	const struct Rectangle4i *const that) {
	assert(this);
	assert(that);
	this->x_min = that->x_min;
	this->x_max = that->x_max;
	this->y_min = that->y_min;
	this->y_max = that->y_max;
}

/** Maps a {bin2i} to a {bin}. Doesn't check for overflow.
 @return Success. */
static unsigned bin2i_to_bin(const struct Vec2i bin2) {
	assert(bin2.x >= 0);
	assert(bin2.y >= 0);
	assert(bin2.x < BIN_FG_SIZE);
	assert(bin2.y < BIN_FG_SIZE);
	return (bin2.y << BIN_FG_LOG_SIZE) + bin2.x;
}
static void bin_to_bin2i(const unsigned bin, struct Vec2i *const bin2) {
	assert(bin < BIN_BIN_FG_SIZE);
	assert(bin2);
	bin2->x = bin & (BIN_FG_SIZE - 1);
	bin2->y = bin >> BIN_FG_LOG_SIZE;
}
static void Vec2f_to_bin(const struct Vec2f *const r, unsigned *const bin) {
	struct Vec2i b;
	assert(r);
	assert(bin);
	b.x = ((int)r->x + BIN_HALF_ENTIRE) >> BIN_FG_LOG_SPACE;
	if(b.x < 0) b.x = 0; else if(b.x >= BIN_FG_SIZE) b.x = BIN_FG_SIZE - 1;
	b.y = ((int)r->y + BIN_HALF_ENTIRE) >> BIN_FG_LOG_SPACE;
	if(b.y < 0) b.y = 0; else if(b.y >= BIN_FG_SIZE) b.y = BIN_FG_SIZE - 1;
	*bin = (b.y << BIN_FG_LOG_SIZE) + b.x;
}
static void bin_to_Vec2i(const unsigned bin, struct Vec2i *const r) {
	assert(bin < BIN_BIN_FG_SIZE);
	assert(r);
	r->x = ((bin & (BIN_FG_SIZE - 1)) << BIN_FG_LOG_SPACE) - BIN_HALF_ENTIRE;
	r->y = ((bin >> BIN_FG_LOG_SIZE) << BIN_FG_LOG_SPACE) - BIN_HALF_ENTIRE;
}

static void rect4i_print(const char *const why, const struct Rectangle4i *rect){
	printf("%s: %d -- %d, %d -- %d\n", why, rect->x_min, rect->x_max, rect->y_min, rect->y_max);
}
static void rect4f_print(const char *const why, const struct Rectangle4f *rect){
	printf("%s: %.1f -- %.1f, %.1f -- %.1f\n", why, rect->x_min, rect->x_max, rect->y_min, rect->y_max);
}





/* Sprites */

/** {Sprite} virtual table. */
struct SpriteVt;
/** Define {Sprite}. */
struct Sprite {
	const struct SpriteVt *vt;
	unsigned bin;
	struct Ortho3f r, v;
	struct Vec2f r_5, r1;
	float bounding, bounding1;
	int is_glowing;
	char label;
};
/** @implements <Bin>Comparator */
static int Sprite_x_cmp(const struct Sprite *a, const struct Sprite *b) {
	return (b->r_5.x < a->r_5.x) - (a->r_5.x < b->r_5.x);
}
/** @implements <Bin>Comparator */
static int Sprite_y_cmp(const struct Sprite *a, const struct Sprite *b) {
	return (b->r_5.x < a->r_5.x) - (a->r_5.x < b->r_5.x);
}
/* declare */
static void Sprite_to_string(const struct Sprite *this, char (*const a)[12]);
/* Define {BinList} and {BinListNode}. */
#define LIST_NAME Sprite
#define LIST_TYPE struct Sprite
#define LIST_UA_NAME X
#define LIST_UA_COMPARATOR &Sprite_x_cmp
#define LIST_UB_NAME Y
#define LIST_UB_COMPARATOR &Sprite_y_cmp
#define LIST_TO_STRING &Sprite_to_string
#include "../../../src/general/List.h"
/** Every sprite has one {bin} based on their position. \see{OrthoMath.h}. */
static struct SpriteList bins[BIN_BIN_FG_SIZE];
/** {vt} is not set, so you should treat this as abstract: only instantiate
 though concrete classes. */
static void Sprite_filler(struct SpriteListNode *const this) {
	struct Sprite *const s = &this->data;
	assert(this);
	s->vt = 0; /* abstract */
	Ortho3f_filler_fg(&s->r);
	s->r_5.x = s->r1.x = s->r.x;
	s->r_5.y = s->r1.y = s->r.y;
	Ortho3f_filler_v(&s->v);
	s->bounding = s->bounding1 = (246.0f * rand() / RAND_MAX + 10.0f) * 0.5f;
	s->is_glowing = 0;
	s->label = '?';
	Vec2f_to_bin(&s->r_5, &s->bin);
	SpriteListPush(bins + s->bin, this);
}
/** Stale pointers from {Set}'s {realloc} are taken care of by this callback.
 One must set it with {*SetSetMigrate}.
 @implements Migrate */
static void Sprite_migrate(const struct Migrate *const migrate) {
	size_t i;
	for(i = 0; i < BIN_BIN_FG_SIZE; i++) {
		SpriteListMigrate(bins + i, migrate);
	}
}





/* Bins. */

/* Temporary pointers a subset of {bins}. */
#define SET_NAME Bin
#define SET_TYPE struct SpriteList *
#include "../../../src/general/Set.h" /* defines BinSet, BinSetNode */
static struct BinSet *draw_bins, *update_bins, *sprite_bins;
static struct Rectangle4i grow4; /* for clipping */

/** New bins calculates which bins are at all visible and which we should
 update, (around the visible,) every frame.
 @order The screen area. */
static void new_bins(void) {
	struct Rectangle4f rect;
	struct Rectangle4i bin4/*, grow4 <- now static */;
	struct Vec2i bin2i;
	struct SpriteList **bin;
	BinSetClear(draw_bins), BinSetClear(update_bins);
	DrawGetScreen(&rect); rect4f_print("new_bins screen", &rect);
	Rectangle4f_to_bin4(&rect, &bin4); rect4i_print("new_bins bins", &bin4);
	/* the updating region extends past the drawing region */
	Rectangle4i_assign(&grow4, &bin4);
	if(grow4.x_min > 0)               grow4.x_min--;
	if(grow4.x_max + 1 < BIN_FG_SIZE) grow4.x_max++;
	if(grow4.y_min > 0)               grow4.y_min--;
	if(grow4.y_max + 1 < BIN_FG_SIZE) grow4.y_max++;
	rect4i_print("new_bins grow", &grow4);
	/* draw in the centre */
	for(bin2i.y = bin4.y_max; bin2i.y >= bin4.y_min; bin2i.y--) {
		for(bin2i.x = bin4.x_min; bin2i.x <= bin4.x_max; bin2i.x++) {
			if(!(bin = BinSetNew(draw_bins))) { perror("draw_bins"); return; }
			*bin = bins + bin2i_to_bin(bin2i);
		}
	}
	/* update around the outside of the screen */
	if((bin2i.y = grow4.y_min) < bin4.y_min) {
		for(bin2i.x = grow4.x_min; bin2i.x <= grow4.x_max; bin2i.x++) {
			if(!(bin = BinSetNew(update_bins))) { perror("update_bins");return;}
			*bin = bins + bin2i_to_bin(bin2i);
		}
	}
	if((bin2i.y = grow4.y_max) > bin4.y_max) {
		for(bin2i.x = grow4.x_min; bin2i.x <= grow4.x_max; bin2i.x++) {
			if(!(bin = BinSetNew(update_bins))) { perror("update_bins");return;}
			*bin = bins + bin2i_to_bin(bin2i);
		}
	}
	if((bin2i.x = grow4.x_min) < bin4.x_min) {
		for(bin2i.y = bin4.y_min; bin2i.y <= bin4.y_max; bin2i.y++) {
			if(!(bin = BinSetNew(update_bins))) { perror("update_bins");return;}
			*bin = bins + bin2i_to_bin(bin2i);
		}
	}
	if((bin2i.x = grow4.x_max) > bin4.x_max) {
		for(bin2i.y = bin4.y_min; bin2i.y <= bin4.y_max; bin2i.y++) {
			if(!(bin = BinSetNew(update_bins))) { perror("update_bins");return;}
			*bin = bins + bin2i_to_bin(bin2i);
		}
	}
}

static FILE *gnu_glob; /* hack for sprite_new_bins */

/** Called from \see{collision_detection_and_respose}. */
static void sprite_new_bins(const struct Sprite *const this) {
	struct Rectangle4f extent;
	struct Rectangle4i bin4;
	struct Vec2i bin2i;
	struct SpriteList **bin;
	float extent_grow = this->bounding1 + shortcut_collision * BIN_FG_SPACE;
	assert(this && sprite_bins);
	BinSetClear(sprite_bins);
	/*extent.x_min = this->r_5.x - 0.5f * (BIN_FG_SPACE - this->bounding);
	extent.x_max = this->r_5.x + 0.5f * (BIN_FG_SPACE + this->bounding);
	extent.y_min = this->r_5.y - 0.5f * (BIN_FG_SPACE - this->bounding);
	extent.y_max = this->r_5.y + 0.5f * (BIN_FG_SPACE + this->bounding);*/
	extent.x_min = this->r_5.x - extent_grow;
	extent.x_max = this->r_5.x + extent_grow;
	extent.y_min = this->r_5.y - extent_grow;
	extent.y_max = this->r_5.y + extent_grow;
	Rectangle4f_to_bin4(&extent, &bin4);
	/* draw in the centre */
	printf("sprite_new_bins(%f, %f)\n", this->r.x, this->r.y);
	rect4f_print("sprite px", &extent);
	rect4i_print("sprite bin", &bin4);
	/* from \see{new_bins}, clip the sprite bins to the update bins */
	if(bin4.x_min < grow4.x_min) bin4.x_min = grow4.x_min;
	if(bin4.x_max > grow4.x_max) bin4.x_max = grow4.x_max;
	if(bin4.y_min < grow4.y_min) bin4.y_min = grow4.y_min;
	if(bin4.y_max > grow4.y_max) bin4.y_max = grow4.y_max;
	/* enumerate bins for the sprite */
	for(bin2i.y = bin4.y_max; bin2i.y >= bin4.y_min; bin2i.y--) {
		for(bin2i.x = bin4.x_min; bin2i.x <= bin4.x_max; bin2i.x++) {
			struct Vec2i bin_pos;
			/*printf("sprite bin2i(%u, %u)\n", bin2i.x, bin2i.y);*/
			if(!(bin = BinSetNew(sprite_bins))) { perror("bins"); return; }
			*bin = bins + bin2i_to_bin(bin2i);
			bin_to_Vec2i(bin2i_to_bin(bin2i), &bin_pos);
			fprintf(gnu_glob, "set arrow from %f,%f to %d,%d lw 0.2 "
				"lc rgb \"#CCEEEE\" front;\n",
				this->r_5.x, this->r_5.y, bin_pos.x, bin_pos.y);
		}
	}
}

/** Temporary action.
 @implements <Sprite>Action */
static void show_sprite(struct Sprite *this) {
	assert(this);
	printf("Sprite (%.1f, %.1f) : %.1f -(%.1f, %.1f)-> (%.1f, %.1f) : %.1f.\n",
		this->r.x, this->r.y, this->bounding,
		this->v.x, this->v.y,
		this->r_5.x, this->r_5.y, this->bounding1);
}

/** Temporary action.
 @implements <Sprite>Action */
static void mark_sprite(struct Sprite *this) {
	static label = 'a';
	assert(this);
	this->is_glowing = 1;
	this->label = label;
	if(label++ == 'z') label = 'a';
}

/** @implements <SpriteList *, SpriteAction *>DiAction */
static void act_bins(struct SpriteList **const pthis, void *const pact_void) {
	struct SpriteList *const this = *pthis;
	const SpriteAction *const pact = pact_void, act = *pact;
	assert(pthis && this && act);
	SpriteListYForEach(this, act);
}

/** @implements <SpriteList *, SpriteAction *>DiAction */
/*static void act_bins_and_sort(struct SpriteList **const pthis, void *const pact_void) {
	struct SpriteList *const this = *pthis;
	const SpriteAction *const pact = pact_void, act = *pact;
	assert(pthis && this && act);
	SpriteListYForEach(this, act);
	SpriteListSort(this);
}*/

/** {{act} \in { Draw }}.
 @implements <Bin>Action */
static void for_each_draw(SpriteAction act) {
	BinSetBiForEach(draw_bins, &act_bins, &act);
}
/** {{act} \in { Update, Draw }}. */
static void for_each_update(SpriteAction act) {
	BinSetBiForEach(update_bins, &act_bins, &act);
	for_each_draw(act);
}
/** {{act} \in { Update, Draw }}. */
/*static void for_each_update_and_sort(SpriteAction act) {
	BinSetBiForEach(update_bins, &act_bins_and_sort, &act);
	BinSetBiForEach(draw_bins,   &act_bins_and_sort, &act);
}*/





/* Sprite transfers for delayed moving sprites while SpriteListForEach. */

struct Transfer {
	struct Sprite *sprite;
	unsigned to_bin;
};

#define SET_NAME Transfer
#define SET_TYPE struct Transfer
#include "../../../src/general/Set.h" /* defines BinSet, BinSetNode */
static struct TransferSet *transfers;

static void transfer_sprite(struct Transfer *const this) {
	assert(this && this->sprite && this->to_bin < BIN_BIN_FG_SIZE);
	printf("Update bin of Sprite at (%f, %f) from Bin%u to Bin%u.\n",
		this->sprite->r.x, this->sprite->r.y, this->sprite->bin, this->to_bin);
	SpriteListRemove(bins + this->sprite->bin, this->sprite);
	SpriteListPush(bins + this->to_bin, (struct SpriteListNode *)this->sprite);
	this->sprite->bin = this->to_bin;
}





/* Forward references for {SpriteVt}. */
struct Ship;
static void ship_delete(struct Ship *const this);
static void Ship_to_string(const struct Ship *this, char (*const a)[12]);
/** Define {SpriteVt}. */
static const struct SpriteVt {
	SpriteAction delete;
	SpriteToString to_string;
} ship_vt = {
	(SpriteAction)&ship_delete,
	(SpriteToString)&Ship_to_string
};
/** @implements <Sprite>ToString */
static void Sprite_to_string(const struct Sprite *this, char (*const a)[12]) {
	this->vt->to_string(this, a);
}

/* Define {ShipSet} and {ShipSetNode}, a subclass of {Sprite}. */
struct Ship {
	struct SpriteListNode sprite;
	char name[16];
};
/** @implements <Ship>ToString */
static void Ship_to_string(const struct Ship *this, char (*const a)[12]) {
	sprintf(*a, "%.11s", this->name);
}
#define SET_NAME Ship
#define SET_TYPE struct Ship
#include "../../../src/general/Set.h"
static struct ShipSet *ships;
/** @implements <Ship>Action */
static void ship_delete(struct Ship *const this) {
	assert(this);
	SpriteListRemove(bins + this->sprite.data.bin, &this->sprite.data);
	ShipSetRemove(ships, this);
}
/** Extends {Sprite_filler}. */
static void Ship_filler(struct Ship *const this) {
	assert(this);
	Sprite_filler(&this->sprite);
	this->sprite.data.vt = &ship_vt;
	Orcish(this->name, sizeof this->name);
}

/* This is for communication with {sprite_data}, {sprite_arrow}, and
 {sprite_velocity}. */
struct OutputData {
	FILE *fp;
	unsigned i, n;
};

/** @implements <Sprite>DiAction */
static void sprite_data(struct Sprite *this, void *const void_out) {
	struct OutputData *const out = void_out;
	fprintf(out->fp, "%f\t%f\t%f\t%f\t%f\t%f\t%f\n", this->r.x, this->r.y,
		this->bounding, (double)out->i++ / out->n, this->r_5.x, this->r_5.y,
		this->bounding1);
}

/** @implements <Sprite>DiAction */
/*static void sprite_arrow(struct Sprite *this, void *const void_out) {
	struct OutputData *const out = void_out;
	struct Vec2i b;
	bin_to_Vec2i(this->bin, &b);
	fprintf(out->fp, "set arrow from %f,%f to %d,%d%s # %u\n", this->r.x,
		this->r.y, b.x, b.y, this->is_glowing ? " lw 3 lc rgb \"red\"" : "",
		this->bin);
}*/

static float dt_ms;

/** @implements <Sprite>DiAction */
static void sprite_velocity(struct Sprite *this, void *const void_out) {
	struct OutputData *const out = void_out;
	fprintf(out->fp, "set arrow from %f,%f to %f,%f lw 1 lc rgb \"blue\" "
		"front;\n", this->r.x, this->r.y,
		this->r.x + this->v.x * dt_ms, this->r.y + this->v.y * dt_ms);
}

/** For communication with {gnu_draw_bins}. */
struct ColourData {
	FILE *fp;
	const char *colour;
};

/** Draws squares for highlighting bins.
 @implements <Bin>DiAction */
static void gnu_draw_bins(struct SpriteList **this, void *const void_col) {
	static object = 1;
	const struct ColourData *const col = void_col;
	const unsigned bin = (unsigned)(*this - bins);
	struct Vec2i bin2i;
	assert(col);
	assert(bin < BIN_BIN_FG_SIZE);
	bin_to_Vec2i(bin, &bin2i);
	fprintf(col->fp, "# bin %u -> %d,%d\n", bin, bin2i.x, bin2i.y);
	fprintf(col->fp, "set object %u rect from %d,%d to %d,%d fc rgb \"%s\" "
		"fs solid noborder;\n", object++, bin2i.x, bin2i.y,
		bin2i.x + 256, bin2i.y + 256, col->colour);
}

/** Calculates temporary values, {r_5}, {r1}, and {bounding1}, in preparation
 for sorting and \see{collision_detection_and_response}. */
static void update_where(struct Sprite *const this) {
	struct Vec2f dx;
	unsigned bin;
	dx.x = this->v.x * dt_ms, dx.y = this->v.y * dt_ms;
	/* sort on this */
	this->r_5.x = this->r.x + 0.5f * dx.x;
	this->r_5.y = this->r.y + 0.5f * dx.y;
	/* where they should be if there's no collision */
	this->r1.x = this->r.x + dx.x;
	this->r1.y = this->r.y + dx.y;
	/* expanded bounding circle; sqrt? overestimate bounded by {Sqrt[2]} */
	this->bounding1 = this->bounding + 0.5f * sqrtf(dx.x * dx.x + dx.y * dx.y);
	/* wandered out of the bin? */
	Vec2f_to_bin(&this->r_5, &bin);
	if(bin != this->bin) {
		struct Transfer *t;
		if(!(t = TransferSetNew(transfers))) { fprintf(stderr,
			"Transfer error: %s.\n", TransferSetGetError(transfers)); return; }
		t->sprite = this;
		t->to_bin = bin;
	}
}

/** This is the most course-grained collision detection in pure two-dimensional
 space using {r_5} and {bounding1}.
 @implements <Sprite, Sprite>BiAction */
static void sprite_sprite_timeless_collide(struct Sprite *const this,
	void *const void_that) {
	struct Sprite *const that = void_that;
	struct Vec2f diff;
	float bounding1;
	assert(this && that);
	/* Break symmetry -- if two objects are near, we only need to report it
	 once. */
	if(this >= that) return;
	printf("this %f, %f that %f, %f.\n", this->r.x, this->r.y, that->r.x, that->r.y);
	/* Calculate the distance between; the sum of {bounding1}, viz, {bounding}
	 projected on time for easy discard. */
	diff.x = this->r_5.x - that->r_5.x, diff.y = this->r_5.y - that->r_5.y;
	bounding1 = this->bounding1 + that->bounding1;
	if(diff.x * diff.x + diff.y * diff.y >= bounding1 * bounding1) return;
	/* We know that they are kind of close. */
	fprintf(gnu_glob, "set arrow from %f,%f to %f,%f nohead lw 0.5 "
		"lc rgb \"red\" front;\n", this->r.x, this->r.y, that->r.x, that->r.y);
}
/** @implements <Bin, Sprite *>BiAction */
static void bin_sprite_collide(struct SpriteList **const pthis,
	void *const target_param) {
	struct SpriteList *const this = *pthis;
	struct Sprite *const target = target_param;
	struct Vec2i b2;
	const unsigned b = (unsigned)(this - bins);
	bin_to_bin2i(b, &b2);
	printf("bin_collide_sprite: bin %u (%u, %u) Sprite: (%f, %f).\n", b, b2.x,
		b2.y, target->r.x, target->r.y);
	SpriteListYBiForEach(this, &sprite_sprite_timeless_collide, target);
}
/** The sprites have been ordered by their {r_5}. Now we can do collisions.
 @implements <Sprite>Action */
static void collision_detection_and_responses(struct Sprite *const this) {
	/*SpriteBiAction biact = &collision_detection_and_response;*/
	printf("--Sprite bins:\n");
	sprite_new_bins(this);
	printf("--Checking:\n");
	BinSetBiForEach(sprite_bins, &bin_sprite_collide, this);
	/* fixme: have a {BinSetBiShortCircuit(bin_sprite_collide_dist);} that stops
	 after the bounding box on edges (maybe! I think, yes, that would make it
	 faster. Hmm, all of the thing is an edge.) */
}





int main(void) {
	FILE *data = 0, *gnu = 0;
	static const char *const data_fn = "Bin.data", *const gnu_fn = "Bin.gnu",
		*const eps_fn = "Bin.eps";
	static const char *gnuplot_format =
#ifdef SANE
		"gnuplot %s"
#else
		"/usr/local/bin/gnuplot %s"
#endif
		;
	char buff[128];
	unsigned i;
	const unsigned seed = /*12005*//*12295*/(unsigned)clock();
	enum { E_NO, E_DATA, E_GNU, E_DBIN, E_UBIN, E_SBIN, E_TRAN, E_SHIP }
		e = E_NO;

	srand(seed), rand(), printf("Seed %u.\n", seed);
	for(i = 0; i < BIN_BIN_FG_SIZE; i++) SpriteListClear(bins + i);
	/* Random camera position. */ {
		struct Vec2f pos;
		Vec2f_filler_fg(&pos);
		DrawSetCamera(pos);
	}
	/* Try. */
	do {
		struct OutputData out;
		struct ColourData col;
		struct Ship *ship;

		if(!(data = fopen(data_fn, "w"))) { e = E_DATA; break; }
		if(!(gnu = fopen(gnu_fn, "w")))   { e = E_GNU;  break; }
		gnu_glob = gnu;
		if(!(draw_bins = BinSet()))       { e = E_DBIN; break; }
		if(!(update_bins = BinSet()))     { e = E_UBIN; break; }
		if(!(sprite_bins = BinSet()))     { e = E_SBIN; break; }
		if(!(transfers = TransferSet()))  { e = E_TRAN; break; }
		if(!(ships = ShipSet()))          { e = E_SHIP; break; }
		ShipSetSetMigrate(ships, &Sprite_migrate);
		for(i = 0; i < sprite_no; i++) {
			if(!(ship = ShipSetNew(ships))) { e = E_SHIP; break; }
			Ship_filler(ship);
		}
		if(e) break;

		/* max 64? 256/64=4 max safe speed? max 32? 8 max safe speed? */
		dt_ms = 25.0f;
		new_bins();
		/* switch these sprites glowing */
		for_each_update(&mark_sprite);
		for_each_update(&show_sprite);
		printf("Updating:\n");
		/*for_each_update_and_sort(&update_where);*/
		for_each_update(&update_where);
		/* transfer bins for those that have wandered out of their bin */
		TransferSetForEach(transfers, &transfer_sprite);
		TransferSetClear(transfers);
		/* fixme: place things in to drawing area */
		printf("Colliding:\n");
		for_each_update(&collision_detection_and_responses);
		/* really? */
		printf("Updated?\n");
		for_each_update(&show_sprite);
		/* output data file */
		printf("Output:\n");
		out.fp = data, out.i = 0, out.n = sprite_no;
		for(i = 0; i < BIN_BIN_FG_SIZE; i++) {
			SpriteListSort(bins + i);
			SpriteListYBiForEach(bins + i, &sprite_data, &out);
		}
		/* output gnuplot script */
		fprintf(gnu, "set term postscript eps enhanced size 20cm, 20cm\n"
			"set output \"Bin.eps\"\n"
			"set size square;\n"
			"set palette defined (1 \"#0000FF\", 2 \"#00FF00\", 3 \"#FF0000\");"
			"\n"
			"set xtics 256 rotate; set ytics 256;\n"
			"set grid;\n"
			"set xrange [-8192:8192];\n"
			"set yrange [-8192:8192];\n"
			"set cbrange [0.0:1.0];\n");
		/* draw bins as squares behind */
		fprintf(gnu, "set style fill transparent solid 0.3 noborder;\n");
		col.fp = gnu, col.colour = "#ADD8E6";
		BinSetBiForEach(draw_bins, &gnu_draw_bins, &col);
		col.fp = gnu, col.colour = "#D3D3D3";
		BinSetBiForEach(update_bins, &gnu_draw_bins, &col);
		out.fp = gnu, out.i = 0, out.n = sprite_no;
		/* draw arrows from each of the sprites to their bins */
		for(i = 0; i < BIN_BIN_FG_SIZE; i++) {
			/*SpriteListXBiForEach(bins + i, &sprite_arrow, &out);*/
			SpriteListYBiForEach(bins + i, &sprite_velocity, &out);
		}
		/* draw the sprites */
		fprintf(gnu, "plot \"Bin.data\" using 5:6:7 with circles \\\n"
			"linecolor rgb(\"#00FF00\") fillstyle transparent "
			"solid 1.0 noborder title \"Velocity\", \\\n"
			"\"Bin.data\" using 1:2:3:4 with circles \\\n"
			"linecolor palette fillstyle transparent solid 0.3 noborder \\\n"
			"title \"Sprites\";\n");
	} while(0); switch(e) {
		case E_NO: break;
		case E_DATA: perror(data_fn); break;
		case E_GNU:  perror(gnu_fn);  break;
		case E_DBIN:
			fprintf(stderr, "Draw bin: %s.\n", BinSetGetError(draw_bins));
			break;
		case E_UBIN:
			fprintf(stderr, "Update bin: %s.\n", BinSetGetError(update_bins));
			break;
		case E_SBIN:
			fprintf(stderr, "Sprite bin: %s.\n", BinSetGetError(sprite_bins));
			break;
		case E_TRAN:
			fprintf(stderr,"Transfer set: %s.\n",TransferSetGetError(transfers))
			; break;
		case E_SHIP:
			fprintf(stderr, "Ship: %s.\n", ShipSetGetError(ships));
			break;
	} {
		for(i = 0; i < BIN_BIN_FG_SIZE; i++) SpriteListClear(bins + i);
		ShipSet_(&ships);
		TransferSet_(&transfers);
		BinSet_(&update_bins);
		BinSet_(&draw_bins);
		if(fclose(data) == EOF) perror(data_fn);
		if(fclose(gnu) == EOF)  perror(gnu_fn);
	}
	if(!e && ((sprintf(buff, gnuplot_format, gnu_fn), system(buff))
		|| (sprintf(buff, "open %s", eps_fn), system(buff)))) {
		fprintf(stderr, "The programme completed, but it was not able to find "
			"gnuplot and open to automate processing.\n");
	}
	return e ? EXIT_FAILURE : EXIT_SUCCESS;
}
