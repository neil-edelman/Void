#include <stdlib.h> /* malloc, free */
#include <stdio.h>  /* fprintf */
#include <stdint.h> /* int* */
#include <string.h> /* memcpy */
#include <math.h>   /* ceil */
#include "Bitmap.h"

/* Loads an uncompressed 24 or 32 bpp bitmap. See
 {@url http://en.wikipedia.org/wiki/BMP_file_format}. Hasn't been tested, but
 works on the images GIMP creates; just don't go creating XRBG or something;
 it's delicate.

 @author	Neil
 @version	2.0, 2015-05
 @since		1.0, 2005? */

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

struct Buffer {
	size_t        size;
	unsigned char *data;
	size_t        cursor;
};

struct Bitmap {
	/*struct Header header;*/
	struct Info   info;
	unsigned char *buf;
	size_t        bufsize;
};

/* compression types */
enum Compress {
	BI_RGB       = 0,
	BI_RLE8      = 1,
	BI_RLE4      = 2,
	BI_BITFIELDS = 3,
	BI_JPEG      = 4,
	BI_PNG       = 5
};

/* private */
struct Bitmap *read_bitmap(struct Buffer *const buf);
struct Bitmap *BitmapFile(const char *const fn);
size_t read(struct Buffer *const buf, void *ptr, size_t n);
int seek(struct Buffer *const buf, const size_t offset);

#if 0
#include "Scorpion_bmp.h"

int main(void) {
	struct Bitmap *bmp = Bitmap(Scorpion_bmp, sizeof(Scorpion_bmp));
	bmp = BitmapFile("foo.bmp");
	bmp = BitmapFile("foo2.bmp");
	return EXIT_SUCCESS;
}
#endif

/* public */

/** Loads a bitmap from memory. */
struct Bitmap *Bitmap(const unsigned char *const data, const size_t size) {
	struct Buffer buf = { 0, 0, 0 };
	struct Bitmap *bmp;

	if(!data || !size) return 0;
	buf.size = size;
	buf.data = (unsigned char *)data;

	if(!(bmp = read_bitmap(&buf))) {
		Bitmap_(&bmp);
		return 0;
	}

	return bmp;
}

/** Loads a bitmap from a file.
 @param fn	File name.
 @return	Bitmap or null on error. */
struct Bitmap *BitmapFile(const char *const fn) {
	FILE *fp = 0;
	enum {
		NONE,
		PERROR,
		FILE,
		SYNTAX
	} err = NONE;
	struct Bitmap *bmp = 0;
	struct Buffer buf = { 0, 0, 0 };
	size_t size;
	unsigned char *temp = 0;

	if(!fn) return 0;

	/* try */ do {
		if(!(fp = fopen(fn, "r")))
			{ err = PERROR; break; }
		/* get the file size */
		fseek(fp, 0, SEEK_END);
		size = ftell(fp);
		rewind(fp);
		if(ferror(fp))
			{ err = FILE; break; }
		/* allocate */
		if(!(temp = malloc(size)))
			{ err = PERROR; break; }
		/*fprintf(stderr, "->alloc temp #%p\n", (void *)temp);*/
		/* read the whole file */
		if(fread(temp, sizeof(char), size, fp) < size)
			{ err = FILE; break; }
		buf.size = size;
		buf.data = temp;
		if(!(bmp = read_bitmap(&buf)))
			{ err = SYNTAX; break; }
		fprintf(stderr, "Bitmap: new <%s>, #%p.\n", fn, (void *)bmp);
	} while(0); /* catch */ if(err != NONE) {
		if(err == PERROR) perror(fn);
		else fprintf(stderr, "Bitmap: couldn't read file, \"%s.\"\n", fn);
	} /* finally */ {
		/*fprintf(stderr, "->dealloc temp #%p\n", (void *)temp);*/
		if(temp) free(temp);
		if(fp)   fclose(fp);
	}

	return bmp;
}

void Bitmap_(struct Bitmap **bitmapPtr) {
	struct Bitmap *bmp;
	if(!bitmapPtr || !(bmp = *bitmapPtr)) return;
	fprintf(stderr, "~Bitmap: erase, #%p.\n", (void *)bmp);
	/*if(bmp->buf) free(bmp->buf);
	bmp->buf = 0; <- part of malloc bmp */
	free(bmp); /* FIXME!!!!! 3-bit images raise this bullshit error,
				Loader(1116) malloc: *** error for object 0x100200a98:
				incorrect checksum for freed object - object was probably
				modified after being freed. (totally not anything anyone has
				ever referenced in the programme)
				*** set a breakpoint in malloc_error_break to debug
				Abort trap (totally incomprehesable, why would the depth have
				anything to do with it?)(no, it's not the 3bit, it's the file
				itself; if I duplicate the exact contents, it's good, rename it,
				doesn't work, unless I name it something lower in the alphbet) */
	*bitmapPtr = bmp = 0;
}

int BitmapGetWidth(const struct Bitmap *const bmp) {
	if(!bmp) return 0;
	return bmp->info.width;
}

int BitmapGetHeight(const struct Bitmap *const bmp) {
	int height;
	if(!bmp) return 0;
	height = bmp->info.height;
	if(height < 0) height = -height;
	return height;
}

int BitmapGetDepth(const struct Bitmap *const bmp) {
	if(bmp->info.bpp == 24) return 3;
	if(bmp->info.bpp == 32) return 4;
	else return 0; /* never */
}

unsigned char *BitmapGetData(const struct Bitmap *const bmp) {
	if(!bmp) return 0;
	return bmp->buf;
}

/* private */

