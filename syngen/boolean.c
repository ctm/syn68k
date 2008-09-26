/*
 *     boolean.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "token.h"
#include "boolean.h"
#include "error.h"


static BoolExpr *create_bool_expr (BoolExprType type, BoolExpr *subexpr);

typedef BoolExpr * (BoolOpHandler)(const List *);
static BoolOpHandler BO_equal, BO_greater_than, BO_less_than, BO_not_equal,
              BO_gr_or_equal, BO_less_or_equal,
              BO_not, BO_and, BO_or, BO_xor;
typedef struct {
  TokenType type;
  BoolOpHandler *func;
} BoolDispatchEntry;

static BoolExpr true_bool_expr  = { E_TRUE, NULL, NULL },
                false_bool_expr = { E_FALSE, NULL, NULL };

BoolExpr *
make_boolean_expr (const List *ls)
{
  char buf[128];
  int i;
  static const BoolDispatchEntry boolop[] = {
    { TOK_EQUAL,            BO_equal         },
    { TOK_GREATER_THAN,     BO_greater_than  },
    { TOK_LESS_THAN,        BO_less_than     },
    { TOK_LESS_OR_EQUAL,    BO_less_or_equal },
    { TOK_GREATER_OR_EQUAL, BO_gr_or_equal   },
    { TOK_NOT_EQUAL,        BO_not_equal     },
    { TOK_NOT,              BO_not           },
    { TOK_AND,              BO_and           },
    { TOK_OR,               BO_or            },
    { TOK_XOR,              BO_xor           } };

  if (ls == NULL)
    parse_error(ls, "Invalid boolean expression.\n");
  else if (ls->car == NULL)
    {
      switch (ls->token.type) {
      case TOK_TRUE:
	return &true_bool_expr;
      case TOK_FALSE:
      case TOK_NIL:
	return &false_bool_expr;
      default:
	break;
      }
      parse_error (ls, "Unknown boolean expression \"%s\".\n",
		   unparse_token (&ls->token, buf));
    }
  else
    {
      for (i = (sizeof boolop / sizeof boolop[0]) - 1; i >= 0; i--)
	if (boolop[i].type == ls->car->token.type)
	  return boolop[i].func (ls->car);
      parse_error (ls, "Unknown boolean operator: \"%s\"\n",
		   unparse_token (&ls->car->token, buf));
    }

  return &false_bool_expr;   /* Default to FALSE, keep compiling */
}


/* =      This can either compare two numerical expressions or two strings */
static BoolExpr *
BO_equal (const List *ls)
{
  if (ls->cdr == NULL || CDDR (ls) == NULL || CDDDR (ls) != NULL)
    {
      parse_error (ls, "= takes two arguments.\n");
      return &false_bool_expr;   /* default = FALSE */
    }

  return tokens_equal (&ls->cdr->token, &(CDDR (ls))->token)
    ? &true_bool_expr : &false_bool_expr;
}


/* <>    This can either compare two numerical expressions or two strings */
static BoolExpr *
BO_not_equal(const List *ls)
{
  if (ls->cdr == NULL || ls->cdr->cdr == NULL || CDDDR (ls) != NULL)
    {
      parse_error (ls, "<> takes two arguments.\n");
      return &false_bool_expr;  /* default = FALSE */
    }

  return !tokens_equal (&ls->cdr->token, &(CDDR (ls))->token)
    ? &true_bool_expr : &false_bool_expr;
}


/* >    Valid only for numerical expressions */
static BoolExpr *
BO_greater_than (const List *ls)
{
  if (ls->cdr == NULL || ls->cdr->cdr == NULL || CDDDR (ls) != NULL)
    {
      parse_error (ls, "> takes two arguments.\n");
      return &false_bool_expr;  /* default = FALSE */
    }
  if (ls->cdr->token.type != TOK_NUMBER || CDDR (ls)->token.type != TOK_NUMBER)
    {
      parse_error (ls,"> can only be used to compare numerical quantities!\n");
      return &false_bool_expr;  /* default = FALSE */
    }

  return (ls->cdr->token.u.n > CDDR (ls)->token.u.n)
    ? &true_bool_expr : &false_bool_expr;
}


