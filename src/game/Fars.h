struct Fars;

void Fars_(struct Fars **const thisp);
struct Fars *Fars(void);
void FarsClear(struct Fars *const this);
struct Planetoid *FarsPlanetoid(struct Fars *const this,
	const struct AutoObjectInSpace *const class);
