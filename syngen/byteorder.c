#include "common.h"
#include "list.h"
#include "byteorder.h"
#include "uniquestring.h"
#include "error.h"
#include "bitstring.h"
#include "syn68k_private.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef enum {
  BO_UNKNOWN, BO_DONT_CARE, BO_NATIVE, BO_BIG_ENDIAN, BO_UNKNOWABLE
  } ByteOrder;

typedef enum {
  BS_UNKNOWN, BS_BYTE, BS_WORD, BS_LONG=4, BS_LONG_LONG=8
} ByteSize;

typedef struct {
  ByteOrder order;
  ByteSize size;
  BOOL sgnd;
} ExprInfo;

static ExprInfo expr_info (const List *expr, ByteOrder hint,
			   ExprInfo *field_info);
static ByteSize expr_list_max_size (List *ls, ByteSize hint,
				    ExprInfo *field_info);
static BOOL expr_list_any_are_unsigned (List *ls, ByteSize hint,
					ExprInfo *field_info);
static void endianness (List *code, ByteSize desired_size, ByteOrder hint,
			BOOL strong_hint, ExprInfo *field_info);
static Token *has_tok_dollar (List *ls, TokenType type, int number);
static BOOL has_tok_dollar_reg (List *ls, int number);


/*
 * NOTE: the code for sorting operands has been "#if 0"'d out for a long time.
 *       I only changed the #if 0 to if defined (SORT_OPERANDS) to make it so
 *       we don't get complaints about compare_bitfields being defined but not
 *       used.  It's not clear that SORT_OPERANDS should ever be defined.
 *       Certainly it shouldn't be turned on by anyone who isn't up to speed
 *       on the workings of Syn68k, and at this date (2003-12-15) nobody fits
 *       that description.
 */

#if defined (SORT_OPERANDS)
static int compare_bitfields (const void *bp1, const void *bp2);
#endif

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

/* This function computes information about all unexpanded operands, placing
 * information needed to extract them into op.  It also determines appropriate
 * byte ordering information and modifies v's code to swap bytes when
 * appropriate.  Returns the number of words of operands.
 */
