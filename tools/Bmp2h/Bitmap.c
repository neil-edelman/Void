/* Loads a 24 or 32 bpp bitmap. -Neil */

#include <stdlib.h> /* malloc, free */
#include <stdio.h>  /* fprintf */
#include <stdint.h> /* int* */
#include "Bitmap.h"

/* http://en.wikipedia.org/wiki/BMP_file_format */

/* bitmap format: */
/* -> magic 'BM' (we'll ignore other magiks) */
struct Header {
	uint32_t size;
	uint16_t creator1;
	uint16_t creator2;
	uint32_t offset;
};
/* -> uint32_t 40 (we'll ignore other formats) */
struct Info {
	int32_t  width;
	int32_t  height;
	uint16_t nplanes;
	uint16_t bpp;
	uint32_t compress;
	uint32_t bmp_byte;
	int32_t  hres;
	int32_t  vres;
	uint32_t ncolors;
	uint32_t nimpcolors;
};

struct Bitmap {
	struct Header head;
	struct Info   info;
	int          alpha;
	unsigned char *buf;
};

/* compression types */
enum Compress {
	BI_RGB = 0,
	BI_RLE8 = 1,
	BI_RLE4 = 2,
	BI_BITFIELDS = 3,
	BI_JPEG = 4,
	BI_PNG = 5
};

/* private */
int read_bitmap(struct Bitmap *bmp, FILE *fp);

/* public */

struct Bitmap *Bitmap(const char *fn) {
	struct Bitmap *bmp;
	FILE *fp;

	if(!fn) return 0;
	if(!(bmp = malloc(sizeof(struct Bitmap)))) {
		perror("Bitmap constructor");
		Bitmap_(&bmp);
		return 0;
	}
	bmp->buf = 0;
	fprintf(stderr, "Bitmap: new <%s>, #%p; ", fn, (void *)bmp);
	if(!(fp = fopen(fn, "rb"))) {
		perror(fn);
		Bitmap_(&bmp);
		return 0;
	}
	if(!read_bitmap(bmp, fp)) {
		fclose(fp);
		Bitmap_(&bmp);
		return 0;
	}
	fclose(fp);

	return bmp;
}

void Bitmap_(struct Bitmap **bitmapPtr) {
	struct Bitmap *bmp;
	if(!bitmapPtr || !(bmp = *bitmapPtr)) return;
	fprintf(stderr, "~Bitmap: erase, #%p.\n", (void *)bmp);
	if(bmp->buf) free(bmp->buf);
	bmp->buf = 0;
	free(bmp);
	*bitmapPtr = bmp = 0;
}

int BitmapGetWidth(const struct Bitmap *bmp) {
	if(!bmp) return 0;
	return bmp->info.width;
}

int BitmapGetHeight(const struct Bitmap *bmp) {
	if(!bmp) return 0;
	return bmp->info.height;
}

int BitmapGetDepth(const struct Bitmap *bmp) {
	if(bmp->info.bpp == 24) return 3;
	if(bmp->info.bpp == 32) return 4;
	else return 0; /* never */
}

unsigned char *BitmapGetData(const struct Bitmap *bmp) {
	if(!bmp) return 0;
	return bmp->buf;
}

/* private */

int read_bitmap(struct Bitmap *bmp, FILE *fp) {
	uint32_t dibHeader;
	size_t bytes, bytes_per_pixel;
	char magic[2];
	int i;

	/* magic */
	if(fread(magic, sizeof(char), 2, fp) != 2) {
		perror("bitmap");
		return 0;
	}
	if(magic[0] != 'B' || magic[1] != 'M') {
		fprintf(stderr, "expeced 'BM,' unsuppored format, '%c%c'.\n", magic[0], magic[1]);
		return 0;
	}
	/* file header */
	if(fread(&bmp->head, sizeof(struct Header), 1, fp) != 1) {
		perror("bitmap");
		return 0;
	}
	if(bmp->head.offset < 2 + sizeof(struct Header) + sizeof(uint32_t) + sizeof(struct Info)) {
		fprintf(stderr, "offset reported %d doesn't make sense.\n", bmp->head.offset);
		return 0;
	}
	/* DIB Header */
	if(fread(&dibHeader, sizeof(uint32_t), 1, fp) != 1) {
		perror("bitmap");
		return 0;
	}
	if(dibHeader != 40) {
		fprintf(stderr, "DIB Header 40, bitmap not supported %d.\n", dibHeader);
		return 0;
	}
	/* bitmap info header */
	if(fread(&bmp->info, sizeof(struct Info), 1, fp) != 1) {
		perror("bitmap");
		return 0;
	}
	if((bmp->info.width & 3) != 0 || bmp->info.width <= 0) {
		fprintf(stderr, "width is %d (must be a multible of 4, positive.)\n", bmp->info.width);
		return 0;
	}
	/* fixme! also, padding! */
	if(bmp->info.height < 0) {
		bmp->info.height = -bmp->info.height;
	} else {
		fprintf(stderr, "flipped; ");
	}
	if(bmp->info.nplanes != 1) {
		fprintf(stderr, "bitmap must be 1 plane, is %d.\n", bmp->info.nplanes);
		return 0;
	}
	if(bmp->info.bpp == 24) {
		bytes_per_pixel = 3;
		fprintf(stderr, "B8 G8 R8; ");
	} else if(bmp->info.bpp == 32) {
		bytes_per_pixel = 4;
		fprintf(stderr, "B8 G8 R8 A8; ");
	} else {
		fprintf(stderr, "%d bpp, must be 24 or 32.\n", bmp->info.bpp);
		return 0;
	}
	if((enum Compress)bmp->info.compress != BI_RGB) {
		fprintf(stderr, "cannot read compression %d.\n", bmp->info.compress);
		return 0;
	}
	if(fseek(fp, bmp->head.offset, SEEK_SET)) return 0;
	bytes = bmp->info.width * bmp->info.height * bytes_per_pixel;
	if(!(bmp->buf = malloc(sizeof(char) * bytes))) {
		perror("Bitmap loader");
		return 0;
	}
	if(fread(bmp->buf, sizeof(char), bytes, fp) != bytes) {
		fprintf(stderr, "ran out of data.\n");
		return 0;
	}
	fprintf(stderr, "read %dx%d, #%p.\n", bmp->info.width, bmp->info.height, bmp->buf);
	/* useless BGR[A] to RGB[A] */
	for(i = 0; i < bytes; i += bytes_per_pixel) {
		bmp->buf[i] ^= bmp->buf[i+2];
		bmp->buf[i+2] ^= bmp->buf[i];
		bmp->buf[i] ^= bmp->buf[i+2];
	}
	return -1;
}
