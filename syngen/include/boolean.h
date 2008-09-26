#ifndef _boolean_h_
#define _boolean_h_

#include "list.h"

typedef enum {
  E_FALSE, E_TRUE, E_UNKNOWN, E_AND, E_OR, E_XOR, E_NOT
  } BoolExprType;

typedef struct BoolExprStruct {
  BoolExprType type;
  struct BoolExprStruct *subexpr;
  struct BoolExprStruct *next;
} BoolExpr;

extern BoolExpr *make_boolean_expr (const List *ls);
extern BoolExprType eval_bool_expr (const BoolExpr *be);
extern void print_boolean_expr (const BoolExpr *be);

#endif  /* not _boolean_h_ */
