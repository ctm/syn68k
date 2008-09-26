/*
 *     parse.c
 */

#include "error.h"
#include "parse.h"
#include "reduce.h"
#include "macro.h"
#include "list.h"
#include "defopcode.h"
#include "bitstring.h"
#include "safe_alloca.h"
#include "uniquestring.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#ifdef NeXT
#include <bsd/libc.h>
#endif

static List *parse_expression (void);
static int parse_define (SymbolTable *sym, List *new);
static int parse_defopcode (SymbolTable *sym, List *new);
static void verify_fields_exist (const char *bits, List *code);


/* Reads in all defines and defopcodes from the tokenizer.  Places macros
 * in the symbol table and generates code for the opcodes.  Returns the number
 * of macros + the number of opcodes parsed.
 */

int
parse_all_expressions (SymbolTable *sym)
{
  List *new;
  int num_parsed = 0;
  List def;

  while (fetch_next_token (&def.token))
    {
      if (def.token.type != TOK_LEFT_PAREN)
	fatal_input_error ("Expecting '(' but not finding one; aborting.\n");

      /* Read in the expression just defined. */
      new = parse_expression ();
      if (new == NULL)
	{
	  input_error ("stray '(' with no following expression!");
	  continue;
	}

      if (new->token.type != TOK_DEFINE && new->token.type != TOK_DEFOPCODE)
	{
	  List dummy = { NULL, 0 /* new */, { /* new->token */0 } };
	  dummy.car = new;
	  dummy.token = new->token;
	  dummy.token.type = TOK_LIST;
	  reduce (&dummy, sym, 0);
	  new = dummy.car;
	}

      if (new != NULL)
	{
	  if (new->token.type == TOK_DEFINE)
	    num_parsed += parse_define (sym, new->cdr);
	  else if (new->token.type == TOK_DEFOPCODE)
	    {
	      num_parsed += parse_defopcode (sym, new->cdr);

	      if (preprocess_only)
		{
		  fputs ("\n(", stdout);
		  print_list (stdout, new);
		  puts (")");
		}
	    }
	}
    }

  return num_parsed;
}


static int
parse_define (SymbolTable *sym, List *new)
{
  const char *name;
  Macro *macro;
  SymbolInfo symbol_info;

  /* Get the name of the macro for (define (foo x y)) or (define foo x). */
  name = NULL;
  switch (new->token.type) {
  case TOK_LIST:
    if (new->car != NULL && new->car->token.type == TOK_IDENTIFIER)
      name = new->car->token.u.string;
    break;
  case TOK_QUOTED_STRING:
  case TOK_IDENTIFIER:
    name = new->token.u.string;
    break;
  default:
    name = NULL;
    break;
  }
  
  /* Make sure the macro name is legitimate. */
  if (name == NULL)
    {
      parse_error (new, "Invalid macro name.  Ignoring...\n");
      return 0;
    }

  /* Create a new macro definition. */
  macro = (Macro *) malloc (sizeof (Macro));
  macro->expr = new;
  macro->next = NULL;
  
  /* If there is already a macro of this name in the table, append this
   * one to the list of macros with this name.
   */
  if (lookup_symbol (sym, name, &symbol_info, NULL) == HASH_NOERR)
    {
      Macro *tmp = (Macro *) symbol_info.p;
      for (; tmp->next != NULL; tmp = tmp->next);
      tmp->next = macro;
    }
  else   /* Add this macro's definition to the hash table. */
    {
      symbol_info.p = macro;
      insert_symbol (sym, name, symbol_info);
    }

  return 1;
}


