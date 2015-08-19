/* resouces in media/Resouces.tsv have their code here; also, individual
 resouces also must have their own .tsv, which tsv2h converts to .h; to add a
 new type of resource:
 1. add a line to media/Resouces.tsv; eg, foo_bar	autoincrement	int
 2. add a new .tsv file with the same name; eg, foo_bar.tsv, containing ints
 3. add a new type to here; eg, FooBar { int id, foo; }
 4. add a new entry in the Makefile under TSV; eg, foo_bar
 5. go to Resouces.c and follow along. */

struct Resources;

struct Bitmap;

struct TypeOfObject {
	char          *name; /* index */
	char          *bmp_name;
	struct Bitmap *bmp;
};

struct ObjectsInSpace {
	int                 id; /* index */
	char                *type_name;
	struct TypeOfObject *type;
	int                 x, y;
};

struct ShipClass {
	char          *name;
	char          *bmp_name; /* index */
	struct Bitmap *bmp;
	int           shield;
	int           ms_shield;
};

struct Alignment {
	char             *name;
	char             *hates_name;
	struct Alignment *hates;
	char             *likes_name;
	struct Alignment *likes;
	/* nestled? */
};

int Resources(void);
void Resources_(void);
struct Map *ResourcesGetImages(void);
