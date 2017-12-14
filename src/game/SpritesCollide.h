/** 2017 Neil Edelman, distributed under the terms of the GNU General
 Public License 3, see copying.txt, or
 \url{ https://opensource.org/licenses/GPL-3.0 }.

 Collision detection and resolution. Part of {Sprites}, but too long. The only
 function external to the file is \see{collide_bin}.

 This is the calculation in \see{collide_circles},
 \${ 	     u = a.dx
             v = b.dx
 if(v-u ~= 0) t doesn't matter, parallel-ish
          p(t) = a0 + ut
          q(t) = b0 + vt
 distance(t)   = |q(t) - p(t)|
 distance^2(t) = (q(t) - p(t))^2
               = ((b0+vt) - (a0+ut))^2
               = ((b0-a0) + (v-u)t)^2
               = (v-u)*(v-u)t^2 + 2(b0-a0)*(v-u)t + (b0-a0)*(b0-a0)
             0 = 2(v-u)*(v-u)t_min + 2(b0-a0)*(v-u)
         t_min = -(b0-a0)*(v-u)/(v-u)*(v-u)
 this is an optimisation, if t \notin [0,frame] then pick the closest; if
 distance^2(t_min) < r^2 then we have a collision, which happened at t0,
           r^2 = (v-u)*(v-u)t0^2 + 2(b0-a0)*(v-u)t0 + (b0-a0)*(b0-a0)
            t0 = [-2(b0-a0)*(v-u) - \sqrt((2(b0-a0)*(v-u))^2
                  - 4((v-u)*(v-u))((b0-a0)*(b0-a0) - r^2))] / 2(v-u)*(v-u)
            t0 = [-(b0-a0)*(v-u) - \sqrt(((b0-a0)*(v-u))^2
                  - ((v-u)*(v-u))((b0-a0)*(b0-a0) - r^2))] / (v-u)*(v-u) }

 @title		SpritesCollide
 @author	Neil
 @std		C89/90
 @version	2017-10 Broke off from Sprites. */

/** Collision handlers. */
typedef void (*CoverCollision)(struct Cover *const, struct Cover *const,
	const float);
/** Unsticking handlers. */
typedef void (*SpriteDiAction)(struct Sprite *const, struct Sprite *const);



/* Used by collision handlers. */

/** Add a collision to the sprite; called from helpers to collision handlers. */
static void add_bounce(struct Sprite *const this, const struct Vec2f v,
	const float t) {
	struct Collision *col;
	assert(sprites && this);
	if((col = this->collision)) {
		/* Average the vectors for multiple collisions -- not really physical,
		 but fast and not often seen. */
		col->no++;
		col->v.x += (v.x - col->v.x) / col->no;
		col->v.y += (v.y - col->v.y) / col->no;
		if(t < col->t) col->t = t;
	} else {
		/* New collision. */
		if(!(col = CollisionStackNew(sprites->collisions)))
			{ fprintf(stderr, "add_bounce: %s.\n",
			CollisionStackGetError(sprites->collisions)); return; }
		col->no  = 1;
		col->v.x = v.x;
		col->v.y = v.y;
		col->t   = t;
		this->collision = col;
	}
}
/** Elastic collision between circles.
 @param t: {ms} after frame that the collision occurs. */
