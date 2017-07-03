#include <stdlib.h>	/* rand */
#include <stdio.h>  /* fprintf */
#include <time.h>	/* clock */
#include <assert.h>	/* assert */

/* M_PI is a widely accepted gnu standard, not C<=99 -- who knew? */
#ifndef M_PI_F
#define M_PI_F (3.141592653589793238462643383279502884197169399375105820974944f)
#endif
#ifndef M_2PI_F
#define M_2PI_F \
(6.283185307179586476925286766559005768394338798750211641949889f)
#endif

/* 16384px de sitter (8192px on each quadrant) */
#define BIN_LOG_SPACE 14
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

#if 0
/* fixme: unsigned -> (struct SpriteList *)? */
/* Define the bins that each frame will concern itself with; these are erased
 and updated every frame. See \see{SpriteUpdate}. These are just ints. */
#define LIST_NAME Bin
#define LIST_TYPE unsigned
#define LIST_UA_NAME Order
#include "../../src/general/List.h" /* defines BinList, BinListNode */
static struct BinList draw_bins, update_bins;
/* The backing for {draw_bins} and {update_bins}. */
#define SET_NAME BinEntry
#define SET_TYPE struct BinListNode
#include "../../src/general/Set.h" /* defines BinEntrySet, BinEntrySetNode */
static struct BinEntrySet *bins;
#endif



struct Ortho3f { float x, y, theta; };
struct Vec3f { float x, y; };
struct Vec2i { int x, y; };

struct Sprite {
	unsigned bin;
	struct Ortho3f r, v;
	float bounding;
};
/** @implements <Sprite>Comparator */
static int Sprite_x_cmp(const struct Sprite *a, const struct Sprite *b) {
	return (b->r.x < a->r.x) - (a->r.x < b->r.x);
}
/** @implements <Sprite>Comparator */
static int Sprite_y_cmp(const struct Sprite *a, const struct Sprite *b) {
	return (b->r.x < a->r.x) - (a->r.x < b->r.x);
}
/** @implements <Sprite>ToString */
static void Sprite_to_string(const struct Sprite *this, char (*const a)[12]) {
	sprintf(*a, "%.1f", this->r.x);
}

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
/* Define {SpriteList} and {SpriteListNode}. */
#define LIST_NAME Sprite
#define LIST_TYPE struct Sprite
#define LIST_UA_NAME X
#define LIST_UA_COMPARATOR &Sprite_x_cmp
#define LIST_UB_NAME Y
#define LIST_UB_COMPARATOR &Sprite_y_cmp
#define LIST_TO_STRING &Sprite_to_string
#include "../../src/general/List.h"
/** Every sprite has one {bin} based on their position. \see{OrthoMath.h}. */
static struct SpriteList sprite_bins[BIN_BIN_FG_SIZE];

#define SET_NAME SpriteEntry
#define SET_TYPE struct SpriteListNode
#include "../../src/general/Set.h"/*defines SpriteEntrySet, SpriteEntrySetNode*/
static struct SpriteEntrySet *sprites;

/** Fills with random sprites. */
static void Sprite_filler(struct SpriteListNode *const this) {
	struct Sprite *const sprite = &this->data;
	struct SpriteList *bin;
	assert(this);
	assert(sprite);
	assert(sprite->bin < BIN_BIN_FG_SIZE);
	Ortho3f_filler_fg(&sprite->r);
	Ortho3f_filler_v(&sprite->v);
	Ortho3f_to_bin(&sprite->r, &sprite->bin);
	sprite->bounding = 246.0f * rand() / RAND_MAX + 10.0f;
	/* put into bin */
	bin = sprite_bins + sprite->bin;
	SpriteListPush(bin, this);
#if 1
	/* this line is ESSENTIAL; I want to take it out, but it crashes */
	printf("x: %f\n", this->data.r.x);
#endif
}

/** @implements <Sprite>Predicate */
static int gnu_print(struct SpriteListNode *const this, void *const void_gnu) {
	FILE *const gnu = (FILE *)void_gnu;
	struct Vec2i b;
	assert(this);
	bin_to_Vec2i(this->data.bin, &b);
	fprintf(gnu, "set arrow from %f,%f to %d,%d # %u\n", this->data.r.x,
		this->data.r.y, b.x, b.y, this->data.bin);
	return 1;
}

