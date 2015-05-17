struct Resources;

struct Bitmap {
	char *name;
	int  width;
	int  height;
	int  depth;
	int  id;
};

struct TypeOfObject {
	char          *name; /* index */
	char          *bmp_name;
	struct Bitmap *bmp;
};

struct ObjectsInSpace {
	int                 id; /* index */
	char                *type_name;
	struct TypeOfObject *type;
	struct Vec2f        position;
};

struct ShipClass {
	char          *name;
	char          *bmp_name; /* index */
	struct Bitmap *bmp;
	int           shield;
	int           msShield;
};

struct Alignment {
	char             *name;
	char             *hates_name;
	struct Alignment *hates;
	char             *likes_name;
	struct Alignment *likes;
	/* nestled? */
};

struct Resources *Resources(void);
void Resources_(struct Resources **r_ptr);
struct Bitmap *ResourcesFindBitmap(const struct Resources *r, const char *name);
struct ObjectsInSpace *ResourcesEnumObjectsInSpace(struct Resources *r);
