#include "common.h"
#include "list.h"
#include "generatecode.h"
#include "bitstring.h"
#include "error.h"
#include "reduce.h"
#include "uniquestring.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

/* #define	TEMPS_AT_TOP */

static char *cc_bits_string (int bits, char *buf);
static char *opcode_bits_string (int bits, const ParsedOpcodeInfo *info,
				 const CCVariant *var, char *buf);
static void generate_code_for_list (List *ls, int m68kop,
				    const ParsedOpcodeInfo *info,
				    const CCVariant *var);
static void binary_op_list (const char *op, List *ls, int m68kop,
			    const ParsedOpcodeInfo *info,
			    const CCVariant *var);
static List *replace_dollar_elements (List *code, int m68kop,
				      const ParsedOpcodeInfo *info,
				      const CCVariant *var);
static void handle_constant_dollar_field (List *ls, int constant,
					  int field_length);
static void output_c_for_amode_ptr (const Token *t, BOOL reversed);
static void generate_temp_decls (List *code);
static void transform_reg_to_var_and_decl (List *code);


/* These are used to make sure that things like preincrement
 * and postdecrement (and other one-time side effects) only happen
 * once even if the responsible expression occurs multiple times.
 */
static char c_preamble[8192], c_postamble[8192];


/* This routine outputs a case synop: to syn68k_c_stream. */
void
generate_c_code (const ParsedOpcodeInfo *info, const CCVariant *var,
		 int m68kop, int synop, SymbolTable *sym,
		 const char *operand_decls, const char *postcode,
		 const OpcodeMappingInfo *map)
{
  static const char cc_name[][16] = { "cpu_state.ccc", "cpu_state.ccn",
					"cpu_state.ccv", "cpu_state.ccx",
					"cpu_state.ccnz" };
  char buf1[6], buf2[6], buf3[6], buf4[17];
  int opcw;
  List *code = copy_list (var->code);
  int i;

  /* Clear out preamble and postamble. */
  c_preamble[0] = c_postamble[0] = '\0';

  /* Replace $ elements with appropriate things. */
  code = replace_dollar_elements (code, m68kop, info, var);

  /* Simplify the resulting code. */
  code = reduce (code, sym, 0);

  /* Output human-readable comment describing this synthetic opcode. */
  fprintf (syn68k_c_stream,
	   "\n      /* %s %s ms:%s mns:%s needs:%s */\n",
	   info->name, opcode_bits_string (m68kop, info, var, buf4),
	   cc_bits_string (var->cc_may_set, buf1),
	   cc_bits_string (var->cc_may_not_set, buf2),
	   cc_bits_string (var->cc_needed, buf3));

  /* Output case. */
  fprintf (syn68k_c_stream, "      CASE (0x%04X)\n", (unsigned) synop);
  fprintf (syn68k_c_stream, "        CASE_PREAMBLE (\"%s\", \"%s\", \"%s\", "
	   "\"%s\", \"%s\")\n",
	   info->name, opcode_bits_string (m68kop, info, var, buf4),
	   cc_bits_string (var->cc_may_set, buf1),
	   cc_bits_string (var->cc_may_not_set, buf2),
	   cc_bits_string (var->cc_needed, buf3));

  /* Output operand declarations. */
  fputs (operand_decls, syn68k_c_stream);

  /* Transform temp variables only used as one type to temp_variables,
   * and output decls for those variables.
   */
  transform_reg_to_var_and_decl (code);

  /* Output temp variable declarations. */
  generate_temp_decls (code);

  if (code->token.type != TOK_LIST)
    {
      parse_error (code, "Expecting code description list!\n");
      return;
    }

  /* Output preamble. */
  if (c_preamble[0] != '\0')
    fprintf (syn68k_c_stream, "        %s\n", c_preamble);
  
  /* Generate the code they specified. */
  fputs ("        ", syn68k_c_stream);
  generate_code_for_list (code, m68kop, info, var);
  fputs (";\n", syn68k_c_stream);
  
  /* Output postamble. */
  if (c_postamble[0] != '\0')
    fprintf (syn68k_c_stream, "        %s\n", c_postamble);
  
  /* Set any cc bits to 0 or 1 that are explicitly mentioned in the
   * cc variant description.  This saves the user some work generating
   * the 68k description file and reduces the chance of error.
   */
  for (i = 0; i < 4; i++)
    if ((var->cc_to_known_value >> (4 - i)) & 1)
      fprintf (syn68k_c_stream, "        %s = %d;\n", cc_name[i],
	       (var->cc_known_values >> (4 - i)) & 1);
  if (var->cc_to_known_value & 1)   /* Invert sense of Z bit. */
    fprintf (syn68k_c_stream, "        %s = %d;\n", cc_name[4],
	     !(var->cc_known_values & 1));

  if (postcode != NULL && postcode[0] != '\0')
    fprintf (syn68k_c_stream, "        %s\n", postcode);

  /* Count opcode words. */
  opcw = synthetic_opcode_size (map) + info->operand_words_to_skip;

  /* Only adjust code ptr if we didn't just end a block. */
  if (!info->ends_block)
    {
      /* Always increment enough so that the next opcode is guaranteed
       * to be a properly aligned pointer.
       */
      fprintf (syn68k_c_stream, "        CASE_POSTAMBLE (ROUND_UP (%d))\n",
	       (int) opcw);
    }

  else    /* If we end a block, profile the next block. */
    {
/* #if !defined (__alpha) /* FIXME -- TODO -- just use __alpha case for everyone */
#if SIZEOF_CHAR_P != 8
      fputs ("\n#ifdef PROFILE\n"
	     "       profile_block (hash_lookup "
	     "(READUL (US_TO_SYN68K (code) - 4)));\n"
	     "#endif\n",
	     syn68k_c_stream);
#else
      fputs ("\n#ifdef PROFILE\n"
	     "       profile_block (hash_lookup "
	     "(READUL_US ((unsigned long)code - 4)));\n"
	     "#endif\n",
	     syn68k_c_stream);
#endif

#if defined (SYNCHRONOUS_INTERRUPTS) && !defined (GENERATE_NATIVE_CODE)
      fprintf (syn68k_c_stream,
"#if SIZEOF_CHAR_P != 8\n"
"	    CHECK_FOR_INTERRUPT (READUL (US_TO_SYN68K (code - PTR_WORDS)));\n"
"#else\n"
"	    CHECK_FOR_INTERRUPT (READUL_US (code - PTR_WORDS));\n"
"#endif\n");
#endif
      fprintf (syn68k_c_stream, "        CASE_POSTAMBLE "
	       "(ROUND_UP (PTR_WORDS))\n");
    }
  /* Done with this instruction. */
}


