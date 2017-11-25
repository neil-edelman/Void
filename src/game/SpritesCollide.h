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



/* Collisions handlers; can not modify list of {Sprites}! Contained in
 \see{collision_matrix}. */

/** Elastic collision between circles; called from \see{collision_matrix}. 
 Degeneracy pressure pushes sprites to avoid interpenetration.
 @param t: {ms} after frame that the collision occurs.
 @implements SpriteCollision */
static void elastic_bounce(struct Sprite *const a, struct Sprite *const b,
	const float t) {
	/* Interpolate position of collision; delta. */
	const struct Vec2f u = { a->x.x + a->v.x * t, a->x.y + a->v.y * t },
	                   v = { b->x.x + b->v.x * t, b->x.y + b->v.y * t },
		d = { v.x - u.x, v.y - u.y };
	/* Normal at point of impact. */
	const float d_mag = sqrtf(d.x * d.x + d.y * d.y);
	const struct Vec2f d_hat = { (d_mag < epsilon) ? 1.0f : d.x / d_mag,
	                             (d_mag < epsilon) ? 0.0f : d.y / d_mag };
	/* {a}'s velocity, normal, tangent, direction. */
	const float a_nrm_s = a->v.x * d_hat.x + a->v.y * d_hat.y;
	const struct Vec2f a_nrm = { a_nrm_s * d_hat.x, a_nrm_s * d_hat.y },
	                   a_tan = { a->v.x - a_nrm.x,  a->v.y - a_nrm.y };
	/* {b}'s velocity, normal, tangent, direction. */
	const float b_nrm_s = b->v.x * d_hat.x + b->v.y * d_hat.y;
	const struct Vec2f b_nrm = { b_nrm_s * d_hat.x, b_nrm_s * d_hat.y },
	                   b_tan = { b->v.x - b_nrm.x, b->v.y - b_nrm.y };
	/* Assume all mass is strictly positive. */
	const float a_m = sprite_get_mass(a), b_m = sprite_get_mass(b);
	const float diff_m = a_m - b_m, invsum_m = 1.0f / (a_m + b_m);
	/* Elastic collision. */
	const struct Vec2f a_v = {
		a_tan.x + (a_nrm.x * diff_m + 2 * b_m * b_nrm.x) * invsum_m,
		a_tan.y + (a_nrm.y * diff_m + 2 * b_m * b_nrm.y) * invsum_m
	}, b_v = {
		b_tan.x - (b_nrm.x * diff_m - 2 * a_m * a_nrm.x) * invsum_m,
		b_tan.y - (b_nrm.y * diff_m - 2 * a_m * a_nrm.y) * invsum_m
	};
	assert(sprites);
	/* Inter-penetration; absolutely do not want objects to get stuck orbiting
	 each other. Happens all the time. */
	if(d_mag < a->bounding + b->bounding) {
		const float push = (a->bounding + b->bounding - d_mag) * 0.55f;
		/*printf("elastic_bounce: \\pushing sprites %f distance apart\n",
			push);*/
		a->x.x -= d_hat.x * push;
		a->x.y -= d_hat.y * push;
		b->x.x += d_hat.x * push;
		b->x.y += d_hat.y * push;
	}
	add_bounce(a, a_v, t);
	add_bounce(b, b_v, t);
}
/** This is like {b} has an infinite mass.
 @implements SpriteCollision */
