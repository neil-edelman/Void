#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* fprintf */
#include "Asteroid_png.h"
#include "Pluto_jpeg.h"
#include "../../../fileformat/lodepng.h"
#include "../../../fileformat/nanojpeg.h"

void pimage(unsigned char const*const img, const unsigned width, const unsigned height, const unsigned depth);

int main(int argc, char *argv[]) {
	unsigned error;
	unsigned char* image;
	unsigned width, height;
	/*const char* filename = "Asteroid.png";
	
	error = lodepng_decode32_file(&image, &width, &height, filename);

	if(error) printf("decoder error %u: %s\n", error, lodepng_error_text(error));
	printf("(%u/%u)\n", width, height);
	pimage(image, width, height);

	free(image);*/

	error = lodepng_decode32(&image, &width, &height,
							 Asteroid_png, sizeof(Asteroid_png) / sizeof(char));

	if(error) {
		printf("decoder error %u: %s\n", error, lodepng_error_text(error));
		return EXIT_FAILURE;
	}
	printf("lodepng: %u:%u\n", width, height);
	pimage(image, width, height, 4);

	free(image);

	/*njInit(); <- this does nothing */
	error = njDecode(Pluto_jpeg, sizeof(Pluto_jpeg) / sizeof(char));
	if(error) {
		printf("Error decoding the file.\n");
        return EXIT_FAILURE;
	}
	pimage(njGetImage(), njGetWidth(), njGetHeight(), 3);
	njDone();

	return EXIT_SUCCESS;
}

void pimage(unsigned char const*const img, const unsigned width, const unsigned height, const unsigned depth) {
	int x, y;
	
	if(!img) {
		printf("0\n");
		return;
	}
	for(y = 0; y < height; y++) {
		for(x = 0; x < width; x++) {
			/* red */
			printf("%1.1d", img[(y * width + x) * depth] >> 5);
		}
		printf("\n");
	}
}