/* Handy tables. */
static const char *ctypes[2][5] = {
  { "void", "uint8", "uint16", "INTERNAL_ERROR", "uint32" },
  { "void", "int8",  "int16",  "INTERNAL_ERROR", "int32"  }
};
static const char *regdesc[2][5] = {
  { "", ".ub.n", ".uw.n", "", ".ul.n" },
  { "", ".sb.n", ".sw.n", "", ".sl.n" }
};


static void
generate_code_for_list (List *ls, int m68kop, const ParsedOpcodeInfo *info,
			const CCVariant *var)
{
  char buf[512];
  Token *t;
  BOOL done;

  if (ls == NULL)
    return;

  /* If they have several sublists of code, generate code for each of them. */
  t = &ls->token;
  switch (t->type) {

    /* If we've found a list, look at the operator for that list and
     * generate code appropriately. */
  case TOK_LIST:
    if (ls->car == NULL)
      return;
    ls = ls->car;
    t = &ls->token;

    switch (ls->token.type) {
    case TOK_EXPLICIT_LIST:
      /* Loop over all the lists of code and generate c for them. */
      for (ls = ls->cdr; ls != NULL; ls = ls->cdr)
	{
	  generate_code_for_list (ls, m68kop, info, var);
	  fputs (";\n", syn68k_c_stream);
	  fputs ("        ", syn68k_c_stream);
	}
      break;

    case TOK_ASSIGN:
      putc ('(', syn68k_c_stream);
      done = FALSE;

      /* If they are writing out to a long, generate appropriate code
       * that will work on a QUADALIGN machine.
       */
      if (ls->cdr->token.type == TOK_LIST
	  && CADR (ls)->token.type == TOK_DEREF
	  && CADR (ls)->token.u.derefinfo.size == 4)
	{
	  fprintf (syn68k_c_stream,
		   "WRITE%cL_UNSWAPPED (SYN68K_TO_US (CLEAN (",
		   CADR (ls)->token.u.derefinfo.sgnd ? 'S' : 'U');
	  generate_code_for_list (CDADR (ls), m68kop, info, var);
	  fputs (")), ", syn68k_c_stream);
	  generate_code_for_list (CDDR (ls), m68kop, info, var);
	  fputs (") ", syn68k_c_stream);
	  done = TRUE;
	}
      if ((ls->cdr->token.type == TOK_AMODE
	   || ls->cdr->token.type == TOK_REVERSED_AMODE
	   || ls->cdr->token.type == TOK_DOLLAR_AMODE
	   || ls->cdr->token.type == TOK_DOLLAR_REVERSED_AMODE)
	  && ls->cdr->token.u.amodeinfo.size == 4)
	{
	  fprintf (syn68k_c_stream,
		   "WRITE%cL_UNSWAPPED (SYN68K_TO_US (CLEAN (",
		   ls->cdr->token.u.derefinfo.sgnd ? 'S' : 'U');
	  switch (ls->cdr->token.type) {
	  case TOK_DOLLAR_AMODE:
	    fputs ("US_TO_SYN68K (cpu_state.amode_p) ", syn68k_c_stream);
	    break;
	  case TOK_DOLLAR_REVERSED_AMODE:
	    fputs ("US_TO_SYN68K (cpu_state.reversed_amode_p) ",
		   syn68k_c_stream);
	    break;
	  case TOK_AMODE:
	    ls->cdr->token.type = TOK_AMODE_PTR;
	    generate_code_for_list (ls->cdr, m68kop, info, var);
	    ls->cdr->token.type = TOK_AMODE;
	    break;
	  case TOK_REVERSED_AMODE:
	  default:
	    ls->cdr->token.type = TOK_REVERSED_AMODE_PTR;
	    generate_code_for_list (ls->cdr, m68kop, info, var);
	    ls->cdr->token.type = TOK_REVERSED_AMODE;
	    break;
	  }
	  fputs (")), ", syn68k_c_stream);
	  generate_code_for_list (CDDR (ls), m68kop, info, var);
	  fputs (") ", syn68k_c_stream);
	  done = TRUE;
	}


      /* Do stuff like (assign d0.ul.n (+ d0.ul.n 5)) -> "d0.ul.n += 5" */
      if (!done && CDDR (ls)->token.type == TOK_LIST
	  && list_length (CADDR (ls)) == 3
	  && tokens_equal (&CDR (CADDR (ls))->token, &CDR (ls)->token))
	{
	  if (CDR (ls)->token.type != TOK_LIST
	      || lists_equal (CADR (ls), CADR (CADDR (ls))))
	    {
	      switch (CADDR (ls)->token.type) {
	      case TOK_BITWISE_AND:
	      case TOK_BITWISE_OR:
	      case TOK_BITWISE_XOR:
	      case TOK_PLUS:
	      case TOK_MINUS:
	      case TOK_MULTIPLY:
	      case TOK_DIVIDE:
	      case TOK_MOD:
	      case TOK_SHIFT_LEFT:
	      case TOK_SHIFT_RIGHT:
		/* Word-sized operations to address registers deal with
		 * the entire address register; the RHS is sign extended.
		 */
		if (ls->cdr->token.type == TOK_ADDRESS_REGISTER
		    && ls->cdr->token.u.reginfo.size == 2)
		  {
		    ls->cdr->token.u.reginfo.size = 4;
		    generate_code_for_list (ls->cdr, m68kop, info, var);
		    fputs (unparse_token (&CADDR (ls)->token, buf),
			   syn68k_c_stream);
		    fputs ("= (int16)", syn68k_c_stream);
		    generate_code_for_list (CDDR (CADDR (ls)), m68kop,
					    info, var);
		  }
		else  /* Not a word-sized op to an address register. */
		  {
		    TokenType optype = CADDR (ls)->token.type;

		    /* Change foo += 1 to ++foo.  Why?  It turns out
		     * that gcc sometimes generates better code for ++foo.
		     * Annoying, isn't it?
		     */
		    if ((optype == TOK_PLUS || optype == TOK_MINUS)
			&& CDDR (CADDR (ls))->token.type == TOK_NUMBER
			&& (CDDR (CADDR (ls))->token.u.n == 1
			    || CDDR (CADDR (ls))->token.u.n == -1))
		      {
			if ((optype == TOK_PLUS)
			    ^ (CDDR (CADDR (ls))->token.u.n == -1))
			  fputs ("(++", syn68k_c_stream);
			else
			  fputs ("(--", syn68k_c_stream);
			generate_code_for_list (ls->cdr, m68kop, info, var);
			fputs (") ", syn68k_c_stream);
		      }
		    /* Change foo += 5 to a weird macro...this also seems
		     * to help gcc.
		     */
		    else if ((optype == TOK_PLUS || optype == TOK_MINUS)
			     && CDDR (CADDR (ls))->token.type == TOK_NUMBER
			     && ls->cdr->token.u.reginfo.size == 4
			     && ls->cdr->token.u.n != (int32) 0x80000000)
		      {
			int32 val = CDDR (CADDR (ls))->token.u.n;
			if (val >= 0)
			  {
			    if (optype == TOK_PLUS)
			      fputs ("(INC_VAR (", syn68k_c_stream);
			    else
			      fputs ("(DEC_VAR (", syn68k_c_stream);
			    generate_code_for_list (ls->cdr, m68kop, info,
						    var);
			    fprintf (syn68k_c_stream, ", %ld)) ", (long) val);
			  }
			else  /* val < 0 */
			  {
			    if (optype == TOK_PLUS)
			      fputs ("(DEC_VAR (", syn68k_c_stream);
			    else
			      fputs ("(INC_VAR (", syn68k_c_stream);
			    generate_code_for_list (ls->cdr, m68kop, info,
						    var);
			    fprintf (syn68k_c_stream, ", %ld)) ", (long) -val);
			  }
		      }
		    else
		      {
			generate_code_for_list (ls->cdr, m68kop, info, var);
			fputs (unparse_token (&CADDR (ls)->token, buf),
			       syn68k_c_stream);
			fputs ("= ", syn68k_c_stream);
			generate_code_for_list (CDDR (CADDR (ls)), m68kop,
						info, var);
		      }
		  }
		done = TRUE;
		break;
	      default:
		break;
	      }
	    }
	}

      /* See if we are assigning something to itself.  This can happen
       * when expanding out code, and gcc hasn't been eliminating these
       * pointless assigns.  Here we just output the LHS of the assignment,
       * in case the value of the assignment as a whole is being used for
       * something (eg, "d0.ul.n = d0.ul.n"  ->  "d0.ul.n")
       */
      if (!done)
	{
	  if (tokens_equal (&ls->cdr->token, &(CDDR (ls))->token)
	      && lists_equal (CADR (ls), CADDR (ls)))
	    {
	      generate_code_for_list (ls->cdr, m68kop, info, var);
	      done = TRUE;
	    }
	}

      if (!done)
	{
	  /* Force word moves into address registers to be sign extended. */
	  if (ls->cdr->token.type == TOK_ADDRESS_REGISTER
	      && ls->cdr->token.u.reginfo.size == 2)
	    {
	      ls->cdr->token.u.reginfo.size = 4;
	      generate_code_for_list (ls->cdr, m68kop, info, var);
	      fputs ("= (int16) ", syn68k_c_stream);
	    }
	  else
	    {
	      generate_code_for_list (ls->cdr, m68kop, info, var);
	      fputs ("= ", syn68k_c_stream);
	    }
	  generate_code_for_list (CDDR (ls), m68kop, info, var);
	}
      fputs (") ", syn68k_c_stream);
      break;

      /* Have to special case boolean xor since C doesn't have ^^ analagous to
       * && and ||.
       */
    case TOK_XOR:
      for (ls = ls->cdr; ls != NULL; ls = ls->cdr)
	{
	  fputs ("(!", syn68k_c_stream);
	  generate_code_for_list (ls, m68kop, info, var);
	  fputs (") ", syn68k_c_stream);
	  if (ls->cdr != NULL)
	    fputs ("^ ", syn68k_c_stream);
	}
      break;
    case TOK_NOT:
      if (ls->cdr == NULL)
	{
	  parse_error (ls, "\"not\" missing an argument!\n");
	  fputs ("0 ", syn68k_c_stream);
	}
      else
	{
	  fputs ("(!", syn68k_c_stream);
	  generate_code_for_list (ls->cdr, m68kop, info, var);
	  fputs (") ", syn68k_c_stream);
	}
      break;

    case TOK_BITWISE_NOT:
      if (ls->cdr == NULL)
	{
	  parse_error (ls, "\"~\" missing an argument!\n");
	  fputs ("0 ", syn68k_c_stream);
	}
      else
	{
	  fputs ("(~", syn68k_c_stream);
	  generate_code_for_list (ls->cdr, m68kop, info, var);
	  fputs (") ", syn68k_c_stream);
	}
      break;

    case TOK_AND:
      binary_op_list ("&&", ls->cdr, m68kop, info, var);
      break;
    case TOK_OR:
      binary_op_list ("||", ls->cdr, m68kop, info, var);
      break;
    case TOK_EQUAL:
      binary_op_list ("==", ls->cdr, m68kop, info, var);  /* 2 args but OK. */
      break;
    case TOK_NOT_EQUAL:
      binary_op_list ("!=", ls->cdr, m68kop, info, var);  /* 2 args but OK. */
      break;
    case TOK_GREATER_THAN:
    case TOK_LESS_THAN:
    case TOK_GREATER_OR_EQUAL:
    case TOK_PLUS:
    case TOK_MINUS:
    case TOK_LESS_OR_EQUAL:
    case TOK_MULTIPLY:
    case TOK_DIVIDE:
    case TOK_MOD:
    case TOK_BITWISE_AND:
    case TOK_BITWISE_OR:
    case TOK_BITWISE_XOR:
      unparse_token (&ls->token, buf);
      binary_op_list (buf, ls->cdr, m68kop, info, var);
      break;

    case TOK_SHIFT_LEFT:
    case TOK_SHIFT_RIGHT:
      unparse_token (&ls->token, buf);
      binary_op_list (buf, ls->cdr, m68kop, info, var);
      break;

    case TOK_DEFAULT:
      fputs ("default ", syn68k_c_stream);
      break;

    case TOK_SWITCH:
      fputs ("switch (", syn68k_c_stream);
      generate_code_for_list (ls->cdr, m68kop, info, var);
      fputs (") {\n", syn68k_c_stream);
      for (ls = CDDR (ls); ls != NULL; ls = ls->cdr)
	{
	  if (ls->car->token.type == TOK_DEFAULT)
	    fputs ("        default: ", syn68k_c_stream);
	  else
	    {
	      fputs ("        case ", syn68k_c_stream);
	      generate_code_for_list (ls->car, m68kop, info, var);
	      fputs (": ", syn68k_c_stream);
	    }
	  generate_code_for_list (CDAR (ls), m68kop, info, var);
	  fputs ("; break;\n", syn68k_c_stream);
	}
      fputs ("}\n        ", syn68k_c_stream);
      break;

    case TOK_IF:
      fputs ("if (", syn68k_c_stream);
      generate_code_for_list (ls->cdr, m68kop, info, var);
      fputs (") { ", syn68k_c_stream);
      generate_code_for_list (CDDR (ls), m68kop, info, var);
      fputs (";} ", syn68k_c_stream);
      if (CDDDR (ls) != NULL)
	{
	  fputs ("else { ", syn68k_c_stream);
	  generate_code_for_list (CDDDR (ls), m68kop, info, var);
	  fputs (";} ", syn68k_c_stream);
	}
      break;

    case TOK_SWAP:
      {
	static const char *swap_macro[2][2][5] = {
	  {{ "ERR", "((uint8) (", "(SWAPUW_IFLE (", "ERR", "(SWAPUL_IFLE (" },
	   { "ERR", "((int8) (",  "(SWAPSW_IFLE (", "ERR", "(SWAPSL_IFLE (" }},
	  {{ "ERR", "((uint8) (", "(SLOW_SWAPUW_IFLE (", "ERR",
	       "(SLOW_SWAPUL_IFLE (" },
	   { "ERR", "((int8) (",  "(SLOW_SWAPSW_IFLE (", "ERR",
	       "(SLOW_SWAPSL_IFLE (" }}
	};

	fputs (swap_macro[(ls->cdr->token.type == TOK_NUMBER)]
			  [t->u.swapinfo.sgnd][t->u.swapinfo.size],
	       syn68k_c_stream);
	generate_code_for_list (ls->cdr, m68kop, info, var);
	fputs (")) ", syn68k_c_stream);
      }
      break;

    case TOK_CAST:
      if (ls->cdr == NULL || ls->cdr->token.type != TOK_QUOTED_STRING)
	{
	  parse_error (ls, "\"cast\" must be followed by a C type "
		       "in quotes.\n");
	}
      else
	{
	  fprintf (syn68k_c_stream, "((%s) ", ls->cdr->token.u.string);
	  generate_code_for_list (CDDR (ls), m68kop, info, var);
	  fputs (") ", syn68k_c_stream);
	}
      break;

    case TOK_DEREF:
      if (t->u.derefinfo.size == 0)  /* Untyped deref? */
	{
	  if (list_length (ls) != 4)
	    parse_error (ls, "Untyped deref requires three args (ptr type, "
			 "value, offset).\n");
	  else if (ls->cdr->token.type != TOK_QUOTED_STRING)
	    parse_error (ls, "Untyped deref must have second arg be "
			 "a quoted string C type.\n");
	  else
	    {
	      /* We intentionally leave out the CLEAN here; you don't
	       * want to CLEAN the code pointer, for instance!  In general
	       * if you use an untyped deref you have to be more careful...
	       */
	      fprintf (syn68k_c_stream, "(*(((%s) ",
		       ls->cdr->token.u.string);
	      generate_code_for_list (CDDR (ls), m68kop, info, var);
	      fputs (") + ", syn68k_c_stream);
	      generate_code_for_list (CDDDR (ls), m68kop, info, var);
	      fputs (")) ", syn68k_c_stream);
	    }
	}
      else
	{
	  if (list_length (ls) != 2)
	    parse_error (ls, "Typed deref must have exactly one argument!\n");
	  else
	    {
	      if (t->u.derefinfo.size != 4)
		fprintf (syn68k_c_stream, "DEREF(%s, SYN68K_TO_US (CLEAN (",
			 ctypes[t->u.derefinfo.sgnd][t->u.derefinfo.size]);
	      else
		fprintf (syn68k_c_stream,
			 "READ%cL_UNSWAPPED ((CLEAN (",
			 t->u.derefinfo.sgnd ? 'S' : 'U');
	      generate_code_for_list (ls->cdr, m68kop, info, var);
	      fputs ("))) ", syn68k_c_stream);
	    }
	}
      break;

    case TOK_FUNC_CALL:
      if (ls->cdr == NULL || ls->cdr->token.type != TOK_QUOTED_STRING)
	{
	  parse_error (ls, "\"call\" must be followed by a function name "
		       "in quotes.\n");
	}
      else
	{
	  List *l2;
	  fprintf (syn68k_c_stream, "%s (", ls->cdr->token.u.string);
	  for (l2 = CDDR (ls); l2 != NULL; l2 = l2->cdr)
	    {
	      generate_code_for_list (l2, m68kop, info, var);
	      if (l2->cdr != NULL)
		fputs (", ", syn68k_c_stream);
	    }
	  fputs (") ", syn68k_c_stream);
	}
      break;

      /* Skip NOPs, however they were introduced. */
    case TOK_NOP:
      break;

    default:
      parse_error (ls, "Unknown operator for code list: %s\n",
		   unparse_token (&ls->token, buf));
      break;
    }
    break;

    /* Skip NOPs, however they were introduced. */
  case TOK_NOP:
    break;

  case TOK_TRUE:
    fputs ("1 ", syn68k_c_stream);
    break;

  case TOK_FALSE:
    fputs ("0 ", syn68k_c_stream);
    break;

  case TOK_CODE:
    fputs ("code ", syn68k_c_stream);
    break;

  case TOK_DOLLAR_DATA_REGISTER:
    {
      Token temp = *t;
      /* Compensate for gcc i386 brain damage. */
      fprintf (syn68k_c_stream, "DATA_REGISTER_%c%c (",
	       t->u.dollarinfo.sgnd ? 'S' : 'U',
	       "?BW?L"[t->u.dollarinfo.size]);
      t->type = TOK_DOLLAR_NUMBER;
      t->u.dollarinfo.sgnd = FALSE;
      t->u.dollarinfo.size = 4;
      generate_code_for_list (ls, m68kop, info, var);
      *t = temp;
      fputs (") ", syn68k_c_stream);
    }
    break;

  case TOK_DOLLAR_ADDRESS_REGISTER:
    {
      Token temp = *t;
      fprintf (syn68k_c_stream, "ADDRESS_REGISTER_%c%c ( ",
	       t->u.dollarinfo.sgnd ? 'S' : 'U',
	       "?BW?L"[t->u.dollarinfo.size]);
      t->type = TOK_DOLLAR_NUMBER;
      t->u.dollarinfo.sgnd = FALSE;
      t->u.dollarinfo.size = 4;
      generate_code_for_list (ls, m68kop, info, var);
      *t = temp;
      fputs (") ", syn68k_c_stream);
    }
    break;

  case TOK_DOLLAR_GENERAL_REGISTER:
    {
      Token temp = *t;
      fprintf (syn68k_c_stream, "GENERAL_REGISTER_%c%c (",
	       t->u.dollarinfo.sgnd ? 'S' : 'U',
	       "?BW?L"[t->u.dollarinfo.size]);
      t->type = TOK_DOLLAR_NUMBER;
      t->u.dollarinfo.sgnd = FALSE;
      t->u.dollarinfo.size = 4;
      generate_code_for_list (ls, m68kop, info, var);
      *t = temp;
      fputs (") ", syn68k_c_stream);
    }
    break;

  case TOK_TEMP_REGISTER:
    fprintf (syn68k_c_stream, "tmp%d%s ", t->u.reginfo.which,
	     regdesc[t->u.reginfo.sgnd][t->u.reginfo.size]);
    break;

  case TOK_DATA_REGISTER:
    fprintf (syn68k_c_stream, "DATA_REGISTER_%c%c (%d) ",
	     t->u.dollarinfo.sgnd ? 'S' : 'U',
	     "?BW?L"[t->u.dollarinfo.size],
	     t->u.reginfo.which);
    break;

  case TOK_ADDRESS_REGISTER:
    fprintf (syn68k_c_stream, "ADDRESS_REGISTER_%c%c (%d) ",
	     t->u.dollarinfo.sgnd ? 'S' : 'U',
	     "?BW?L"[t->u.dollarinfo.size],
	     t->u.reginfo.which);
    break;

  case TOK_AMODE_PTR:
  case TOK_REVERSED_AMODE_PTR:
    fputs ("(US_TO_SYN68K (", syn68k_c_stream);
    output_c_for_amode_ptr (t, (t->type == TOK_REVERSED_AMODE_PTR));
    fputs ("))", syn68k_c_stream);
    break;

  case TOK_AMODE:
  case TOK_REVERSED_AMODE:
    if (t->u.dollarinfo.size != 4)
      {
	fprintf (syn68k_c_stream, "DEREF(%s, CLEAN (",
		 ctypes[t->u.dollarinfo.sgnd][t->u.dollarinfo.size]);
	output_c_for_amode_ptr (t, (t->type == TOK_REVERSED_AMODE));
	fputs (")) ", syn68k_c_stream);
      }
    else
      {
#if 0
	fputs ("\n"
	       "#ifdef QUADALIGN\n"
	       "# ifdef BIGENDIAN\n", syn68k_c_stream);

	fprintf (syn68k_c_stream, "((%s) (((((uint16 *) CLEAN (",
		 ctypes[t->u.dollarinfo.sgnd][t->u.dollarinfo.size]);
	output_c_for_amode_ptr (t, (t->type == TOK_REVERSED_AMODE));
	fputs ("))[0]) << 16) | (((uint16 *) CLEAN (", syn68k_c_stream);
	output_c_for_amode_ptr (t, (t->type == TOK_REVERSED_AMODE));
	fputs ("))[1])))\n", syn68k_c_stream);
	
	fputs ("# else\n", syn68k_c_stream);
	
	fprintf (syn68k_c_stream, "((%s) ((((uint16 *) CLEAN (",
		 ctypes[t->u.dollarinfo.sgnd][t->u.dollarinfo.size]);
	output_c_for_amode_ptr (t, (t->type == TOK_REVERSED_AMODE));
	fputs ("))[0]) | ((((uint16 *) CLEAN (", syn68k_c_stream);
	output_c_for_amode_ptr (t, (t->type == TOK_REVERSED_AMODE));
	fputs ("))[1]) << 16)))\n", syn68k_c_stream);

	fputs ("# endif\n"
	       "#else\n", syn68k_c_stream);
	fprintf (syn68k_c_stream, "(*(%s *) CLEAN (",
		 ctypes[t->u.dollarinfo.sgnd][t->u.dollarinfo.size]);
	output_c_for_amode_ptr (t, (t->type == TOK_REVERSED_AMODE));
	fputs (")) ", syn68k_c_stream);
	fputs ("\n#endif\n", syn68k_c_stream);
#else
	fprintf (syn68k_c_stream, "READ%cL_UNSWAPPED (CLEAN",
		 t->u.dollarinfo.sgnd ? 'S' : 'U');
	output_c_for_amode_ptr (t, (t->type == TOK_REVERSED_AMODE));
	fputs (") ", syn68k_c_stream);
#endif
      }
    break;

  case TOK_NUMBER:
    if (t->u.n >= 100 || t->u.n <= -100)
      fprintf (syn68k_c_stream, "((int32) 0x%lX) ", (unsigned long)t->u.n);
    else fprintf (syn68k_c_stream, "%ld ", t->u.n);
    break;

    /* Pass quoted strings on to the C compiler. */
  case TOK_QUOTED_STRING:
    fprintf (syn68k_c_stream, "%s ", t->u.string);  /* Append a space. */
    break;

  case TOK_CCC:  fputs ("cpu_state.ccc ",  syn68k_c_stream); break;
  case TOK_CCN:  fputs ("cpu_state.ccn ",  syn68k_c_stream); break;
  case TOK_CCV:  fputs ("cpu_state.ccv ",  syn68k_c_stream); break;
  case TOK_CCX:  fputs ("cpu_state.ccx ",  syn68k_c_stream); break;
  case TOK_CCNZ: fputs ("cpu_state.ccnz ", syn68k_c_stream); break;

    /* If we still haven't been able to resolve the addressing mode,
     * a pointer to the data should have been put in the appropriate
     * local variable by a preceding instruction.
     */
  case TOK_DOLLAR_AMODE:
    if (t->u.dollarinfo.size != 4)
      fprintf (syn68k_c_stream, "DEREF(%s, cpu_state.amode_p) ",
	       ctypes[t->u.dollarinfo.sgnd][t->u.dollarinfo.size]);
    else
      fprintf (syn68k_c_stream, "READ%cL_UNSWAPPED ( US_TO_SYN68K (cpu_state.amode_p)) ",
	       t->u.dollarinfo.sgnd ? 'S' : 'U');
    break;

  case TOK_DOLLAR_REVERSED_AMODE:
    if (t->u.dollarinfo.size != 4)
      fprintf (syn68k_c_stream, "DEREF(%s, cpu_state.reversed_amode_p) ",
	       ctypes[t->u.dollarinfo.sgnd][t->u.dollarinfo.size]);
    else
      fprintf (syn68k_c_stream,
	       "READ%cL_UNSWAPPED ( US_TO_SYN68K (cpu_state.reversed_amode_p)) ",
	       t->u.dollarinfo.sgnd ? 'S' : 'U');
    break;

  case TOK_DOLLAR_AMODE_PTR:
    fprintf (syn68k_c_stream, "US_TO_SYN68K (cpu_state.amode_p) ");
    break;

  case TOK_DOLLAR_REVERSED_AMODE_PTR:
    fprintf (syn68k_c_stream, "US_TO_SYN68K (cpu_state.reversed_amode_p) ");
    break;

  case TOK_DOLLAR_NUMBER:
#if	1 || !defined(TEMPS_AT_TOP)
    fprintf (syn68k_c_stream, "operand_%d ", t->u.dollarinfo.which);
#else	/* defined(TEMPS_AT_TOP) */
    fprintf (syn68k_c_stream, "operand_%s%d_%d ",
				t->u.dollarinfo.sgnd ? "" : "u",
				t->u.dollarinfo.size*8, t->u.dollarinfo.which);
#endif	/* defined(TEMPS_AT_TOP) */
    break;

  default:
    parse_error (ls, "Unknown code element: %s\n", unparse_token (t, buf));
    break;
  }
}


