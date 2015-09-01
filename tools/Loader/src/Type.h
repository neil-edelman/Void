struct Type;
struct Reader;

char *TypeGetName(const struct Type *const type);
char *TypeGetTypeName(const struct Type *const type);
int TypeIsLoaded(const struct Type *const type);
int TypeLoader(const struct Type *const type, char **datum_ptr, struct Reader *r);
int TypeComparator(const struct Type *const type, const void *s1, const void *s2);
struct Type *TypeFromString(const char *str);
void TypeResetAutoincrement(void);
int TypePrintData(const struct Type *const type, const char *const*const datum_ptr);
struct Type *TypeGetForeign(const char *const fkstr);
