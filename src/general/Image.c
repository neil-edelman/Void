/** Copyright 2014 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt */

#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* fprintf */
#include <string.h> /* strcmp */
#include "Image.h"

/* Really simple; the hard part is precompiled in Bmp2h.

 @author	Neil
 @version	2, 2015-05
 @since		1, 2014 */

struct Image {
	int             width;
	int             height;
	int             depth;
	int             texture;
	unsigned char   *data;
};

/* public */

/** Constructor.
 @param width
 @param height
 @param depth	The dimensions.
 @param img		The bitmap; must be width * height * depth amount of data.
 @return a Image or a null pointer if the object couldn't be created */
struct Image *Image(const int width, const int height, const int depth, const unsigned char *bmp) {
	struct Image *img;

	if(width <= 0 || height <= 0 || depth <= 0 || !bmp) {
		fprintf(stderr, "Image: invalid.\n");
		return 0;
	}
	if(!(img = malloc(sizeof(struct Image)))) {
		perror("Image constructor");
		Image_(&img);
		return 0;
	}
	img->width  = width;
	img->height = height;
	img->depth  = depth;
	img->texture= 0; /* zero can't be a valid texture id in OpenGl */
	img->data   = (unsigned char *)bmp;
	fprintf(stderr, "Image: new %dx%dx%d, #%p.\n", width, height, depth, (void *)img);

	return img;
}

/** Destructor.
 @param b_ptr a reference to the object that is to be deleted */
void Image_(struct Image **i_ptr) {
	struct Image *img;
	
	if(!i_ptr || !(img = *i_ptr)) return;
	fprintf(stderr, "~Image: erase #%p.\n", (void *)img);
	free(img);
	*i_ptr = img = 0;
}

int ImageGetWidth(const struct Image *img) {
	if(!img) return 0;
	return img->width;
}

int ImageGetHeight(const struct Image *img) {
	if(!img) return 0;
	return img->height;
}

int ImageGetDepth(const struct Image *img) {
	if(!img) return 0;
	return img->depth;
}

unsigned char *ImageGetData(const struct Image *img) {
	if(!img) return 0;
	return img->data;
}

/** Image has no association with OpenGl, so we have to set it manually. */
void ImageSetTexture(struct Image *img, const int texture) {
	if(!img) return;
	img->texture = texture;
}

int ImageGetTexture(const struct Image *img) {
	if(!img) return 0;
	return img->texture;
}

/** Prints out the red channel in text format for debugging purposes; please
 don't call it on large images!
 @param img		The image.
 @param fp		File pointer where you want the image to go; eg, stdout. */
void ImagePrint(const struct Image *img, FILE *const fp) {
	int x, y;

	if(!img) fprintf(fp, "0\n");
	for(y = 0; y < img->height; y++) {
		for(x = 0; x < img->width; x++) {
			/* red */
			fprintf(fp, "%1.1d", img->data[(y * img->width + x) * img->depth] >> 5);
		}
		fprintf(fp, "\n");
	}
}
