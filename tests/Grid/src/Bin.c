#include <stdlib.h>	/* rand */
#include <stdio.h>  /* fprintf */
#include <time.h>	/* clock */
#include <assert.h>	/* assert */
#include "Orcish.h"

/* M_PI is a widely accepted gnu standard, not C<=99 -- who knew? */
#ifndef M_PI_F
#define M_PI_F (3.141592653589793238462643383279502884197169399375105820974944f)
#endif
#ifndef M_2PI_F
#define M_2PI_F \
(6.283185307179586476925286766559005768394338798750211641949889f)
#endif

/* 16384px de sitter (8192px on each quadrant) */
#define BIN_LOG_ENTIRE 14
#define BIN_SPACE (1 << BIN_LOG_ENTIRE)
/* divided between positive and negative for greater floating point accuracy */
#define BIN_HALF_ENTIRE (1 << (BIN_LOG_ENTIRE - 1))
/* 256px bins in the foreground, (ships, etc.) */
#define BIN_FG_LOG_SPACE 8
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



/* Math things. */

struct Vec2f { float x, y; };
struct Vec2i { int x, y; };
struct Ortho3f { float x, y, theta; };
struct Rectangle4f { float x_min, x_max, y_min, y_max; };
struct Rectangle4i { int x_min, x_max, y_min, y_max; };

static struct Vec2f camera = { 0.0f, 0.0f }, camera_extent = { 320.0f, 240.0f };

/** Sets the camera location.
 @param x: (x, y) in pixels. */
static void DrawSetCamera(const struct Vec2f x) { camera.x = x.x, camera.y = x.y; }

/** Gets the visible part of the screen. */
static void DrawGetScreen(struct Rectangle4f *const rect) {
	if(!rect) return;
	printf("%f\n", camera.x - camera_extent.x);
	rect->x_min = camera.x - camera_extent.x;
	rect->x_max = camera.x + camera_extent.x;
	rect->y_min = camera.y - camera_extent.y;
	rect->y_max = camera.y + camera_extent.y;
	printf("ymin %f\n", rect->y_min);
}