static void sprite_elastic_bounce(struct Sprite *const a,
	struct Sprite *const b, const float t) {
	struct Vec2f d_hat, a_v, b_v;
	assert(a && b && t >= 0.0f);
	/* Extrapolate and find the eigenvalue. */
	{
		struct Vec2f u, v, d;
		float d_mag;
		/* {u} {v} are the positions of {a} {b} extrapolated to impact. */
		u.x = a->x.x + a->v.x * t, u.y = a->x.y + a->v.y * t;
		v.x = b->x.x + b->v.x * t, v.y = b->x.y + b->v.y * t;
		/* {d} difference between; use to set up a unitary frame, {d_hat}. */
		d.x = v.x - u.x, d.y = v.y - u.y;
		if((d_mag = sqrtf(d.x * d.x + d.y * d.y)) < epsilon) {
			d_hat.x = 1.0f, d_hat.y = 0.0f;
		} else {
			d_hat.x = d.x / d_mag, d_hat.y = d.y / d_mag;
		}
	}
	/* Elastic collision. */
	{
		/* All mass is strictly positive. */
		const float a_m = sprite_get_mass(a), b_m = sprite_get_mass(b);
		const float diff_m = a_m - b_m, invsum_m = 1.0f / (a_m + b_m);
		/* Transform vectors into eigenvector transformation above. */
		struct Vec2f a_v_nrm, b_v_nrm;
		const float a_nrm_s = a->v.x * d_hat.x + a->v.y * d_hat.y;
		const float b_nrm_s = b->v.x * d_hat.x + b->v.y * d_hat.y;
		a_v_nrm.x = a_nrm_s * d_hat.x, a_v_nrm.y = a_nrm_s * d_hat.y;
		b_v_nrm.x = b_nrm_s * d_hat.x, b_v_nrm.y = b_nrm_s * d_hat.y;
		a_v.x = a->v.x - a_v_nrm.x
			+ (a_v_nrm.x * diff_m + 2 * b_m * b_v_nrm.x) * invsum_m,
		a_v.y = a->v.y - a_v_nrm.y
			+ (a_v_nrm.y * diff_m + 2 * b_m * b_v_nrm.y) * invsum_m;
		b_v.x = b->v.x - b_v_nrm.x
			- (b_v_nrm.x * diff_m - 2 * a_m * a_v_nrm.x) * invsum_m,
		b_v.y = b->v.y - b_v_nrm.y
			- (b_v_nrm.y * diff_m - 2 * a_m * a_v_nrm.y) * invsum_m;
	}
	/* Record. */
	add_bounce(a, a_v, t);
	add_bounce(b, b_v, t);
}
/** Perfectly inelastic. */
static void sprite_inelastic_stick(struct Sprite *const a,
	struct Sprite *const b, const float t) {
	/* All mass is strictly positive. */
	const float a_m = sprite_get_mass(a), b_m = sprite_get_mass(b),
		invsum_m = 1.0f / (a_m + b_m);
	const struct Vec2f v = {
		(a_m * a->v.x + b_m * b->v.x) * invsum_m,
		(a_m * a->v.y + b_m * b->v.y) * invsum_m
	};
	assert(a && b && t >= 0.0f);
	add_bounce(a, v, t);
	add_bounce(b, v, t);
}
/** This is like {b} has an infinite mass. */
static void sprite_bounce_a(struct Sprite *const a, struct Sprite *const b,
	const float t) {
	struct Vec2f d_hat, a_v;
	/* Extrapolate and find the eigenvalue. */
	{
		struct Vec2f u, v, d;
		float d_mag;
		/* {u} {v} are the positions of {a} {b} extrapolated to impact. */
		u.x = a->x.x + a->v.x * t, u.y = a->x.y + a->v.y * t;
		v.x = b->x.x + b->v.x * t, v.y = b->x.y + b->v.y * t;
		/* {d} difference between; use to set up a unitary frame, {d_hat}. */
		d.x = v.x - u.x, d.y = v.y - u.y;
		if((d_mag = sqrtf(d.x * d.x + d.y * d.y)) < epsilon) {
			d_hat.x = 1.0f, d_hat.y = 0.0f;
		} else {
			d_hat.x = d.x / d_mag, d_hat.y = d.y / d_mag;
		}
	}
	/* Bounce {a}. */
	{
		/* Transform vectors into eigenvector transformation above. */
		const float a_nrm_s = a->v.x * d_hat.x + a->v.y * d_hat.y;
		struct Vec2f a_v_nrm;
		a_v_nrm.x = a_nrm_s * d_hat.x, a_v_nrm.y = a_nrm_s * d_hat.y;
		/* fixme: Prove. */
		a_v.x = 2.0f * b->v.x - a_v_nrm.x,
		a_v.y = 2.0f * b->v.y - a_v_nrm.y;
	}
	/* Record. */
	add_bounce(a, a_v, t);
}



