struct Reader;

struct Reader *Reader(const char *fn);
void Reader_(struct Reader **reader_ptr);
void ReaderSetLineNumber(struct Reader *r, int lineNumber);
unsigned ReaderGetLineNumber(const struct Reader *r);
char *ReaderReadLine(struct Reader *r);
const char *ReaderGetFilename(const struct Reader *r);
