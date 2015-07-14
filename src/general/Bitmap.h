struct Bitmap;

enum BitmapType { B_DONT_KNOW, B_BACKGROUND, B_SPRITE };

struct Bitmap *Bitmap(const int width, const int height, const int depth, const unsigned char *bmp, const enum BitmapType type);
void Bitmap_(struct Bitmap **b_ptr);
int BitmapCompare(const struct Bitmap **a, const struct Bitmap **b);
int BitmapGetWidth(const struct Bitmap *bmp);
int BitmapGetHeight(const struct Bitmap *bmp);
int BitmapGetDepth(const struct Bitmap *bmp);
unsigned char *BitmapGetData(const struct Bitmap *bmp);
enum BitmapType BitmapGetType(const struct Bitmap *bmp);
void BitmapSetImageUnit(struct Bitmap *bmp, const int unit);
int BitmapGetImageUnit(const struct Bitmap *bmp);
