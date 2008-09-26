/*
 *     list.c
 */

#include "token.h"
#include "parse.h"
#include <stdlib.h>


/* Allocates and initializes a new list. */

List *
alloc_list ()
{
  List *new;

  new = (List *) malloc (sizeof (List));
  new->cdr = new->car = NULL;
  new->token.type     = TOK_UNKNOWN;
  new->token.u.string = "<Uninitialized>";
  new->token.lineno   = -666;
  new->token.filename = "<Uninitialized>";

  return new;
}


/* Creates an independent copy of ls and returns it. */

List *
copy_list (const List *ls)
{
  List *new;

  if (ls == NULL)
    return NULL;

  new = alloc_list ();
  *new = *ls;
  new->car = copy_list (ls->car);
  new->cdr = copy_list (ls->cdr);

  return new;
}


/* Copies the filename and line number from original to every element of ls. */

void
propagate_fileinfo (const Token *original, List *ls)
{
  while (ls != NULL)
    {
      ls->token.filename = original->filename;
      ls->token.lineno   = original->lineno;
      if (ls->car != NULL)
	propagate_fileinfo (original, ls->car);
      ls = ls->cdr;
    }
}


void
replace_list (List *old, List *new)
{
  List *tmp;

  if (new == NULL)
    {
      old->token.type = TOK_EMPTY;
      return;
    }

  /* Find the end of the new list. */
  for (tmp = new; tmp->cdr != NULL; tmp = tmp->cdr);
  
  /* Insert it into where the old list was. */
  tmp->cdr = old->cdr;
  *old = *new;
}


/* Returns the length of a given list. */

int
list_length (const List *ls)
{
  int len;

  for (len = 0; ls != NULL; ls = ls->cdr)
    len++;
  return len;
}



static int
list_size_aux (const List *ls, int level)
{
  if (level > 1000)  /* avoid infinite death */
    abort ();
  if (ls == NULL)  /* Recursion base case. */
    return 0;
  return (1 + list_size_aux (ls->car, level + 1)
	  + list_size_aux (ls->cdr, level + 1));
}


/* Total number of cons cells in a list. */
int
list_size (const List *ls)
{
  return list_size_aux (ls, 0);
}


BOOL
lists_equal (const List *l1, const List *l2)
{
  if (l1 == l2)   /* Handles NULL == NULL case. */
    return TRUE;
  if (l1 == NULL || l2 == NULL)
    return FALSE;
  if (!tokens_equal (&l1->token, &l2->token))
    return FALSE;
  return lists_equal (l1->car, l2->car) && lists_equal (l1->cdr, l2->cdr);
}


/* Returns NULL if no element anywhere in the list or sublist has a token
 * of the specified type, else returns a pointer to an Token of the specified
 * type.
 */
Token *
has_token_of_type (List *list, TokenType type)
{
  Token *t;

  if (list == NULL)
    return NULL;
  if (list->token.type == type)
    return &list->token;
  t = has_token_of_type (list->car, type);
  if (t != NULL)
    return t;
  t = has_token_of_type (list->cdr, type);
  if (t != NULL)
    return t;
  return NULL;
}


/* Goes through a list and all sublists and replaces all tokens of the
 * specified type with tokens of another type.  Does NOT replace
 * the auxiliary token info.
 */
void
replace_tokens_of_type (List *list, TokenType old, TokenType new)
{
  if (list == NULL)
    return;
  if (list->token.type == old)
    list->token.type = new;
  replace_tokens_of_type (list->car, old, new);
  replace_tokens_of_type (list->cdr, old, new);
}


/* Goes through a list and all sublists and replaces all tokens of the
 * specified type with the specified token.
 */
void
replace_tokens_of_type_with_list (List *list, TokenType old, const List *new)
{
  if (list == NULL)
    return;

  if (list->token.type == old)
    {
      List *old_cdr = list->cdr;
      List *n = copy_list (new);
      *list = *n;
      list->cdr = old_cdr;
    }
  replace_tokens_of_type_with_list (list->car, old, new);
  replace_tokens_of_type_with_list (list->cdr, old, new);
}


/* Recursive helper function for print_list (). */

static void
print_list_aux (const List *ls, int indent, FILE *out)
{
  BOOL print_space = FALSE;
  int i;
  char buf[256];
  const List *l2;

  for (; ls != NULL; ls = ls->cdr)
    {
      /* Print a space before each token but the first. */
      if (print_space) putc (' ', out);
      else print_space = TRUE;

      /* Print lists recursively. */
      if (ls->token.type == TOK_LIST)
	{
	  /* If it has sublists, or it's a sequence, put it down on another
           * line and indent it.
	   */
	  for (l2 = ls->car; l2 != NULL; l2 = l2->cdr)
	    if (l2->token.type == TOK_LIST)
	      break;
	  if (l2 != NULL)
	    {
	      putc ('\n', out);
	      for (i = 0; i < indent; i++)
		putc (' ', out);
	      putc ('(', out);
	      print_list_aux (ls->car, indent + 2, out);
	      putc (')', out);
	    }
	  else    /* No sublists? */
	    {
	      putc ('(', out);
	      print_list_aux (ls->car, indent, out);
	      putc (')', out);
	    }
	}
      else fputs (unparse_token (&ls->token, buf), out);
    }
}


/* Outputs a list in human-readable format. */

void
print_list (FILE *out, const List *ls)
{
  print_list_aux (ls, 0, out);
}


void
dump_list (const List *ls)
{
  print_list (stdout, ls);
}


BOOL
list_overlap (const List *l1, const List *l2)
{
  if (l1 == NULL || l2 == NULL)
    return FALSE;
  if (l1 == l2)
    return TRUE;
  return (list_overlap (l1, l2->car)
	  || list_overlap (l1, l2->cdr)
	  || list_overlap (l1->car, l2)
	  || list_overlap (l1->cdr, l2));
}