static int
parse_defopcode (SymbolTable *sym, List *new)
{
#if	!defined(__GNUC__)	/* It's not clear which .h file I should */
  extern void *alloca(int);	/* pull in to get this declaration */
#endif
  const char *name = NULL;
  List dummy1 = { /* new */ 0, NULL, { /* new->token */ 0 } };
  List dummy2 = { NULL, /* &dummy1 */ 0, { /* new->token */ 0 } };
  SAFE_DECL ();

  dummy1.cdr = new;
  dummy1.token = new->token;
  dummy2.car = &dummy1;
  dummy2.token = new->token;

  dummy1.token.type = TOK_DEFOPCODE;
  dummy1.token.u.string = "defopcode";
  dummy2.token.type = TOK_LIST;
  dummy2.token.u.string = NULL;

  if (new->token.type == TOK_QUOTED_STRING
      || new->token.type == TOK_IDENTIFIER)
    name = new->token.u.string;

  /* Make sure the opcode name is legitimate. */
  if (name == NULL)
    {
      parse_error (new, "Invalid opcode name.  Ignoring...\n");
      return 0;
    }

  /* Perform initial macro substitutions. */
  reduce (&dummy2, sym, 0);

  /* Generate the opcode. */
  if (!preprocess_only)
    {
      ParsedOpcodeInfo info;
      List *l = CDR (CADDAR (&dummy2)), *err;
      CCVariant *var;
      int num_variants;
      List *code_backup[MAX_VARIANTS];
      const char *bits_to_expand[MAX_VARIANTS][MAX_VARIANTS];
      int num_bits_to_expand[MAX_VARIANTS];
      int max_num_bits_to_expand = 0;
      int i;

      num_variants = list_length (&dummy1) - 3;
      if (list_length (l) != 4)
	{
	  parse_error (l, "Missing arguments; should be 4, found %d\n",
		       list_length (l));
	  return 0;
	}

      if (num_variants > MAX_VARIANTS)
	{
	  parse_error (l, "Error: too many variants.  Only %d allowed.  To "
		       "raise, change MAX_VARIANTS in defopcode.h.\n",
		       MAX_VARIANTS);
	  return 0;
	}

      /* Parse in the instruction information. */
      memset (&info, 0, sizeof info);
      info.name = CDAR (&dummy2)->token.u.string;
      info.cpu = l->token.u.n;
      info.amode = CDR (l);
      info.misc_flags = CDDR (l);

      if (info.misc_flags->token.type != TOK_LIST)
	parse_error (info.misc_flags, "Expecting a list of misc flags!\n");
      else
	{
	  List *ls;
	  for (ls = info.misc_flags->car; ls != NULL; ls = ls->cdr)
	    {
	      char buf[256];

	      switch (ls->token.type) {
	      case TOK_ENDS_BLOCK:
		info.ends_block = TRUE;
		break;
	      case TOK_DONT_POSTINCDEC_UNEXPANDED:
		info.dont_postincdec_unexpanded = TRUE;
		break;
	      case TOK_NEXT_BLOCK_DYNAMIC:
		info.next_block_dynamic = TRUE;
		break;
	      case TOK_SKIP_TWO_OPERAND_WORDS:
		info.operand_words_to_skip = 2;
		break;
	      case TOK_SKIP_FOUR_OPERAND_WORDS:
		info.operand_words_to_skip = 4;
		break;
	      case TOK_SKIP_ONE_POINTER:
	        info.operand_words_to_skip = PTR_WORDS;
	        break;
	      case TOK_SKIP_TWO_POINTERS:
	        info.operand_words_to_skip = PTR_WORDS * 2;
	        break;
	      default:
		parse_error (ls, "Unknown misc flag: %s",
			     unparse_token (&ls->token, buf));
		break;
	      }
	    }
	}

      info.opcode_bits[0] = '\0';
      err = CDDDR (l);  /* For later, in case we need it. */
      if (CDDDR (l)->token.type != TOK_LIST)
	{
	  parse_error (CDDDR (l), "Expecting explicit list of opcode bits.\n");
	  strcpy (info.opcode_bits, "0000000000000000");
	}
      else
	for (l = CDR (CADDDR (l)); l != NULL; l = l->cdr)
	  {
	    if (0 && strlen (l->token.u.string) != 16)
	      {
		/* This seems a little harsh... */
		parse_error (l, "Need exactly 16 characters in each bit "
			     "pattern specification string, found %d.\n",
			     strlen (l->token.u.string));
		strcat (info.opcode_bits, "0000000000000000");
	      }
	    else
	      strcat (info.opcode_bits, l->token.u.string);
	  }
      if (strlen (info.opcode_bits) % 16)
	{
	  parse_error (err, "Not a multiple of 16 opcode bits.\n");
	  strcpy (info.opcode_bits, "0000000000000000");
	}
      info.cc_variant = NULL;

      /* Parse the information for the cc variants. */
      var = (CCVariant *) SAFE_alloca (num_variants * sizeof (CCVariant));
      memset (var, 0, num_variants * sizeof (CCVariant));
      info.cc_variant = var;

      /* Loop through and parse each cc variant. */
      for (i = num_variants - 1, l = CDDDR (&dummy1); i >= 0; i--, l = l->cdr)
	{
	  static const struct {
	    char character;
	    unsigned char cc_may_set:1;
	    unsigned char cc_may_not_set:1;
	    unsigned char cc_to_known_value:1;
	    unsigned char cc_known_values:1;
	  } cc_bit_info[] = {
	    { '0', 1, 0, 1, 0 },    /* 0  ->  always force cc bit to 0.    */
	    { '1', 1, 0, 1, 1 },    /* 1  ->  always force cc bit to 1.    */
	    { 'C', 1, 0, 0, 0 },    /* Any letter means will always set    */
	    { 'N', 1, 0, 0, 0 },    /*     cc bit to either 0 or 1, but    */
	    { 'V', 1, 0, 0, 0 },    /*     we can't determine which at     */
	    { 'X', 1, 0, 0, 0 },    /*     compile time.                   */
	    { 'Z', 1, 0, 0, 0 },    /*                                     */
	    { '-', 0, 1, 0, 0 },    /* -  ->  cc bit always unchanged.     */
	    { '>', 1, 1, 0, 0 },    /* >  ->  cc set to 1 or unchanged.    */
	    { '<', 1, 1, 0, 0 },    /* <  ->  cc set to 0 or unchanged.    */
	    { '?', 1, 0, 0, 0 },    /* ?  ->  cc bit undefined.            */
	    { '%', 1, 1, 0, 0 },    /* %  ->  cc might change, might not.  */
	  };
	  int guess, bit;
	  const char *b;
	  List *bte;
	  
	  /* Insert them in reverse order. */
	  if (i > 0)
	    var[i - 1].next = &var[i];

	  /* Parse the cc bits set specs. */
	  b = (CDAR (l))->token.u.string;
	  if (strlen (b) != 5)
	    {
	      parse_error (CDDAR (l), "Need exactly five characters in "
			   "cc bits specification.\n");
	      b = "-----";
	    }
			 
	  for (bit = 0; bit < 5; bit++)
	    {
	      for (guess = sizeof cc_bit_info / sizeof cc_bit_info[0] - 1;
		   guess >= 0; guess--)
		if (cc_bit_info[guess].character == b[4 - bit])
		  {
		    var[i].cc_may_set |= cc_bit_info[guess].cc_may_set << bit;
		    var[i].cc_may_not_set
		      |= cc_bit_info[guess].cc_may_not_set << bit;
		    var[i].cc_to_known_value
		      |= cc_bit_info[guess].cc_to_known_value << bit;
		    var[i].cc_known_values
		      |= cc_bit_info[guess].cc_known_values << bit;
		    break;
		  }
	      if (guess < 0)
		parse_error (CDAR (l), "Unknown character '%c' in cc bit "
			     "specification.\n", b[4 - bit]);
	    }

	  /* Parse the cc bits needed specs. */
	  b = (CDDAR (l))->token.u.string;
	  if (strlen (b) != 5)
	    {
	      parse_error (CDDAR (l), "Need exactly five characters in "
			   "cc bits specification.\n");
	      b = "-----";
	    }
	  
	  for (bit = 0; bit < 5; bit++)
	    {
	      if (strchr ("-CNVXZ", b[4 - bit]) == NULL)
		parse_error (CDDAR (l), "Illegal character '%c' in cc bits "
			     "needed specification.  Must be one of "
			     "-,C,N,V,X,Z.\n",
			     b[4 - bit]);
	      var[i].cc_needed |= (b[4 - bit] != '-') << bit;
	    }


	  /* Fetch all of the bits_to_expand strings into an array.
	   * Terminate the list provided by the user with "----------------"
	   * so we guarantee that all legal bit patterns get code generated
	   * for them.
	   */
	  num_bits_to_expand[i] = 0;
	  bte = CADDR (CDAR (l));
	  if (bte->token.type != TOK_EXPLICIT_LIST)
	    parse_error (bte, "Expecting a list of bit pattern strings.\n");
	  else
	    {
	      for (bte = bte->cdr; bte != NULL; bte = bte->cdr)
		bits_to_expand[i][num_bits_to_expand[i]++]
		  = bte->token.u.string;
	    }
	  bits_to_expand[i][num_bits_to_expand[i]++]
	    = "----------------";
	  if (num_bits_to_expand[i] > max_num_bits_to_expand)
	    max_num_bits_to_expand = num_bits_to_expand[i];
	  var[i].code = CDDR (CDDAR (l));

	  {
	    List *native = CDDR (CDDAR (l));
	    if (native != NULL && CAR (native) != NULL
		&& (CAR (native))->token.type == TOK_NATIVE_CODE)
	      {
		if ((CDAR (native))->token.type != TOK_QUOTED_STRING)
		  parse_error (CAR (native), "Faulty native code specifier.");
#ifdef GENERATE_NATIVE_CODE

		{
		  char buf[1024];
		  List *xx;

		  buf[0] = '\0';
		  for (xx = CDAR (native); xx != NULL; xx = xx->cdr)
		    {
		      if (xx->token.type == TOK_QUOTED_STRING)
			strcat (buf, xx->token.u.string);
		      else
			parse_error (xx, "Expected string here.");
		    }
		  if (buf[0] == '\0' || !strcmp (buf, "none"))
		    var[i].native_code_info = NULL;
		  else
		    var[i].native_code_info = unique_string (buf);
		}
#endif
		var[i].code = native->cdr;
	      }
	    else
	      {
#ifdef GENERATE_NATIVE_CODE
		var[i].native_code_info = NULL;
#endif
		var[i].code = CDDR (CDDAR (l));
	      }
	  }

#ifndef GENERATE_NATIVE_CODE
	  var[i].native_code_info = NULL;
#endif
	}

      /* Verify that all of the fields they specified actually exist. */
      for (i = 0; i < num_variants; i++)
	verify_fields_exist (info.opcode_bits, var[i].code);

      /* Loop through and generate code for each bits_to_expand specified.
       * This is not elegant; originally, only one bits_to_expand string
       * could be specified, and its semantics were more limited than the
       * new, more powerful versions.  To compensate, we will call the
       * lower level routines once for each bits_to_expand string, and
       * modify the legal addressing mode expression to provide us with the
       * new literal bits matching capability of the new bits_to_expand
       * strings.
       */
      
      for (i = 0; i < num_variants; i++)
	{
/*
 * TODO:  FIXME -- I don't like "17" below
 */
	  /* We allocate 17 bytes because we can only expand up to 16
	   * bits, and we have one extra for the terminating 0.
	   */
	  var[i].bits_to_expand = (char *) malloc (17);
	  code_backup[i] = var[i].code;
	}

      for (i = 0; i < max_num_bits_to_expand; i++)
	{
	  char intersect[MAX_VARIANTS][17];
	  List *saved_amode = info.amode;
	  List *old_cdr = info.amode->cdr;
	  int j;
	  
	  info.amode->cdr = NULL;

	  /* Preserve code. */
	  for (j = 0; j < num_variants; j++)
	    var[j].code = copy_list (code_backup[j]);

	  /* Grab all of the bits_to_expand strings. */
	  for (j = 0; j < num_variants; j++)
	    {
	      char *p;
	      strcpy (var[j].bits_to_expand, ((i < num_bits_to_expand[j])
					      ? bits_to_expand[j][i]
					      : "----------------"));
	      
	      /* Generate the intersection string. */
	      strcpy (intersect[j], var[j].bits_to_expand);
	      for (p = intersect[j]; *p != '\0'; p++)
		if (*p == '-')
		  *p = 'x';
	      
	      /* Expand all literal bits by replacing them with 'x'.*/
	      for (p = var[j].bits_to_expand; *p != '\0'; p++)
		if (*p == '0' || *p == '1')
		  *p = 'x';
	    }
	  
	  /* Intersect with all of the literal bits we know about. */
	  for (j = 0; j < num_variants; j++)
	    if (strcmp (intersect[j], "xxxxxxxxxxxxxxxx"))
	      {
		List *isect = alloc_list ();
		isect->token.type = TOK_LIST;
		isect->car = alloc_list ();
		isect->car->token.type = TOK_INTERSECT;
		isect->car->token.u.string = "intersect";
		isect->car->cdr = alloc_list ();
		isect->car->cdr->token.type = TOK_QUOTED_STRING;
		isect->car->cdr->token.u.string = intersect[j];
		isect->car->cdr->cdr = info.amode;
		info.amode = isect;
	      }

	  /* Generate the actual code. */
	  generate_opcode (&info, sym);

	  /* Memory leak here, but who cares? */
	  info.amode = saved_amode;
	  info.amode->cdr = old_cdr;
	}
	ASSERT_SAFE(var);
    }

  return 1;
}