/* Writes out a human readable string describing a set of cc bits into buf.
 * For example, "11001" becomes "CN--Z" (the cc bits are in alphabetical
 * order).  Returns buf.
 */
static char *
cc_bits_string (int bits, char *buf)
{
  int i;

  /* Generate the string. */
  for (i = 0; i < 5; i++)
    buf[i] = (bits & (1 << (4 - i))) ? "CNVXZ"[i] : '-';
  buf[5] = '\0';

  return buf;
}


/* Generates a string describing what bits were expanded for this particular
 * CC variant of this opcode.
 */
static char *
opcode_bits_string (int bits, const ParsedOpcodeInfo *info,
		    const CCVariant *var, char *buf)
{
  const char *p, *i = info->opcode_bits;
  char *b = buf;
  int mask;
  
  /* Generate the string. */
  for (mask = 1 << 15, p = var->bits_to_expand; mask != 0; i++,p++, mask >>= 1)
    *b++ = ((*p == '-' && *i != '0' && *i != '1')
	    ? '-' : ((bits & mask) ? '1' : '0'));
  buf[16] = '\0';

  return buf;
}


/* Helper function; takes an operator string, like "+" or "&&" and a list
 * of expressions and outputs c code: (expr1 op expr2 op expr3 op ... exprn)
 */
