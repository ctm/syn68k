/*
 *     token.c
 */

#include <stdio.h>
#include <string.h>
#include <sys/param.h>

#if defined (__MINGW32__)
#include <stddef.h> /* needed for ctype.h */
#endif

#include <ctype.h>
#include <stdlib.h>
#include "token.h"
#include "error.h"
#include "common.h"
#include "hash.h"
#include "tokenlist.h"
#include "uniquestring.h"

static InputFile file_stack[MAX_INCLUDE_DEPTH];
static int file_stack_ptr = 0;
static SymbolTable *tok_sym_table = NULL;
static const char *include_dirs[] = { ".", NULL };
static unsigned char break_char_table[257];
static unsigned char num_conv_table[256];

#define BINARY_MASK      16
#define OCTAL_MASK       32
#define DECIMAL_MASK     64
#define HEX_MASK         128
#define ALL_MASKS        (BINARY_MASK | OCTAL_MASK | DECIMAL_MASK | HEX_MASK)

/* Private routines. */
static long parse_number (const char *num);
static BOOL raw_fetch_next_token (Token *t);


/* Initializes the tokenizer.  Call it once before any calls to any other
 * token routines.  Do not call it more than once; the only way to reset
 * everything is to keep fetching tokens until there are none left.
 */

void
init_tokenizer ()
{
  static const unsigned char break_chars[] = " \t\n\r();";
  const unsigned char *p;
  SymbolInfo sym;
  int i;

  /* Start out with 0 open streams. */
  file_stack_ptr = 0;

  /* Initialize the symbol table. */
  if (tok_sym_table != NULL)
    free_symbol_table (tok_sym_table);

  tok_sym_table = make_symbol_table ();
  for (i = 0; i < (sizeof token_list / sizeof token_list[0]); i++)
    {
      sym.n = token_list[i].value;
      insert_symbol (tok_sym_table, token_list[i].name, sym);
    }

  /* Initialize break character table. */
  for (p = &break_chars[0]; *p; p++)
    break_char_table[*p + 1] = TRUE;
  break_char_table[EOF + 1] = TRUE;

  /* Initialize ascii -> number conversion table. */
  num_conv_table['0'] = (ALL_MASKS + 0);
  num_conv_table['1'] = (ALL_MASKS + 1);
  for (i = '2'; i < '8'; i++)
    num_conv_table[i] = (ALL_MASKS - BINARY_MASK) + i - '0';
  num_conv_table['8'] = (DECIMAL_MASK + HEX_MASK + 8);
  num_conv_table['9'] = (DECIMAL_MASK + HEX_MASK + 9);
  for (i = 0; i < 7; i++)
    num_conv_table[i + 'A'] = num_conv_table[i + 'a'] = (HEX_MASK + i + 10);
}


/* Skips whitespace and comments.  Returns the first non-whitespace character
 * encountered, or EOF if there wasn't one.
 */

int
skip_to_next_token ()
{
  InputFile *f;
  FILE *fp;
  int c;

  /* Skip leading whitespace & handle EOF by popping up one file. */
  while (1)
    {
      if (file_stack_ptr <= 0)
	return EOF;

      f = &file_stack[file_stack_ptr - 1];
      fp = f->fp;

      /* Skip leading spaces & comments */
      while (1)
	{
	  while (isspace (c = fgetc (fp)))  /* Linux dies if you use getc. */
	    if (c == '\n') f->lineno++;
	  
	  if (c == ';')    /* Skip over comments */
	    {
	      while ((c = getc (fp)) != '\n' && c != EOF);
	      if (c == '\n') f->lineno++;
	    }
	  else break;
	}
  
      if (c == EOF)
	close_file ();
      else break;
    }

  return c;
}


/* Same as raw_fetch_next_token, but processes include directives. */

