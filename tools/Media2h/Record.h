/* want it to be the same as Lore */
#define MAX_FIELDS (64)

struct Record;
/*struct Field;*/
struct Type;

enum TypeCode {
	T_UNKNOWN,
	T_SPACER,
	T_AI,
	T_FLOAT,
	T_FK,
	T_IMAGE,
	T_INT,
	T_STRING,
	T_TEXT
};

int Record(const char *const fn);
void Record_(void);
char *RecordGetName(const struct Record *const r);
/*char *RecordFieldType(const struct Field *const field);*/
void RecordOutput(void);
int RecordLoadInstance(const struct Record *const r, char *data[MAX_FIELDS], struct Reader *read);
void RecordEraseInstance(const struct Record *const record, char *data[MAX_FIELDS]);
/*int RecordNextField(struct Record *r, struct Field **field_ptr);*/
struct Record *RecordSearch(const char *str);