int
compute_operand_info (OperandInfo *op, const ParsedOpcodeInfo *p,
		      const CCVariant *v)
{
  PatternRange range;
  ExprInfo field_info[16];
  char decls[2048];
  int words_in = p->operand_words_to_skip;
  int i;

  /* Zero out everything by default. */
  memset (op, 0, sizeof *op);

  /* Start out with the endianness/size of all operands unknown. */
  memset (field_info, 0, sizeof field_info);

  /* Figure out whatever we can about various fields. */
  for (i = 1; pattern_range (p->opcode_bits, i, &range); i++)
    {
      /* Was this field expanded? */
      if (field_expanded (i, p->opcode_bits, v->bits_to_expand))
	{
	  /* Yep; it's going to expand to a constant number,
	   * so specify the byte order as NATIVE.
	   */
	  field_info[i].order = BO_NATIVE;
	  break;
	}
    }

  /* Loop through unexpanded fields and nail down their size + signedness. */
  for (i = 1; pattern_range (p->opcode_bits, i, &range); i++)
    {
      Token *dollartok;
  
      /* If this field was expanded, no need to save it as an operand. */
      if (field_expanded (i, p->opcode_bits, v->bits_to_expand))
	continue;

      /* See if this field was actually used as a numeric constant. */
      if ((dollartok = has_tok_dollar (v->code, TOK_DOLLAR_NUMBER, i)) != NULL)
	{
	  field_info[i].sgnd = dollartok->u.dollarinfo.sgnd;
	  field_info[i].size = dollartok->u.dollarinfo.size;
	}
      else if (has_tok_dollar_reg (v->code, i))
	{
	  /* Make register numbers be 32 bit uints, since this seems to
	   * make most compilers the happiest when doing array references.
	   */
	  field_info[i].sgnd  = FALSE;
	  field_info[i].size  = BS_LONG;
	  field_info[i].order = BO_NATIVE;
	}
    }

  /* Recursively set/determine the endianness of all expressions and
   * add in swap's where appropriate.
   */
  endianness (v->code, BO_UNKNOWN, BO_NATIVE, FALSE, field_info);

  /* For all of the fields that weren't expanded, create bitfield
   * information for them and create a declaration.
   */
  for (i = 1; pattern_range (p->opcode_bits, i, &range); i++)
    {
      BitfieldInfo *b;

      /* If this field was expanded, no need to save it as an operand. */
      if (field_expanded (i, p->opcode_bits, v->bits_to_expand))
	continue;

      /* See if this field was actually used as a numeric constant,
       * or as a register number...
       */
      if (has_tok_dollar (v->code, TOK_DOLLAR_NUMBER, i) == NULL
	  && !has_tok_dollar_reg (v->code, i))
	continue;

      /* If we just don't care about byte order for a numeric constant,
       * make it default to BO_NATIVE.
       */
      if (field_info[i].order == BO_UNKNOWN
	  && has_tok_dollar (v->code, TOK_DOLLAR_NUMBER, i) != NULL)
	field_info[i].order = BO_NATIVE;

      /* It wasn't expanded, so create a bitfield for it. */
      if (field_info[i].order == BO_UNKNOWN
	  || field_info[i].size == BS_UNKNOWN)
	{
	  parse_error (v->code, "Error: Unable to determine best byte order "
		       "or size of operand field %d.  "
		       "(order == %d, size == %d)\n", i, field_info[i].order,
		       field_info[i].size);
	  print_list (stderr, v->code);
	  putc ('\n', stderr);
	}
      else
	{
	  b = &op->bitfield[op->num_bitfields++];
	  b->rev_amode = (has_tok_dollar (v->code, TOK_DOLLAR_REVERSED_AMODE,i)
			  != NULL);
	  b->index = range.index;
	  b->length = range.length - 1;
	  b->sign_extend = field_info[i].sgnd;
	  b->make_native_endian = (field_info[i].order == BO_NATIVE
				   || field_info[i].size == BS_BYTE);

#if 0
	  /* We prefer to translate 16 bit operands to the synthetic
	   * instruction stream as 16 bit operands.  However, if the operand
	   * is to be sign- or zero-extended to 32 bits, and it is not to be
	   * native endian, we extend it at translation time.  The logic
	   * behind this is that extending a 16 bit number is a free
	   * operation on the 386 (movesx/movezx) and a cheap operation on
	   * other CPU's, but this will only work for operands in native
	   * format.  Also, since 32 bit operands force operand alignment for
	   * QUADALIGN machines, using 16 bit operands can avoid alignment
	   * NOPs.
	   */
	  if (b->make_native_endian && range.length <= 16)
	    b->words = 1 - 1;  /* 1 word. */
	  else
	    b->words = ((field_info[i].size == BS_LONG) ? 2 : 1) - 1;
#else
	  /* We now prefer to use BS_LONG operands.  These are generally
	   * faster on RISC chips, and on the Pentium movswl, etc. are
	   * apparently not pairable.
	   */
	  if (b->make_native_endian)
	    field_info[i].size = BS_LONG;
	  b->words = ((field_info[i].size == BS_LONG) ? 2 : 1) - 1;
#endif

	  if (b->index == MAGIC_END_INDEX && b->length == MAGIC_END_LENGTH)
	    fatal_error ("Illegal bitfield index/length (%d/%d).  This "
			 "combination is used as a magic value and shouldn't "
			 "show up in real code!", MAGIC_END_INDEX,
			 MAGIC_END_LENGTH);
	}
    }

  if (op->num_bitfields > MAX_BITFIELDS)
    fatal_error ("Too many operand bitfields (%d) for OpcodeMappingInfo "
		 "struct.\n", op->num_bitfields);

#if defined (SORT_OPERANDS)
  /* Sort operands by size and then by field number. */
  {
#ifdef GENERATE_NATIVE_CODE
    BitfieldInfo *save;
    int size = op->num_bitfields * sizeof op->bitfield[0];
    save = (BitfieldInfo *) alloca (size);
    memcpy (save, op->bitfield, size);
#endif
    qsort (op->bitfield, op->num_bitfields, sizeof op->bitfield[0],
	   compare_bitfields);
#ifdef GENERATE_NATIVE_CODE
    if (v->native_code_info != NULL && memcmp (save, op->bitfield, size))
      parse_error (v->code, "byteorder.c insists on reordering your operands!"
		   "  This is incompatible with native code generation.");
#endif
  }
#endif /* defined (SORT_OPERANDS) */

  /* Create decls string. */
  decls[0] = '\0';
  for (i = 0; i < op->num_bitfields; i++)
    {
      int field = field_with_index (p->opcode_bits, op->bitfield[i].index);

      /* Output declaration. */
#if	1 || !defined(TEMPS_AT_TOP)
      sprintf (decls + strlen (decls), "        %sint%d operand_%d = ",
	       op->bitfield[i].sign_extend ? "" : "u",
	       field_info[field].size * 8, field);
#else	/* defined(TEMPS_AT_TOP) */
      sprintf (decls + strlen (decls), "        operand_%s%d_%d = ",
	       op->bitfield[i].sign_extend ? "" : "u",
	       field_info[field].size * 8, field);
#endif	/* defined(TEMPS_AT_TOP) */

      /* Fetch initial value. */
      if (op->bitfield[i].words == 1 - 1  /* One 16 bit word. */ )
	{
	  if (op->bitfield[i].sign_extend)
	    strcat (decls, "(int16) ");
	  sprintf (decls + strlen (decls), "code[%d];", words_in);
	  /* We leave a gap here.  We used to pack all the 16 bit
	   * operands together, but they have largely been abandoned
	   * in favor of 32 bit operands.  They only crop up occasionally
	   * when we're doing byte swapping of immediate constants.
	   * We leave a gap here so everything stays aligned.
	   */
	  words_in += 2;
	}
      else  /* Two 16 bit words. */
	{
	  strcat (decls, "*((");
	  if (!op->bitfield[i].sign_extend)
	    strcat (decls, "u");
	  if (words_in != 0)
	    sprintf (decls + strlen (decls), "int32 *) (code + %d));",
		     words_in);
	  else
	    sprintf (decls + strlen (decls), "int32 *) code);");
	  words_in += 2;
	}

      /* Add comment indicating swappedness. */
      if (op->bitfield[i].make_native_endian)
	strcat (decls, "   /* Native endian */\n");
      else
	strcat (decls, "   /* Big endian */\n");
    }

  op->operand_decls = unique_string (decls);
  return words_in;
}


