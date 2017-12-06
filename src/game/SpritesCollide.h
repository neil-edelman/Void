/** 2017 Neil Edelman, distributed under the terms of the GNU General
 Public License 3, see copying.txt, or
 \url{ https://opensource.org/licenses/GPL-3.0 }.

 Collision detection and resolution. Part of {Sprites}, but too long; sort of
 hacky. The course grained functions are at the bottom, calling the
 fine-grained and response as it moves to the top.

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
typedef void (*SpriteCollision)(struct Sprite *const, struct Sprite *const,
	const float);
/** Unsticking handlers. */
typedef void (*SpriteDiAction)(struct Sprite *const, struct Sprite *const);

/** Add a collision to the sprite; called from collision handlers. */
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



/* Collision handlers; can not modify list of {Sprites}! Contained in
 \see{collision_matrix}. */

/** Elastic collision between circles; called from \see{collision_matrix}. 
 Degeneracy pressure pushes sprites to avoid interpenetration.
 @param t: {ms} after frame that the collision occurs.
 @implements SpriteCollision */
static void elastic_bounce(struct Sprite *const a, struct Sprite *const b,
	const float t) {
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
/** Perfectly inelastic.
 @implements SpriteCollision */
static void inelastic_stick(struct Sprite *const a,
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
/** This is like {b} has an infinite mass.
 @implements SpriteCollision */
static void bounce_a(struct Sprite *const a, struct Sprite *const b,
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
/** @implements SpriteCollision */
static void bounce_b(struct Sprite *const a, struct Sprite *const b,
	const float t) {
	bounce_a(b, a, t);
}

/** @implements SpriteCollision */
static void wmd_debris(struct Sprite *w, struct Sprite *d, const float t) {
	/* avoid inifinite destruction loop */
	/*if(SpriteIsDestroyed(w) || SpriteIsDestroyed(d)) return;
	push(d, atan2f(d->y - w->y, d->x - w->x), w->mass);
	SpriteRecharge(d, -SpriteGetDamage(w));
	SpriteDestroy(w);*/
	/*char a[12], b[12];
	sprite_to_string(w, &a);
	sprite_to_string(d, &b);
	printf("hit %s -- %s.\n", a, b);*/
	printf("BOOM!\n");
	inelastic_stick(d, w, t);
	/* sprite_delete(w); <- fixme: why is it crashing? */
}
/** @implements SpriteCollision */
static void debris_wmd(struct Sprite *d, struct Sprite *w, const float t) {
	wmd_debris(w, d, t);
}

/** @implements SpriteCollision */
static void wmd_ship(struct Sprite *w, struct Sprite *s, const float t) {
	/*char a[12], b[12];
	sprite_to_string(w, &a);
	sprite_to_string(s, &b);
	printf("wmd_shp: %s -- %s\n", a, b);*/
	/* avoid inifinite destruction loop */
	/*if(SpriteIsDestroyed(w) || SpriteIsDestroyed(s)) return;
	push(s, atan2f(s->y - w->y, s->x - w->x), w->mass);
	SpriteRecharge(s, -SpriteGetDamage(w));*/
	inelastic_stick(s, w, t);
	/* sprite_delete(w); <- fixme */
}
/** @implements SpriteCollision */
static void ship_wmd(struct Sprite *s, struct Sprite *w, const float t) {
	wmd_ship(w, s, t);
}

/** @implements SpriteCollision */
static void ship_gate(struct Sprite *s, struct Sprite *g, const float t) {
	/*void (*fn)(struct Sprite *const, struct Sprite *);*/
	/*Info("Shp%u colliding with Eth%u . . . \n", ShipGetId(ship), EtherealGetId(eth));*/
	/*if((fn = EtherealGetCallback(eth))) fn(eth, s);*/
	/* while in iterate! danger! */
	/*if(e->sp.ethereal.callback) e->sp.ethereal.callback(e, s);*/
	struct Ship *const ship = (struct Ship *)s;
	float x, y, /*vx, vy,*/ gate_norm_x, gate_norm_y, proj/*, h*/;
	assert(s && g && s->vt->class == SC_SHIP);
	x = s->x.x - g->x.x;
	y = s->x.y - g->x.y;
	gate_norm_x = cosf(g->x.theta);
	gate_norm_y = sinf(g->x.theta);
	proj = x * gate_norm_x + y * gate_norm_y;
	if(ship->dist_to_horizon > 0 && proj < 0) {
		char a[12], b[12];
		sprite_to_string(s, &a);
		sprite_to_string(g, &b);
		printf("ship_into_gate: %s crossed into the event horizon of %s.\n",a,b);
#if 0
		if(ship == GameGetPlayer()) {
			/* trasport to zone immediately */
			Event(0, 0, 0, FN_CONSUMER, &ZoneChange, g);
		} else {
			/* disappear */
			/* fixme: test! */
		}
#endif
	}/* else*/
	/* fixme: unreliable */
	ship->dist_to_horizon = proj;
	UNUSED(t);
}
/** @implements SpriteCollision */
static void gate_ship(struct Sprite *g, struct Sprite *s, const float t) {
	ship_gate(s, g, t);
}



/* Degeneracy handlers come into play on inter-penetration. */

#if 1 /* <- pressure: Apply along normal. Leads to bouncing. */

/* Apply degeneracy pressure evenly.
 @implements SpriteDiAction
 @fixme This function contains some duplicate code.
 @fixme This should be called WAY less! Why is it letting inter-penetration on
 some sprites? */
static void pressure_even(struct Sprite *const a, struct Sprite *const b) {
	struct Vec2f z, z_hat;
	float z_mag, push;
	const float r = a->bounding + b->bounding;
	assert(a && b);
	z.x = b->x.x - a->x.x, z.y = b->x.y - a->x.y;
	z_mag = sqrtf(z.x * z.x + z.y * z.y);
	push = (r - z_mag) * 0.5f + 0.25f; /* Big epsilon. */
	printf("Sprites (%.1f, %.1f) -> %.1f apart with combined radius of %.1f pushing %.1f.\n", z.x, z.y, z_mag, r, push);
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
	printf("Pushing (a) sprite %f apart.\n", push);
	if(z_mag < epsilon) {
		z_hat.x = 1.0f, z_hat.y = 0.0f;
	} else {
		z_hat.x = z.x / z_mag, z_hat.y = z.y / z_mag;
	}
	a->x.x -= z_hat.x * push, a->x.y -= z_hat.y * push;
}
/* Apply degeneracy pressure to {a}.
 @implements SpriteDiAction */
static void pressure_b(struct Sprite *const a, struct Sprite *const b) {
	pressure_a(b, a);
}

#else /* pressure --><-- random: Doesn't lead to bouncing, but is very
 confusing to get stuck in. */

/* Resolve randomly degeneracy pressure to {a} and {b}.
 @implements SpriteDiAction */
static void pressure_even(struct Sprite *const a, struct Sprite *const b) {
	const struct Vec2f mid = { (a->x.x+b->x.x) * 0.5f, (a->x.y+b->x.y) * 0.5f };
	const float r = (a->bounding + b->bounding) * 0.5f + 0.01f;
	const float theta = random_pm_max(M_PI_F);
	a->x.x = mid.x - cosf(theta) * r;
	a->x.y = mid.y - sinf(theta) * r;
	b->x.x = mid.x + cosf(theta) * r;
	b->x.y = mid.y + sinf(theta) * r;
}
/* Resolve randomly degeneracy pressure to {a}.
 @implements SpriteDiAction */
static void pressure_a(struct Sprite *const a, struct Sprite *const b) {
	const float r = a->bounding + b->bounding + 0.01f;
	const float theta = random_pm_max(M_PI_F);
	a->x.x = b->x.x + cosf(theta) * r;
	a->x.y = b->x.y + sinf(theta) * r;
}
/* Resolve randomly degeneracy pressure to {b}.
 @implements SpriteDiAction */
static void pressure_b(struct Sprite *const a, struct Sprite *const b) {
	pressure_a(b, a);
}

#endif /* random --> */



/* What sort of collisions the subclasses of Sprites engage in. This is set by
 the sprite class, { SC_SHIP, SC_DEBRIS, SC_WMD, SC_GATE }. */
static const struct {
	const SpriteCollision handler;
	const SpriteDiAction degeneracy;
} collision_matrix[][4] = {
	{ /* [ship, *] */
		{ &elastic_bounce, &pressure_even },
		{ &elastic_bounce, &pressure_even },
		{ &ship_wmd,       &pressure_b },
		{ &ship_gate,      0 }
	}, { /* [debris, *] */
		{ &elastic_bounce, &pressure_even },
		{ &elastic_bounce, &pressure_even },
		{ &debris_wmd,     &pressure_b },
		{ &bounce_a,       &pressure_a }
	}, { /* [wmd, *] */
		{ &wmd_ship,       &pressure_a },
		{ &wmd_debris,     &pressure_a },
		{ 0,               0 },
		{ 0,               0 }
	}, { /* [gate, *] */
		{ &gate_ship,      0 },
		{ &bounce_b,       &pressure_b },
		{ 0,               0 },
		{ 0,               0 }
	}
};



/** Checks whether two sprites intersect using inclined cylinders in
 three-dimensions, where the third dimension is linearly-interpolated time.
 Called from \see{collide_boxes}. If we have a collision, calls the functions
 contained in the {collision_matrix}. */
static void collide_circles(struct Sprite *const a, struct Sprite *const b) {
	struct Vec2f v, z;
	float t, v2, vz, z2, zr2, disc;
	const float r = a->bounding + b->bounding;
	assert(sprites);
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
			struct Vec2f *spot;
			char a_str[12], b_str[12];
			sprite_to_string(a, &a_str), sprite_to_string(b, &b_str);
			printf("Degeneracy pressure between %s and %s.\n", a_str, b_str);
			/* Debug show hair. */
			spot = InfoStackNew(sprites->info);
			spot->x = (a->x.x + b->x.x) * 0.5f;
			spot->y = (a->x.y + b->x.y) * 0.5f;
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
	if(v2 <= 1e-32 || (disc = vz * vz - v2 * zr2) < 0.0f) return;
	t = (-vz - sqrtf(disc)) / v2;
	/* Entirely in the future or entirely in the past. */
	if(t >= sprites->dt_ms || (t < 0.0f && (-vz + sqrtf(disc)) / v2 <= 0.0f))
		return;
	/* Assert supposed to be checked earlier in the path. Collision. */
	assert(collision_matrix[a->vt->class][b->vt->class].handler);
	collision_matrix[a->vt->class][b->vt->class].handler(a, b, t);
}

/** This first applies the most course-grained collision detection in
 two-dimensional space, Hahn–Banach separation theorem using {box}. If passed,
 calls \see{collide_circles}. Called from \see{collide_bin}. */
static void collide_boxes(struct Sprite *const a, struct Sprite *const b) {
	assert(a && b);
	/* https://stackoverflow.com/questions/306316/determine-if-two-rectangles-overlap-each-other */
	if(!(a->box.x_min <= b->box.x_max && b->box.x_min <= a->box.x_max &&
		 b->box.y_min <= a->box.y_max && a->box.y_min <= b->box.y_max)) return;
	collide_circles(a, b);
}

/** This is the function that's calling everything else. Call after
 {extrapolate}; needs and consumes {covers}. This is {n^2} inside of the
 {bin}. */
static void collide_bin(unsigned bin) {
	struct CoverStack *const cover = sprites->bins[bin].covers;
	struct Cover *cover_a, *cover_b;
	struct Sprite *a, *b;
	size_t index_b;
	assert(sprites && bin < LAYER_SIZE);

	/*printf("bin %u: %lu covers\n", bin, ref->size);*/
	/* This is {O({covers}^2)/2} within the bin. {a} is poped . . . */
	while((cover_a = CoverStackPop(cover))) {
		/* . . . then {b} goes down the list. */
		if(!(index_b = CoverStackGetSize(cover))) break;
		do {
			index_b--;
			cover_b = CoverStackGetElement(cover, index_b);
			assert(cover_a && cover_b);
			/* Another {bin} takes care of it? */
			if(!cover_a->is_corner && !cover_b->is_corner) continue;
			a = cover_a->sprite, b = cover_b->sprite;
			assert(a && b);
			/* If the sprites have no collision handler thing, don't bother.
			 Mostly for weapons that ignore collisions with themselves. */
			if(!(collision_matrix[a->vt->class][b->vt->class].handler))
				continue;
			/* Pass it to the next LOD. */
			collide_boxes(a, b);
		} while(index_b);
	}
}
