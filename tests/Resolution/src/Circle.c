/** Copyright 2015 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt */

#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* fprintf */
#include <time.h>
#include <math.h>
#include <assert.h>

/* 2-Body collision detection test.

 @author Neil
 @version 1
 @since 2015 */

static const float epsilon = 0.0005f;
static const float scale   = 128.0f;
static const float dt_ms   = 25.0f;

struct Vec2f { float x, y; };
struct Collision { unsigned no; struct Vec2f v; float t; };

struct Circle {
	struct Vec2f x, v;
	float r, m;
	struct Collision *collision;
};

/** Add a collision to the sprite; called from collision handlers. */
static void add_bounce(struct Circle *const this, const struct Vec2f v,
	const float t) {
	static struct Collision collisions[8];
	static unsigned collisions_i;
	struct Collision *col;
	assert(this);
	if((col = this->collision)) {
		/* Average the vectors for multiple collisions -- not really physical,
		 but fast and not often seen. */
		col->no++;
		col->v.x += (v.x - col->v.x) / col->no;
		col->v.y += (v.y - col->v.y) / col->no;
		if(t < col->t) col->t = t;
	} else {
		/* New collision. */
		assert(collisions_i < sizeof collisions / sizeof *collisions);
		col = collisions + collisions_i++;
		col->no  = 1;
		col->v.x = v.x;
		col->v.y = v.y;
		col->t   = t;
		this->collision = col;
	}
}

/** Finds the eigenvector of the difference between {a} and {b} at {t} into the
 frame, normalised. */
static struct Vec2f eigenvector_hat(struct Circle *const a,
	struct Circle *const b, const float t) {
	struct Vec2f e_hat, u, v, d;
	float e_mag;
	assert(a && b && t >= 0);
	/* {u} {v} are the positions of {a} {b} extrapolated to impact. */
	u.x = a->x.x + a->v.x * t, u.y = a->x.y + a->v.y * t;
	v.x = b->x.x + b->v.x * t, v.y = b->x.y + b->v.y * t;
	/* {d} difference between; use to set up a unitary frame, {d_hat}. */
	d.x = v.x - u.x, d.y = v.y - u.y;
	if((e_mag = sqrtf(d.x * d.x + d.y * d.y)) < epsilon) {
		e_hat.x = 1.0f, e_hat.y = 0.0f;
	} else {
		e_hat.x = d.x / e_mag, e_hat.y = d.y / e_mag;
	}
	return e_hat;
}

/** Apply degeneracy pressure. */
static void degeneracy_pressure(struct Circle *const a,
	struct Circle *const b) {
	struct Vec2f z, z_hat;
	float z_mag, push;
	const float r = a->r + b->r;
	assert(a && b);
	z.x = b->x.x - a->x.x, z.y = b->x.y - a->x.y;
	z_mag = sqrtf(z.x * z.x + z.y * z.y);
	push = (r - z_mag) * 0.501f;
	if(z_mag < epsilon) {
		z_hat.x = 1.0f, z_hat.y = 0.0f;
	} else {
		z_hat.x = z.x / z_mag, z_hat.y = z.y / z_mag;
	}
	printf("Degeneracy pressure pushing circles %.1f apart.\n", push);
	a->x.x -= z_hat.x * push, a->x.y -= z_hat.y * push;
	b->x.x += z_hat.x * push, b->x.y += z_hat.y * push;
}

/** Elastic collision between circles; called from \see{collision_matrix}. 
 Degeneracy pressure pushes sprites to avoid interpenetration.
 @param t: {ms} after frame that the collision occurs.
 @implements SpriteCollision */
static void elastic_bounce(struct Circle *const a, struct Circle *const b,
	const float t) {
	const struct Vec2f e_hat = eigenvector_hat(a, b, t);
	struct Vec2f a_v, b_v;
	/* All mass is strictly positive. */
	const float a_m = a->m, b_m = b->m;
	const float diff_m = a_m - b_m, invsum_m = 1.0f / (a_m + b_m);
	/* Elastic collision. */
	struct Vec2f a_v_nrm, b_v_nrm;
	const float a_nrm_s = a->v.x * e_hat.x + a->v.y * e_hat.y;
	const float b_nrm_s = b->v.x * e_hat.x + b->v.y * e_hat.y;
	a_v_nrm.x = a_nrm_s * e_hat.x, a_v_nrm.y = a_nrm_s * e_hat.y;
	b_v_nrm.x = b_nrm_s * e_hat.x, b_v_nrm.y = b_nrm_s * e_hat.y;
	a_v.x = a->v.x - a_v_nrm.x
		+ (a_v_nrm.x * diff_m + 2 * b_m * b_v_nrm.x) * invsum_m,
	a_v.y = a->v.y - a_v_nrm.y
		+ (a_v_nrm.y * diff_m + 2 * b_m * b_v_nrm.y) * invsum_m;
	b_v.x = b->v.x - b_v_nrm.x
		- (b_v_nrm.x * diff_m - 2 * a_m * a_v_nrm.x) * invsum_m,
	b_v.y = b->v.y - b_v_nrm.y
		- (b_v_nrm.y * diff_m - 2 * a_m * a_v_nrm.y) * invsum_m;
	/* Record. */
	add_bounce(a, a_v, t);
	add_bounce(b, b_v, t);
}