static void
binary_op_list (const char *op, List *ls, int m68kop,
		const ParsedOpcodeInfo *info, const CCVariant *var)
{
  putc ('(', syn68k_c_stream);

  /* Output something for empty list. */
  if (ls == NULL)
    fputs ("0 ", syn68k_c_stream);
  for (; ls != NULL; ls = ls->cdr)
    {
      generate_code_for_list (ls, m68kop, info, var);
      if (ls->cdr != NULL)
	fprintf (syn68k_c_stream, "%s ", op);
    }
  putc (')', syn68k_c_stream);
}


/* Recursive helper function for replace_dollar_elements(). */
static void
rde_aux (List *code, const PatternRange *ranges, int m68kop,
	 const ParsedOpcodeInfo *info, const CCVariant *var, int expand_mask)
{
  Token *t;

  /* Base case: NULL list. */
  if (code == NULL)
    return;

  /* If we find a $ identifier, replace it with the appropriate expression. */
  t = &code->token;
  if (IS_DOLLAR_TOKEN (t->type))
    {
      int index, length;
      BOOL is_constant = FALSE;
      int constant;
      
      /* Fetch the bit range of that field. */
      index = ranges[t->u.dollarinfo.which].index;
      length = ranges[t->u.dollarinfo.which].length;
      
      /* See if this collapses to a known constant. */
      if (index + length <= 16)
	{
	  int i;
	  
	  /* Grab the constant and see if it's legitimate. */
	  constant = 0;
	  is_constant = TRUE;
	  for (i = 0; i < length; i++)
	    {
	      if (var->bits_to_expand[i + index] == '-')
		{
		  is_constant = FALSE;
		  break;
		}
	      else if (m68kop & ((1 << (15 - index)) >> i))
		constant |= (1 << (length - 1)) >> i;
	    }
	}
      else
	constant = 0;  /* To avoid compiler warnings. */

      if (is_constant)
	handle_constant_dollar_field (code, constant, length);
      else
	{
	}
    }

  /* Recurse on the rest of the list. */
  rde_aux (code->car, ranges, m68kop, info, var, expand_mask);
  rde_aux (code->cdr, ranges, m68kop, info, var, expand_mask);
}