static void elastic_bounce_a(struct Sprite *const a, struct Sprite *const b,
	const float t) {
	/* Interpolate position of collision; delta. */
	const struct Vec2f u = { a->x.x + a->v.x * t, a->x.y + a->v.y * t },
	                   v = { b->x.x + b->v.x * t, b->x.y + b->v.y * t },
		d = { v.x - u.x, v.y - u.y };
	/* Normal at point of impact. */
	const float d_mag = sqrtf(d.x * d.x + d.y * d.y);
	const struct Vec2f d_hat = { (d_mag < epsilon) ? 1.0f : d.x / d_mag,
	                             (d_mag < epsilon) ? 0.0f : d.y / d_mag };
	/* {a}'s velocity, normal, tangent, direction. */
	const float a_nrm_s = a->v.x * d_hat.x + a->v.y * d_hat.y;
	const struct Vec2f a_nrm = { a_nrm_s * d_hat.x, a_nrm_s * d_hat.y },
	                   a_tan = { a->v.x - a_nrm.x,  a->v.y - a_nrm.y };
	/* {a} bounces off {b}. fixme: Prove this. fixme: It's wrong. */
	const struct Vec2f a_v = { a_tan.x - a_nrm.x, a_tan.y - a_nrm.y };
	assert(sprites);
	/* Inter-penetration; absolutely do not want objects to get stuck orbiting
	 each other. */
	if(d_mag < a->bounding + b->bounding) {
		const float push = (a->bounding + b->bounding - d_mag) * 1.1f;
		/*printf("elastic_bounce_a: \\pushing sprites %f distance apart\n",
			push);*/
		a->x.x += d_hat.x * push;
		a->x.y += d_hat.y * push;
	}
	add_bounce(a, a_v, t);
}
/** @implements SpriteCollision */
static void elastic_bounce_b(struct Sprite *const a, struct Sprite *const b,
	const float t) {
	elastic_bounce_a(b, a, t);
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
	elastic_bounce(d, w, t); /* _a <- fixme: {elastic_bounce_a} is wonky. */
	sprite_delete(w);
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
	elastic_bounce_a(s, w, t);
	sprite_delete(w);
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

/* What sort of collisions the subclasses of Sprites engage in. This is set by
 the sprite class, { SC_SHIP, SC_DEBRIS, SC_WMD, SC_GATE }. */
static const SpriteCollision collision_matrix[][4] = {
	{ &elastic_bounce, &elastic_bounce,   &ship_wmd,   &ship_gate },
	{ &elastic_bounce, &elastic_bounce,   &debris_wmd, &elastic_bounce_a },
	{ &wmd_ship,       &wmd_debris,       0,           0 },
	{ &gate_ship,      &elastic_bounce_b, 0,           0 }
};



/** Checks whether two sprites intersect using inclined cylinders in
 three-dimensions, where the third dimension is linearly-interpolated time.
 Called from \see{collide_boxes}. If we have a collision, calls the functions
 contained in the {collision_matrix}. */
static void collide_circles(struct Sprite *const a, struct Sprite *const b) {
	struct Vec2f v, z, dist;
	float t, v2, vz, z2, dist_2, det;
	const float r = a->bounding + b->bounding;
	assert(a && b);
	v.x = a->v.x - b->v.x, v.y = a->v.y - b->v.y;
	z.x = a->x.x - b->x.x, z.y = a->x.y - b->x.y;
	v2 = v.x * v.x + v.y * v.y;
	vz = v.x * z.x + v.y * z.y;
	/* Time of closest approach. */
	if(v2 < epsilon) {
		t = 0.0f;
	} else {
		t = -vz / v2;
		if(     t < 0.0f)           t = 0.0f;
		else if(t > sprites->dt_ms) t = sprites->dt_ms;
	}
	/* Distance(t_min). */
	dist.x = z.x + v.x * t, dist.y = z.y + v.y * t;
	dist_2 = dist.x * dist.x + dist.y * dist.y;
	if(dist_2 >= r * r) return;
	/* The first root or zero is when the collision happens. */
	z2 = z.x * z.x + z.y * z.y;
	det = (vz * vz - v2 * (z2 - r * r));
	t = (det <= 0.0f) ? 0.0f : (-vz - sqrtf(det)) / v2;
	if(t < 0.0f) t = 0.0f; /* bounded, dist < r^2; fixme: really want this? */
	assert(collision_matrix[a->vt->class][b->vt->class]);
	collision_matrix[a->vt->class][b->vt->class](a, b, t);
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
 {extrapolate}; needs and consumes {covers}. */
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
			/* Mostly for weapons that ignore collisions with themselves. */
			/*if(!(sprite_get_self_mask(a) & sprite_get_others_mask(b))
				&& !(sprite_get_others_mask(a) & sprite_get_self_mask(b)))
				continue;*/
			/* If the sprites have no collision handler thing, don't bother. */
			if(!collision_matrix[a->vt->class][b->vt->class]) continue;
			/* Pass it to the next LOD. */
			collide_boxes(a, b);
		} while(index_b);
	}
}