BOOL
fetch_next_token (Token *t)
{
  static Token saved_token = { TOK_EMPTY };
  Token temp, temp2;

  if (saved_token.type != TOK_EMPTY)
    {
      *t = saved_token;
      saved_token.type = TOK_EMPTY;
      return YES;
    }

  /* Read in the next token.  If it's not an open paren, pass it on. */
  if (!raw_fetch_next_token (t))
    return NO;
  if (t->type != TOK_LEFT_PAREN)
    return YES;

  /* Check to see if this is an #include directive. */
  if (!raw_fetch_next_token (&saved_token))
    return YES;

  if (saved_token.type != TOK_INCLUDE)
    return YES;
  saved_token.type = TOK_EMPTY;
  
  if (!raw_fetch_next_token (&temp))
    {
      fatal_input_error ("include directive is missing filename!\n");
    }
  
  if (temp.type != TOK_QUOTED_STRING)
    {
      /* Punt unless it's an identifier; if it is, include the file anyway. */
      if (temp.type != TOK_IDENTIFIER)
	{
	  fatal_input_error ("Filename for include must be in \"\" quotes.\n");
	}
      input_error ("Filename for include must be in \"\" quotes.\n");
    }

  raw_fetch_next_token (&temp2);  /* Eat the close paren. */
  if (temp2.type != TOK_RIGHT_PAREN)
    {
      fatal_input_error ("Too many arguments to include directive.\n");
    }
  open_file (temp.u.string, include_dirs);
  return fetch_next_token (t);
}


/* Fetches the next token from the stack of input streams.  Returns FALSE
 * if there are no tokens left.
 */

static BOOL
raw_fetch_next_token (Token *t)
{
  char buf[MAX_TOKEN_SIZE], *p;
  FILE *fp;
  InputFile *f;
  SymbolInfo sym;
  int c, i;

  c = skip_to_next_token ();
  if (c == EOF)
    return NO;

  /* Set up convenience variables pointing to the current file. */
  f = &file_stack[file_stack_ptr - 1];
  fp = f->fp;

  /* Set up file and line number fields of the token. */
  t->filename = f->filename;
  t->lineno   = f->lineno;

  /* Special case for paren's, as they are both legal tokens and break chars */
  if (c == '(')
    {
      t->type = TOK_LEFT_PAREN;
      t->u.string = "(";
      return YES;
    }
  else if (c == ')')
    {
      t->type = TOK_RIGHT_PAREN;
      t->u.string = ")";
      return YES;
    }

  /* Loop and grab all the characters in this token, putting them in buf. */

  p = buf, i = MAX_TOKEN_SIZE - 1;

  /* Special case for quoted strings. */
  if (c == '\"')
    {
      int backslash = 0;
      do
	{
	  if (c == '\\')
	    {
	      if (!backslash)
		{
		  backslash = 1;
		  c = getc (fp);
		  continue;
		}
	      backslash = 0;
	    }
	  else if (backslash)
	    {
	      backslash = 0;
	      switch (c) {
	      case '\n':
		continue;
	      case 'n':
		c = '\n';
		break;
	      case 't':
		c = '\t';
		break;
	      case '\\':
		break;
	      default:
		input_error ("Unknown escape sequence '\\%c'.\n", c);
		break;
	      }
	    }
	  else if (c == '\n')
	    {
	      input_error ("Unterminated string.\n");
	      return raw_fetch_next_token (t);
	    }

	  *p++ = c;
	  c = getc (fp);
	}
      while (c != '\"' && --i);

      *p = '\0';
      t->type = TOK_QUOTED_STRING;
      t->u.string = unique_string (buf + 1);
      return YES;
    }
  else  /* Not a quoted string... */
    {
      do
	{
	  *p++ = c;
	  c = getc (fp);
	}
      while (!break_char_table[c + 1] && --i);
      ungetc (c, fp);
      *p = '\0';
    }

  /* Is it a normal token we recognize? */
  if (lookup_symbol (tok_sym_table, buf, &sym, &t->u.string) == HASH_NOERR)
    {
      int buflen = strlen (buf);

      t->type = sym.n;

      if (t->type == TOK_TEMP_REGISTER)
	{
	  if (isdigit (buf[3]))
	    t->u.reginfo.which = atoi (buf + 3);
	  else
	    t->u.reginfo.which = 1;
	  t->u.reginfo.sgnd = (buf[buflen - 2] == 's');
	  switch (buf[buflen - 1]) {
	  case 'b': t->u.reginfo.size = 1; break;
	  case 'w': t->u.reginfo.size = 2; break;
	  case 'l': t->u.reginfo.size = 4; break;
	  default:
	    fatal_error ("Internal error, token.c: impossible register "
			 "size '%c'\n", buf[buflen - 1]);
	    break;
	  }
	}
      else if (t->type == TOK_DEREF)
	{
	  if (!strcmp (buf, "deref"))
	    {
	      t->u.derefinfo.sgnd = FALSE;
	      t->u.derefinfo.size = 0;   /* untyped deref. */
	    }
	  else
	    {
	      t->u.derefinfo.sgnd = (buf[buflen - 2] == 's');
	      switch (buf[buflen - 1]) {
	      case 'b': t->u.derefinfo.size = 1; break;
	      case 'w': t->u.derefinfo.size = 2; break;
	      case 'l': t->u.derefinfo.size = 4; break;
	      default:
		fatal_error ("Internal error, token.c: impossible deref "
			     "size '%c'\n", buf[buflen - 1]);
		break;
	      }
	    }
	}
      else if (t->type == TOK_SWAP)
	{
	  t->u.derefinfo.sgnd = (buf[buflen - 2] == 's');
	  switch (buf[buflen - 1]) {
	  case 'b': t->u.derefinfo.size = 1; break;
	  case 'w': t->u.derefinfo.size = 2; break;
	  case 'l': t->u.derefinfo.size = 4; break;
	  default:
	    fatal_error ("Internal error, token.c: impossible swap "
			 "size '%c'\n", buf[buflen - 1]);
	    break;
	  }
	}
      else if (IS_DOLLAR_TOKEN (t->type))
	{
	  t->u.dollarinfo.which = atoi (buf + 1);

	  switch (t->type) {
	  case TOK_DOLLAR_DATA_REGISTER:
	  case TOK_DOLLAR_ADDRESS_REGISTER:
	  case TOK_DOLLAR_GENERAL_REGISTER:
	  case TOK_DOLLAR_REVERSED_AMODE:
	  case TOK_DOLLAR_AMODE:
	  case TOK_DOLLAR_NUMBER:
	  case TOK_DOLLAR_AMODE_PTR:
	  case TOK_DOLLAR_REVERSED_AMODE_PTR:
	    t->u.dollarinfo.sgnd = (buf[buflen - 2] == 's');
	    switch (buf[buflen - 1]) {
	    case 'b': t->u.dollarinfo.size = 1; break;
	    case 'w': t->u.dollarinfo.size = 2; break;
	    case 'l': t->u.dollarinfo.size = 4; break;
	    default:
	      fatal_error ("Internal error, token.c: impossible dollar "
			   "size '%c'\n", buf[buflen - 1]);
	      break;
	    }
	    break;
	  default:
	    fatal_error ("Internal error, token.c: IS_DOLLAR_IDENTIFIER must "
			 "be invalid.\n");
	    break;
	  }
	}
      return YES;
    }

  /* Is it a number? */
  else if (isdigit (buf[0]) || (buf[0] == '-' && isdigit (buf[1])))
    {
      t->type = TOK_NUMBER;
      t->u.n = parse_number (buf);
      return YES;
    }

  /* Is it a register? eg d0.ub, d3.w, d7.l, a2.b, a4.w, a1.uw, etc. */
  else if ((buf[0] == 'a' || buf[0] == 'd')
	   && buf[1] >= '0' && buf[1] <= '7')
    {
      if (buf[2] != '.')
	input_error ("Missing size/signedness specifier for register.\n");
      
      t->type = (buf[0] == 'a') ? TOK_ADDRESS_REGISTER : TOK_DATA_REGISTER;
      t->u.reginfo.sgnd = (buf[3] != 'u');
      switch (buf[strlen (buf) - 1]) {
      case 'b':
	t->u.reginfo.size = 1;
	break;
      case 's':
      case 'w':
	t->u.reginfo.size = 2;
	break;
      case 'l':
      default:
	t->u.reginfo.size = 4;
	break;
      }
      t->u.reginfo.which = buf[1] - '0';
      return YES;
    }

  /* Must be a label. */
  t->type = TOK_IDENTIFIER;
  t->u.string = unique_string (buf);
  return YES;
}