/* Replaces all of the dollar elements with non-dollar expressions. */
static List *
replace_dollar_elements (List *code, int m68kop, const ParsedOpcodeInfo *info,
			 const CCVariant *var)
{
  int i, expand_mask = 0;
  PatternRange ranges[32];

  /* Compute the mask for the bits we are expanding. */
  for (i = 0; i < 16; i++)
    if (var->bits_to_expand[15 - i] != '-')
      expand_mask = (1 << i);

  /* Compute the field ranges for all of the fields. */
  for (i = 1; pattern_range (info->opcode_bits, i, &ranges[i]); i++)
    ;

  /* Recursively replace all of the $ identifiers with expressions. */
  rde_aux (code, ranges, m68kop, info, var, expand_mask);

  return code;
}


static void
handle_constant_dollar_field (List *ls, int constant, int field_length)
{
  Token *t = &ls->token;
  Token copy_of_t = *t;
  
  /* Switch on what type of argument this is. */
  switch (t->type) {
    
    /* Data register. */
  case TOK_DOLLAR_DATA_REGISTER:
    t->type            = TOK_DATA_REGISTER;
    t->u.reginfo.which = constant;
    t->u.reginfo.sgnd  = copy_of_t.u.dollarinfo.sgnd;
    t->u.reginfo.size  = copy_of_t.u.dollarinfo.size;
    break;
    
    /* Address register. */
  case TOK_DOLLAR_ADDRESS_REGISTER:
    t->type            = TOK_ADDRESS_REGISTER;
    t->u.reginfo.which = constant;
    t->u.reginfo.sgnd  = copy_of_t.u.dollarinfo.sgnd;
    t->u.reginfo.size  = copy_of_t.u.dollarinfo.size;
    break;

    /* General register. */
  case TOK_DOLLAR_GENERAL_REGISTER:
    t->type = (constant >= 8 ? TOK_ADDRESS_REGISTER : TOK_DATA_REGISTER);
    t->u.reginfo.which = constant % 8;
    t->u.reginfo.sgnd  = copy_of_t.u.dollarinfo.sgnd;
    t->u.reginfo.size  = copy_of_t.u.dollarinfo.size;
    break;

  case TOK_DOLLAR_AMODE:
    if (((constant >> 3) & 7) == 0)
      {
	t->type = TOK_DATA_REGISTER;
	t->u.reginfo.which = constant & 7;
	t->u.reginfo.sgnd  = copy_of_t.u.dollarinfo.sgnd;
	t->u.reginfo.size  = copy_of_t.u.dollarinfo.size;
      }
    else if (((constant >> 3) & 7) == 1)
      {
	t->type = TOK_ADDRESS_REGISTER;
	t->u.reginfo.which = constant & 7;
	t->u.reginfo.sgnd  = copy_of_t.u.dollarinfo.sgnd;
	t->u.reginfo.size  = copy_of_t.u.dollarinfo.size;
      }
    else
      {
	t->type = TOK_AMODE;
	t->u.amodeinfo.which = constant;
	t->u.amodeinfo.sgnd  = copy_of_t.u.dollarinfo.sgnd;
	t->u.amodeinfo.size  = copy_of_t.u.dollarinfo.size;
      }
    break;

  case TOK_DOLLAR_REVERSED_AMODE:
    if ((constant & 7) == 0)
      {
	t->type = TOK_DATA_REGISTER;
	t->u.reginfo.which = constant & 7;
	t->u.reginfo.sgnd  = copy_of_t.u.dollarinfo.sgnd;
	t->u.reginfo.size  = copy_of_t.u.dollarinfo.size;
      }
    else if ((constant & 7) == 1)
      {
	t->type = TOK_ADDRESS_REGISTER;
	t->u.reginfo.which = constant & 7;
	t->u.reginfo.sgnd  = copy_of_t.u.dollarinfo.sgnd;
	t->u.reginfo.size  = copy_of_t.u.dollarinfo.size;
      }
    else
      {
	t->type = TOK_REVERSED_AMODE;  /* We swap the bits here anyway. */
	t->u.amodeinfo.which = ((constant & 7) << 3) | ((constant >> 3) & 7);
	t->u.amodeinfo.sgnd  = copy_of_t.u.dollarinfo.sgnd;
	t->u.amodeinfo.size  = copy_of_t.u.dollarinfo.size;
      }
    break;

  case TOK_DOLLAR_NUMBER:
    t->type = TOK_NUMBER;

    /* Sign extend if we have to.  Assumes 2's complement machine. */
    if (t->u.dollarinfo.sgnd && (constant & (1L << (field_length - 1))))
      constant |= ~((1L << field_length) - 1);

    /* Save the new, sign extended number. */
    t->u.n = constant;
    break;
  default:
    fatal_error ("Internal error: generate_code.c: IS_DOLLAR_TOKEN() "
		 "must be bad.\n");
    break;
  }
}


