/** 2017 Neil Edelman, distributed under the terms of the GNU General
 Public License 3, see copying.txt, or
 \url{ https://opensource.org/licenses/GPL-3.0 }.

 Used to map a floating point zero-centred position into an array of discrete
 size.

 @title		Bins
 @author	Neil
 @std		C89/90
 @version	2017-10 Broke off from Sprites.
 @since		2017-05 Generics.
			2016-01
			2015-06 */

#include <stdio.h> /* stderr */
#include <limits.h> /* INT_MAX */
#include "../general/OrthoMath.h" /* Rectangle4f, etc */
#include "Bins.h"

static const float epsilon = 0.0005f;

#define POOL_NAME Bin
#define POOL_TYPE int
#include "../templates/Pool.h"

struct Bins {
	int side_size;
	float half_space, one_each_bin;
	struct BinPool *pool;
};

void Bins_(struct Bins **const pthis) {
	struct Bins *this;
	if(!pthis || !(this = *pthis)) return;
	BinPool_(&this->pool);
	this = *pthis = 0;
}

/** @param side_size: The size of a side; the {BinsTake} returns
 {[0, side_size^2-1]}.
 @param each_bin: How much space per bin.
 @fixme Have max values. */
struct Bins *Bins(const size_t side_size, const float each_bin) {
	struct Bins *this;
	if(!side_size || side_size > INT_MAX || each_bin < epsilon)
		{ fprintf(stderr, "Bins: parameters out of range.\n"); return 0; }
	if(!(this = malloc(sizeof *this))) { perror("Bins"); Bins_(&this);return 0;}
	this->side_size = (int)side_size;
	this->half_space = (float)side_size * each_bin / 2.0f;
	this->one_each_bin = 1.0f / each_bin;
	if(!(this->pool = BinPool(0, 0))) { fprintf(stderr, "Bins: %s.\n",
		BinPoolGetError(0)); Bins_(&this); return 0; }
	return this;
}

unsigned BinsVector(struct Bins *const this, struct Vec2f *const vec) {
	struct Vec2i v2i;
	if(!this || !vec) return 0;
	v2i.x = (vec->x + this->half_space) * this->one_each_bin;
	if(v2i.x < 0) v2i.x = 0;
	else if(v2i.x >= this->side_size) v2i.x = this->side_size - 1;
	v2i.y = (vec->y + this->half_space) * this->one_each_bin;
	if(v2i.y < 0) v2i.y = 0;
	else if(v2i.y >= this->side_size) v2i.y = this->side_size - 1;
	return v2i.y * this->side_size + v2i.x;
}

/** @return Success. */
int BinsSetRectangle(struct Bins *const this, struct Rectangle4f *const rect) {
	struct Rectangle4i bin4;
	struct Vec2i bin2i;
	int *bin;
	if(!this || !rect) return 0;
	BinPoolClear(this->pool);
	/* Map floating point rectangle {rect} to index rectangle {bin4}. */
	bin4.x_min = (rect->x_min + this->half_space) * this->one_each_bin;
	if(bin4.x_min < 0) bin4.x_min = 0;
	bin4.x_max = (rect->x_max + this->half_space) * this->one_each_bin;
	if(bin4.x_max >= this->side_size) bin4.x_max = this->side_size - 1;
	bin4.y_min = (rect->y_min + this->half_space) * this->one_each_bin;
	if(bin4.y_min < 0) bin4.y_min = 0;
	bin4.y_max = (rect->y_max + this->half_space) * this->one_each_bin;
	if(bin4.y_max >= this->side_size) bin4.y_max = this->side_size - 1;
	/* Generally goes faster when you follow the scan-lines; not sure whether
	 this is so important with buffering. */
	for(bin2i.y = bin4.y_max; bin2i.y >= bin4.y_min; bin2i.y--) {
		for(bin2i.x = bin4.x_min; bin2i.x <= bin4.x_max; bin2i.x++) {
			/*printf("sprite bin2i(%u, %u)\n", bin2i.x, bin2i.y);*/
			if(!(bin = BinPoolNew(this->pool))) { fprintf(stderr,
				"BinsAdd: %s.\n", BinPoolGetError(this->pool)); return 0; }
			*bin = bin2i.y * this->side_size + bin2i.x;
			/*bin_to_Vec2i(bin2i_to_fg_bin(bin2i), &bin_pos);
			fprintf(gnu_glob, "set arrow from %f,%f to %d,%d lw 0.2 "
			 "lc rgb \"#CCEEEE\" front;\n",
			 this->x_5.x, this->x_5.y, bin_pos.x, bin_pos.y);*/
		}
	}
	return 1;
}