/* Parses an ASCII number held in buf.  Recognizes 0x prefix as hexadecimal,
 * 0b as binary and 0 followed by more digits as octal.  Other numbers
 * are interpreted as decimal.
 */

static long
parse_number (const char *buf)
{
  int sign = 1, base;
  long n = 0;
  unsigned char mask, v;

  /* Check for sign. */
  if (buf[0] == '-')
    sign = -1, buf++;
  
  /* Figure out which base the number is. */
  if (buf[0] == '0')   /* Either octal, hexadecimal, or binary. */
    {
      if (buf[1] == 'x')
	base = 16, mask = HEX_MASK, buf += 2;
      else if (buf[1] == 'b')
	base = 2, mask = BINARY_MASK, buf += 2;
      else base = 8, mask = OCTAL_MASK;
    }
  else base = 10, mask = DECIMAL_MASK;

  /* Convert it to an int. */
  while ((v = *buf++))
    {
      if (num_conv_table[v] & mask)
	n = (n * base) + (num_conv_table[v] & 15);
      else
	{
	  input_error ("Illegal character in numeric constant.\n");
	  return 0;
	}
    }

  return n * sign;
}


/* Returns a pointer to the InputFile struct for a file being parsed.
 * levels_back specifies how many #include levels to pop back.  A levels_back
 * of zero will return the current input file.  levels_back must be >= 0.
 */

