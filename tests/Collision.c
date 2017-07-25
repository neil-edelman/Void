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
static const float dt_ms   = 25.0f;

struct Vec2f { float x, y; };

struct Collide {
	struct Vec2f x, v;
	float r;
};

/** Computes for two {Collide} the closest approach, distance and time, and
 whether they intersect.
 @param d_ptr: Return the distance of closest approach.
 @param t_ptr: Return the time of closest approach.
 @param t0_ptr: If collision is true, sets this to the intersecting time.
 @return If they collided. */
static int collision(const struct Collide *const a,
	const struct Collide *const b, float *d_ptr, float *t_ptr, float *t0_ptr) {
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

	struct Vec2f v, z, dist;
	float t, v2, vz, z2, dist_2, det;
	const float r = a->r + b->r;
	v.x = a->v.x - b->v.x, v.y = a->v.y - b->v.y;
	z.x = a->x.x - b->x.x, z.y = a->x.y - b->x.y;
	v2 = v.x * v.x + v.y * v.y;
	vz = v.x * z.x + v.y * z.y;

	/* Time of closest approach. */
	if(v2 < 0.0005f) {
		t = 0.0f;
	} else {
		t = -vz / v2;
		if(     t < 0.0f)  t = 0.0f;
		else if(t > dt_ms) t = dt_ms;
	}
	*t_ptr = t;

	/* distance(t_min) */
	dist.x = z.x + v.x * t, dist.y = z.y + v.y * t;
	dist_2 = dist.x * dist.x + dist.y * dist.y;
	*d_ptr = sqrtf(dist_2);

	/* not colliding */
	if(dist_2 >= r * r) return 0;

	z2 = z.x * z.x + z.y * z.y;
	det = (vz * vz - v2 * (z2 - r * r));

	t = (det <= 0.0f) ? 0.0f : (-vz - sqrtf(det)) / v2;
	if(t < 0.0f) t = 0.0f; /* bounded, dist < r^2 */

	*t0_ptr = t;

	return -1;
}

static void Collide_filler(struct Collide *const this) {
	const struct Vec2f target = {
		scale * rand() / (RAND_MAX + 1.0f),
		scale * rand() / (RAND_MAX + 1.0f)
	};
	this->x.x = scale * rand() / (RAND_MAX + 1.0f);
	this->x.y = scale * rand() / (RAND_MAX + 1.0f);
	this->v.x = (target.x - this->x.x) / dt_ms;
	this->v.y = (target.y - this->x.y) / dt_ms;
	this->r = 16.0f;
}

static void test(FILE *data, FILE *gnu) {
	struct Collide a, b;
	struct Vec2f p, q, x;
	float d, t, t0 = -1, r;
	int is_collision;

	Collide_filler(&a);
	Collide_filler(&b);

	for(t = 0.0f; t <= dt_ms; t += 0.25f) {
		p.x = a.x.x + a.v.x * t, p.y = a.x.y + a.v.y * t;
		q.x = b.x.x + b.v.x * t, q.y = b.x.y + b.v.y * t;
		x.x = q.x - p.x, x.y = q.y - p.y;
		d   = sqrtf(x.x * x.x + x.y * x.y);
		fprintf(data, "%f\t%f\t%f\t%f\t%f\t%f\n", t, d, p.x, p.y, q.x, q.y);
	}

	is_collision = collision(&a, &b, &d, &t, &t0);
	r = a.r + b.r;

	fprintf(gnu, "set term postscript eps enhanced size 20cm, 10cm\n");
	fprintf(gnu, "set output \"Collision.eps\"\n");
	fprintf(gnu, "set multiplot layout 1, 2\n");
	fprintf(gnu, "set parametric\n");
	fprintf(gnu, "set colorbox user horizontal origin 0.32,0.385 size 0.18,0.035 front\n");
	fprintf(gnu, "set palette defined (1 \"#00FF00\", 2 \"#0000FF\")\n");
	fprintf(gnu, "set zlabel \"t\"\n");
	fprintf(gnu, "set vrange [0:%f]\n", dt_ms);
	fprintf(gnu, "set xrange [0:%f]\n", scale);
	fprintf(gnu, "set yrange [0:%f]\n", scale);
	fprintf(gnu, "p_x(t) = %f + %f*t\n", a.x.x, a.v.x);
	fprintf(gnu, "p_y(t) = %f + %f*t\n", a.x.y, a.v.y);
	fprintf(gnu, "q_x(t) = %f + %f*t\n", b.x.x, b.v.x);
	fprintf(gnu, "q_y(t) = %f + %f*t\n", b.x.y, b.v.y);
	fprintf(gnu, "splot p_x(v), p_y(v), v title \"p(t) (%3.1f, %3.1f)->(%3.1f, %3.1f) radius %.0f\" lt palette, q_x(v), q_y(v), v title \"q(t) (%3.1f, %3.1f)->(%3.1f, %3.1f) radius %.0f\" lt palette lw 10\n", a.x.x, a.x.y, a.v.x, a.v.y, a.r, b.x.x, b.x.y, b.v.x, b.v.y, b.r);

	fprintf(gnu, "set xlabel \"t\"\n");
	fprintf(gnu, "set ylabel \"d\"\n");
	fprintf(gnu, "set xrange [0 : %f]\n", dt_ms);
	fprintf(gnu, "set yrange [-1 : *]\n");
	/*fprintf(gnu, "set label \"t = %f\" at %f, graph 0.05\n", t, t + 0.01f);*/
	fprintf(gnu, "set label \"d_{min} = %f\\nt_{min} = %f\\nt_0 = %f\" at graph 0.05, graph 0.10\n", d, t, t0);
	fprintf(gnu, "set arrow 1 from graph 0, first %f to graph 1, first %f nohead lc rgb \"blue\" lw 3\n", r, r);
	fprintf(gnu, "set arrow 2 from first %f, graph 0 to first %f, graph 1 nohead\n", t, t);
	if(t0 > 0.0f) fprintf(gnu, "set arrow 3 from first %f, graph 0 to first %f, graph 1 nohead\n", t0, t0);
	fprintf(gnu, "plot \"Collision.data\" using 1:2 title \"Distance |q(t) - p(t)|\" with linespoints\n");
}

/** Entry point.
 @return		Either EXIT_SUCCESS or EXIT_FAILURE. */
int main(void) {
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
