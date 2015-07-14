/** Copyright 2014 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt */

#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* fprintf */
#include <string.h> /* strcmp */
#include "Bitmap.h"

/* Really simple; the hard part is precompiled in Bmp2h.

 @author	Neil
 @version	2, 2015-05
 @since		1, 2014 */

struct Bitmap {
	enum BitmapType type;
	int             width;
	int             height;
	int             depth;
	int             unit;
	unsigned char   *data;
};

/* public */

/** Constructor.
 @param name	The name of the bitmap.
 @param width
 @param height
 @param depth	The dimensions.
 @param bmp		The bitmap; must be width * height * depth amount of data.
 @return a Bitmap or a null pointer if the object couldn't be created */
struct Bitmap *Bitmap(const int width, const int height, const int depth, const unsigned char *bmp, const enum BitmapType type) {
	struct Bitmap *b;

	if(width <= 0 || height <= 0 || depth <= 0 || !bmp) {
		fprintf(stderr, "Bitmap: invalid.\n");
		return 0;
	}
	if(!(b = malloc(sizeof(struct Bitmap)))) {
		perror("Bitmap constructor");
		Bitmap_(&b);
		return 0;
	}
	b->type   = type;
	b->width  = width;
	b->height = height;
	b->depth  = depth;
	b->unit   = 0; /* zero can't be a valid texture id in OpenGl */
	b->data   = (unsigned char *)bmp;
	fprintf(stderr, "Bitmap: new %dx%dx%d, #%p.\n", width, height, depth, (void *)b);

	return b;
}

/** Destructor.
 @param b_ptr a reference to the object that is to be deleted */
void Bitmap_(struct Bitmap **b_ptr) {
	struct Bitmap *b;
	
	if(!b_ptr || !(b = *b_ptr)) return;
	fprintf(stderr, "~Bitmap: erase #%p.\n", (void *)b);
	free(b);
	*b_ptr = b = 0;
}

int BitmapGetWidth(const struct Bitmap *bmp) {
	if(!bmp) return 0;
	return bmp->width;
}

int BitmapGetHeight(const struct Bitmap *bmp) {
	if(!bmp) return 0;
	return bmp->height;
}

int BitmapGetDepth(const struct Bitmap *bmp) {
	if(!bmp) return 0;
	return bmp->depth;
}

unsigned char *BitmapGetData(const struct Bitmap *bmp) {
	if(!bmp) return 0;
	return bmp->data;
}

enum BitmapType BitmapGetType(const struct Bitmap *bmp) {
	if(!bmp) return B_DONT_KNOW;
	return bmp->type;
}

void BitmapSetImageUnit(struct Bitmap *bmp, const int unit) {
	if(!bmp) return;
	bmp->unit = unit;
}

int BitmapGetImageUnit(const struct Bitmap *bmp) {
	if(!bmp) return 0;
	return bmp->unit;
}
