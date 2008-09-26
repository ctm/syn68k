#ifndef _parse_h_
#define _parse_h_

#include "token.h"
#include "list.h"
#include "hash.h"

extern int parse_all_expressions (SymbolTable *sym);
extern List *string_to_list (const char *string, const char *include_dirs[]);

#endif  /* Not _parse_h_ */
