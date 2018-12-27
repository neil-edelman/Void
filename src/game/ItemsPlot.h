/* This is a debug thing; outputs data in Gnuplot form. */

/* This is for communication with {sprite_data}, {sprite_arrow},
 {sprite_velocity}, and {gnu_draw_bins} for outputting a gnu plot. */
struct PlotData {
	FILE *fp;
	unsigned i, n, object, bin;
	const char *colour;
};
/** @implements <Sprite, OutputData>DiAction */
static void item_count(struct Item *item, void *const void_out) {
	struct PlotData *const out = void_out;
	(void)item;
	out->n++;
}
/** @implements <Sprite, OutputData>DiAction */
static void print_item_data(struct Item *item, void *const void_out) {
	struct PlotData *const out = void_out;
	char a[12];
	item_to_string(item, &a);
	fprintf(out->fp, "%f\t%f\t%f\t%f\t%f\t%f\t%f\t\"%s\"\n",
		item->x.x, item->x.y,
		item->bounding, (double)out->i++ / out->n, item->v.x, item->v.y, /*??*/
		item->bounding, a);
}
/** @implements <Sprite, OutputData>DiAction */
static void print_item_velocity(struct Item *item, void *const void_out) {
	struct PlotData *const out = void_out;
	fprintf(out->fp, "set arrow from %f,%f to %f,%f lw 1 lc rgb \"blue\" "
		"front;\n", item->x.x, item->x.y,
		item->x.x + item->v.x * items.dt_ms * 256.0f,
		item->x.y + item->v.y * items.dt_ms * 256.0f);
}
/* @implements <Cover>BiAction */
static void item_to_bin(struct Cover *const cover, void *const plot_void) {
	struct PlotData *const plot = plot_void;
	struct Item *s;
	struct Vec2f to = { 0.0f, 0.0f };
	assert(cover && plot_void);
	/* @fixme Uhmmmmm, how? */
	if(!(s = cover_to_item(cover))) return;
	/*if(!(s = cover->proxy_index->item)) return;*/
	LayerGetBinMarker(items.layer, plot->bin, &to);
	to.x += 50.0f, to.y += 50.0f;
	fprintf(plot->fp, "set arrow from %f,%f to %f,%f lw 1 lc rgb \"%s\" "
		"front;\n", s->x.x, s->x.y, to.x, to.y,
		cover->is_corner ? "red" : "pink");
}
/* @implements LayerAcceptPlot */
static void item_to_bin_bin(const unsigned idx, struct PlotData *const plot) {
	assert(plot);
	plot->bin = idx;
	/* @fixme CoverPoolBiForEach(items.bins[idx].covers, &item_to_bin, plot);*/
}
/** Draws squares for highlighting bins. Called in \see{space_plot}.
 @implements LayerAcceptPlot */
static void gnu_shade_bins(const unsigned bin, struct PlotData *const plot) {
	struct Vec2f marker = { 0.0f, 0.0f };
	assert(plot && bin < LAYER_SIZE);
	LayerGetBinMarker(items.layer, bin, &marker);
	fprintf(plot->fp, "# bin %u -> %.1f,%.1f\n", bin, marker.x, marker.y);
	fprintf(plot->fp, "set object %u rect from %f,%f to %f,%f fc rgb \"%s\" "
		"fs transparent pattern 4 noborder;\n", plot->object++,
		marker.x, marker.y, marker.x + 256.0f, marker.y + 256.0f, plot->colour);
}

/** Debugging plot.
 @implements Action */
static void space_plot(void) {
	FILE *data = 0, *gnu = 0;
	const char *data_fn = "Space.data", *gnu_fn = "Space.gnu",
		*eps_fn = "Space.eps";
	enum { E_NO, E_DATA, E_GNU } e = E_NO;
	fprintf(stderr, "Will output %s at the current time, and a gnuplot script, "
		"%s of the current items.\n", data_fn, gnu_fn);
	do {
		struct PlotData plot;
		unsigned i;
		if(!(data = fopen(data_fn, "w"))) { e = E_DATA; break; }
		if(!(gnu = fopen(gnu_fn, "w")))   { e = E_GNU;  break; }
		plot.fp = data, plot.i = 0; plot.n = 0;
		for(i = 0; i < LAYER_SIZE; i++)
			ItemListBiForEach(&items.bins[i].items, &item_count, &plot);
		for(i = 0; i < LAYER_SIZE; i++)
			ItemListBiForEach(&items.bins[i].items, &print_item_data,
			&plot);
		fprintf(gnu, "set term postscript eps enhanced size 256cm, 256cm\n"
			"set output \"%s\"\n"
			"set size square;\n"
			"set palette defined (1 \"#0000FF\", 2 \"#00FF00\", 3 \"#FF0000\");"
			"\n"
			"set xtics 256 rotate; set ytics 256;\n"
			"set grid;\n"
			"set xrange [-8192:8192];#[-2048:2048];\n"
			"set yrange [-8192:8192];#[-2048:2048];\n"
			"set cbrange [0.0:1.0];\n", eps_fn);
		/* draw bins as squares behind */
		fprintf(gnu, "set style fill transparent solid 0.3 noborder;\n");
		plot.fp = gnu, plot.colour = "#ADD8E6", plot.object = 1;
		LayerForEachScreenPlot(items.layer, &gnu_shade_bins, &plot);
		/*col.fp = gnu, col.colour = "#D3D3D3";
		BinPoolBiForEach(update_bins, &gnu_shade_bins, &col);*/
		/* draw arrows from each of the items to their bins */
		plot.fp = gnu, plot.i = 0;
		for(i = 0; i < LAYER_SIZE; i++)
			ItemListBiForEach(&items.bins[i].items,
			&print_item_velocity, &plot);
		LayerForEachScreenPlot(items.layer, &item_to_bin_bin, &plot);
		/* draw the items */
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

/** Sets a flag that indicates the space should be plotted on the next frame. */
void ItemsPlotSpace(void) {
	items.plot |= PLOT_SPACE;
	EventsRunnable(0, &TimerPause); /* Delay this until it has time to plot. */
}