static void
output_c_for_amode_ptr (const Token *t, BOOL reversed)
{
  int reg = t->u.amodeinfo.which & 7, mode = (t->u.amodeinfo.which >> 3) & 7;
  char buf[512];
  int size = t->u.amodeinfo.size;

  switch (mode) {
  case 0:
  case 1:
    fatal_error ("Internal error: called output_c_for_amode_ptr with "
		 "a register addressing mode!");
    break;

  case 2:
    fprintf (syn68k_c_stream, "(ADDRESS_REGISTER_UL (%d)) ", reg);
    break;

  case 3:
    fprintf (syn68k_c_stream, "(ADDRESS_REGISTER_UL (%d)) ", reg);

    /* Perform postincrement, but force sp to remain even! */
    if (size == 1 && reg != 7)
      sprintf (buf, "++ADDRESS_REGISTER_UL (%d); ", reg);
    else
      sprintf (buf, "INC_VAR (ADDRESS_REGISTER_UL (%d), %d); ", reg,
	       (reg != 7 || size != 1) ? size : 2);
    if (strstr (c_postamble, buf) == NULL)
      strcat (c_postamble, buf);
    break;
  case 4:
    fprintf (syn68k_c_stream, "(ADDRESS_REGISTER_UL (%d) - %d) ", reg,
	     (reg != 7 || size != 1) ? size : 2);

    /* Perform predecrement, but force sp to remain even! */
    if (size == 1 && reg != 7)
      sprintf (buf, "--ADDRESS_REGISTER_UL (%d); ", reg);
    else
      sprintf (buf, "DEC_VAR (ADDRESS_REGISTER_UL (%d), %d); ", reg,
	       (reg != 7 || size != 1) ? size : 2);
    if (strstr (c_postamble, buf) == NULL)
      strcat (c_postamble, buf);
    break;
  case 5:
    fatal_error ("Internal error: attempting to generate code for fixed "
		 "amode 5.  It should have been expanded and replaced with "
		 "explicit code to compute the pointer.\n");
    break;

    /* Modes 6 and 7 are too complicated to expand; a pointer to the data
     * will be placed in the appropriate place by the synthetic instruction
     * right before this one.
     */
  case 6:
  case 7:
    if (!reversed)
      fprintf (syn68k_c_stream, "US_TO_SYN68K (cpu_state.amode_p) ");
    else
      fprintf (syn68k_c_stream, "US_TO_SYN68K (cpu_state.reversed_amode_p) ");
    break;
  }
}