static void
swap (List *ls, ByteSize size, BOOL sgnd)
{
  List *swap, *new;

  /* Pointless to swap bytes, assuming they aren't trying to use
   * swapsb or swapub as a cast!
   */
  if (size == BS_BYTE)
    return;

  /* Verify that we have a legal size. */
  if (size == BS_UNKNOWN)
    {
      parse_error (ls, "Internal error, byteorder.c: Attempting to swap with "
		   "size == BS_UNKNOWN!\n");
      return;
    }

  swap = alloc_list ();
  new = alloc_list ();

  /* Copy the specified list. */
  *new = *ls;
  new->cdr = NULL;

  /* Create swap expr. */
  swap->token.type = TOK_SWAP;

  assert (size == 1 || size == 2 || size == 4);

  swap->token.u.swapinfo.size = size;
  swap->token.u.swapinfo.sgnd = sgnd;
  swap->cdr = new;

  /* Make the specified list be a real list that contains (swap expr). */
  ls->token.type = TOK_LIST;
  ls->car = swap;

  propagate_fileinfo (&ls->token, ls);
}


/* This magic function marches through a list of code and makes everything
 * a workable endianness by either inserting swaps or requesting that
 * operands be translated with a certain endianness.  If strong_hint
 * is TRUE, the expression is forced to assume the endianness specified
 * by hint.
 */
