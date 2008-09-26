#ifndef _reduce_h_
#define _reduce_h_

#include "list.h"
#include "hash.h"

extern List *reduce_list (List *ls, const SymbolTable *sym, int macro_level);
extern List *reduce (List *ls, const SymbolTable *sym, int macro_level);

#endif  /* Not _reduce_h_ */
