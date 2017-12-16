#if 0

/** @implements <Bin*, InfoOutput*>DiAction */
static void draw_bin_literally(struct SpriteList **const pthis, void *const pout_void) {
	struct SpriteList *const sprites = *pthis;
	const InfoOutput *const pout = pout_void, out = *pout;
	const struct AutoImage *hair = AutoImageSearch("Hair.png");
	struct Vec2i bin_in_space;
	struct Vec2f bin_float;
	assert(pthis && sprites && out && hair);
	bin_to_Vec2i((unsigned)(sprites - bins), &bin_in_space);
	bin_float.x = bin_in_space.x, bin_float.y = bin_in_space.y;
	out(&bin_float, hair);
}
/** Must call \see{SpriteUpdate} before this, because it updates everything.
 Use when the Info GPU shader is loaded. */
void SpritesDrawInfo(InfoOutput draw) {
	/*const struct Vec2f a = { 10.0f, 10.0f };*/
	/*const struct AutoImage *hair = AutoImageSearch("Hair.png");*/
	assert(draw);
	BinPoolBiForEach(draw_bins, &draw_bin_literally, &draw);
	/*draw(&a, hair);*/
}
#endif




/* This is a debug thing. */

/* This is for communication with {sprite_data}, {sprite_arrow},
 {sprite_velocity}, and {gnu_draw_bins} for outputting a gnu plot. */
struct PlotData {
	FILE *fp;
	unsigned i, n, object;
	const char *colour;
};
/** @implements <Sprite, OutputData>DiAction */
static void sprite_count(struct Sprite *sprite, void *const void_out) {
	struct PlotData *const out = void_out;
	UNUSED(sprite);
	out->n++;
}
/** @implements <Sprite, OutputData>DiAction */
static void print_sprite_data(struct Sprite *sprite, void *const void_out) {
	struct PlotData *const out = void_out;
	char a[12];
	sprite_to_string(sprite, &a);
	fprintf(out->fp, "%f\t%f\t%f\t%f\t%f\t%f\t%f\t\"%s\"\n",
		sprite->x.x, sprite->x.y,
		sprite->bounding, (double)out->i++ / out->n, sprite->x.x, sprite->x.y,
		sprite->bounding, a);
}
/** @implements <Sprite, OutputData>DiAction */
static void print_sprite_velocity(struct Sprite *sprite, void *const void_out) {
	struct PlotData *const out = void_out;
	fprintf(out->fp, "set arrow from %f,%f to %f,%f lw 1 lc rgb \"blue\" "
		"front;\n", sprite->x.x, sprite->x.y,
		sprite->x.x + sprite->v.x * sprites->dt_ms * 1000.0f,
		sprite->x.y + sprite->v.y * sprites->dt_ms * 1000.0f);
}
/** Draws squares for highlighting bins. Called in \see{space_plot}.
 @implements LayerAcceptPlot */
static void gnu_shade_bins(const unsigned bin, struct PlotData *const plot) {
	struct Vec2i bin2i;
	assert(plot && sprites && bin < LAYER_SIZE);
	LayerGetBin2(sprites->layer, bin, &bin2i);
	fprintf(plot->fp, "# bin %u -> %d,%d\n", bin, bin2i.x, bin2i.y);
	fprintf(plot->fp, "set object %u rect from %d,%d to %d,%d fc rgb \"%s\" "
			"fs solid noborder;\n", plot->object++, bin2i.x, bin2i.y,
			bin2i.x + 256, bin2i.y + 256, plot->colour);
}

/** Debugging plot.
 @implements Action */