/** @implements <Sprite>Predicate */
/*static int print_sprite(struct Sprite *const this, void *const void_gnu) {
	FILE *const gnu = (FILE *)void_gnu;
	fprintf(gnu, "%f\t%f\t%f\n", this->r.x, this->r.y, this->bounding);
	return 1;
}*/

static void print_space(void) {
	struct SpriteList *sl;
	int i;
	printf("Space:\n");
	for(i = 0; i < BIN_BIN_FG_SIZE; i++) {
		sl = sprite_bins + i;
		if(!SpriteListXGetFirst(sl)) continue;
		printf("%d: %s\n", i, SpriteListXToString(sl));
		/*fprintf(stderr, "Sprite bin %u:\n", i);
		 SpriteListSetParam(sl, stderr);
		 SpriteListXShortCircuit(sl, &print_sprite);*/
	}
	printf("done.\n");
}

static void test(FILE *data, FILE *gnu) {
	struct SpriteListNode *snode;
	/*struct Sprite *s;*/
	int i;
	const int n = 2000;

	/* populate */
	print_space();
	for(i = 0; i < n; i++) {
		if(!(snode = SpriteEntrySetNew(sprites))) {
			fprintf(stderr, "test: %s.\n", SpriteEntrySetGetError(sprites));
			return;
		}
		Sprite_filler(snode);
		fprintf(data, "%f\t%f\t%f\t%f\n", snode->data.r.x, snode->data.r.y,
			snode->data.bounding, (double)i / n);
	}
	print_space();
	/*for(i = 0; i < BIN_BIN_FG_SIZE; i++) {
		sl = sprite_bins + i;
		for(snode = SpriteListXGetFirst(sl); snode;
			snode = SpriteListNodeXGetNext(snode)) {
			if(!(s = &snode->data)) {
				fprintf(stderr, "wtf bin %u.\n", i);
				continue;
			}
			if(i > 100) break;
			fprintf(data, "%f\t%f\t%f\t%d\n", snode->data.r.x, snode->data.r.y,
				snode->data.bounding, i);
		}
	}*/
	fprintf(gnu, "set term postscript eps enhanced size 20cm, 20cm\n"
		"set output \"Bin.eps\"\n"
		"set size square;\n"
		"set palette defined (1 \"#0000FF\", 2 \"#00FF00\", 3 \"#FF0000\")\n"
		"set xtics 256 rotate; set ytics 256;\n"
		"set grid;\n"
		"set xrange [-8192:8192];\n"
		"set yrange [-8192:8192];\n"
		"set cbrange [0.0:1.0];\n");
	SpriteEntrySetSetParam(sprites, gnu);
	SpriteEntrySetShortCircuit(sprites, &gnu_print);
	fprintf(gnu, "plot \"Bin.data\" using 1:2:(0.5*$3):4 with circles \\\n"
		"linecolor palette fillstyle transparent solid 0.3 noborder \\\n"
		"title \"Sprites\";");
}

int main(void) {
	FILE *data, *gnu;
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

	srand((unsigned)clock());

	for(i = 0; i < BIN_BIN_FG_SIZE; i++) SpriteListClear(sprite_bins + i);
	/*if(!(bins = BinEntrySet())) return
		fprintf(stderr, "Bin: %s.\n", BinEntrySetGetError(bins)), EXIT_FAILURE;*/
	if(!(sprites = SpriteEntrySet())) return fprintf(stderr, "Sprite: %s.\n",
		SpriteEntrySetGetError(sprites)), EXIT_FAILURE;

	if(!(data = fopen(data_fn, "w"))) {
		perror(data_fn);
		return EXIT_FAILURE;
	}
	if(!(gnu = fopen(gnu_fn, "w"))) {
		fclose(data);
		perror(gnu_fn);
		return EXIT_FAILURE;
	}

	test(data, gnu);

	for(i = 0; i < BIN_BIN_FG_SIZE; i++) SpriteListClear(sprite_bins + i);
	SpriteEntrySet_(&sprites);

	if(fclose(data) == EOF) perror(data_fn);
	if(fclose(gnu) == EOF)  perror(gnu_fn);

	if((sprintf(buff, gnuplot_format, gnu_fn), system(buff))
		|| (sprintf(buff, "open %s", eps_fn), system(buff))) {
		fprintf(stderr, "The programme completed, but it was not able to find "
			"gnuplot and open to automate processing.\n");
	}

	return EXIT_SUCCESS;
}