const InputFile *
get_input_file (int levels_back)
{
  if (levels_back < 0 || file_stack_ptr - levels_back - 1 < 0)
    return NULL;
  return &file_stack[file_stack_ptr - levels_back - 1];
}


/* Opens a file and pushes it onto the stack of files being parsed.  "file"
 * is the filename of the file to be #include'd, search_dirs is a
 * NULL-terminated list of all directories to check.  These directories will
 * not be checked if file has a leading '/'.
 */

void
open_file (const char *file, const char *search_dirs[])
{
  FILE *fp;
  const char **dir;
  char buf[MAXPATHLEN];
  
  /* See if we've opened too many files already. */
  if (file_stack_ptr >= MAX_INCLUDE_DEPTH)
    fatal_input_error ("Too many levels of nested #include's.\n");

  /* If the filename has a leading slash, don't check directories.
   * Otherwise, check all the directories in the search path.
   */
  fp = NULL;
  if (file[0] == '/')
    fp = fopen (file, "r");
  else for (dir = &search_dirs[0]; *dir != NULL; dir++)
    {
      sprintf (buf, "%s/%s", *dir, file);
      fp = fopen (buf, "r");
      if (fp != NULL)
	break;
    }
  if (fp == NULL)
    fatal_input_error ("%s: No such file or directory.\n", file);

  open_stream (file, fp);
}


/* Adds a stream to the stack of input files.  The reason this routine
 * is distinct from open_file (above) is so that opening stdin is trivial.
 */

void
open_stream (const char *name, FILE *fp)
{
  InputFile *new = &file_stack[file_stack_ptr];

  if (file_stack_ptr >= MAX_INCLUDE_DEPTH)
    return;

  /* Add this file to the file input stack. */
  new->fp = fp;
  new->lineno = 1;
  strncpy (new->filename, name, MAX_FILENAME_LENGTH - 1);
  new->filename[MAX_FILENAME_LENGTH - 1] = '\0';
  file_stack_ptr++;
}


FILE *
current_stream ()
{
  if (file_stack_ptr > 0)
    return file_stack[file_stack_ptr - 1].fp;
  return NULL;
}


/* Closes the file currently being read and pops back to the file that was
 * #including the current one, if any.
 */

void
close_file ()
{
  if (file_stack_ptr > 0)
    fclose (file_stack[--file_stack_ptr].fp);
}


BOOL
tokens_equal (const Token *t1, const Token *t2)
{
  if (t1->type != t2->type)
    return FALSE;

  switch (t1->type) {
  case TOK_IDENTIFIER:
  case TOK_QUOTED_STRING:
    return !strcmp (t1->u.string, t2->u.string);
  case TOK_NUMBER:
    return (t1->u.n == t2->u.n);
  case TOK_DOLLAR_AMODE:
  case TOK_DOLLAR_REVERSED_AMODE:
  case TOK_DOLLAR_AMODE_PTR:
  case TOK_DOLLAR_REVERSED_AMODE_PTR:
  case TOK_DOLLAR_NUMBER:
  case TOK_DOLLAR_DATA_REGISTER:
  case TOK_DOLLAR_ADDRESS_REGISTER:
  case TOK_DOLLAR_GENERAL_REGISTER:
    return (t1->u.dollarinfo.which == t2->u.dollarinfo.which
	    && t1->u.dollarinfo.size == t2->u.dollarinfo.size
	    && t1->u.dollarinfo.sgnd == t2->u.dollarinfo.sgnd);
  case TOK_DEREF:
    return (t1->u.derefinfo.sgnd == t2->u.derefinfo.sgnd
	    && t1->u.derefinfo.size == t2->u.derefinfo.size);
  case TOK_AMODE:
  case TOK_REVERSED_AMODE:
  case TOK_AMODE_PTR:
  case TOK_REVERSED_AMODE_PTR:
    return (t1->u.amodeinfo.sgnd == t2->u.amodeinfo.sgnd
	    && t1->u.amodeinfo.size == t2->u.amodeinfo.size
	    && t1->u.amodeinfo.which == t2->u.amodeinfo.which);
  case TOK_DATA_REGISTER:
  case TOK_ADDRESS_REGISTER:
  case TOK_TEMP_REGISTER:
    return (t1->u.reginfo.sgnd == t2->u.reginfo.sgnd
	    && t1->u.reginfo.size == t2->u.reginfo.size
	    && t1->u.reginfo.which == t2->u.reginfo.which);
  default:
    return TRUE;
  }
}