/* Collision handlers contained in {collision_matrix}. */

/** @implements CoverCollision */
static void elastic_bounce(struct Cover *a, struct Cover *b, const float t) {
	assert(a && a->sprite_ref && *a->sprite_ref
		&& b && b->sprite_ref && *b->sprite_ref);
	sprite_elastic_bounce(*a->sprite_ref, *b->sprite_ref, t);
}
/** @implements CoverCollision */
static void bounce_a(struct Cover *a, struct Cover *b, const float t) {
	assert(a && a->sprite_ref && *a->sprite_ref
		&& b && b->sprite_ref && *b->sprite_ref);
	sprite_bounce_a(*a->sprite_ref, *b->sprite_ref, t);
}
/** @implements CoverCollision */
static void bounce_b(struct Cover *a, struct Cover *b, const float t) {
	assert(a && a->sprite_ref && *a->sprite_ref
		&& b && b->sprite_ref && *b->sprite_ref);
	sprite_bounce_a(*b->sprite_ref, *a->sprite_ref, t);
}
/** @implements CoverCollision */
static void wmd_generic(struct Cover *a, struct Cover *b, const float t) {
	assert(a && a->sprite_ref && *a->sprite_ref
		&& b && b->sprite_ref && *b->sprite_ref);
	sprite_inelastic_stick(*b->sprite_ref, *a->sprite_ref, t);
	sprite_put_damage(*b->sprite_ref, sprite_get_damage(*a->sprite_ref));
	sprite_delete(*a->sprite_ref), a->sprite_ref = 0;
}
/** @implements CoverCollision */
static void generic_wmd(struct Cover *g, struct Cover *w, const float t) {
	wmd_generic(w, g, t);
}

/** @implements CoverCollision */
static void ship_gate(struct Cover *cs, struct Cover *cg, const float t) {
	struct Sprite *const s = *cs->sprite_ref, *const g = *cg->sprite_ref;
	struct Ship *const ship = (struct Ship *)s;
	struct Vec2f diff, gate_norm;
	float proj;
	assert(sprites && cs->sprite_ref && cg->sprite_ref && s && g
		&& s->vt->class == SC_SHIP);
	diff.x = s->x.x - g->x.x;
	diff.y = s->x.y - g->x.y;
	gate_norm.x = cosf(g->x.theta);
	gate_norm.y = sinf(g->x.theta);
	proj = diff.x * gate_norm.x + diff.y * gate_norm.y;
	if(ship->dist_to_horizon > 0 && proj < 0) {
		char a[12], b[12];
		sprite_to_string(s, &a);
		sprite_to_string(g, &b);
		printf("ship_gate: %s crossed into the event horizon of %s.\n", a, b);
		if(ship == get_player()) {
			/* trasport to zone immediately. fixme!!!: events is not handled by
			 migrate sprites. */
			EventsSpriteConsumer(0.0f, (SpriteConsumer)&ZoneChange, g);
		} else {
			sprite_delete(s), cs->sprite_ref = 0; /* Disappear! */
		}
	}
	ship->dist_to_horizon = proj; /* fixme: unreliable? */
	UNUSED(t);
}
/** @implements CoverCollision */
static void gate_ship(struct Cover *g, struct Cover *s, const float t) {
	ship_gate(s, g, t);
}



/* Degeneracy handlers come into play on inter-penetration; they can't modify
 the topology of the sprite list. In a perfect world, every sprite would sense
 every other, but since we are limited to checking two at a time, sometimes
 it's messy and we just have to fake it. Also contained in
 {collision_matrix}. */

/* Apply degeneracy pressure evenly.
 @implements SpriteDiAction */
