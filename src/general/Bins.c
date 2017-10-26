/** 2017 Neil Edelman, distributed under the terms of the GNU General
 Public License 3, see copying.txt, or
 \url{ https://opensource.org/licenses/GPL-3.0 }.

 A {Bin} is like a 2D hash bucket for space. Several {Bins} {Sprite}'s can be
 iterated over.

 @title		Bins
 @author	Neil
 @std		C89/90
 @version	2017-10 Broke off from Sprites.
 @since		2017-05 Generics.
			2016-01
			2015-06 */

#include <stdio.h> /* stderr */
#include "../general/OrthoMath.h" /* Rectangle4f, etc */
#include "Bins.h"



/***************** Declare types. ****************/

struct SpriteList;
#define POOL_NAME SpriteBin
#define POOL_TYPE struct SpriteList *
#include "../templates/Pool.h"

struct FarList;
#define POOL_NAME FarBin
#define POOL_TYPE struct FarList *
#include "../templates/Pool.h"

/** Alias. */
struct SpriteBins {
	struct SpriteBinPool pool;
};

/** Alias. */
struct FarBins { struct FarBinPool pool; };

void SpriteBins_(struct SpriteBins **const pthis) {
	struct SpriteBins *this;
	struct SpriteBinPool *pool;
	if(!pthis || !(this = *pthis)) return;
	pool = &this->pool;
	SpriteBinPool_(&pool);
	this = *pthis = 0;
}

void FarBins_(struct FarBins **const pthis) {
	struct FarBins *this;
	struct FarBinPool *pool;
	if(!pthis || !(this = *pthis)) return;
	pool = &this->pool;
	FarBinPool_(&pool);
	this = *pthis = 0;
}

struct SpriteBins *SpriteBins(void) {
	struct SpriteBins *this = (struct SpriteBins *)SpriteBinPool(0, 0);
	if(!this) fprintf(stderr, "SpriteBins: %s.\n", SpriteBinPoolGetError(0));
	return this;
}

struct FarBins *FarBins(void) {
	struct FarBins *this = (struct FarBins *)FarBinPool(0, 0);
	if(!this) fprintf(stderr, "FarBins: %s.\n", FarBinPoolGetError(0));
	return this;
}

void SpriteBinsClear(struct SpriteBins *const this) {
	if(!this) return;
	SpriteBinPoolClear(&this->pool);
}

void FarBinsClear(struct FarBins *const this) {
	if(!this) return;
	FarBinPoolClear(&this->pool);
}

/** @return Success. */
int SpriteBinsAdd(struct SpriteBins *const this,
	struct SpriteList **const bins, struct Rectangle4f *const rect) {
	struct SpriteBinPool *pool;
	struct SpriteList **bin;
	struct Rectangle4i bin4;
	struct Vec2i bin2i;
	if(!this || !rect) return 0;
	pool = &this->pool;
	Rectangle4f_to_fg_bin4(rect, &bin4);
	for(bin2i.y = bin4.y_max; bin2i.y >= bin4.y_min; bin2i.y--) {
		for(bin2i.x = bin4.x_min; bin2i.x <= bin4.x_max; bin2i.x++) {
			/*printf("sprite bin2i(%u, %u)\n", bin2i.x, bin2i.y);*/
			if(!(bin = SpriteBinPoolNew(pool))) { fprintf(stderr,
				"SpriteBinsAdd: %s.\n", SpriteBinPoolGetError(pool)); return 0;}
			bin = bins + bin2i_to_fg_bin(bin2i);
			/*bin_to_Vec2i(bin2i_to_fg_bin(bin2i), &bin_pos);
			fprintf(gnu_glob, "set arrow from %f,%f to %d,%d lw 0.2 "
			 "lc rgb \"#CCEEEE\" front;\n",
			 this->x_5.x, this->x_5.y, bin_pos.x, bin_pos.y);*/
		}
	}
	return 1;
}

/** @return One of the {SpriteList *} bins or null if there are no bins left. */
const struct SpriteList *SpriteBinsTake(struct SpriteBins *const this) {
	struct SpriteBinPool *pool;
	struct SpriteList **entry;
	if(!this) return 0;
	pool = &this->pool;
	entry = SpriteBinPoolElement(pool);
	SpriteBinPoolRemove(pool, entry);
	if(!entry) return 0;
	return *entry;
}
/* fixme:
 while((bin = SpriteBinsTake(visible))) SpriteListForEach(bin, &draw); */


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
