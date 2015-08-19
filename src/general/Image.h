struct Image;

struct Image *Image(const int width, const int height, const int depth, const unsigned char *bmp);
void Image_(struct Image **i_ptr);
int ImageGetWidth(const struct Image *);
int ImageGetHeight(const struct Image *);
int ImageGetDepth(const struct Image *);
unsigned char *ImageGetData(const struct Image *);
void ImageSetTexture(struct Image *, const int);
int ImageGetTexture(const struct Image *);
void ImagePrint(const struct Image *img, FILE *const fp);