/* Dumps a token in human-readable format.  For debugging purposes only. */

void
dump_token (const Token *t)
{
  if (t->type == TOK_NUMBER)
    printf ("type = %d,\tn = %ld,  \tfilename = \"%s\",\tlineno = %lu\n",
	    t->type, t->u.n, t->filename, t->lineno);
  else
    printf ("type = %d,\tstring = \"%s\",  \tfilename = \"%s\",\t"
	    "lineno = %lu\n", t->type, t->u.string, t->filename, t->lineno);
}


/* Copies a human-readable version of the token to buf and returns buf. */

char *
unparse_token (const Token *t, char *buf)
{
  static const char *regdesc[2][5] = {
    { "", "ub", "uw",  "", "ul" },
    { "", "sb", "sw",  "", "sl" }
  };

  switch (t->type) {
  case TOK_NUMBER:
    sprintf (buf, "%ld", t->u.n);
    break;
  case TOK_QUOTED_STRING:
    sprintf (buf, "\"%s\"", t->u.string);
    break;
  case TOK_EMPTY:
    strcpy (buf, "[EMPTY]");
    break;
  case TOK_DEREF:
    if (t->u.derefinfo.size == 0)
      strcpy (buf, "deref");
    else sprintf (buf, "deref%s",
		  regdesc[t->u.derefinfo.sgnd][t->u.derefinfo.size]);
    break;
  case TOK_SWAP:
    sprintf (buf, "swap%s", regdesc[t->u.derefinfo.sgnd][t->u.derefinfo.size]);
    break;
  case TOK_DATA_REGISTER:
    sprintf (buf, "d%d.%s", t->u.reginfo.which,
	     regdesc[t->u.reginfo.sgnd][t->u.reginfo.size]);
    break;
  case TOK_ADDRESS_REGISTER:
    sprintf (buf, "a%d.%s", t->u.reginfo.which,
	     regdesc[t->u.reginfo.sgnd][t->u.reginfo.size]);
    break;
  case TOK_TEMP_REGISTER:
    sprintf (buf, "tmp%d.%s", t->u.reginfo.which,
	     regdesc[t->u.reginfo.sgnd][t->u.reginfo.size]);
    break;
  case TOK_DOLLAR_DATA_REGISTER:
    sprintf (buf, "$%d.d%s", t->u.dollarinfo.which,
	     regdesc[t->u.dollarinfo.sgnd][t->u.dollarinfo.size]);
    break;
  case TOK_DOLLAR_ADDRESS_REGISTER:
    sprintf (buf, "$%d.a%s", t->u.dollarinfo.which,
	     regdesc[t->u.dollarinfo.sgnd][t->u.dollarinfo.size]);
    break;
  case TOK_DOLLAR_GENERAL_REGISTER:
    sprintf (buf, "$%d.g%s", t->u.dollarinfo.which,
	     regdesc[t->u.dollarinfo.sgnd][t->u.dollarinfo.size]);
    break;
  case TOK_DOLLAR_AMODE:
    sprintf (buf, "$%d.m%s", t->u.dollarinfo.which,
	     regdesc[t->u.dollarinfo.sgnd][t->u.dollarinfo.size]);
    break;
  case TOK_DOLLAR_REVERSED_AMODE:
    sprintf (buf, "$%d.r%s", t->u.dollarinfo.which,
	     regdesc[t->u.dollarinfo.sgnd][t->u.dollarinfo.size]);
    break;
  case TOK_DOLLAR_AMODE_PTR:
    sprintf (buf, "$%d.p%s", t->u.dollarinfo.which,
	     regdesc[t->u.dollarinfo.sgnd][t->u.dollarinfo.size]);
    break;
  case TOK_DOLLAR_REVERSED_AMODE_PTR:
    sprintf (buf, "$%d.q%s", t->u.dollarinfo.which,
	     regdesc[t->u.dollarinfo.sgnd][t->u.dollarinfo.size]);
    break;
  case TOK_DOLLAR_NUMBER:
    sprintf (buf, "$%d.%s", t->u.dollarinfo.which,
	     regdesc[t->u.dollarinfo.sgnd][t->u.dollarinfo.size]);
    break;
  default:
    strcpy (buf, t->u.string);
    break;
  }

  return buf;
}