static void
endianness (List *code, ByteSize desired_size, ByteOrder hint,
	    BOOL strong_hint, ExprInfo *field_info)
{
  ExprInfo info = expr_info (code, hint, field_info);
  Token *t = &code->token;
  char buf[256];

  if (code == NULL)
    return;

#if 0
  printf ("Endianness: desired_size = %d, hint = %d, strong_hint = %d\n",
	  desired_size, hint, strong_hint);
  print_list (stdout, code);
  putchar ('\n');
#endif

  if (strong_hint && t->type == TOK_DOLLAR_NUMBER)
    {
      field_info[t->u.dollarinfo.which].order = hint;
      return;
    }
    
  if (t->type == TOK_LIST)
    {
      List *l2;
      
      t = &code->car->token;
      switch (t->type) {
      case TOK_SHIFT_RIGHT:
      case TOK_SHIFT_LEFT:
      case TOK_PLUS:
      case TOK_MINUS:
      case TOK_MULTIPLY:
      case TOK_DIVIDE:
      case TOK_MOD:
      case TOK_BITWISE_AND:
      case TOK_BITWISE_OR:
      case TOK_BITWISE_XOR:
      case TOK_BITWISE_NOT:
	/* Force all operands to be the same endianness... */
	for (l2 = CDAR (code); l2 != NULL; l2 = l2->cdr)
	  {
	    endianness (l2, ((info.size == BS_UNKNOWN)
			     ? desired_size : info.size),
			info.order, TRUE, field_info);
	  }
	break;

      case TOK_EXPLICIT_LIST:
	for (l2 = CDAR (code); l2 != NULL; l2 = l2->cdr)
	  {
	    /* Just process everything else. */
	    endianness (l2, desired_size, hint, strong_hint, field_info);
	  }
	break;

      case TOK_LIST:
	endianness (code->car, desired_size, hint, strong_hint, field_info);
	break;

      case TOK_ASSIGN:
	if (IS_CCBIT_TOKEN (CDAR (code)->token.type))
	  {
#ifdef CCR_ELEMENT_8_BITS
	    endianness (CDDAR (code), BS_UNKNOWN, BO_NATIVE, TRUE,
			field_info);
#else   /* !CCR_ELEMENT_8_BITS */
	    endianness (CDDAR (code), desired_size, hint, strong_hint,
			field_info);
#endif  /* !CCR_ELEMENT_8_BITS */
	  }
	else
	  {
	    if (info.size == BS_UNKNOWN)
	      {
		parse_error (CDAR (code), "Attempting to assign to object "
			     "of unknown size!\n");
		print_list (stderr, code);
		putc ('\n', stderr);
	      }

	    /* Process LHS. */
	    endianness (CDAR (code), info.size, hint, FALSE, field_info);
	    
	    /* Force RHS to have same type as LHS. */
	    endianness (CDDAR (code), info.size, info.order, TRUE, field_info);
	  }
	break;

      case TOK_SWAP:
	/* Process operand. */
	endianness (CDAR (code), t->u.swapinfo.size,
		    (hint == BO_NATIVE) ? BO_BIG_ENDIAN : BO_NATIVE,
		    strong_hint, field_info);
	break;

      case TOK_DEREF:
	/* Force address to be native. */
	if (t->u.derefinfo.size != 0)
	  endianness (CDAR (code), BS_LONG, BO_NATIVE, TRUE, field_info);
	else
	  endianness (CDDAR (code), BS_LONG, BO_NATIVE, TRUE, field_info);
	break;

      case TOK_EQUAL:
      case TOK_NOT_EQUAL:
	{
	  /* Force both operands to be the same endianness (don't
	   * care which since we are testing equality).
	   */
	  ExprInfo info2 = expr_info (CDAR (code), BO_NATIVE, field_info);
	  endianness (CDAR (code), expr_list_max_size (CDAR (code),
						       BO_NATIVE,
						       field_info),
		      info2.order, TRUE, field_info);
	}
	break;

      case TOK_GREATER_THAN:
      case TOK_GREATER_OR_EQUAL:
      case TOK_LESS_THAN:
      case TOK_LESS_OR_EQUAL:
	/* Force all operands to be BO_NATIVE. */
	for (l2 = CDAR (code); l2 != NULL; l2 = l2->cdr)
	  {
	    endianness (l2, expr_list_max_size (CDAR (code), BO_NATIVE,
						field_info),
			BO_NATIVE, TRUE, field_info);
	  }
	break;

      case TOK_AND:
      case TOK_OR:
      case TOK_XOR:
      case TOK_NOT:
	/* Process all of the boolean things.  Don't care about endianness. */
	for (l2 = CDAR (code); l2 != NULL; l2 = l2->cdr)
	  {
	    endianness (l2, BO_UNKNOWN, BO_NATIVE, FALSE, field_info);
	  }
	break;

      case TOK_IF:
	for (l2 = CDAR (code); l2 != NULL; l2 = l2->cdr)
	  {
	    /* Just process everything. */
	    endianness (l2, BS_UNKNOWN, BO_NATIVE, FALSE, field_info);
	  }
	break;

      case TOK_SWITCH:
	/* Force switch value to be native. */
	endianness (CDAR (code), BS_WORD, BO_NATIVE, TRUE, field_info);
	for (l2 = CDDAR (code); l2 != NULL; l2 = l2->cdr)
	  {
	    /* Force case value to be native. */
	    endianness (l2->car, BS_WORD, BO_NATIVE, TRUE, field_info);
	    
	    /* Process case code. */
	    endianness (CDAR (l2), BS_UNKNOWN, BO_NATIVE, FALSE, field_info);
	  }
	break;

      case TOK_FUNC_CALL:
	for (l2 = CDDAR (code); l2 != NULL; l2 = l2->cdr)
	  {
	    /* Force everything to be native */
	    endianness (l2, BS_UNKNOWN, BO_NATIVE, TRUE, field_info);
	  }
	break;

      case TOK_CAST:
	for (l2 = CDDAR (code); l2 != NULL; l2 = l2->cdr)
	  {
	    /* Just process everything. */
	    endianness (l2, BS_UNKNOWN, BO_NATIVE, TRUE, field_info);
	  }
	break;

      default:
	parse_error (code->car, "Unhandled operator %s in endianness().\n",
		     unparse_token (t, buf));
	break;
      }
    }

  /* If we are backwards and this upsets someone, swap! */
  info = expr_info (code, hint, field_info);
  if (strong_hint && info.order != hint)
    {
      ByteSize s;

      if (desired_size == BS_UNKNOWN)
	s = info.size;
      else if (info.size == BS_UNKNOWN)
	s = desired_size;
      else
	s = MIN (desired_size, info.size);

      swap (code, s, info.sgnd);
    }
}



