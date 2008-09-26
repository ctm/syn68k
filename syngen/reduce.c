/*
 *    reduce.c
 */

#include <stdlib.h>
#include "reduce.h"
#include "token.h"
#include "list.h"
#include "error.h"
#include "macro.h"

static void reduce_arithmetic_op (List *ls);
static void reduce_relational_op (List *ls);
/* static void reduce_swap (List *ls); */
static void reduce_boolean_op (List *ls);
static BOOL reduce_if (List *ls, const SymbolTable *sym, int macro_level);
static BOOL reduce_switch (List *list, const SymbolTable *sym,
			   int macro_level);
static void set_list_to_boolean (List *ls, TokenType type);


/* This is a helper function for reduce (below) although it may be called
 * independently.  You would call this with the first element in a list
 * as opposed to the list as a whole.  This will cdr down the list, reducing
 * everything it encounters.  It will return the resulting list, which may
 * be NULL if everything in the list reduced to nothing.
 */

List *
reduce_list (List *ls, const SymbolTable *sym, int macro_level)
{
  List *tmp, **last, *ret = ls;

  for (tmp = ls, last = &ret; tmp != NULL; tmp = tmp->cdr)
    {
      reduce (tmp, sym, macro_level);
      if (tmp->token.type == TOK_EMPTY)   /* Was it deleted? */
	{
	  *last = tmp->cdr;
	  continue;
	}
      last = &tmp->cdr;
    }

  return ret;
}


/* This hefty procedure performs reduction of compile-time evaluatable
 * expressions, like (+ 2 3), (if (= 0 1) 4 2), etc.
 * As well as performing macro substition.  Returns ls.
 */

List *
reduce (List *ls, const SymbolTable *sym, int macro_level)
{
  TokenType operator_type;

  /* Ignore NULL lists. */
  if (ls == NULL)
    return NULL;

  /* See if our macro expansion depth is too high. */
  if (macro_level >= MAX_MACRO_EXPANSIONS)
    {
      parse_error (ls, "Too many levels of macro expansion.\n");
      return ls;
    }

  /* Perform macro substitution on identifiers. */
  if (ls->token.type == TOK_IDENTIFIER)
    {
      if (macro_sub (ls, sym, FALSE))
	reduce (ls, sym, macro_level + 1);
      return ls;
    }

  /* Make sure this is a list with stuff in it before proceeding. */
  if (ls->token.type != TOK_LIST || ls->car == NULL)
    return ls;

  operator_type = ls->car->token.type;

  /* if's are special in that they must evaluate their boolean predicate
   * before evaluating the expressions.  Otherwise, you can go into infinite
   * recursion evaluating a clause that would not be executed.
   */
  if (operator_type == TOK_IF && reduce_if (ls, sym, macro_level))
    {
      return reduce (ls, sym, macro_level);
    }

  /* Reduce all the arguments (eval). */
  ls->car = reduce_list (ls->car, sym, macro_level);
  if (ls->car == NULL)
    return ls;

  /* Now reduce the list as a whole (apply). */
  switch (operator_type) {

  case TOK_PLUS:
  case TOK_MINUS:
  case TOK_MULTIPLY:
  case TOK_DIVIDE:
  case TOK_MOD:
  case TOK_BITWISE_AND:
  case TOK_BITWISE_OR:
  case TOK_BITWISE_XOR:
  case TOK_BITWISE_NOT:
  case TOK_SHIFT_LEFT:
  case TOK_SHIFT_RIGHT:
    reduce_arithmetic_op (ls);
    break;

  case TOK_EQUAL:
  case TOK_NOT_EQUAL:
  case TOK_GREATER_THAN:
  case TOK_LESS_THAN:
  case TOK_GREATER_OR_EQUAL:
  case TOK_LESS_OR_EQUAL:
    reduce_relational_op (ls);
    break;

  case TOK_AND:
  case TOK_OR:
  case TOK_XOR:
  case TOK_NOT:
    reduce_boolean_op (ls);
    break;
/*
  case TOK_SWAP:
    reduce_swap (ls);
    break;
*/
  case TOK_NUMARGS:
    ls->token.type = TOK_NUMBER;
    ls->token.u.n = list_length (ls->car->cdr);
    ls->car = NULL;
    break;

  case TOK_IF:
    reduce_if (ls, sym, macro_level);
    break;
    
  case TOK_SWITCH:
    reduce_switch (ls, sym, macro_level);
    break;

  case TOK_IDENTIFIER:
    if (macro_sub (ls, sym, TRUE))
      reduce (ls, sym, macro_level + 1);
    else  /* Failed to substitute!  Make it a NOP. */
      {
	ls->car = NULL;
	ls->token.type = TOK_NOP;
	ls->token.u.string = "nop";
      }
    break;

  default:
    /* FIXME */
    break;
  }

  return ls;
}