static void space_plot(void) {
	FILE *data = 0, *gnu = 0;
	const char *data_fn = "Space.data", *gnu_fn = "Space.gnu",
		*eps_fn = "Space.eps";
	enum { E_NO, E_DATA, E_GNU } e = E_NO;
	assert(sprites);
	fprintf(stderr, "Will output %s at the current time, and a gnuplot script, "
		"%s of the current sprites.\n", data_fn, gnu_fn);
	do {
		struct PlotData plot;
		unsigned i;
		if(!(data = fopen(data_fn, "w"))) { e = E_DATA; break; }
		if(!(gnu = fopen(gnu_fn, "w")))   { e = E_GNU;  break; }
		plot.fp = data, plot.i = 0; plot.n = 0;
		for(i = 0; i < LAYER_SIZE; i++)
			SpriteListBiForEach(&sprites->bins[i].sprites, &sprite_count,&plot);
		for(i = 0; i < LAYER_SIZE; i++)
			SpriteListBiForEach(&sprites->bins[i].sprites, &print_sprite_data,
			&plot);
		fprintf(gnu, "set term postscript eps enhanced size 20cm, 20cm\n"
			"set output \"%s\"\n"
			"set size square;\n"
			"set palette defined (1 \"#0000FF\", 2 \"#00FF00\", 3 \"#FF0000\");"
			"\n"
			"set xtics 256 rotate; set ytics 256;\n"
			"set grid;\n"
			"set xrange [-2048:2048];#[-8192:8192];\n"
			"set yrange [-2048:2048];#[-8192:8192];\n"
			"set cbrange [0.0:1.0];\n", eps_fn);
		/* draw bins as squares behind */
		fprintf(gnu, "set style fill transparent solid 0.3 noborder;\n");
		plot.fp = gnu, plot.colour = "#ADD8E6", plot.object = 1;
		LayerForEachScreenPlot(sprites->layer, &gnu_shade_bins, &plot);
		/*col.fp = gnu, col.colour = "#D3D3D3";
		BinPoolBiForEach(update_bins, &gnu_shade_bins, &col);*/
		/* draw arrows from each of the sprites to their bins */
		plot.fp = gnu, plot.i = 0;
		for(i = 0; i < LAYER_SIZE; i++)
			SpriteListBiForEach(&sprites->bins[i].sprites,
			&print_sprite_velocity, &plot);
		/* draw the sprites */
		fprintf(gnu, "plot \"%s\" using 5:6:7 with circles \\\n"
			"linecolor rgb(\"#00FF00\") fillstyle transparent "
			"solid 1.0 noborder title \"Velocity\", \\\n"
			"\"%s\" using 1:2:3:4 with circles \\\n"
			"linecolor palette fillstyle transparent solid 0.3 noborder \\\n"
			"title \"Sprites\", \\\n"
			"\"%s\" using 1:2:8 with labels notitle;\n",
			data_fn, data_fn, data_fn);
	} while(0); switch(e) {
		case E_NO: break;
		case E_DATA: perror(data_fn); break;
		case E_GNU:  perror(gnu_fn);  break;
	} {
		if(fclose(data) == EOF) perror(data_fn);
		if(fclose(gnu)  == EOF) perror(gnu_fn);
	}
}

void SpritesPlotSpace(void) {
	if(!sprites) return;
	sprites->plots |= PLOT_SPACE;
	TimerPause();
}

#if 0
/** Output sprites. */
void SpritesOut(void) {
	struct OutputData out;
	unsigned i;
	out.fp = stdout, out.i = 0; out.n = 0;
	for(i = 0; i < BIN_BIN_FG_SIZE; i++)
		SpriteListUnorderBiForEach(bins + i, &sprite_count, &out);
	for(i = 0; i < BIN_BIN_FG_SIZE; i++)
		SpriteListUnorderBiForEach(bins + i, &print_sprite_data, &out);
}

#endif

/* THIS IS OLD CODE */

#if 0

/** Define {Ship} as a subclass of {Sprite}. */
struct Ship {
	struct SpriteListNode sprite;
	const struct AutoShipClass *class;
	unsigned     ms_recharge_wmd; /* ms */
	unsigned     ms_recharge_hit; /* ms */
	struct Event *event_recharge;
	int          hit, max_hit; /* GJ */
	float        max_speed2;
	float        acceleration;
	float        turn; /* deg/s -> rad/ms */
	float        turn_acceleration;
	float        horizon;
	enum SpriteBehaviour behaviour;
	char         name[16];
};
/* Define ShipPool. */
#define POOL_NAME Ship
#define POOL_TYPE struct Ship
#include "../templates/Pool.h"

/** Define {Wmd} as a subclass of {Sprite}. */
struct Wmd {
	struct SpriteListNode sprite;
	const struct AutoWmdType *class;
	const struct Sprite *from;
	float mass;
	unsigned expires;
	unsigned light;
};
/* Define ShipPool. */
#define POOL_NAME Wmd
#define POOL_TYPE struct Wmd
#include "../templates/Pool.h"

/** @implements <Ship>Action */
static void ship_out(struct Ship *const sprites) {
	assert(sprites);
	printf("Ship at (%f, %f) in %u integrity %u/%u.\n", sprites->sprite.data.r.x,
		   sprites->sprite.data.r.y, sprites->sprite.data.bin, sprites->hit, sprites->max_hit);
}
/** @implements <Ship>Action */
static void ship_delete(struct Ship *const sprites) {
	/*char a[12];*/
	assert(sprites);
	/*Ship_to_string(this, &a);
	 printf("Ship_delete %s#%p.\n", a, (void *)&this->sprite.data);*/
	Event_(&sprites->event_recharge);
	sprite_remove(&sprites->sprite.data);
	ShipPoolRemove(ships, sprites);
}
/** @implements <Ship>Action */
static void ship_death(struct Ship *const sprites) {
	sprites->hit = 0;
	/* fixme: explode */
}
/** @implements <Ship>Predicate */
static void ship_action(struct Ship *const sprites) {
	if(sprites->hit <= 0) ship_death(sprites);
}

