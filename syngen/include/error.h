#ifndef _error_h_
#define _error_h_

#include "parse.h"

extern void error (const char *fmt, ...);
#if defined(__GNUC__) && !defined(__STRICT_ANSI__)
extern volatile void fatal_error (const char *fmt, ...);
extern volatile void fatal_parse_error (const List *ls, const char *fmt, ...);
extern volatile void fatal_input_error (const char *fmt, ...);
#else
extern void fatal_error (const char *fmt, ...);
extern void fatal_parse_error (const List *ls, const char *fmt, ...);
extern void fatal_input_error (const char *fmt, ...);
#endif
extern void parse_error (const List *ls, const char *fmt, ...);
extern void input_error (const char *fmt, ...);

#endif  /* Not _error_h_ */