static void
verify_fields_exist (const char *bits, List *code)
{
  PatternRange r;

  if (code == NULL)
    return;
  if (IS_DOLLAR_TOKEN (code->token.type)
      && !pattern_range (bits, code->token.u.dollarinfo.which, &r))
    parse_error (code, "Unknown field number %d; there are not that many "
		 "fields.\n", code->token.u.dollarinfo.which);
  verify_fields_exist (bits, code->car);
  verify_fields_exist (bits, code->cdr);
}


List *
string_to_list (const char *string, const char *include_dirs[])
{
  FILE *temp;
  List *enclosing_list;
  Token t;
  char buf[256];
  char filename[32] = "/tmp/syngenXXXXXX";
  static int busy_p = FALSE;
  int fd;

  assert (!busy_p);
  busy_p = TRUE;

  /* POOR IMPLEMENTATION - creates temp file to get a FILE * ! */
#if 0
  /* Loses mysteriously under linux.  Be totally paranoid now. */
  temp = tmpfile ();
#else
    assert ((fd = mkstemp (filename)) >= 0);
    temp = fdopen (fd, "w");
#endif
  if (temp == NULL)
    {
#if !defined (__MINGW32__)
      extern int errno;
      fatal_error ("string_to_list: Unable to create temp file!  %s\n",
		   strerror (errno));
#else
      fatal_error ("string_to_list: Unable to create temp file!\n");
#endif
    }

  fputs (string, temp);
  fclose (temp);
  open_file (filename, NULL);
  temp = current_stream ();

  if (!fetch_next_token (&t))
    {
      fatal_error ("Empty expression specified.\n");
      return NULL;
    }
  if (t.type != TOK_LEFT_PAREN)
    {
      input_error ("Expression must begin with open paren, not \"%s\"\n",
		   unparse_token (&t, buf));
      return NULL;
    }
 
  enclosing_list = alloc_list ();
  enclosing_list->car = parse_expression ();
  enclosing_list->token.type     = TOK_LIST;
  enclosing_list->token.u.string = "[LIST]";
  enclosing_list->token.lineno   = 1;
  enclosing_list->token.filename = "<command line>";

  /* No need to fclose here; that will automatically happen when
   * the parser runs out of tokens for this file.
   */
  if (current_stream () == temp)
    {
      close_file ();
      assert (current_stream () != temp);
    }
#ifdef NeXT
  remove (filename);
#else
  unlink (filename);
#endif

  busy_p = FALSE;

  return enclosing_list;
}


/* Recursively parses a list from the token stream.  It assumes that, on entry,
 * the '(' starting the list has already been consumed.  A list is terminated
 * by a ')'.
 */

static List *
parse_expression ()
{
  List *ret = NULL, *new, **last = &ret;
  Token t;

  while (1)
    {
      if (!fetch_next_token (&t))
	fatal_error ("Premature EOF.\n");
      if (t.type == TOK_RIGHT_PAREN)
	return ret;
      
      /* Create a new list and append it to the current one. */
      new = *last = alloc_list ();
      new->token = t;
      last = &new->cdr;
      
      if (t.type == TOK_LEFT_PAREN)
	{
	  new->token.type = TOK_LIST;
	  new->car = parse_expression ();
	}
    }
}