/* Several arithmetic operations are possible: +*-/^&|~ mod abs.
 * When it makes sense, these ops may be applied to many arguments;
 * for example (* 1 2 3) equals 6, and (/ 16 2 4) equals 2.  They may also
 * be applied to one argument; (+ 2) = 2, (- 9) = 9, (/ 5) = 5, etc.
 * All arithmetic is integer only.  Division always rounds toward zero,
 * and (mod a b) always returns the smallest non-negative integer y such that
 * bk + y = a for some y.
 */

static void
reduce_arithmetic_op (List *list)
{
  List *ls = list->car, *tmp;
  long val, shift;

  ls = list->car;

  /* Make sure there are some arguments. */
  if (ls->cdr == NULL)
    {
      parse_error (ls, "Not enough arguments to %s operator.\n",
		   ls->token.u.string);
      return;
    }

  /* Make sure all the arguments reduced to numeric constants. */
  for (tmp = ls->cdr; tmp != NULL; tmp = tmp->cdr)
    if (tmp->token.type != TOK_NUMBER)
      return;

  /* Default values. */
  list->token.type = TOK_NUMBER;
  list->car = NULL;
  list->token.u.n = 0;

  val = ls->cdr->token.u.n;

#define APPLY_TO_LIST(OP) \
  for (tmp = CDDR (ls); tmp != NULL; tmp = tmp->cdr) \
    val OP tmp->token.u.n;

  switch (ls->token.type) {
    
    /* First the trivial ones. */
  case TOK_PLUS:
    APPLY_TO_LIST (+=);
    break;
  case TOK_MINUS:
    APPLY_TO_LIST (-=);
    break;
  case TOK_MULTIPLY:
    APPLY_TO_LIST (*=);
    break;
  case TOK_BITWISE_AND:
    APPLY_TO_LIST (&=);
    break;
  case TOK_BITWISE_OR:
    APPLY_TO_LIST (|=);
    break;
  case TOK_BITWISE_XOR:
    APPLY_TO_LIST (^=);
    break;

    /* The single argument ones. */
  case TOK_BITWISE_NOT:
    if (CDDR (ls) != NULL)
      {
	parse_error (CDDR (ls), "Too many arguments to %s operator.\n",
		     ls->token.u.string);
	return;
      }
    val = ~val;
    break;
#if 0
  case TOK_ABS:
    if (CDDR (ls) != NULL)
      {
	parse_error (CDDR (ls), "Too many arguments to %s operator.\n",
		     ls->token.u.string);
	return;
      }
    if (val < 0)
      val = -val;
    break;
#endif

  case TOK_SHIFT_LEFT:
    shift = (CDDR (ls))->token.u.n;
    if (shift >= 0)
      val <<= shift;
    else val >>= -shift;
    break;
  case TOK_SHIFT_RIGHT:
    shift = (CDDR (ls))->token.u.n;
    if (shift >= 0)
      val >>= shift;
    else val <<= -shift;
    break;

    /* Finally divide and mod. */
  case TOK_DIVIDE:
    for (tmp = CDDR (ls); tmp != NULL; tmp = tmp->cdr)
      {
	if (tmp->token.u.n == 0)
	  {
	    parse_error (tmp, "Division by zero.\n");
	    return;
	  }
	val = ldiv (val, tmp->token.u.n).quot;
      }
    break;
  case TOK_MOD:
    for (tmp = CDDR (ls); tmp != NULL; tmp = tmp->cdr)
      {
	if (tmp->token.u.n == 0)
	  {
	    parse_error (tmp, "Division by zero.\n");
	    return;
	  }
	val = ldiv (val, tmp->token.u.n).rem;
	while (val < 0)
	  val += tmp->token.u.n;
      }
    break;

  default:
    fatal_parse_error (ls, "reduce.c internal error: non-arithmetic op passed "
		       "to reduce_arithmetic_op().\n");
    break;
  }

  list->token.u.n = val;
}


/* Six relational ops are supported: = <> > < >= <=.
 * = and <> apply to arbitrary strings or numbers, while the rest apply
 * exclusively to numbers.  Each of these takes exactly two arguments.
 */