static void Ortho3f_filler_fg(struct Ortho3f *const this) {
	/* static int thing;
	 this->x = -8191.9f + 256.0f * thing++;
	 this->y = -8191.9f; */
	this->x = 1.0f * rand() / RAND_MAX * BIN_SPACE - BIN_HALF_ENTIRE;
	this->y = 1.0f * rand() / RAND_MAX * BIN_SPACE - BIN_HALF_ENTIRE;
	this->theta = M_2PI_F * rand() / RAND_MAX - M_PI_F;
}
static void Ortho3f_filler_v(struct Ortho3f *const this) {
	this->x = 1.0f * rand() / RAND_MAX * 10.0f - 5.0f;
	this->y = 1.0f * rand() / RAND_MAX * 10.0f - 5.0f;
	this->theta = 0.01f * rand() / RAND_MAX - 0.005f;
}
static void Ortho3f_to_bin(const struct Ortho3f *const r, unsigned *const bin) {
	struct Vec2i b;
	assert(r);
	assert(bin);
	b.x = ((int)r->x + BIN_HALF_ENTIRE) >> BIN_FG_LOG_SPACE;
	if(b.x < 0) b.x = 0; else if(b.x >= BIN_FG_SIZE) b.x = BIN_FG_SIZE - 1;
	b.y = ((int)r->y + BIN_HALF_ENTIRE) >> BIN_FG_LOG_SPACE;
	if(b.y < 0) b.y = 0; else if(b.y >= BIN_FG_SIZE) b.y = BIN_FG_SIZE - 1;
	*bin = (b.y << BIN_FG_LOG_SIZE) + b.x;
	/*printf("(%.1f,%.1f)->(%d,%d)->%u\n", r->x, r->y, b.x, b.y, *bin);*/
}
static void bin_to_Vec2i(const unsigned bin, struct Vec2i *const r) {
	assert(bin < BIN_BIN_FG_SIZE);
	assert(r);
	r->x = ((bin & (BIN_FG_SIZE - 1)) << BIN_FG_LOG_SPACE) - BIN_HALF_ENTIRE;
	r->y = ((bin >> BIN_FG_LOG_SIZE) << BIN_FG_LOG_SPACE) - BIN_HALF_ENTIRE;
}
/** Maps a recangle from pixel space, {pixel}, to bin2 space, {bin}. */
static void Rectangle4f_to_bin4(const struct Rectangle4f *const pixel,
	struct Rectangle4i *const bin) {
	int temp;
	assert(pixel);
	assert(pixel->x_min <= pixel->x_max);
	assert(pixel->y_min <= pixel->y_max);
	assert(bin);
	printf("camera(%f, %f) extent(%f %f) %f -- %f, %f -- %f\n", camera.x, camera.y, camera_extent.x, camera_extent.y, pixel->x_min, pixel->x_max, pixel->y_min, pixel->y_max);
	printf("x_min %f + HALF %u >> LOG %u\n", pixel->x_min, BIN_HALF_ENTIRE, BIN_FG_LOG_SPACE);
	temp = ((int)pixel->x_min + BIN_HALF_ENTIRE) >> BIN_FG_LOG_SPACE;
	if(temp < 0) temp = 0;
	else if((unsigned)temp >= BIN_FG_SIZE) temp = BIN_FG_SIZE - 1;
	bin->x_min = temp;
	printf("x_min %f -> %d\n", pixel->x_min, temp);
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
	printf("y_max %f + HALF %u >> LOG %u\n", pixel->y_max, BIN_HALF_ENTIRE, BIN_FG_LOG_SPACE);
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

static void rect4i_print(const struct Rectangle4i *rect) {
	printf("%d -- %d, %d -- %d\n", rect->x_min, rect->x_max, rect->y_min, rect->y_max);
}
static void rect4f_print(const struct Rectangle4f *rect) {
	printf("%.1f -- %.1f, %.1f -- %.1f\n", rect->x_min, rect->x_max, rect->y_min, rect->y_max);
}

/** {Sprite} virtual table. */
struct SpriteVt;
/** Define {Sprite}. */
struct Sprite {
	const struct SpriteVt *vt;
	unsigned bin;
	struct Ortho3f r, v;
	float bounding;
};
/** @implements <Bin>Comparator */
static int Sprite_x_cmp(const struct Sprite *a, const struct Sprite *b) {
	return (b->r.x < a->r.x) - (a->r.x < b->r.x);
}
/** @implements <Bin>Comparator */
static int Sprite_y_cmp(const struct Sprite *a, const struct Sprite *b) {
	return (b->r.x < a->r.x) - (a->r.x < b->r.x);
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
	Ortho3f_filler_v(&s->v);
	s->bounding = 246.0f * rand() / RAND_MAX + 10.0f;
	Ortho3f_to_bin(&s->r, &s->bin);
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

/* Temporary pointers a subset of {bins}. */
#define SET_NAME Bin
#define SET_TYPE struct SpriteList *
#include "../../../src/general/Set.h" /* defines BinSet, BinSetNode */
static struct BinSet *draw_bins, *update_bins;

static void new_bins(void) {
	struct Rectangle4f rect;
	struct Rectangle4i bin4, grow4;
	struct Vec2i bin2i;
	struct SpriteList **bin;
	BinSetClear(draw_bins), BinSetClear(update_bins);
	DrawGetScreen(&rect); rect4f_print(&rect);
	Rectangle4f_to_bin4(&rect, &bin4); rect4i_print(&bin4);
	/* the updating region extends past the drawing region */
	Rectangle4i_assign(&grow4, &bin4);
	if(grow4.x_min > 0)               grow4.x_min--;
	if(grow4.x_max + 1 < BIN_FG_SIZE) grow4.x_max++;
	if(grow4.y_min > 0)               grow4.y_min--;
	if(grow4.y_max + 1 < BIN_FG_SIZE) grow4.y_max++;
	rect4i_print(&grow4);
	/* draw in the centre */
	for(bin2i.y = bin4.y_max; bin2i.y >= bin4.y_min; bin2i.y--) {
		for(bin2i.x = bin4.x_min; bin2i.x <= bin4.x_max; bin2i.x++) {
			printf("bin2i(%u, %u)\n", bin2i.x, bin2i.y);
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

/* This is for communication with {SpriteListDiForEach}. */
struct OutputData {
	FILE *fp;
	unsigned i, n;
};

/** @implements <Sprite>DiAction */
static void sprite_data(struct Sprite *this, void *const void_out) {
	struct OutputData *const out = void_out;
	fprintf(out->fp, "%f\t%f\t%f\t%f\n", this->r.x, this->r.y,
		this->bounding, (double)out->i++ / out->n);
}

/** @implements <Sprite>DiAction */
static void sprite_arrow(struct Sprite *this, void *const void_out) {
	struct OutputData *const out = void_out;
	struct Vec2i b;
	bin_to_Vec2i(this->bin, &b);
	fprintf(out->fp, "set arrow from %f,%f to %d,%d # %u\n", this->r.x,
		this->r.y, b.x, b.y, this->bin);
}

/** For communication. */
struct ColourData {
	FILE *fp;
	char *colour;
};

/** @implements <Bin>DiAction */
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
	const unsigned seed = (unsigned)clock(), sprite_no = 1000;
	enum { E_NO, E_DATA, E_GNU, E_DBIN, E_UBIN, E_SHIP } e = E_NO;

	srand(seed), rand(), printf("Seed %u.\n", seed);
	for(i = 0; i < BIN_BIN_FG_SIZE; i++) SpriteListClear(bins + i);
	do {
		struct OutputData out;
		struct ColourData col;
		struct Ship *ship;

		if(!(data = fopen(data_fn, "w"))) { e = E_DATA; break; }
		if(!(gnu = fopen(gnu_fn, "w")))   { e = E_GNU;  break; }
		if(!(draw_bins = BinSet()))       { e = E_DBIN; break; }
		if(!(update_bins = BinSet()))     { e = E_UBIN; break; }
		if(!(ships = ShipSet()))          { e = E_SHIP; break; }
		ShipSetSetMigrate(ships, &Sprite_migrate);
		for(i = 0; i < sprite_no; i++) {
			if(!(ship = ShipSetNew(ships))) { e = E_SHIP; break; }
			Ship_filler(ship);
		}
		if(e) break;

		/* output data file */
		out.fp = data, out.i = 0, out.n = sprite_no;
		for(i = 0; i < BIN_BIN_FG_SIZE; i++) {
			SpriteListSort(bins + i);
			SpriteListXBiForEach(bins + i, &sprite_data, &out);
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
		fprintf(gnu, "set style fill transparent solid 0.3 noborder;\n");
		new_bins();
		col.fp = gnu, col.colour = "#ADD8E6";
		BinSetBiForEach(draw_bins, &gnu_draw_bins, &col);
		col.fp = gnu, col.colour = "#D3D3D3";
		BinSetBiForEach(update_bins, &gnu_draw_bins, &col);
		out.fp = gnu, out.i = 0, out.n = sprite_no;
		for(i = 0; i < BIN_BIN_FG_SIZE; i++) {
			SpriteListXBiForEach(bins + i, &sprite_arrow, &out);
		}
		fprintf(gnu, "plot \"Bin.data\" using 1:2:(0.5*$3):4 with circles \\\n"
			"linecolor palette fillstyle transparent solid 0.3 noborder \\\n"
			"title \"Sprites\";");
	} while(0); switch(e) {
		case E_NO: break;
		case E_DATA: perror(data_fn); break;
		case E_GNU:  perror(gnu_fn);  break;
		case E_DBIN:
			fprintf(stderr, "Bin: %s.\n", BinSetGetError(draw_bins)); break;
		case E_UBIN:
			fprintf(stderr, "Bin: %s.\n", BinSetGetError(update_bins)); break;
		case E_SHIP:
			fprintf(stderr, "Ship: %s.\n", ShipSetGetError(ships)); break;
	} {
		for(i = 0; i < BIN_BIN_FG_SIZE; i++) SpriteListClear(bins + i);
		ShipSet_(&ships);
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