/** @implements <Wmd>Action */
static void wmd_out(struct Wmd *const sprites) {
	assert(sprites);
	printf("Wmd%u.\n", (WmdListNode *)sprites - wmds);
}
/** @implements <Wmd>Action */
static void wmd_delete(struct Wmd *const sprites) {
	/*char a[12];*/
	assert(sprites);
	/*printf("wmd_delete %s#%p.\n", a, (void *)&this->sprite.data);*/
	sprite_remove(&sprites->sprite.data);
	WmdPoolRemove(gates, sprites);
	/* fixme: explode */
}
/** @implements <Wmd>Action */
static void wmd_death(struct Wmd *const sprites) {
	fprintf(stderr, "Tryi to %s.\n", sprites->);
}
/** @implements <Wmd>Action */
static void wmd_action(struct Wmd *const sprites) {
	UNUSED(sprites);
}

/** @implements <Gate>Action */
static void gate_out(struct Gate *const sprites) {
	assert(sprites);
	printf("Gate at (%f, %f) in %u goes to %s.\n", sprites->sprite.data.r.x,
		   sprites->sprite.data.r.y, sprites->sprite.data.bin, sprites->to->name);
}
/** @implements <Gate>Action */
static void gate_delete(struct Gate *const sprites) {
	/*char a[12];*/
	assert(sprites);
	/*Gate_to_string(this, &a);
	 printf("Gate_delete %s#%p.\n", a, (void *)&this->sprite.data);*/
	sprite_remove(&sprites->sprite.data);
	GatePoolRemove(gates, sprites);
	/* fixme: explode */
}
/** @implements <Gate>Action */
static void gate_death(struct Gate *const sprites) {
	fprintf(stderr, "Trying to destroy gate to %s.\n", sprites->to->name);
}
/** @implements <Gate>Action */
static void gate_action(struct Gate *const sprites) {
	UNUSED(sprites);
}



/** @implements <Ship>Action */
static void ship_recharge(struct Ship *const sprites) {
	if(!sprites) return;
	debug("ship_recharge: %s %uGJ/%uGJ\n", sprites->name, sprites->hit,sprites->max_hit);
	if(sprites->hit >= sprites->max_hit) return;
	sprites->hit++;
	/* this checks if an Event is associated to the sprite, we momentarily don't
	 have an event, even though we are getting one soon! alternately, we could
	 call Sprite::recharge and not stick the event down there.
	 SpriteRecharge(a, 1);*/
	if(sprites->hit >= sprites->max_hit) {
		debug("ship_recharge: %s shields full %uGJ/%uGJ.\n",
			  sprites->name, sprites->hit, sprites->max_hit);
		return;
	}
	Event(&sprites->event_recharge, sprites->ms_recharge_hit, shp_ms_sheild_uncertainty, FN_CONSUMER, &ship_recharge, sprites);
}
void ShipRechage(struct Ship *const sprites, const int recharge) {
	if(recharge + sprites->hit >= sprites->max_hit) {
		sprites->hit = sprites->max_hit; /* cap off */
	} else if(-recharge < sprites->hit) {
		sprites->hit += (unsigned)recharge; /* recharge */
		if(!sprites->event_recharge
		   && sprites->hit < sprites->max_hit) {
			pedantic("SpriteRecharge: %s beginning charging cycle %d/%d.\n", sprites->name, sprites->hit, sprites->max_hit);
			Event(&sprites->event_recharge, sprites->ms_recharge_hit, 0, FN_CONSUMER, &ship_recharge, sprites);
		}
	} else {
		sprites->hit = 0; /* killed */
	}
}


/** See \see{GateFind}.
 @implements <Gate>Predicate */
static int find_gate(struct Gate *const sprites, void *const vgate) {
	struct Gate *const gate = vgate;
	return sprites != gate;
}
/** Linear search for {gates} that go to a specific place. */
struct Gate *GateFind(struct AutoSpaceZone *const to) {
	pedantic("GateIncoming: SpaceZone to: #%p %s.\n", to, to->name);
	GatePoolPoolParam(gates, to);
	return GatePoolShortCircuit(gates, &find_gate);
}

#endif
