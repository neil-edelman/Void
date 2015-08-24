struct Type;
struct Reader;

char *TypeGetName(const struct Type *const type);
char *TypeGetTypeName(const struct Type *const type);
int TypeIsLoaded(const struct Type *const type);
int TypeLoader(const struct Type *const type, char **datum_ptr, struct Reader *r);
struct Type *TypeFromString(const char *str);
