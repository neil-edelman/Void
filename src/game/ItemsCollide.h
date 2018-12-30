/** 2017 Neil Edelman, distributed under the terms of the GNU General
 Public License 3, see copying.txt, or
 \url{ https://opensource.org/licenses/GPL-3.0 }.

 Collision detection and resolution. Part of {Items}, but too long. The only
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

 @title		ItemsCollide
 @author	Neil
 @std		C89/90
 @version	2017-10 Broke off from Sprites. */

/** Collision handlers. */
typedef void (*CoverCollision)(struct Cover *const, struct Cover *const,
	const float);
/** Unsticking handlers. */
typedef void (*ItemDiAction)(struct Item *const, struct Item *const);



/* Used by collision handlers. */

/** Add a collision to the item; called from helpers to collision handlers. */
static void add_bounce(struct Item *const this, const struct Vec2f v,
	const float t) {
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
		/* New collision. Link the collision with the item. */
		if(!(col = CollisionPoolNew(&items.collisions)))
			{ perror("collision"); return; }
		col->item = this;
		col->no   = 1;
		col->v.x  = v.x;
		col->v.y  = v.y;
		col->t    = t;
		this->collision = col;
	}
}
/** Elastic collision between circles.
 @param t: {ms} after frame that the collision occurs. */
