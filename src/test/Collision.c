/** Copyright 2015 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt */

#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* fprintf */
#include <time.h>
#include <math.h>

/* 2-Body collision detection test.

 @author Neil
 @version 1
 @since 2015 */

static const float epsilon = 0.0005f;
static const float scale   = 128.0f;

/** Computes for two points the closest approach in a time frame given the
 points at the start and end for both points, then the distance, and if that
 distance is less then r, the collision time.
 @param a_x
 @param a_y
 @param b_x
 @param b_y		Point u moves from a to b.
 @param c_x
 @param c_y
 @param d_x
 @param d_y		Point v moves from c to d.
 @param r		The distance to test.
 @param d_ptr	Return the distance of closest approch.
 @param t_ptr	Return the time of closest approch; [0, 1], where 0 is the
				start of the interval and 1 is the end
 @param t0_ptr	If collision is true, sets this to the intersecting time.
 @return		If they collided. */
static int collision(const float a_x, const float a_y,
					 const float b_x, const float b_y,
					 const float c_x, const float c_y,
					 const float d_x, const float d_y,
					 const float r,
					 float *d_ptr, float *t_ptr, float *t0_ptr) {
	/* t \in [0,1]
	             u = b - a
	             v = d - c
	 if(v-u ~= 0) t = doesn't matter, parallel
	          p(t) = a + (b-a)t
	          q(t) = c + (d-c)t
	 distance(t)   = |q(t) - p(t)|
	 distance^2(t) = (q(t) - p(t))^2
	               = ((c+vt) - (a+ut))^2
	               = ((c-a) + (v-u)t)^2
	               = (v-u)*(v-u)t^2 + 2(c-a)*(v-u)t + (c-a)*(c-a)
	             0 = 2(v-u)*(v-u)t_min + 2(c-a)*(v-u)
	         t_min = -(c-a)*(v-u)/(v-u)*(v-u)
	 this is a linear optimisation, if t \notin [0,1] then pick the closest
	 if distance^2(t_min) < r^2 then we have a collision, which happened at t0,
	           r^2 = (v-u)*(v-u)t0^2 + 2(c-a)*(v-u)t0 + (c-a)*(c-a)
	            t0 = [-2(c-a)*(v-u) - sqrt((2(c-a)*(v-u))^2
	                 - 4((v-u)*(v-u))((c-a)*(c-a) - r^2))] / 2(v-u)*(v-u)
	            t0 = [-(c-a)*(v-u) - sqrt(((c-a)*(v-u))^2
	                 - ((v-u)*(v-u))((c-a)*(c-a) - r^2))] / (v-u)*(v-u) */

	const float u_x   = b_x - a_x, u_y  = b_y - a_y;
	const float v_x   = d_x - c_x, v_y  = d_y - c_y;
	const float vu_x  = v_x - u_x, vu_y = v_y - u_y; /* vu = v - u */
	const float vu_2  = vu_x * vu_x + vu_y * vu_y;
	const float ca_x  = c_x - a_x, ca_y = c_y - a_y;
	const float ca_vu = ca_x * vu_x + ca_y * vu_y;
	float       t, dist_x, dist_y, dist_2, ca_2, det;
	float       dist; /* not really necessary */

	if(vu_2 < epsilon) {
		/* we want the first contact */
		t = 0.0f;
	} else {
		t = -ca_vu / vu_2;
		if(     t < 0.0f) t = 0.0f;
		else if(t > 1.0f) t = 1.0f;
	}

	*t_ptr = t;

	/* distance(t_min) */

	dist_x = ca_x + vu_x * t;
	dist_y = ca_y + vu_y * t;
	dist_2 = dist_x * dist_x + dist_y * dist_y;
	dist   = sqrt(dist_2); /* just for show */

	*d_ptr = dist;

	/* not colliding */
	if(dist_2 >= r * r) return 0;

	ca_2  = ca_x * ca_x + ca_y * ca_y;
	det   = (ca_vu * ca_vu - vu_2 * (ca_2 - r * r));

	t = (det <= 0.0f) ? 0.0f : (-ca_vu - sqrt(det)) / vu_2;
	if(t < 0.0f) t = 0.0f; /* it can not be > 1 because dist < r^2 */

	*t0_ptr = t;

	return -1;
}

