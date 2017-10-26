struct SpriteList;
struct Rectangle4f;
struct SpriteBins;
struct FarBins;

void SpriteBins_(struct SpriteBins **const pthis);
void FarBins_(struct FarBins **const pthis);
struct SpriteBins *SpriteBins(void);
struct FarBins *FarBins(void);
void SpriteBinsClear(struct SpriteBins *const this);
void FarBinsClear(struct FarBins *const this);
int SpriteBinsAdd(struct SpriteBins *const this,
	struct SpriteList **const bins, struct Rectangle4f *const rect);
int FarBinsAdd(struct FarBins *const this, struct Rectangle4f *const rect);
const struct SpriteList *SpriteBinsTake(struct SpriteBins *const this);
