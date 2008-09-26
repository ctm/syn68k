#ifndef _macro_h_
#define _macro_h_

#include "list.h"
#include "hash.h"

#define MAX_MACRO_EXPANSIONS 64

typedef struct MacroStruct {
  List *expr;
  struct MacroStruct *next;
} Macro;

extern BOOL macro_sub (List *ls, const SymbolTable *sym, BOOL flag_err);

#endif  /* Not _macro_h_ */