static void pressure_even(struct Sprite *const a, struct Sprite *const b) {
	struct Vec2f z, z_hat;
	float z_mag, push;
	const float r = a->bounding + b->bounding;
	assert(a && b);
	z.x = b->x.x - a->x.x, z.y = b->x.y - a->x.y;
	z_mag = sqrtf(z.x * z.x + z.y * z.y);
	push = (r - z_mag) * 0.5f + 0.25f; /* Big epsilon. */
	/*printf("Sprites (%.1f, %.1f) -> %.1f apart with combined radius of %.1f pushing %.1f.\n", z.x, z.y, z_mag, r, push);*/
	if(z_mag < epsilon) {
		z_hat.x = 1.0f, z_hat.y = 0.0f;
	} else {
		z_hat.x = z.x / z_mag, z_hat.y = z.y / z_mag;
	}
	a->x.x -= z_hat.x * push, a->x.y -= z_hat.y * push;
	b->x.x += z_hat.x * push, b->x.y += z_hat.y * push;
}
/* Apply degeneracy pressure to {a}.
 @implements SpriteDiAction
 @fixme This function contains some duplicate code. */
static void pressure_a(struct Sprite *const a, struct Sprite *const b) {
	struct Vec2f z, z_hat;
	float z_mag, push;
	const float r = a->bounding + b->bounding;
	assert(a && b);
	z.x = b->x.x - a->x.x, z.y = b->x.y - a->x.y;
	z_mag = sqrtf(z.x * z.x + z.y * z.y);
	/* fixme: {epsilon} is necessary to avoid infinite recursion; why? */
	push = (r - z_mag) + 0.5f; /* Big epsilon. */
	/*printf("Pushing (a) sprite %f apart.\n", push);*/
	if(z_mag < epsilon) {
		z_hat.x = 1.0f, z_hat.y = 0.0f;
	} else {
		z_hat.x = z.x / z_mag, z_hat.y = z.y / z_mag;
	}
	a->x.x -= z_hat.x * push, a->x.y -= z_hat.y * push;
}
/* Apply degeneracy pressure to {b}.
 @implements SpriteDiAction */
static void pressure_b(struct Sprite *const a, struct Sprite *const b) {
	pressure_a(b, a);
}



/* What sort of collisions the subclasses of Sprites engage in. This is set by
 the sprite class, { SC_SHIP, SC_DEBRIS, SC_WMD, SC_GATE }. */
static const struct Matrix {
	const CoverCollision handler;
	const SpriteDiAction degeneracy;
} collision_matrix[][4] = {
	{ /* [ship, *] */
		{ &elastic_bounce, &pressure_even },
		{ &elastic_bounce, &pressure_even },
		{ &generic_wmd,    &pressure_b },
		{ &ship_gate,      0 }
	}, { /* [debris, *] */
		{ &elastic_bounce, &pressure_even },
		{ &elastic_bounce, &pressure_even },
		{ &generic_wmd,    &pressure_b },
		{ &bounce_a,       &pressure_a }
	}, { /* [wmd, *] */
		{ &wmd_generic,    &pressure_a },
		{ &wmd_generic,    &pressure_a },
		{ 0,               0 },
		{ 0,               0 }
	}, { /* [gate, *] */
		{ &gate_ship,      0 },
		{ &bounce_b,       &pressure_b },
		{ 0,               0 },
		{ 0,               0 }
	}
};



/* Collision detection. */

/** Checks whether two sprites intersect using inclined cylinders in
 three-dimensions, where the third dimension is linearly-interpolated time.
 @param t_ptr: If it returns true, this will be set to the time after frame
 time (ms) that it collides. */
