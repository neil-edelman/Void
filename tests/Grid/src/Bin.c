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
#define BIN_LOG_SPACE 10/*14*/
#define BIN_SPACE (1 << BIN_LOG_SPACE)
/* divided between positive and negative for greater floating point accuracy */
#define BIN_HALF_SPACE (1 << (BIN_LOG_SPACE - 1))
/* 256px bins in the foreground, (ships, etc.) */
#define BIN_FG_LOG_SPACE 8
#define BIN_FG_LOG_SIZE (BIN_LOG_SPACE - BIN_FG_LOG_SPACE)
#define BIN_FG_SIZE (1 << BIN_FG_LOG_SIZE)
#define BIN_BIN_FG_SIZE (BIN_FG_SIZE * BIN_FG_SIZE)
/* 1024px bins in the background, (planets, etc.) */
#define BIN_BG_LOG_SPACE 10
#define BIN_BG_LOG_SIZE (BIN_LOG_SPACE - BIN_BG_LOG_SPACE)
#define BIN_BG_SIZE (1 << BIN_FG_LOG_SIZE)
#define BIN_BIN_BG_SIZE (BIN_BG_SIZE * BIN_BG_SIZE)

/* changed? update shader Far.vs! should be 0.00007:14285.714; space is
 big! too slow to get anywhere, so fudge it; basically, this makes space much
 smaller: ratio of fg:bg distance. */
static const float foreshortening = 0.2f, one_foreshortening = 5.0f;



/* Math things. */

struct Ortho3f { float x, y, theta; };
struct Vec3f { float x, y; };
struct Vec2i { int x, y; };

static void Ortho3f_filler_fg(struct Ortho3f *const this) {
	/* static int thing;
	 this->x = -8191.9f + 256.0f * thing++;
	 this->y = -8191.9f; */
	this->x = 1.0f * rand() / RAND_MAX * BIN_SPACE - BIN_HALF_SPACE;
	this->y = 1.0f * rand() / RAND_MAX * BIN_SPACE - BIN_HALF_SPACE;
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
	b.x = ((int)r->x + BIN_HALF_SPACE) >> BIN_FG_LOG_SPACE;
	if(b.x < 0) b.x = 0; else if(b.x >= BIN_FG_SIZE) b.x = BIN_FG_SIZE - 1;
	b.y = ((int)r->y + BIN_HALF_SPACE) >> BIN_FG_LOG_SPACE;
	if(b.y < 0) b.y = 0; else if(b.y >= BIN_FG_SIZE) b.y = BIN_FG_SIZE - 1;
	*bin = (b.y << BIN_FG_LOG_SIZE) + b.x;
	/*printf("(%.1f,%.1f)->(%d,%d)->%u\n", r->x, r->y, b.x, b.y, *bin);*/
}
static void bin_to_Vec2i(const unsigned bin, struct Vec2i *const r) {
	assert(bin < BIN_BIN_FG_SIZE);
	assert(r);
	r->x = ((bin & (BIN_FG_SIZE - 1)) << BIN_FG_LOG_SPACE) - BIN_HALF_SPACE;
	r->y = ((bin >> BIN_FG_LOG_SIZE) << BIN_FG_LOG_SPACE) - BIN_HALF_SPACE;
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
/** Updates the bin. */
static void Sprite_update_bin(struct SpriteListNode *const this) {
	unsigned bin;
	assert(this);
	Ortho3f_to_bin(&this->data.r, &bin);
	if(bin == this->data.bin) return;
	SpriteListRemove(bins + this->data.bin, &this->data);
	this->data.bin = bin;
	SpriteListPush(bins + bin, this);
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







#if 0
/** @implements <Sprite>Predicate */
static int print_sprite(struct Sprite *const this, void *const void_gnu) {
	FILE *const gnu = (FILE *)void_gnu;
	fprintf(gnu, "%f\t%f\t%f\n", this->r.x, this->r.y, this->bounding);
	return 1;
}
/** @implements <Sprite>DiAction */
static void sprite_data(struct Sprite *this, void *const void_data) {
	struct OutputData *const data = void_data;
	int i;
	printf("Space:\n");
	for(i = 0; i < BIN_BIN_FG_SIZE; i++) {
		bin = bins + i;
		printf("%d: %s\n", i, SpriteListXToString(bin));
	}
	printf("done.\n");
}
#endif




struct OutputData {
	FILE *file;
	unsigned i, n;
};

/** @implements <Sprite>DiAction */
static void sprite_data(struct Sprite *this, void *const void_out) {
	struct OutputData *const out = void_out;
	fprintf(out->file, "%f\t%f\t%f\t%f\n", this->r.x, this->r.y,
		this->bounding, (double)out->i++ / out->n);
}

/** @implements <Sprite>DiAction */
static void sprite_arrow(struct Sprite *this, void *const void_out) {
	struct OutputData *const out = void_out;
	struct Vec2i b;
	bin_to_Vec2i(this->bin, &b);
	fprintf(out->file, "set arrow from %f,%f to %d,%d # %u\n", this->r.x,
		this->r.y, b.x, b.y, this->bin);
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
	const unsigned seed = (unsigned)clock(), sprite_no = 10;
	enum { E_NO, E_DATA, E_GNU, E_SHIP } e = E_NO;

	srand(seed), rand(), printf("Seed %u.\n", seed);
	for(i = 0; i < BIN_BIN_FG_SIZE; i++) SpriteListClear(bins + i);
	do {
		struct OutputData out;
		struct Ship *ship;

		if(!(data = fopen(data_fn, "w"))) { e = E_DATA; break; }
		if(!(gnu = fopen(gnu_fn, "w")))   { e = E_GNU;  break; }
		if(!(ships = ShipSet()))          { e = E_SHIP; break; }
		ShipSetSetMigrate(ships, &Sprite_migrate);
		for(i = 0; i < sprite_no; i++) {
			if(!(ship = ShipSetNew(ships))) { e = E_SHIP; break; }
			Ship_filler(ship);
		}
		if(e) break;

		/* output data file */
		out.file = data, out.i = 0, out.n = sprite_no;
		for(i = 0; i < BIN_BIN_FG_SIZE; i++) {
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
		out.file = gnu, out.i = 0, out.n = sprite_no;
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
		case E_SHIP: fprintf(stderr, "Ship: %s.\n", ShipSetGetError(ships));
	} {
		for(i = 0; i < BIN_BIN_FG_SIZE; i++) SpriteListClear(bins + i);
		ShipSet_(&ships);
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