struct Bitmap *read_bitmap(struct Buffer *const buf) {
	struct Bitmap *bmp;
	struct Header header;
	struct Info   info;
	uint32_t      dib_header;
	size_t bytes, stride, bytes_per_pixel;
	int is_inverted;
	char magic[2];
	unsigned i, how_much, height, line;

	if(!buf) return 0;

	/* magic */
	if((i = read(buf, magic, sizeof(char) * 2)) != 2) {
		perror("bitmap");
		return 0;
	}
	if(magic[0] != 'B' || magic[1] != 'M') {
		fprintf(stderr, "Expeced 'BM,' unsuppored format, '%c%c'.\n", magic[0], magic[1]);
		return 0;
	}
	/* file header */
	if(read(buf, &header, sizeof(struct Header)) != sizeof(struct Header)) {
		perror("bitmap");
		return 0;
	}
	if(header.offset < 2 + sizeof(struct Header) + sizeof(uint32_t) + sizeof(struct Info)) {
		fprintf(stderr, "Offset reported %d doesn't make sense.\n", header.offset);
		return 0;
	}
	/* DIB Header */
	if(read(buf, &dib_header, sizeof(uint32_t)) != sizeof(uint32_t)) {
		perror("bitmap");
		return 0;
	}
	if(dib_header != 40) {
		fprintf(stderr, "DIB Header 40, bitmap not supported %d.\n", dib_header);
		return 0;
	}
	/* bitmap info header */
	if(read(buf, &info, sizeof(struct Info)) != sizeof(struct Info)) {
		perror("bitmap");
		return 0;
	}
	/* fixme: this is pathetic -- took it out, but way harder loading
	if((info.width & 3) != 0 || info.width <= 0) {
		fprintf(stderr, "Width is %d (must be a multible of 4, positive.)\n", info.width);
		return 0;
	}*/
	if(info.width <= 0) {
		fprintf(stderr, "Width is negative, %d.\n", info.width);
		return 0;
	}
	if(info.nplanes != 1) {
		fprintf(stderr, "Bitmap must be 1 plane, is %d.\n", info.nplanes);
		return 0;
	}
	if(info.bpp == 24) {
		bytes_per_pixel = 3;
		fprintf(stderr, "B8 G8 R8\n");
	} else if(info.bpp == 32) {
		bytes_per_pixel = 4;
		fprintf(stderr, "B8 G8 R8 A8\n");
	} else {
		fprintf(stderr, "%d bpp, must be 24 or 32.\n", info.bpp);
		return 0;
	}
	if((enum Compress)info.compress != BI_RGB) {
		fprintf(stderr, "Cannot read compression %d.\n", info.compress);
		return 0;
	}
	/* \/ if(fseek(fp, bmp->head.offset, SEEK_SET)) return 0;*/
	if(!seek(buf, header.offset)) {
		fprintf(stderr, "Header offset %u; position outside the file.\n", header.offset);
		return 0;
	}
	/* we create the struct */
	height      = (unsigned)(info.height > 0 ? info.height : -info.height);
	bytes       = info.width * height * bytes_per_pixel;
	stride      = (int)ceil(info.width * info.bpp / 32.0) * 4;
	line        = info.width * bytes_per_pixel;
	is_inverted = info.height > 0 ? 0 : -1;
	if(!(bmp = malloc(sizeof(struct Bitmap) + sizeof(char) * bytes))) {
		perror("Bitmap loader");
		return 0;
	}
	/*memcpy(&bmp->header, &header, sizeof(struct Header));*/
	memcpy(&bmp->info,   &info,   sizeof(struct Info));
	bmp->buf     = (unsigned char *)(bmp + 1);
	bmp->bufsize = bytes;

	/* copy the image; bmps and opengl are in the same order, except when the
	 height is negative */
	/*printf("bytes %u: stride %u, line %u, height %u (%d: %d)\n", bytes, (unsigned)stride, line, height, info.height, is_inverted);*/
	for(i = 0; i < height; i++) {
		if(!seek(buf,
		   header.offset + (is_inverted ? height - i - 1 : i) * stride)
		   || (how_much = read(buf, &bmp->buf[stride * i], line)) != line) {
			free(bmp);
			fprintf(stderr, "Error reading bitmap.\n");
			return 0;
		}
	}
	/*if((how_much = read(buf, bmp->buf, sizeof(char) * bytes)) != bytes) {
		fprintf(stderr, "Ran out of data at %u.\n", how_much);
		free(bmp);
		return 0;
	}*/
	/* useless BGR[A] to RGB[A] */
	for(i = 0; i < bytes; i += bytes_per_pixel) {
		bmp->buf[i]   ^= bmp->buf[i+2];
		bmp->buf[i+2] ^= bmp->buf[i];
		bmp->buf[i]   ^= bmp->buf[i+2];
	}
	fprintf(stderr, "Read %dx%dx%u#%p, #%p.\n", bmp->info.width, bmp->info.height, (unsigned)bytes_per_pixel, bmp->buf, (void *)bmp);
	return bmp;
}

/** Reads from nbytes from bmp at *mem_ptr and stores them at ptr.
 @return	The number of bytes actually read. */
size_t read(struct Buffer *const buf, void *ptr, size_t n) {
	size_t left;
	if(!buf || !ptr || !n || !(left = buf->size - buf->cursor)) return 0;
	if(n > left) n = left;
	memcpy(ptr, &buf->data[buf->cursor], n);
	buf->cursor += n;
	return n;
}

/** @return		True if success. */
int seek(struct Buffer *const buf, const size_t offset) {
	if(!buf || offset >= buf->size) return 0;
	buf->cursor = offset;
	return -1;
}