/** @param pbin: The variable pointed-to is set to an element index if {Bins}
 had more elements, in the range {[0, size_side^2-1]}.
 @return Whether the {Bins} had an element. */
void BinsForEach(struct Bins *const this, const BinsAction action) {
	size_t i = 0;
	if(!this || !action) return;
	while(BinPoolIsElement(this->pool, i))
		action(*BinPoolGetElement(this->pool, i++));
}
void BinsBiForEach(struct Bins *const this, const BinsBiAction action,
	void *const param) {
	size_t i = 0;
	if(!this || !action) return;
	while(BinPoolIsElement(this->pool, i))
		action(*BinPoolGetElement(this->pool, i++), param);
}




#if 0
BinPool_(&sprite_bins);
BinPool_(&bg_bins);
BinPool_(&update_bins);
BinPool_(&draw_bins);

if(!(draw_bins = BinPool())
   || !(update_bins = BinPool())
   || !(bg_bins = BinPool())
   || !(sprite_bins = BinPool())

/** Every sprite has one {bin} based on their position. \see{OrthoMath.h}. */
   static struct SpriteList bins[BIN_BIN_FG_SIZE + 1];
   static struct SpriteList *const holding_bin = bins + BIN_BIN_FG_SIZE;
   static struct SpriteList fars[BIN_BIN_BG_SIZE];

/** Called from \see{collide}. */
   static void sprite_new_bins(const struct Sprite *const this) {
	   struct Rectangle4f extent;
	   struct Rectangle4i bin4;
	   struct Vec2i bin2i;
	   struct SpriteList **bin;
	   float extent_grow = this->bounding1 + SHORTCUT_COLLISION * BIN_FG_SPACE;
	   assert(this && sprite_bins);
	   BinPoolClear(sprite_bins);
	   extent.x_min = this->x_5.x - extent_grow;
	   extent.x_max = this->x_5.x + extent_grow;
	   extent.y_min = this->x_5.y - extent_grow;
	   extent.y_max = this->x_5.y + extent_grow;
	   Rectangle4f_to_fg_bin4(&extent, &bin4);
	   /* draw in the centre */
	   /*printf("sprite_new_bins(%f, %f)\n", this->x.x, this->x.y);*/
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
			   if(!(bin = BinPoolNew(sprite_bins))) { perror("bins"); return; }
			   *bin = bins + bin2i_to_fg_bin(bin2i);
			   bin_to_Vec2i(bin2i_to_fg_bin(bin2i), &bin_pos);
			   /*fprintf(gnu_glob, "set arrow from %f,%f to %d,%d lw 0.2 "
				"lc rgb \"#CCEEEE\" front;\n",
				this->x_5.x, this->x_5.y, bin_pos.x, bin_pos.y);*/
		   }
	   }
   }
   
/** @implements <SpriteList *, SpriteAction *>DiAction */
   static void act_bins(struct SpriteList **const pthis, void *const pact_void) {
	   struct SpriteList *const this = *pthis;
	   const SpriteAction *const pact = pact_void, act = *pact;
	   assert(pthis && this && act);
	   SpriteListForEach(this, act);
   }
/** {{act} \in { Draw }}. */
   static void for_each_draw(SpriteAction act) {
	   BinPoolBiForEach(draw_bins, &act_bins, &act);
   }
/** {{act} \in { Update, Draw }}.
 @fixme Uhm, not sure these should be different, now that I've played with it.*/
   static void for_each_update(SpriteAction act) {
	   BinPoolBiForEach(update_bins, &act_bins, &act);
	   for_each_draw(act);
   }
/** Transfer one sprite from the {holding_bin} to one of the {bins} based on
 {x_5}; must be in {holding_bin}; \see{clear_holding_bin}. */
   static void clear_holding_sprite(struct Sprite *this) {
	   assert(this);
	   SpriteListRemove(holding_bin, this);
	   Vec2f_to_fg_bin(&this->x_5, &this->bin);
	   SpriteListPush(bins + this->bin, (struct SpriteListNode *)this);
   }
/** Transfer all Sprites from the spawning bin to their respective places. */
   static void clear_holding_bin(void) {
	   SpriteListForEach(holding_bin, &clear_holding_sprite);
   }
#endif
