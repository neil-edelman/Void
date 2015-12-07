struct Bitmap;

struct Bitmap *Bitmap(const unsigned char *const data, const size_t size);
struct Bitmap *BitmapFile(const char *const fn);
void Bitmap_(struct Bitmap **bitmapPtr);
int BitmapGetWidth(const struct Bitmap *const bmp);
int BitmapGetHeight(const struct Bitmap *const bmp);
int BitmapGetDepth(const struct Bitmap *const bmp);
unsigned char *BitmapGetData(const struct Bitmap *const bmp);