static void
reduce_relational_op (List *list)
{
  BOOL equal, val;
  List *ls = list->car;

  /* Make sure there are the right # of args. */
  if (list_length (ls) != 3)
    {
      parse_error (ls, "Incorrect number of arguments to %s operator.\n",
		   ls->token.u.string);
      return;
    }
  
  /* Make sure we are comparing two things of equal and appropriate types. */
  if (ls->cdr->token.type != (CDDR (ls))->token.type
      || (ls->cdr->token.type != TOK_NUMBER
	  && ls->cdr->token.type != TOK_IDENTIFIER
	  && ls->cdr->token.type != TOK_QUOTED_STRING))
    return;

  /* Are the two tokens equal? */
  equal = tokens_equal (&ls->cdr->token, &CDDR(ls)->token);

  /* See if we need to do a numeric or an arbitrary comparison. */
  if (ls->token.type == TOK_EQUAL)
    val = equal;
  else if (ls->token.type == TOK_NOT_EQUAL)
    val = !equal;
  else  /* It's a numerical comparison. */
    {
      long n1, n2;

      if (ls->cdr->token.type != TOK_NUMBER
	  || CDDR(ls)->token.type != TOK_NUMBER)
	{
	  return;
	}

      /* Fetch the two numbers to be compared. */
      n1 = ls->cdr->token.u.n;
      n2 = CDDR(ls)->token.u.n;

      switch (ls->token.type) {
      case TOK_GREATER_THAN:
	val = (n1 > n2);
	break;
      case TOK_LESS_THAN:
	val = (n1 < n2);
	break;
      case TOK_GREATER_OR_EQUAL:
	val = (n1 >= n2);
	break;
      case TOK_LESS_OR_EQUAL:
	val = (n1 <= n2);
	break;
      default:
	val = FALSE;
	fatal_parse_error (ls, "reduce.c internal error: non-relational op "
			   "passed to reduce_relational_op().\n");
	break;
      }
    }

  set_list_to_boolean (list, val ? TOK_TRUE : TOK_FALSE);
}


/* reducing a swap is a bad thing; it will swap the constant, and if we are
 * on a big-endian machine that is a bad thing.  Better to let the C compiler
 * do it via the appropriate defines.
 */
#if 0
static void
reduce_swap (List *list)
{
  List *ls = list->car;
  Token *t;
  char buf[256];
  long n;

  if (ls->cdr == NULL)
    {
      parse_error (ls, "Incorrect number of arguments to %s operator.\n",
		   unparse_token (&ls->token, buf));
      return;
    }
  if (ls->cdr->token.type != TOK_NUMBER)
    return;

  t = &ls->cdr->token;
  switch (ls->token.u.swapinfo.size * 2 + ls->token.u.swapinfo.sgnd) {
  case 2 + 0:   /* swapub */
    n = t->u.n & 0xFF;
    break;
  case 2 + 1:   /* swapsb */
    n = t->u.n & 0xFF;
    if (n & 0x80)
      n |= ~0xFF;
    break;
  case 4 + 0:   /* swapuw */
    n = ((t->u.n >> 8) & 0xFF) | ((t->u.n & 0xFF) << 8);
    break;
  case 4 + 1:   /* swapsw */
    n = ((t->u.n >> 8) & 0xFF) | ((t->u.n & 0xFF) << 8);
    if (n & 0x8000)
      n |= ~0xFFFF;
    break;
  case 8 + 0:   /* swapul */
  case 8 + 1:   /* swapsl */
    n = (((t->u.n >> 24) & 0xFF) | ((t->u.n >> 8) & 0xFF00)
	 | ((t->u.n & 0xFF00) << 8) | ((t->u.n & 0xFF) << 24));
    break;
  default:
    parse_error (ls, "Internal error: Unknown size/sgnd for swap! (%d/%d)\n",
		 ls->token.u.swapinfo.size, ls->token.u.swapinfo.sgnd);
    break;
  }

  /* Replace the whole list with a numeric constant. */
  list->token = ls->cdr->token;
  list->car = NULL;
  list->token.u.n = n;
}
#endif


/* Reduces a boolean operator.  (and false (expr)) -> false and so on.
 * If (expr) is not compile-time evaluatable, (and true expr) is not converted
 * to (expr). and, or, xor take >= 1 arguments.  not takes only one.
 * (xor false) = false.
 */