/* <    Valid only for numerical expressions */
static BoolExpr *
BO_less_than (const List *ls)
{
  if (ls->cdr == NULL || ls->cdr->cdr == NULL || CDDDR (ls) != NULL)
    {
      parse_error (ls, "< takes two arguments.\n");
      return &false_bool_expr;  /* default = FALSE */
    }
  if (ls->cdr->token.type != TOK_NUMBER || CDDR (ls)->token.type != TOK_NUMBER)
    {
      parse_error (ls,"< can only be used to compare numerical quantities!\n");
      return &false_bool_expr;  /* default = FALSE */
    }

  return (ls->cdr->token.u.n < CDDR (ls)->token.u.n)
    ? &true_bool_expr : &false_bool_expr;
}


/* >=    Valid only for numerical expressions */
static BoolExpr *
BO_gr_or_equal (const List *ls)
{
  if (ls->cdr == NULL || ls->cdr->cdr == NULL || CDDDR (ls) != NULL)
    {
      parse_error (ls, ">= takes two arguments.\n");
      return &false_bool_expr;  /* default = FALSE */
    }
  if (ls->cdr->token.type != TOK_NUMBER || CDDR (ls)->token.type != TOK_NUMBER)
    {
      parse_error (ls,
		   ">= can only be used to compare numerical quantities!\n");
      return &false_bool_expr;  /* default = FALSE */
    }

  return (ls->cdr->token.u.n >= CDDR (ls)->token.u.n)
    ? &true_bool_expr : &false_bool_expr;
}


/* <=   Valid only for numerical expressions */
static BoolExpr *
BO_less_or_equal (const List *ls)
{
  if (ls->cdr == NULL || CDDR (ls) == NULL || CDDDR (ls) != NULL)
    {
      parse_error (ls, "<= takes two arguments.\n");
      return &false_bool_expr;  /* default = FALSE */
    }
  if (ls->cdr->token.type != TOK_NUMBER || CDDR (ls)->token.type != TOK_NUMBER)
    {
      parse_error (ls,
		   "<= can only be used to compare numerical quantities!\n");
      return &false_bool_expr;  /* default = FALSE */
    }

  return (ls->cdr->token.u.n <= CDDR (ls)->token.u.n)
    ? &true_bool_expr : &false_bool_expr;
}


/* not */
static BoolExpr *
BO_not (const List *ls)
{
  BoolExpr *subexpr = make_boolean_expr (ls->cdr);

  if (subexpr == NULL)
    {
      parse_error(ls, "not takes an argument!\n");
      return &false_bool_expr;  /* default = FALSE */
    }
  if (subexpr->type == E_FALSE) return &true_bool_expr;
  if (subexpr->type == E_TRUE ) return &false_bool_expr;
  return create_bool_expr (E_NOT, subexpr);
}

/* and */
static BoolExpr *
BO_and(const List *ls)
{
  BoolExpr *subexpr, *ret;
  BOOL hit_true = FALSE;

  ret = create_bool_expr (E_AND, NULL);
  while ((ls = ls->cdr))
    {
      subexpr = make_boolean_expr (ls);
      if (subexpr->type == E_FALSE)  /* If we ever hit FALSE, expr is FALSE */
	return &false_bool_expr;
      if (subexpr->type != E_TRUE)   /* No sense adding in TRUE statements */
	{
	  subexpr->next = ret->subexpr;
	  ret->subexpr = subexpr;
	}
      else hit_true = TRUE;
    }
  if (ret->subexpr == NULL)
    {
      if (hit_true)   /* If we hit nothing but TRUE's return TRUE */
	return &true_bool_expr;
      parse_error(ls, "and takes arguments!\n");
      return &false_bool_expr;  /* default = FALSE */
    }
  
  if (ret->subexpr->next == NULL) /* (and a) == a */
    return ret->subexpr;   
  
  return ret;
}


/* or */
static BoolExpr *
BO_or (const List *ls)
{
  BOOL hit_false = FALSE;
  BoolExpr *subexpr, *ret;

  ret = create_bool_expr (E_OR, NULL);
  while ((ls = ls->cdr))
    {
      subexpr = make_boolean_expr (ls);
      if (subexpr->type == E_TRUE)
	return &true_bool_expr;
      if (subexpr->type == E_FALSE)  /* No sense adding in FALSE statements*/
	hit_false = TRUE;
      else
	{
	  subexpr->next = ret->subexpr;
	  ret->subexpr = subexpr;
	}
    }
  if (ret->subexpr == NULL)
    {
      if (hit_false)   /* If we hit nothing but FALSE's return FALSE */
	return &false_bool_expr;
      parse_error(ls, "or takes arguments!\n");
      return &false_bool_expr;   /* Default = FALSE */
    }
  
  if (ret->subexpr->next == NULL) /* (or a) == a */
    return ret->subexpr;
  
  return ret;
}
  
