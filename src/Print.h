/* defaut is to print debug messages; turn it off by defining PRINT_NDEBUG */
#ifndef PRINT_NDEBUG
#define PRINT_DEBUG
#endif

/* default is not to print pedantic messages; turn them on PRINT_PEDANTIC */

void Warn(const char *format, ...);
void Info(const char *format, ...);

#ifndef PRINT_DEBUG
#define Debug(fmt, ...) ((void)0)
#else
void Debug(const char *format, ...);
#endif

#ifndef PRINT_PEDANTIC
#define Pedantic(fmt, ...) ((void)0)
#else
void Pedantic(const char *format, ...);
#endif