static void
reduce_boolean_op (List *list)
{
  List *ls = list->car, *tmp;
  BOOL val, hit_unknown;

  if (ls->cdr == NULL || (ls->token.type == TOK_NOT && CDDR (ls) != NULL))
    {
      parse_error (ls, "Incorrect number of arguments to %s operator.\n",
		   ls->token.u.string);
      return;
    }

  switch (ls->token.type) {

  case TOK_AND:
    hit_unknown = FALSE;
    for (tmp = ls->cdr; tmp != NULL; tmp = tmp->cdr)
      {
	if (tmp->token.type == TOK_FALSE)
	  {
	    set_list_to_boolean (list, TOK_FALSE);
	    return;
	  }
	else if (tmp->token.type != TOK_TRUE)
	  hit_unknown = TRUE;
      }
    if (!hit_unknown)
      {
	set_list_to_boolean (list, TOK_TRUE);
	return;
      }
    break;

  case TOK_OR:
    hit_unknown = FALSE;
    for (tmp = ls->cdr; tmp != NULL; tmp = tmp->cdr)
      {
	if (tmp->token.type == TOK_TRUE)
	  {
	    set_list_to_boolean (list, TOK_TRUE);
	    return;
	  }
	else if (tmp->token.type != TOK_FALSE)
	  hit_unknown = TRUE;
      }
    if (!hit_unknown)
      {
	set_list_to_boolean (list, TOK_FALSE);
	return;
      }
    break;

  case TOK_XOR:
    val = FALSE;
    for (tmp = ls->cdr; tmp != NULL; tmp = tmp->cdr)
      if (tmp->token.type == TOK_TRUE)
	val = !val;
      else if (tmp->token.type != TOK_FALSE)
	return;   /* Any unknown means we can't reduce it.  Give up. */
    set_list_to_boolean (list, val ? TOK_TRUE : TOK_FALSE);
    break;

  case TOK_NOT:
    tmp = ls->cdr;
    if (tmp->token.type == TOK_FALSE)            /* (not false) = true */
      set_list_to_boolean (list, TOK_TRUE);
    else if (tmp->token.type == TOK_TRUE)        /* (not true) = false */
      set_list_to_boolean (list, TOK_FALSE);
    else if (tmp->token.type == TOK_LIST && tmp->car != NULL &&
	     tmp->car->token.type == TOK_NOT)  /* (not (not e)) = e */
      {
	List *tmp_cdr = list->cdr;
	*list = *tmp->car->cdr;
	list->cdr = tmp_cdr;
      }
    break;
  default:
    fatal_parse_error (ls, "reduce.c internal error: non-boolean op "
		       "passed to reduce_boolean_op().\n");
    break;
  }
}


/* Reduces (if pred expr1 expr2) to expr1 if pred is true, expr2 if
 * pred is false.  Reduces (if pred expr) to expr if pred is true, or
 * nothing if pred is false.  If pred's truth value is unknown the expression
 * is unchanged.
 * Returns TRUE iff list was changed.
 */

static BOOL
reduce_if (List *list, const SymbolTable *sym, int macro_level)
{
  int len;
  List *ls = list->car, *list_cdr = list->cdr;

  len = list_length (ls);
  if (len < 3 || len > 4)
    {
      parse_error (ls, "Wrong number of arguments (%d) to if statement.\n",
		   len);
      ls->token.type = TOK_NOP;     /* Make it a NOP and keep compiling... */
      ls->token.u.string = "nop";
      ls->cdr = NULL;
      return TRUE;
    }

  /* Simplify only the boolean expression; leave the rest unevaluated!! */
  ls->cdr = reduce (ls->cdr, sym, macro_level);

  switch (ls->cdr->token.type) {
  case TOK_TRUE:
    *list = *(CDDR (ls));
    list->cdr = list_cdr;
    return TRUE;
  case TOK_FALSE:
    if (len == 4)    /* Is there a false clause? */
      {
	*list = *(CDDDR (ls));
	list->cdr = list_cdr;
      }
    else
      {
	list->token.type = TOK_EMPTY;  /* Delete it entirely. */
      }
    return TRUE;
  default:     /* oh well, not a compile-time expression. */
    break;
  }
  return FALSE;
}


static BOOL
reduce_switch (List *list, const SymbolTable *sym, int macro_level)
{
  List *ls = list->car, *tmp;
  List *match = NULL;

  /* We can only reduce switches on compile-time constants. */
  if (ls->cdr->token.type != TOK_NUMBER)
    return FALSE;

  /* Loop over all of the switch clauses and find the first one that
   * matches.
   */
  for (tmp = CDDR (ls); tmp != NULL; tmp = tmp->cdr)
    {
      if (tmp->car->token.type == TOK_DEFAULT)
	match = CDAR (tmp);   /* Make this the default. */
      else if (tmp->car->token.type != TOK_NUMBER)
	parse_error (tmp->car, "Expecting a number for switch case value.\n");
      else if (tmp->car->token.u.n == ls->cdr->token.u.n)
	{
	  match = CDAR (tmp);
	  break;
	}
    }

  /* If the switch didn't hit ANYTHING, replace with a TOK_EMPTY. */
  if (match == NULL)
    list->token.type = TOK_EMPTY;
  else
    {
      replace_list (list, match);
      return TRUE;
    }

  return FALSE;
}


static void
set_list_to_boolean (List *ls, TokenType type)
{
  ls->car = NULL;
  ls->token.type = type;
  ls->token.u.string = (type == TOK_TRUE) ? "true" : "false";
}