/* xor */
static BoolExpr *
BO_xor (const List *ls)
{
  BoolExpr *subexpr, *ret;
  BoolExprType answer = E_FALSE;
  BOOL odd_trues = NO;

  ret = create_bool_expr (E_XOR, NULL);
  while ((ls = ls->cdr))
    {
      subexpr = make_boolean_expr (ls);
      if (subexpr->type == E_TRUE)
	{
	  odd_trues = !odd_trues;

	  if (answer == E_TRUE) answer = E_FALSE;
	  else if (answer == E_FALSE) answer = E_TRUE;
	  else
	    {
	      subexpr->next = ret->subexpr;
	      ret->subexpr = subexpr;
	    }
	}
      else if (subexpr->type != E_FALSE)
	{
	  subexpr->next = ret->subexpr;
	  ret->subexpr = subexpr;
	  answer = E_UNKNOWN;
	}
    }

  if (ret->subexpr == NULL)
    {
      parse_error(ls, "xor takes arguments!\n");
      return &false_bool_expr;   /* Default = FALSE */
    }

  if (answer == E_TRUE)
    return &true_bool_expr;
  else if (answer == E_FALSE)
    return &false_bool_expr;
  
  if (ret->subexpr->next == NULL && !odd_trues) /* (xor a) == a */
    return ret->subexpr;
  if (odd_trues)  /* If there were an odd number of TRUE's, toss one in. */
    {
      subexpr = create_bool_expr (E_TRUE, NULL);
      subexpr->next = ret->subexpr;
      ret->subexpr = subexpr;
    }
  
  return ret;
}
  
static BoolExpr *
create_bool_expr (BoolExprType type, BoolExpr *subexpr)
{
  BoolExpr *be = (BoolExpr *) malloc (sizeof (BoolExpr));
  
  be->type      = type;
  be->subexpr   = subexpr;
  be->next      = NULL;

  return be;
}


/* Given a boolean expression and (un)known ifull and ofull conditions, this
 * function returns E_FALSE, E_TRUE or E_UNKNOWN reflecting the truth or
 * falsity of the boolean expression.
 */

BoolExprType
eval_bool_expr (const BoolExpr *be)
{
  BoolExprType deflt;

  switch (be->type) {
  case E_TRUE:  return E_TRUE;
  case E_FALSE: return E_FALSE;
  case E_NOT:
    switch (eval_bool_expr (be->subexpr)) {
    case E_TRUE:  return E_FALSE;
    case E_FALSE: return E_TRUE;
    default:      return E_UNKNOWN;
    }
  case E_AND:
    deflt = E_TRUE;
    for (be=be->subexpr; be != NULL; be=be->next)
      switch (eval_bool_expr (be)) {
      case E_FALSE:   return E_FALSE;
      case E_UNKNOWN: deflt = E_UNKNOWN; break;   /* Could still be E_FALSE */
      default: break;
      }
    return deflt;
  case E_OR:
    deflt = E_FALSE;
    for (be=be->subexpr; be != NULL; be=be->next)
      switch (eval_bool_expr (be)) {
      case E_TRUE:    return E_TRUE;
      case E_UNKNOWN: deflt = E_UNKNOWN; break;   /* Could still be E_TRUE */
      default: break;
      }
    return deflt;
  default: return E_UNKNOWN;
  }
}


void
print_boolean_expr (const BoolExpr *be)
{
  switch (be->type) {
  case E_TRUE:    printf ("true ");   break;
  case E_FALSE:   printf ("false ");   break;
  case E_UNKNOWN: printf ("? ");    break;
  case E_NOT:
    printf ("(not ");
    print_boolean_expr (be->subexpr);
    printf (") ");
    break;
  case E_AND:
    printf ("(and ");
    for (be=be->subexpr; be != NULL; be=be->next)
      print_boolean_expr (be);
    printf (") ");
    break;
  case E_OR:
    printf ("(or ");
    for (be=be->subexpr; be != NULL; be=be->next)
      print_boolean_expr (be);
    printf (") ");
    break;
  case E_XOR:
    printf ("(xor ");
    for (be=be->subexpr; be != NULL; be=be->next)
      print_boolean_expr (be);
    printf (") ");
    break;
  }
}
