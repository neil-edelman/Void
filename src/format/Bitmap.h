struct Bitmap *Bitmap();
struct Bitmap *BitmapFile(const char *const fn);
void Bitmap_(struct Bitmap **bitmapPtr);
int BitmapGetWidth(const struct Bitmap *bmp);
int BitmapGetHeight(const struct Bitmap *bmp);
int BitmapGetDepth(const struct Bitmap *bmp);
unsigned char *BitmapGetData(const struct Bitmap *bmp);	
int BitmapRead(struct Bitmap *bmp, const char *fn);
