/*
 *     macro.c
 */

#include "macro.h"
#include "error.h"
#include "token.h"

/*
#define DEBUG
*/


/* Private helper function. */
static BOOL better_match (const List *ls, const List *new,const List *best);
static void substitute_args (const List *argval, const List *argname,
			     List *expr);

/* This function replaces a given list with the macro expansion contained in
 * the symbol table.  More than one macro with the same name may be defined;
 * the best match will be expanded (see heuristics, below.)
 *
 * A macro needn't be a function.  (define my-favorite-number 7) is legal.
 *
 * A macro definition can contain any number of literals which must be
 * matched exactly in the invocation to be expanded (see heuristic 4 below.)
 * Additionally, variable numbers of arguments are supported.  If the last
 * argument to a macro is "+tail", +tail will be shorthand for an arbitrarily
 * long list of arguments.  For example, (define (foo x +tail)).  Now
 * (foo north south east) will have x bound to "north", and tail bound to
 * "south east".  Any subsequent usage of +tail will be exactly equivalent to
 * using the two words "south east"; it will be treated just like two
 * arguments.
 *
 * Here is an example that sums an arbitrarily long list of numbers:
 *  (define (sum) 0)
 *  (define (sum n +tail) (+ n (sum +tail)))
 */


BOOL
macro_sub (List *ls, const SymbolTable *sym, BOOL flag_failure)
{
  const Macro *m;
  List *best = NULL, *new, *op;
  SymbolInfo info;
  const char *name;

#ifdef DEBUG
  printf ("macro_sub: "); print_list (ls, stdout); putchar ('\n');
#endif


  /* Get the name of the macro to be expanded. */
  op = NULL;
  if (ls->token.type == TOK_LIST)
    {
      if (ls->car == NULL)
	{
	  if (flag_failure)
	    parse_error (ls, "Macro name is missing!\n");
	  return FALSE;
	}
      if (ls->car->token.type == TOK_IDENTIFIER)
	name = ls->car->token.u.string;
      else
	{
	  op = ls->car;
	  name = NULL;
	}
    }
  else if (ls->token.type == TOK_IDENTIFIER)
    name = ls->token.u.string;
  else
    {
      op = ls;
      name = NULL;
    }

  /* Look up the macro list in the symbol table. */
  if (lookup_symbol (sym, name, &info, NULL) == HASH_NOTFOUND)
    {
      if (flag_failure)
	{
	  char buf[1024];
	  if (name == NULL)
	    {
	      if (op != NULL)
		parse_error (op, "Unknown operator \"%s\"\n",
			     unparse_token (&op->token, buf));
	      else
		parse_error (ls, "Unknown operator.\n");
	    }
	  else
	    parse_error (ls, "Unknown operator \"%s\".\n", name);
	}
      return FALSE;
    }
  m = (const Macro *) info.p;


  /* Now we have a list of macros and we have to choose the best match by the
   * following heuristics, in order:
   *  (1) Invocation form matching.  "foo" and "(foo)" can only match
   *      (define foo ...) and (define (foo) ...), respectively.
   *  (2) The number of arguments must match.  Note that the "+tail" construct
   *      can allow an arbitrarily high number of arguments.
   *  (3) All literals must match (see 3, below)
   *  (4) Number of literal matches.  For example, it is legal to both
   *      (define (foo a1 a2 0) x) and (define (foo a1 a2 a3) y).  In this
   *      example, an invocation of (foo 2 1 0) would expand to x.
   *  (5) Larger number of arguments.  Whichever macro names the most arguments
   *      (disregarding any "+tail", of course) will be preferred.
   *  (6) Prefer the macro without a "+tail".
   *  (7) Lexical predecence.  In case of a tie, the first macro defined will
   *      be chosen.
   */

  /* Find the best match. */
  for (best = NULL; m != NULL; m = m->next)
    if (better_match (ls, m->expr, best))
      best = m->expr;

  /* Was there no match at all? */
  if (best == NULL)
    {
      if (flag_failure)
	parse_error (ls, "No applicable version of the \"%s\" macro exists "
		     "for these arguments.\n", name);
      return FALSE;
    }

  /* Is this simple substitution (ie, no macro arguments, etc.) */
  if (ls->token.type != TOK_LIST)
    {
      if (best->cdr == NULL)  /* Empty list? */
	ls->token.type = TOK_EMPTY;
      else if (CDDR (best) == NULL && best->cdr->token.type != TOK_LIST)
	{
	  ls->token.type = best->cdr->token.type;
	  ls->token.u    = best->cdr->token.u;
	}
      else
	{
	  new = copy_list (best->cdr);
	  propagate_fileinfo (&ls->token, new);  /* So we don't forget... */
	  replace_list (ls, new);
	}

      return TRUE;
    }

  /* It's a complicate macro with arguments.  Substitute in the arguments. */
  new = copy_list (best->cdr);
  substitute_args (CDAR (ls), CDAR (best), new);
  replace_list (ls, new);

  return TRUE;
}


static BOOL
has_tail (const List *ls)
{
  if (ls == NULL)
    return FALSE;
  while (ls->cdr != NULL)
    ls = ls->cdr;
  return (ls->token.type == TOK_TAIL);
}