static void item_elastic_bounce(struct Item *const a,
	struct Item *const b, const float t) {
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
		const float a_m = get_mass(a), b_m = get_mass(b);
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
static void item_inelastic_stick(struct Item *const a,
	struct Item *const b, const float t) {
	/* All mass is strictly positive. */
	const float a_m = get_mass(a), b_m = get_mass(b),
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
static void item_bounce_a(struct Item *const a, struct Item *const b,
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

/** This is only within the frame; deleted covers are possible, so it can
 return null. */
static struct Item *cover_to_item(struct Cover *const cover) {
	struct Active *const active
		= ActivePoolGet(&items.actives, cover->active_index);
	assert(cover && active);
	return active->item;
}
/** This means nothing referring to it is valid anymore; breaks the link
 between the {item} and {active}; we can't delete it yet because multiple
 covers might refer to it still. */
static void delete_cover(struct Cover *const cover) {
	struct Active *const active = ActivePoolGet(&items.actives, cover->active_index);
	assert(cover && active && active->item && active == active->item->active);
	active->item->active = 0;
	active->item = 0;
}

/** @implements CoverCollision */
static void elastic_bounce(struct Cover *a, struct Cover *b, const float t) {
	item_elastic_bounce(cover_to_item(a), cover_to_item(b), t);
}
/** @implements CoverCollision */
static void bounce_a(struct Cover *a, struct Cover *b, const float t) {
	item_bounce_a(cover_to_item(a), cover_to_item(b), t);
}
/** @implements CoverCollision */
static void bounce_b(struct Cover *a, struct Cover *b, const float t) {
	item_bounce_a(cover_to_item(b), cover_to_item(a), t);
}
/** @implements CoverCollision */
static void wmd_generic(struct Cover *a, struct Cover *b, const float t) {
	struct Item *const ai = cover_to_item(a), *const bi = cover_to_item(b);
	item_inelastic_stick(bi, ai, t);
	/* @fixme Must check if b is deleted and delete it from the cover. Should
	 be ItemPredicate? */
	/*if(!*/put_damage(bi, get_damage(ai)) /*) b->proxy->item = 0; */;
	delete_cover(a);
}
/** @implements CoverCollision */
static void generic_wmd(struct Cover *g, struct Cover *w, const float t) {
	wmd_generic(w, g, t);
}

/** Collisions with gates are okay, but we have to trigger going though a gate
 if the ship enters the event horizon.
 @implements CoverCollision */
static void ship_gate(struct Cover *cs, struct Cover *cg, const float t) {
	struct Item *const s = cover_to_item(cs),
		*const g = cover_to_item(cg);
	/* @fixme this will not work if it isn't the first in the struct */
	struct Ship *const ship = (struct Ship *)s;
	struct Vec2f diff, gate_norm;
	assert(s && g && s->vt->class == SC_SHIP);
	/* Not useful; t = 0 largely because it's interpenetrating all the time. */
	(void)t;
	/* Gates don't move much so this is a good approximation. */
	gate_norm.x = cosf(g->x.theta);
	gate_norm.y = sinf(g->x.theta);
	/* It has to be in front of the event horizon at {t = 0} to cross. */
	diff.x = s->x.x - g->x.x;
	diff.y = s->x.y - g->x.y;
	if(diff.x * gate_norm.x + diff.y * gate_norm.y < 0) return;
	/* Behind the horizon at {t = 1}. It's very difficult in our system to do
	 collisions with less than a frame beproxy the event horizon because we are
	 doing it now and we are not finished, so just guess: where the item
	 would be if it didn't collide with anything on this frame. */
	diff.x += s->v.x * items.dt_ms;
	diff.y += s->v.y * items.dt_ms;
	if(diff.x * gate_norm.x + diff.y * gate_norm.y >= 0) return;
	{
		char a[12], b[12];
		item_to_string(s, &a);
		item_to_string(g, &b);
		printf("ship_gate: %s crossed into the event horizon of %s.\n", a, b);
	}
	if(ship == get_player()) {
		/* transport to zone immediately. @fixme Events is not handled by
		 migrate items, so this could slightly cause a segfault. */
		EventsItemConsumer(0.0f, (ItemConsumer)&ZoneChange, g);
	} else {
		delete_cover(cs); /* Disappear! */
	}
}
/** @implements CoverCollision */
static void gate_ship(struct Cover *g, struct Cover *s, const float t) {
	ship_gate(s, g, t);
}



/* Degeneracy handlers come into play on inter-penetration; they can't modify
 the topology of the item list. In a perfect world, every item would sense
 every other, but since we are limited to checking two at a time, sometimes
 it's messy and we just have to fake it. Also contained in
 {collision_matrix}. */

/* Apply degeneracy pressure evenly.
 @implements ItemDiAction */
static void pressure_even(struct Item *const a, struct Item *const b) {
	struct Vec2f z, z_hat;
	float z_mag, push;
	const float r = a->bounding + b->bounding;
	assert(a && b);
	z.x = b->x.x - a->x.x, z.y = b->x.y - a->x.y;
	z_mag = sqrtf(z.x * z.x + z.y * z.y);
	push = (r - z_mag) * 0.5f + 0.25f; /* Big epsilon. */
	/*printf("Items (%.1f, %.1f) -> %.1f apart with combined radius of %.1f pushing %.1f.\n", z.x, z.y, z_mag, r, push);*/
	if(z_mag < epsilon) {
		z_hat.x = 1.0f, z_hat.y = 0.0f;
	} else {
		z_hat.x = z.x / z_mag, z_hat.y = z.y / z_mag;
	}
	a->x.x -= z_hat.x * push, a->x.y -= z_hat.y * push;
	b->x.x += z_hat.x * push, b->x.y += z_hat.y * push;
}
/* Apply degeneracy pressure to {a}.
 @implements ItemDiAction
 @fixme This function contains some duplicate code. */
static void pressure_a(struct Item *const a, struct Item *const b) {
	struct Vec2f z, z_hat;
	float z_mag, push;
	const float r = a->bounding + b->bounding;
	assert(a && b);
	z.x = b->x.x - a->x.x, z.y = b->x.y - a->x.y;
	z_mag = sqrtf(z.x * z.x + z.y * z.y);
	/* fixme: {epsilon} is necessary to avoid infinite recursion; why? */
	push = (r - z_mag) + 0.5f; /* Big epsilon. */
	/*printf("Pushing (a) item %f apart.\n", push);*/
	if(z_mag < epsilon) {
		z_hat.x = 1.0f, z_hat.y = 0.0f;
	} else {
		z_hat.x = z.x / z_mag, z_hat.y = z.y / z_mag;
	}
	a->x.x -= z_hat.x * push, a->x.y -= z_hat.y * push;
}
/* Apply degeneracy pressure to {b}.
 @implements ItemDiAction */
static void pressure_b(struct Item *const a, struct Item *const b) {
	pressure_a(b, a);
}



/* What sort of collisions the subclasses of Items engage in. This is set by
 the item class, { SC_SHIP, SC_DEBRIS, SC_WMD, SC_GATE }. */
static const struct Matrix {
	const CoverCollision handler;
	const ItemDiAction degeneracy;
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

/** Checks whether two items intersect using inclined cylinders in
 three-dimensions, where the third dimension is linearly-interpolated time.
 @param t_ptr: If it returns true, this will be set to the time after frame
 time (ms) that it collides. */
static int collide_circles(struct Item *const a, struct Item *const b,
	float *const t_ptr) {
	/* Items used in debugging. */
	static const struct AutoImage *icon_expand;
	struct Vec2f v, z;
	float t, v2, vz, z2, zr2, disc;
	const float r = a->bounding + b->bounding;
	if(!icon_expand) icon_expand = AutoImageSearch("Expand16.png");
	assert(a && b && t_ptr && icon_expand);
	v.x = b->v.x - a->v.x, v.y = b->v.y - a->v.y; /* Relative velocity. */
	z.x = b->x.x - a->x.x, z.y = b->x.y - a->x.y; /* Distance(zero). */
	/* { vt + z = r -> v^2t^2 + 2vzt + z^2 - r^2 = 0 } is always { >= -r^2 } */
	v2 = v.x * v.x + v.y * v.y;
	vz = v.x * z.x + v.y * z.y;
	z2 = z.x * z.x + z.y * z.y;
	zr2 = z2 - r * r;
	/* Inter-penetration; the degeneracy code MUST resolve it. */
	if(zr2 < 0.0f) {
		ItemDiAction d;
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
	 above -- this has a very small epsilon to prevent items from going
	 slowly into @ other, but it's the denominator of the next equation.
	 Otherwise, the discriminant determines whether a collision occurred. */
	if(v2 <= 1e-32 || (disc = vz * vz - v2 * zr2) < 0.0f) return 0;
	t = (-vz - sqrtf(disc)) / v2;
	/* Entirely in the future or entirely in the past. */
	if(t >= items.dt_ms || (t < 0.0f && (-vz + sqrtf(disc)) / v2 <= 0.0f))
		return 0;
	/* Collision. */
	*t_ptr = t;
	return 1;
}
/** This applies the most course-grained collision detection in two-dimensional
 space, Hahn–Banach separation theorem using {box}. The box must be set
 beproxyhand (viz, in \see{extrapolate}.) */
static int collide_boxes(const struct Item *const a,
	const struct Item *const b) {
	assert(a && b);
	/* https://stackoverflow.com/questions/306316/determine-if-two-rectangles-overlap-each-other */
	return (a->box.x_min <= b->box.x_max && b->box.x_min <= a->box.x_max
		&&  b->box.y_min <= a->box.y_max && a->box.y_min <= b->box.y_max)
		? 1 : 0;
}



/* This is the function that's calling everything else and is the only function
 that matters to the rest of {Items.c}. */

/** Call after {extrapolate}; needs and consumes {covers}. This is {n^2} inside
 of the {bin}. {covers} is pointing to the items, so we need to be careful
 that we invalidate the {cover} on deleting the item. Also, position hasn't
 been finalised. */
static void collide_bin(unsigned bin) {
	struct CoverPool *const covers = &items.bins[bin].covers;
	struct Cover *cover_a, *cover_b;
	const struct Matrix *matrix;
	struct Item *a, *b;
	struct Active *ap, *bp;
	float t;
	assert(bin < LAYER_SIZE);
	/*printf("bin %u: %lu covers\n", bin, ref->size);*/
	/* This is {O({covers}^2)/2} within the bin. {a} is popped . . . */
	while((cover_a = CoverPoolPop(covers))) {
		/* . . . then {b} goes though each element. */
		/* @fixme Uhm, could {is_corner} be off the screen? I don't think so. */
		cover_b = 0;
		while((cover_b = CoverPoolNext(covers, cover_b))) {
			/* Another {bin} takes care of it? */
			if(!cover_a->is_corner && !cover_b->is_corner) continue;
			/* Respond appropriately if it was deleted on the fly. */
			ap = ActivePoolGet(&items.actives, cover_a->active_index);
			assert(ap);
			if(!(a = ap->item)) break;
			bp = ActivePoolGet(&items.actives, cover_b->active_index);
			assert(bp); /* @fixme This assertion fails . . . size 22, proxy_index 28. Doesn't make any sense. */
			if(!(b = bp->item)) continue;
			/* Extract the info on the type of collision. */
			matrix = &collision_matrix[a->vt->class][b->vt->class];
			/* If the items have no collision handler, don't bother. */
			if(!(matrix->handler)) continue;
			/* {collide_boxes} >= {collide_circles} */
			if(!collide_boxes(a, b) || !collide_circles(a, b, &t)) continue;
			/* Collision. */
			matrix->handler(cover_a, cover_b, t);
		}
	}
}