/* Helper function for generate_temp_decls, below. */
static void
gtd_aux (List *code, BOOL *decld, BOOL *any_so_far)
{
  Token *t;

  if (code == NULL)
    return;

  t = &code->token;
  if (t->type == TOK_TEMP_REGISTER)
    {
      if (!decld[t->u.reginfo.which])
	{
	  if (!*any_so_far)
	    {
	      fputs ("        M68kReg", syn68k_c_stream);
	      *any_so_far = TRUE;
	    }
	  else
	    putc (',', syn68k_c_stream);
	  fprintf (syn68k_c_stream, " tmp%d", t->u.reginfo.which);
	  decld[t->u.reginfo.which] = TRUE;
	}
    }

  gtd_aux (code->car, decld, any_so_far);
  gtd_aux (code->cdr, decld, any_so_far);
}


/* This generates local declarations for all of the temp variables we
 * actually use.  We declare a new set for each case statement to help
 * the compiler identify dead variables.
 */
static void
generate_temp_decls (List *code)
{
#if	!defined(TEMPS_AT_TOP)
  BOOL decld[128];
  BOOL found = FALSE;
  memset (decld, 0, sizeof decld);
  gtd_aux (code, decld, &found);
  if (found)
    fputs (";\n", syn68k_c_stream);
#endif
}