static ExprInfo
expr_info (const List *expr, ByteOrder hint, ExprInfo *field_info)
{
  ExprInfo info = { BO_UNKNOWN, BS_UNKNOWN, FALSE };   /* Defaults. */
  const Token *t;
  char buf[256];

  if (expr == NULL)
    return info;

  t = &expr->token;
  switch (t->type) {
  case TOK_DATA_REGISTER:
  case TOK_ADDRESS_REGISTER:
  case TOK_TEMP_REGISTER:
    info.order = BO_NATIVE;
    info.size  = t->u.reginfo.size;
    info.sgnd  = t->u.reginfo.sgnd;
    break;

  case TOK_DOLLAR_DATA_REGISTER:
  case TOK_DOLLAR_ADDRESS_REGISTER:
  case TOK_DOLLAR_GENERAL_REGISTER:
    info.order = BO_NATIVE;
    info.size  = t->u.dollarinfo.size;
    info.sgnd  = t->u.dollarinfo.sgnd;
    break;

  case TOK_DOLLAR_AMODE:              /* We assume that by the time we reach */
  case TOK_DOLLAR_REVERSED_AMODE:     /*  here, all reg amodes have been     */
    info.order = BO_BIG_ENDIAN;       /*  converted to dollar_reg's.         */
    info.size  = t->u.dollarinfo.size;
    info.sgnd  = t->u.dollarinfo.sgnd;
    break;

  case TOK_DOLLAR_AMODE_PTR:
  case TOK_DOLLAR_REVERSED_AMODE_PTR:
    info.order = BO_NATIVE;
    info.size  = sizeof (void *);
    info.sgnd  = TRUE;
    break;

  case TOK_AMODE:                     /* We assume that by the time we reach */
  case TOK_REVERSED_AMODE:            /*  here, all reg amodes have been     */
    info.order = BO_BIG_ENDIAN;       /*  converted to dollar_reg's.         */
    info.size  = t->u.amodeinfo.size;
    info.sgnd  = t->u.amodeinfo.sgnd;
    break;

  case TOK_CODE:
    info.order = BO_NATIVE;
    info.size  = sizeof (void *);
    info.sgnd  = FALSE;
    break;

  case TOK_NUMBER:
    info.order = BO_NATIVE;
    info.size  = BO_UNKNOWN;
    info.sgnd  = TRUE;
    break;

  case TOK_DEFAULT:
    info.order = BO_NATIVE;   /* So it's a legal case number. */
    info.size  = BS_LONG;
    info.sgnd  = FALSE;
    break;

  case TOK_CCC:
  case TOK_CCN:
  case TOK_CCV:
  case TOK_CCX:
  case TOK_CCNZ:
    info.order = BO_NATIVE;
#ifdef CCR_ELEMENT_8_BITS
    info.size  = BS_BYTE;
#else
    info.size  = BS_UNKNOWN;
#endif
    info.sgnd  = FALSE;
    break;

  case TOK_DOLLAR_NUMBER:
    info.order = field_info[t->u.dollarinfo.which].order;
    info.size  = t->u.dollarinfo.size;
    info.sgnd  = t->u.dollarinfo.sgnd;
    if (field_info[t->u.dollarinfo.which].size != BS_UNKNOWN
	&& field_info[t->u.dollarinfo.which].size != info.size)
      parse_error (expr, "Operand field %s defined with more than one "
		   "different size! (%d/%d)\n",
		   unparse_token (t, buf),
		   field_info[t->u.dollarinfo.which].size,
		   t->u.dollarinfo.size);
    break;

  case TOK_FALSE:
  case TOK_TRUE:
    info.order = BO_NATIVE;
    info.size  = BS_UNKNOWN;
    info.sgnd  = NO;
    break;

  case TOK_QUOTED_STRING:
    info.order = BO_NATIVE;
    info.size  = BS_LONG;  /* Who knows what is appropriate here. */
    info.sgnd  = TRUE;     /* Sure, why not. */
    break;

  case TOK_LIST:
    expr = expr->car;
    t = &expr->token;
    switch (t->type) {
    case TOK_PLUS:
    case TOK_MINUS:
    case TOK_MULTIPLY:
    case TOK_DIVIDE:
    case TOK_MOD:
      info.order = BO_NATIVE;
      info.size  = expr_list_max_size (expr->cdr, hint, field_info);
      info.sgnd  = !expr_list_any_are_unsigned (expr->cdr, hint, field_info);
      break;

    case TOK_SHIFT_LEFT:
    case TOK_SHIFT_RIGHT:
      info = expr_info (expr->cdr, hint, field_info);  /* Get defaults. */
      info.order = BO_NATIVE;
      break;

    case TOK_BITWISE_AND:
    case TOK_BITWISE_OR:
    case TOK_BITWISE_XOR:
    case TOK_BITWISE_NOT:
      {
	List *l2;
	int big = 0, native = 0;
	info.sgnd = TRUE;
	for (l2 = expr->cdr; l2 != NULL; l2 = l2->cdr)
	  {
	    if (l2->token.type != TOK_NUMBER)
	      {
		ExprInfo info2 = expr_info (l2, hint, field_info);
		if (info2.order == BO_BIG_ENDIAN)
		  big++;
		else if (info2.order == BO_NATIVE)
		  native++;
		if (!info2.sgnd)
		  info.sgnd = FALSE;
	      }
	  }
	if (big == native)  /* Tie?  Then stick with the hint. */
	  info.order = hint;
	else info.order = (big > native) ? BO_BIG_ENDIAN : BO_NATIVE;
	info.size = expr_list_max_size (expr->cdr, hint, field_info);
      }
      break;

    case TOK_SWAP:
      info = expr_info (expr->cdr, hint, field_info);
      if (info.order == BO_BIG_ENDIAN)
	info.order = BO_NATIVE;
      else if (info.order == BO_NATIVE)
	info.order = BO_BIG_ENDIAN;
      info.size = t->u.swapinfo.size;
      info.sgnd = t->u.swapinfo.sgnd;
      break;

    case TOK_LIST:
      /* Stick with "unknown" default. */
      break;

    case TOK_ASSIGN:
#ifndef CCR_ELEMENT_8_BITS
      if (IS_CCBIT_TOKEN (expr->cdr->token.type))
	{
	  info = expr_info (CDDR (expr), hint, field_info);   /* rhs */
	}
      else
#endif
	{
	  info = expr_info (expr->cdr, hint, field_info);     /* lhs */
	}
      break;

    case TOK_CAST:
      info.order = BO_NATIVE;
      info.size  = BS_UNKNOWN;
      info.sgnd = FALSE;  /* Who knows? */
      break;

    case TOK_DEREF:
      if (t->u.derefinfo.size == 0)
	{
	  info.order = BO_NATIVE;
	  info.size  = BS_UNKNOWN;
	  info.sgnd  = FALSE;  /* Who knows? */
	}
      else
	{
	  info.order = BO_BIG_ENDIAN;
	  info.size  = t->u.derefinfo.size;
	  info.sgnd  = t->u.derefinfo.sgnd;
	}
      break;

    case TOK_EQUAL:
    case TOK_NOT_EQUAL:
    case TOK_GREATER_THAN:
    case TOK_LESS_THAN:
    case TOK_GREATER_OR_EQUAL:
    case TOK_LESS_OR_EQUAL:
    case TOK_AND:
    case TOK_OR:
    case TOK_XOR:
    case TOK_NOT:
      info.order = BO_NATIVE;
      info.size  = BS_BYTE;
      info.sgnd  = FALSE;
      break;

    case TOK_SWITCH:
    case TOK_IF:
      break;

    case TOK_DEFAULT:
      info.order = BO_NATIVE;   /* So it's a legal case number. */
      info.size  = BS_LONG;
      info.sgnd  = FALSE;
      break;

    case TOK_EXPLICIT_LIST:   /* Unknown everything. */
      break;

    case TOK_FUNC_CALL:
      info.order = BO_NATIVE;
      info.size  = BS_LONG;
      info.sgnd  = TRUE;  /* Who knows? */
      break;
      
    default:
      parse_error (expr, "byteorder.c: Don't know how to process "
		   "endianness/byte size for operator %s.\n",
		   unparse_token (t, buf));
      break;
    }
    break;

  default:
    parse_error (expr, "byteorder.c: Don't know how to process "
		 "endianness/byte size for %s.\n", unparse_token (t, buf));
    break;
  }

  return info;
}