static int collide_circles(struct Sprite *const a, struct Sprite *const b,
	float *const t_ptr) {
	struct Vec2f v, z;
	float t, v2, vz, z2, zr2, disc;
	const float r = a->bounding + b->bounding;
	assert(sprites && a && b && t_ptr);
	v.x = b->v.x - a->v.x, v.y = b->v.y - a->v.y; /* Relative velocity. */
	z.x = b->x.x - a->x.x, z.y = b->x.y - a->x.y; /* Distance(zero). */
	/* { vt + z = r -> v^2t^2 + 2vzt + z^2 - r^2 = 0 } is always { >= -r^2 } */
	v2 = v.x * v.x + v.y * v.y;
	vz = v.x * z.x + v.y * z.y;
	z2 = z.x * z.x + z.y * z.y;
	zr2 = z2 - r * r;
	/* Inter-penetration; the degeneracy code MUST resolve it. */
	if(zr2 < 0.0f) {
		SpriteDiAction d;
		if((d = collision_matrix[a->vt->class][b->vt->class].degeneracy)) {
			/* Debug: show degeracy pressure. */
			struct Vec2f x = { (a->x.x + b->x.x) * 0.5f,
				(a->x.y + b->x.y) * 0.5f };
			Info(&x, icon_expand);
			/* Force it. */
			d(a, b);
			v.x = b->v.x - a->v.x, v.y = b->v.y - a->v.y;
			z.x = b->x.x - a->x.x, z.y = b->x.y - a->x.y;
			v2 = v.x * v.x + v.y * v.y;
			vz = v.x * z.x + v.y * z.y;
			z2 = z.x * z.x + z.y * z.y;
			zr2 = z2 - r * r;
		}
	}
	/* The relative velocity is zero then there can be no collision except as
	 above -- this has a very small epsilon to prevent sprites from going
	 slowly into @ other, but it's the denominator of the next equation.
	 Otherwise, the discriminant determines whether a collision occurred. */
	if(v2 <= 1e-32 || (disc = vz * vz - v2 * zr2) < 0.0f) return 0;
	t = (-vz - sqrtf(disc)) / v2;
	/* Entirely in the future or entirely in the past. */
	if(t >= sprites->dt_ms || (t < 0.0f && (-vz + sqrtf(disc)) / v2 <= 0.0f))
		return 0;
	/* Collision. */
	*t_ptr = t;
	return 1;
}
/** This applies the most course-grained collision detection in two-dimensional
 space, Hahn–Banach separation theorem using {box}. The box must be set
 beforehand (viz, in \see{extrapolate}.) */
static int collide_boxes(const struct Sprite *const a,
	const struct Sprite *const b) {
	assert(a && b);
	/* https://stackoverflow.com/questions/306316/determine-if-two-rectangles-overlap-each-other */
	return (a->box.x_min <= b->box.x_max && b->box.x_min <= a->box.x_max
		&&  b->box.y_min <= a->box.y_max && a->box.y_min <= b->box.y_max)
		? 1 : 0;
}



/* This is the function that's calling everything else and is the only function
 that matters to the rest of {Sprites.c}. */

/** Call after {extrapolate}; needs and consumes {covers}. This is {n^2} inside
 of the {bin}. */
static void collide_bin(unsigned bin) {
	struct CoverStack *const cover = sprites->bins[bin].covers;
	struct Cover *cover_a, *cover_b;
	const struct Matrix *matrix;
	struct Sprite *a, *b;
	float t;
	size_t index_b;
	assert(sprites && bin < LAYER_SIZE);
	
	/*printf("bin %u: %lu covers\n", bin, ref->size);*/
	/* This is {O({covers}^2)/2} within the bin. {a} is popped . . . */
	while((cover_a = CoverStackPop(cover))) {
		/* . . . then {b} goes down the list. */
		if(!(index_b = CoverStackGetSize(cover))) break;
		do {
			index_b--;
			cover_b = CoverStackGetElement(cover, index_b);
			assert(cover_a && cover_b);
			/* Another {bin} takes care of it? */
			if(!cover_a->is_corner && !cover_b->is_corner) continue;
			/* Respond appropriately if it was deleted on the fly. */
			if(!(a = cover_a->sprite_ref)) break;
			if(!(b = cover_b->sprite_ref)) continue;
			/* Extract the info on the type of collision. */
			matrix = &collision_matrix[a->vt->class][b->vt->class];
			/* If the sprites have no collision handler, don't bother. */
			if(!(matrix->handler)) continue;
			/* {collide_boxes} >= {collide_circles} */
			if(!collide_boxes(a, b) || !collide_circles(a, b, &t)) continue;
			/* Collision. */
			matrix->handler(cover_a, cover_b, t);
		} while(index_b);
	}
}