/* Helper function for transform_rtvad_aux, below.  This replaces all
 * instances of a given temp register with a TOK_QUOTED_STRING containing
 * the name of the C variable that is replacing it.
 */
static void
transform_reg_to_var (List *code, Token *to_replace, const char *var_name)
{
  /* Recursion base case. */
  if (code == NULL)
    return;

  /* If this is the appropriate temp register, replace it. */
  if (tokens_equal (&code->token, to_replace))
    {
      code->token.type = TOK_QUOTED_STRING;
      code->token.u.string = var_name;
    }

  /* Recurse. */
  transform_reg_to_var (code->car, to_replace, var_name);
  transform_reg_to_var (code->cdr, to_replace, var_name);
}


/* Helper function for transform_rtvad_aux, below.  This function takes
 * an example usage of a given temp register, and determines whether every
 * other usage of that temp register is of the same form.  For example,
 * for (assign foo (+ tmp2.ub tmp2.ub)) this would return TRUE,
 * but for (assign foo (+ tmp2.ub tmp2.sl)) this would return FALSE.
 */
static BOOL
temp_use_consistent (List *code, Token *one_use)
{
  Token *t;

  /* Recursion base case. */
  if (code == NULL)
    return TRUE;

  /* If this is a usage of the specified temp reg, see if it is used in
   * the same way as ONE_USE.
   */
  t = &code->token;
  if (t->type == TOK_TEMP_REGISTER
      && t->u.reginfo.which == one_use->u.reginfo.which)
    {
      if (t->u.reginfo.sgnd != one_use->u.reginfo.sgnd
	  || t->u.reginfo.size != one_use->u.reginfo.size)
	return FALSE;
    }

  /* Recurse. */
  return (temp_use_consistent (code->car, one_use)
	  && temp_use_consistent (code->cdr, one_use));
}


/* Helper function for transform_reg_to_var_and_decl, below.  This
 * recurses over all of the code and determines when it can replace
 * a given temp register with a variable of a simpler C type.
 */
static void
transform_rtvad_aux (List *root, List *code, char decls[2][5][1024])
{
  Token *t;

  /* Recursion base case. */
  if (code == NULL)
    return;

  t = &code->token;
  if (t->type == TOK_TEMP_REGISTER && temp_use_consistent (root, t))
    {
      char buf[128], *s = decls[t->u.reginfo.sgnd][t->u.reginfo.size];
      const char *name;
      Token copy_of_t = *t;

      /* Choose a descriptive + unique name for this new temp variable. */
      sprintf (buf, "tmp%c%c%d", t->u.reginfo.sgnd ? 's' : 'u',
	       "?bw?l"[t->u.reginfo.size], t->u.reginfo.which);
      name = unique_string (buf);

      /* Add this variable's name to the list of variables of this type
       * to declare.
       */
      if (s[0] != '\0')  /* Add a comma if there are previous decls. */
	strcat (s, ",");
      sprintf (s + strlen (s), " %s", name);

      /* Replace all occurrences of this temp register with the temp
       * variable we just made.
       */
      transform_reg_to_var (root, &copy_of_t, name);
    }

  /* Recurse on the rest of the code. */
  transform_rtvad_aux (root, code->car, decls);
  transform_rtvad_aux (root, code->cdr, decls);
}



/* This function simplifies TEMP_REGISTERs that are only used in one way
 * by transforming them from the complex union they normally comprise to
 * a simple C type.  For example, if "tmp2" is only used as "tmp2.ub" in the
 * entire scope of its lifetime, we can replace it with a variable of
 * type "uint8".  This should help the compiler and make syn68k.c somewhat
 * easier to read.
 */
static void
transform_reg_to_var_and_decl (List *code)
{
#if	!defined(TEMPS_AT_TOP)
  char decls[2][5][1024];  /* decls[signed][byte size][variable names] */
  int sgnd, i;

  /* Clear out all decls. */
  for (i = 0; i < 5; i++)
    decls[0][i][0] = decls[1][i][0] = '\0';

  /* Recursively make all the transformations and declarations. */
  transform_rtvad_aux (code, code, decls);

  /* Loop through and output decls for each variable type that we use. */
  for (sgnd = 0; sgnd <= 1; sgnd++)
    for (i = 1; i < 5; i++)
      {
	if (decls[sgnd][i][0] != '\0')
	  fprintf (syn68k_c_stream, "        %s%s;\n", ctypes[sgnd][i],
		   decls[sgnd][i]);
      }
#endif
}