static ByteSize
expr_list_max_size (List *ls, ByteSize hint, ExprInfo *field_info)
{
  ByteSize s = BO_UNKNOWN;

  for (; ls != NULL; ls = ls->cdr)
    {
      ExprInfo info = expr_info (ls, hint, field_info);
      if (info.size > s)
	s = info.size;
    }

  return s;
}


static BOOL
expr_list_any_are_unsigned (List *ls, ByteSize hint, ExprInfo *field_info)
{
  for (; ls != NULL; ls = ls->cdr)
    {
      ExprInfo info = expr_info (ls, hint, field_info);
      if (!info.sgnd)
	return TRUE;
    }
  return FALSE;
}


static Token *
has_tok_dollar (List *ls, TokenType type, int number)
{
  Token *t;

  if (ls == NULL)
    return NULL;
  if (ls->token.type == type
      && ls->token.u.dollarinfo.which == number)
    return &ls->token;
  t = has_tok_dollar (ls->car, type, number);
  if (t != NULL)
    return t;
  return has_tok_dollar (ls->cdr, type, number);
}


static BOOL
has_tok_dollar_reg (List *ls, int number)
{
  if (ls == NULL)
    return FALSE;
  if ((ls->token.type == TOK_DOLLAR_DATA_REGISTER
       || ls->token.type == TOK_DOLLAR_ADDRESS_REGISTER
       || ls->token.type == TOK_DOLLAR_GENERAL_REGISTER)
      && ls->token.u.dollarinfo.which == number)
    return TRUE;
  return (has_tok_dollar_reg (ls->car, number)
	  || has_tok_dollar_reg (ls->cdr, number));
}

#if defined (SORT_OPERANDS)

/* qsort helper function.  Places longs before words, breaks ties by
 * whichever bitfield has the lowest bit index into the 68k instruction
 * stream (ie the lowest field number).
 */
static int
compare_bitfields (const void *bp1, const void *bp2)
{
  const BitfieldInfo *b1 = (const BitfieldInfo *)bp1;
  const BitfieldInfo *b2 = (const BitfieldInfo *)bp2;

  if (b2->words - b1->words != 0)
    return ((int) b2->words) - ((int) b1->words);
  return ((int) b1->index) - ((int) b2->index);
}
#endif /* defined (SORT_OPERANDS) */