static void test(FILE *data, FILE *gnu) {
	float a_x, a_y, b_x, b_y, c_x, c_y, d_x, d_y;
	float p_x, p_y, q_x, q_y, dist_x, dist_y, dist;
	float a_radius = 16.0f, c_radius = 16.0f, radius = a_radius + c_radius;
	float d = 0, t0 = -1, t;
	int is;

	a_x = scale * (float)rand() / RAND_MAX;
	a_y = scale * (float)rand() / RAND_MAX;
	b_x = scale * (float)rand() / RAND_MAX;
	b_y = scale * (float)rand() / RAND_MAX;

	c_x = scale * (float)rand() / RAND_MAX;
	c_y = scale * (float)rand() / RAND_MAX;
	d_x = scale * (float)rand() / RAND_MAX;
	d_y = scale * (float)rand() / RAND_MAX;

	for(t = 0.0f; t <= 1.0f; t += 0.01f) {
		p_x = a_x + (b_x - a_x) * t;
		p_y = a_y + (b_y - a_y) * t;
		q_x = c_x + (d_x - c_x) * t;
		q_y = c_y + (d_y - c_y) * t;
		dist_x = q_x - p_x;
		dist_y = q_y - p_y;
		dist   = sqrt(dist_x * dist_x + dist_y * dist_y);
		fprintf(data, "%f\t%f\t%f\t%f\t%f\t%f\n", t, dist, p_x, p_y, q_x, q_y);
	}

	is = collision(a_x, a_y, b_x, b_y, c_x, c_y, d_x, d_y, radius, &d, &t, &t0);

	fprintf(gnu, "set term postscript eps enhanced size 20cm, 10cm\n");
	fprintf(gnu, "set output \"Collision.eps\"\n");
	fprintf(gnu, "set multiplot layout 1, 2\n");
	/*fprintf(gnu, "reset\n");*/
	fprintf(gnu, "set parametric\n");
	fprintf(gnu, "set colorbox user horizontal origin 0.32,0.385 size 0.18,0.035 front\n");
	fprintf(gnu, "set palette defined (1 \"#00FF00\", 2 \"#0000FF\")\n");
	fprintf(gnu, "set zlabel \"t\"\n");
	fprintf(gnu, "set vrange [0:1]\n");
	fprintf(gnu, "set xrange [0:%f]\n", scale);
	fprintf(gnu, "set yrange [0:%f]\n", scale);
	fprintf(gnu, "p_x(t) = %f + (%f - %f)*t\n", a_x, b_x, a_x);
	fprintf(gnu, "p_y(t) = %f + (%f - %f)*t\n", a_y, b_y, a_y);
	fprintf(gnu, "q_x(t) = %f + (%f - %f)*t\n", c_x, d_x, c_x);
	fprintf(gnu, "q_y(t) = %f + (%f - %f)*t\n", c_y, d_y, c_y);
	fprintf(gnu, "splot p_x(v), p_y(v), v title \"p(t) (%3.1f, %3.1f)->(%3.1f, %3.1f) radius %.0f\" lt palette, q_x(v), q_y(v), v title \"q(t) (%3.1f, %3.1f)->(%3.1f, %3.1f) radius %.0f\" lt palette lw 10\n", a_x, a_y, b_x, b_y, a_radius, c_x, c_y, d_x, d_y, c_radius);

	fprintf(gnu, "set xlabel \"t\"\n");
	fprintf(gnu, "set ylabel \"d\"\n");
	fprintf(gnu, "set xrange [0 : 1]\n");
	fprintf(gnu, "set yrange [-1 : *]\n");
	/*fprintf(gnu, "set label \"t = %f\" at %f, graph 0.05\n", t, t + 0.01f);*/
	fprintf(gnu, "set label \"d_{min} = %f\\nt_{min} = %f\\nt_0 = %f\" at graph 0.05, graph 0.10\n", d, t, t0);
	fprintf(gnu, "set arrow 1 from graph 0, first %f to graph 1, first %f nohead lc rgb \"blue\" lw 3\n", radius, radius);
	fprintf(gnu, "set arrow 2 from first %f, graph 0 to first %f, graph 1 nohead\n", t, t);
	if(t0 > 0.0f) fprintf(gnu, "set arrow 3 from first %f, graph 0 to first %f, graph 1 nohead\n", t0, t0);
	fprintf(gnu, "plot \"Collision.data\" using 1:2 title \"Distance |q(t) - p(t)|\" with linespoints\n");
}

/** Entry point.
 @param argc	The number of arguments, starting with the programme name.
 @param argv	The arguments.
 @return		Either EXIT_SUCCESS or EXIT_FAILURE. */
int main(int argc, char **argv) {
	FILE *data, *gnu;

	srand(clock());

	if(!(data = fopen("Collision.data", "w"))) {
		perror("Collision.data");
		return EXIT_FAILURE;
	}
	if(!(gnu = fopen("Collision.gnu", "w"))) {
		fclose(data);
		perror("Collision.data");
		return EXIT_FAILURE;
	}

	test(data, gnu);

	if(fclose(data) == EOF) perror("Collision.data");
	if(fclose(gnu) == EOF)  perror("Collision.gnu");

	if(system("gnuplot Collision.gnu")) return EXIT_FAILURE;
	if(system("open Collision.eps"))    return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