static void bounce_a(struct Circle *const a, struct Circle *const b,
	const float t) {
	const struct Vec2f e_hat = eigenvector_hat(a, b, t);
	struct Vec2f a_v;
	/* Bounce {a}. */
	const float a_nrm_s = a->v.x * e_hat.x + a->v.y * e_hat.y;
	struct Vec2f a_v_nrm;
	a_v_nrm.x = a_nrm_s * e_hat.x, a_v_nrm.y = a_nrm_s * e_hat.y;
	/* fixme: Prove. */
	a_v.x = 2.0f * b->v.x - a_v_nrm.x,
	a_v.y = 2.0f * b->v.y - a_v_nrm.y;
	/* Record. */
	add_bounce(a, a_v, t);
}

/** Computes for two {Collide} the closest approach, distance and time, and
 whether they intersect.
 @param d_ptr: Return the distance of closest approach.
 @param t_ptr: Return the time of closest approach.
 @param t0_ptr: If collision is true, sets this to the intersecting time.
 @return If they collided. */
static int collision(struct Circle *const a,
	struct Circle *const b, float *t0_ptr) {
	struct Vec2f v, z;
	float t, v2, vz, z2, disc;
	const float r = a->r + b->r;
	v.x = b->v.x - a->v.x, v.y = b->v.y - a->v.y; /* Relative velocity. */
	z.x = b->x.x - a->x.x, z.y = b->x.y - a->x.y; /* Distance(zero). */
	/* Quadratic { (vt + z = r)^2 -> v^2t^2 + 2vzt + z^2 - r^2 = 0 }. */
	v2 = v.x * v.x + v.y * v.y;
	vz = v.x * z.x + v.y * z.y;
	z2 = z.x * z.x + z.y * z.y;
	/* The discriminant determines whether a collision occurred. */
	if((disc = vz * vz - v2 * (z2 - r * r)) < 0.0f)
		return printf("Discriminant %.1f.\n", disc), 0;
	/* The first root. */
	t = (-vz - sqrtf(disc)) / v2;
	printf("Discriminant %.1f; t %.1f.\n", disc, t);
	/* Entirely in the future. */
	if(t >= dt_ms) return printf("In future.\n"), 0;
	/* In the past. */
	if(t < 0.0f) {
		/* Entirely in the past. */
		if((-vz + sqrtf(disc)) / v2 <= 0.0f) return printf("In past.\n"), 0;
		/* Otherwise, we have a problem. */
		degeneracy_pressure(a, b);
		/* If the math is correct, this will not happen again. */
		return collision(a, b, t0_ptr);
	}
	/* Collision. */
	*t0_ptr = t;
	elastic_bounce(a, b, t);
	/*bounce_a(a, b, t);*/
	return 1;
}

static void Circle_filler(struct Circle *const this) {
	struct Vec2f target;
	target.x = scale * rand() / (RAND_MAX + 1.0f),
	target.y = scale * rand() / (RAND_MAX + 1.0f);
	this->x.x = scale * rand() / (RAND_MAX + 1.0f);
	this->x.y = scale * rand() / (RAND_MAX + 1.0f);
	this->v.x = (target.x - this->x.x) / dt_ms;
	this->v.y = (target.y - this->x.y) / dt_ms;
	this->r = 16.0f;
	this->m = 1.0f;
	this->collision = 0;
}

