
/* must be in the same order */
enum Error {
	E_NO_ERROR,
	E_MEMORY,
	E_NO_DATA,
	E_MAX_RECORDS,
	E_MAX_FIELDS,
	E_SYNTAX,
	E_KEYWORD,
	E_NOT_RECORD,
	E_MAX_LORE,
	E_RECORD
};

void Error(enum Error e);
char *ErrorString(void);
int ErrorIsError(void);
