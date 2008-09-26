/*
 *     error.c
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "token.h"
#include "common.h"
#include "list.h"


/* Prints out an error message and returns. */

void
error (const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
}


/* Prints out an error message and aborts. */

void
fatal_error (const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  fputs ("*** Exit\n", stderr);
  abort ();
}


/* Prints out an error message for a given List and returns. */

void
parse_error (const List *ls, const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
  fprintf (stderr, "%s, line %lu:\t", ls->token.filename, ls->token.lineno);
  vfprintf (stderr, fmt, ap);
}


/* Prints out an error message for a given List and aborts. */

void
fatal_parse_error (const List *ls, const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
  fprintf (stderr, "%s, line %lu:\t", ls->token.filename, ls->token.lineno);
  vfprintf (stderr, fmt, ap);
  fputs ("*** Exit\n", stderr);
  exit (-1);
}


/* Prints out an error message for the current file and returns. */

void
input_error (const char *fmt, ...)
{
  va_list ap;
  const InputFile *current = get_input_file (0);
  const InputFile *tmp = get_input_file (1);
  int lev = 2;

  if (tmp != NULL)
    {
      error ("In file included from %s:%d", tmp->filename, tmp->lineno);
      for (tmp = get_input_file (2); tmp != NULL; tmp = get_input_file (++lev))
	error (", from %s:%d", tmp->filename, tmp->lineno);
      error (":\n");
    }
  if (current != NULL)
    error ("%s: %d: ", current->filename, current->lineno);

  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
}


/* Prints out an error message for the current file and aborts. */

void
fatal_input_error (const char *fmt, ...)
{
  va_list ap;
  const InputFile *current = get_input_file (0);
  const InputFile *tmp = get_input_file (1);
  int lev = 2;

  if (tmp != NULL)
    {
      error ("In file included from %s:%d", tmp->filename, tmp->lineno);
      for (tmp = get_input_file (2); tmp != NULL; tmp = get_input_file (++lev))
	error (", from %s:%d", tmp->filename, tmp->lineno);
      error (":\n");
    }
  if (current != NULL)
    error ("%s: %d: ", current->filename, current->lineno);

  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  fputs ("*** Exit\n", stderr);
  exit (-1);
}