static void test(FILE *data, FILE *gnu) {
	struct Circle a, b;
	struct Vec2f p, q, x, a_c = { 0, 0 }, *a_v, b_c = { 0, 0 }, *b_v;
	float t, d, t0_a = 0.0f, t0_b = 0.0f, t0 = -1.0f, r;
	int is_collision;

	Circle_filler(&a);
	Circle_filler(&b);

	is_collision = collision(&a, &b, &t0);
	r = a.r + b.r;

	a_v = &a.v, b_v = &b.v;
	for(t = 0.0f; t <= dt_ms && (!is_collision || t <= t0); t += 0.25f) {
		p.x = a.x.x + a_v->x * t, p.y = a.x.y + a_v->y * t;
		q.x = b.x.x + b_v->x * t, q.y = b.x.y + b_v->y * t;
		x.x = q.x - p.x, x.y = q.y - p.y;
		d   = sqrtf((float)x.x * x.x + x.y * x.y);
		fprintf(data, "%f\t%f\t%f\t%f\t%f\t%f\n", t, d, p.x, p.y, q.x, q.y);
	}
	if(a.collision) {
		printf("A collides.\n");
		a_c.x = a.x.x + a.v.x * a.collision->t,
		a_c.y = a.x.y + a.v.y * a.collision->t;
		a_v = &a.collision->v;
		t0_a = a.collision->t;
	} else {
		a_c.x = a.x.x, a_c.y = a.x.y;
	}
	if(b.collision) {
		printf("B collides.\n");
		b_c.x = b.x.x + b.v.x * b.collision->t,
		b_c.y = b.x.y + b.v.y * b.collision->t;
		b_v = &b.collision->v;
		t0_b = b.collision->t;
	} else {
		b_c.x = b.x.x, b_c.y = b.x.y;
	}
	for( ; t <= dt_ms; t += 0.25f) {
		p.x = a_c.x + a_v->x * (t - t0_a), p.y = a_c.y + a_v->y * (t - t0_a);
		q.x = b_c.x + b_v->x * (t - t0_b), q.y = b_c.y + b_v->y * (t - t0_b);
		x.x = q.x - p.x, x.y = q.y - p.y;
		d   = sqrtf((float)x.x * x.x + x.y * x.y);
		fprintf(data, "%f\t%f\t%f\t%f\t%f\t%f\n", t, d, p.x, p.y, q.x, q.y);
	}

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
	fprintf(gnu, "set view 5,30\n");
	/*fprintf(gnu, "p_x(t) = %f + %f*t\n", a.x.x, a.v.x);
	fprintf(gnu, "p_y(t) = %f + %f*t\n", a.x.y, a.v.y);
	fprintf(gnu, "q_x(t) = %f + %f*t\n", b.x.x, b.v.x);
	fprintf(gnu, "q_y(t) = %f + %f*t\n", b.x.y, b.v.y);
	fprintf(gnu, "splot p_x(v), p_y(v), v title \"p(t) (%3.1f, %3.1f)->(%3.1f, %3.1f) radius %.0f\" lt palette, q_x(v), q_y(v), v title \"q(t) (%3.1f, %3.1f)->(%3.1f, %3.1f) radius %.0f\" lt palette lw 10\n", a.x.x, a.x.y, a.v.x, a.v.y, a.r, b.x.x, b.x.y, b.v.x, b.v.y, b.r);*/
	/*fprintf(gnu, "splot \"Collision.data\" using 3:4:1 with lines\n");*/
	fprintf(gnu, "splot \"Collision.data\" using 3:4:1 title \"p(t) (%3.1f, %3.1f)->(%3.1f, %3.1f) radius %.0f\" lt palette, \"Collision.data\" using 5:6:1 title \"q(t) (%3.1f, %3.1f)->(%3.1f, %3.1f) radius %.0f\" lt palette lw 10\n", a.x.x, a.x.y, a.v.x, a.v.y, a.r, b.x.x, b.x.y, b.v.x, b.v.y, b.r);

	fprintf(gnu, "set xlabel \"t\"\n");
	fprintf(gnu, "set ylabel \"d\"\n");
	fprintf(gnu, "set xrange [0 : %f]\n", dt_ms);
	fprintf(gnu, "set yrange [-1 : *]\n");
	/*fprintf(gnu, "set label \"t = %f\" at %f, graph 0.05\n", t, t + 0.01f);*/
	/*fprintf(gnu, "set label \"d_{min} = %f\\nt_{min} = %f\\nt_0 = %f\" at graph 0.05, graph 0.10\n", d, t, t0);*/
	fprintf(gnu, "set arrow 1 from graph 0, first %f to graph 1, first %f nohead lc rgb \"blue\" lw 3\n", r, r);
	fprintf(gnu, "set arrow 2 from first %f, graph 0 to first %f, graph 1 nohead\n", t, t);
	if(t0 > 0.0f) fprintf(gnu, "set arrow 3 from first %f, graph 0 to first %f, graph 1 nohead\n", t0, t0);
	fprintf(gnu, "plot \"Collision.data\" using 1:2 title \"Distance |q(t) - p(t)|\" with linespoints\n");
}

/** Entry point.
 @return Either EXIT_SUCCESS or EXIT_FAILURE. */
int main(void) {
	FILE *data, *gnu;
	const int seed = /*2498*//*2557*/(int)clock();

	srand(seed);
	printf("Seed: %i.\n", seed);

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