#define IS_LITERAL(L) ((L)->token.type != TOK_IDENTIFIER \
			&& (L)->token.type != TOK_TAIL)

static int
count_literals (const List *ls)
{
  int lit;
  
  for (lit = 0; ls != NULL; ls = ls->cdr)
    if (IS_LITERAL (ls))
      lit++;

  return lit;
}


/* Returns TRUE iff new is a better match than best according to the
 * heuristics above.  If new is not legal, returns FALSE.  best == NULL
 * is OK and is treated as the worst possible match (ie, any legal new beats
 * it.)
 */

static BOOL
better_match (const List *ls, const List *new, const List *best)
{
  BOOL new_has_tail, best_has_tail;
  int ls_length, new_length, best_length, new_lit, best_lit;
  const List *l1, *l2;

#ifdef DEBUG
  printf ("ls = "); print_list (ls, stdout);
  printf ("  new = "); print_list (new, stdout);
  putchar ('\n');
#endif

  /* Heuristic (1) - must both be functions or not functions. */
  if (ls->token.type != new->token.type)
    return FALSE;

  /* If it's of the form (define foo 0), apply heuristic 6, since there
   * is only one kind of match.
   */
  if (ls->token.type == TOK_IDENTIFIER)
    return (best == NULL);

#ifdef DEBUG
  puts ("Passed h1");
#endif


  /* Heuristic (2) - must have compatible #'s of arguments. */
  ls_length  = list_length (ls->car);
  new_length = list_length (new->car);
  new_has_tail = has_tail (new->car);

  if ((ls_length > new_length && !new_has_tail)   /* Too many args? */
      || ls_length < new_length - new_has_tail)   /* Too few? */
    return FALSE;

#ifdef DEBUG
  puts ("Passed h2");
#endif

  
  /* Heuristic (3) - all literals must match. */
  for (l1 = CDAR (ls), l2 = CDAR (new); l1 != NULL && l2 != NULL;
       l1 = l1->cdr, l2 = l2->cdr)
    {
      if (IS_LITERAL (l2))
	{
	  if (!tokens_equal (&l1->token, &l2->token))
	    return FALSE;
	}
    }

  /* At this point, new is legal.  Therefore, if best == NULL, return TRUE. */
  if (best == NULL)
    return TRUE;

#ifdef DEBUG
  puts ("Passed h3");
#endif


  /* Heuristic (4) - number of literals. */
  new_lit  = count_literals (new->car->cdr);
  best_lit = count_literals (best->car->cdr);
  if (new_lit != best_lit)
    return (new_lit > best_lit);


#ifdef DEBUG
  puts ("Passed h4");
#endif

  /* Heuristic (5) - number of arguments (not counting +tail). */
  best_has_tail = has_tail (best->car);
  best_length = list_length (best->car);
  if (new_length - new_has_tail != best_length - best_has_tail)
    return (new_length - new_has_tail > best_length - best_has_tail);


#ifdef DEBUG
  puts ("Passed h5");
#endif


  /* Heuristic (6) - prefer the macro without +tail. */
  if (best_has_tail ^ new_has_tail)
    return best_has_tail;

#ifdef DEBUG
  puts ("Passed h6");
#endif

  /* Heuristic (7) - recency.  Assuming we process them in order... */
  return (best == NULL);
}


static void
substitute_args (const List *argval, const List *argname, List *expr)
{
  const List *name, *val, *tail = NULL;
  BOOL has_tail = FALSE;

#ifdef DEBUG
  List *original_expr = expr;

  printf ("substitute_args: ");
  printf ("argval  = ");   print_list (argval, stdout);
  printf ("\nargname = "); print_list (argname, stdout);
  printf ("\nexpr    = "); print_list (expr, stdout);
  putchar ('\n');
#endif

  /* See if there is a +tail, and make it tail point to the list of args. */
  for (name = argname, val = argval; name != NULL;
       name = name->cdr, val = val->cdr)
    if (name->token.type == TOK_TAIL)
      {
	tail = val;
	has_tail = TRUE;
	break;
      }


  for (; expr != NULL; expr = expr->cdr)
    {
      switch (expr->token.type) {
      case TOK_LIST:
	substitute_args (argval, argname, expr->car);
	break;
      case TOK_IDENTIFIER:
	for (name = argname, val = argval; name != NULL && val != NULL;
	     name = name->cdr, val = val->cdr)
	  if (!IS_LITERAL (name) && tokens_equal (&name->token, &expr->token))
	    {
	      List *tmp_cdr = expr->cdr;
	      *expr = *val;
	      expr->car = copy_list (val->car);
	      expr->cdr = tmp_cdr;
	      break;
	    }
	break;
      case TOK_TAIL:
	if (!has_tail)
	  parse_error (expr, "But this macro has no tail!\n");
	replace_list (expr, copy_list (tail));
	break;

      default: break;
      }
    }

#ifdef DEBUG
  printf ("*** expr after argument substitutions:\n");
  print_list (original_expr, stdout);
  putchar ('\n');
#endif

}
