#include <stdlib.h>	/* rand */
#include <stdio.h>  /* fprintf */
#include <time.h>	/* clock */

#define CLIP(c, a, z) ((c) <= (a) ? (a) : (c) >= (z) ? (z) : (c))

const float de_sitter = 8192.0f;
static const int max_size_pow  = 8;
static const int waypoint_half_size = 32;

/** Clips c to [min, max]. */
static int clip(const int c, const int min, const int max) {
	return c <= min ? min : c >= max ? max : c;
}

float r(const double max) {
	return ((double)rand() / (1.0 + RAND_MAX) - 0.5) * 2.0 * max;
}

static void test(FILE *data, FILE *gnu) {
	float x;
	int i, w, a;

	for(i = 0; i < 10000; i++) {
		x = r(de_sitter);
		w = (int)x >> max_size_pow;
		a = clip(w, -waypoint_half_size, waypoint_half_size - 1) + waypoint_half_size;
		fprintf(data, "%f\t%d\t%d\n", x, w, a);
	}

	fprintf(gnu, "set term postscript eps enhanced size 20cm, 10cm\n");
	fprintf(gnu, "set output \"Waypoint.eps\"\n");
	fprintf(gnu, "set palette defined (1 \"#00FF00\", 2 \"#0000FF\")\n");
	fprintf(gnu, "plot \"Waypoint.data\" using 1:3 title \"Waypoint\" with points\n");
}

int main(void) {
	FILE *data, *gnu;

	srand(clock());

	if(!(data = fopen("Waypoint.data", "w"))) {
		perror("Waypoint.data");
		return EXIT_FAILURE;
	}
	if(!(gnu = fopen("Waypoint.gnu", "w"))) {
		fclose(data);
		perror("Waypoint.data");
		return EXIT_FAILURE;
	}

	test(data, gnu);

	if(fclose(data) == EOF) perror("Waypoint.data");
	if(fclose(gnu) == EOF)  perror("Waypoint.gnu");

	if(system("gnuplot Waypoint.gnu")) return EXIT_FAILURE;
	if(system("open Waypoint.eps"))    return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
