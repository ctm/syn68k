#include "xlate.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>


/* This file builds some of the lookup tables used at runtime for
 * native code generation.  It takes the contents of xlatetable.c
 * as input.
 */

static guest_code_descriptor_t *alloc_gcd (void);
static char *create_name (const char *n, int size, int src_amode,
			  int dst_amode, int which);
static guest_code_descriptor_t *process_move (const xlate_descriptor_t *x);
static guest_code_descriptor_t *process_binary (const xlate_descriptor_t *x);
static guest_code_descriptor_t *process_unary (const xlate_descriptor_t *x);
static void default_desc (const xlate_descriptor_t *x, size_mask_t size,
			  guest_code_descriptor_t *g);
static int compare_regops (const void *p1, const void *p2);


guest_code_descriptor_t *
process_xlate_descriptor (const xlate_descriptor_t *x)
{
  guest_code_descriptor_t *g, *gd;

  switch (x->type)
    {
    case OP_UNARY:
      g = process_unary (x);
      break;
    case OP_BINARY:
      g = process_binary (x);
      break;
    case OP_MOVE:
      g = process_move (x);
      break;
    default:
      g = NULL;
      abort ();
    }

  /* Sort the regop arrays for correctness and efficiency. */
  for (gd = g; gd != NULL; gd = gd->next)
    {
      int i;

      /* Count the number of legitmate reg operands. */
      for (i = 0; gd->reg_op_info[i].legitimate_p; i++)
	;
      qsort (&gd->reg_op_info, i, sizeof gd->reg_op_info[0], compare_regops);
    }

  return g;
}


static int
bits (unsigned n)
{
  int b;
  for (b = 0; n != 0; n &= n - 1)
    b++;
  return b;
}


static int
compare_regops (const void *p1, const void *p2)
{
  const reg_operand_info_t *r1, *r2;

  r1 = (const reg_operand_info_t *)p1;
  r2 = (const reg_operand_info_t *)p2;

  /* We _must_ put all REQUEST_SPARE_REG operands last! */
  if (r1->request_type != r2->request_type)
    return r1->request_type - r2->request_type;

  /* Put all address registers first, to tend to avoid AGI delays
   * on the x86 (this is a free optimization, so why not?)
   */
  if (r1->add8_p != r2->add8_p)
    return r2->add8_p - r1->add8_p;

  /* Put the most restrictive allowable host regsets first. */
  if (bits (r1->regset) != bits (r2->regset))
    return bits (r1->regset) - bits (r2->regset);

  /* Put the most restrictive mappings first. */
  if (bits (r1->acceptable_mapping) != bits (r2->acceptable_mapping))
    return bits (r1->acceptable_mapping) - bits (r2->acceptable_mapping);

  /* Default to sorting by operand number, highest first.  Higher operands
   * tend to be source operands, so we might as well put them first to
   * avoid AGI delays.
   */
  return r2->operand_num - r1->operand_num;
}


/* Returns TRUE if the operation is a bitwise one, in which case we
 * don't need to byteswap quite so often (or we can byteswap the
 * constant).  We can't do this sort of tomfoolery when we need
 * certain cc bits, though.
 */
static int
bitwise_op_p (const char *i386_op)
{
  static const char *known_bitwise_ops[] = { "and", "or", "xor", "not" };
  int i;

  for (i = 0; i < NELEM (known_bitwise_ops); i++)
    if (!strcmp (i386_op, known_bitwise_ops[i]))
      return TRUE;
  return FALSE;
}


static int
compare_op_p (const char *i386_op)
{
  static const char *known_compare_ops[] = { "cmp", "test" };
  int i;

  for (i = 0; i < NELEM (known_compare_ops); i++)
    if (!strcmp (i386_op, known_compare_ops[i]))
      return TRUE;
  return FALSE;
}


static int
shift_op_p (const char *i386_op)
{
  static const char *known_shift_ops[] = { "sal", "sar", "shl", "shr" };
  int i;

  for (i = 0; i < NELEM (known_shift_ops); i++)
    if (!strcmp (i386_op, known_shift_ops[i]))
      return TRUE;
  return FALSE;
}


#ifdef __GNUC__
#define G(x) x
#else
#define G(x)
#endif


/* This table maps an addressing mode to how many registers it uses. */
static const int amode_reg_operands[] =
{
  G ([AMODE_NONE]=)         0,
  G ([AMODE_IMM]=)          0,
  G ([AMODE_REG]=)          1,
  G ([AMODE_AREG]=)         1,
  G ([AMODE_IND]=)          1,
  G ([AMODE_POSTINC]=)      1,
  G ([AMODE_PREDEC]=)       1,
  G ([AMODE_INDOFF]=)       1,
  G ([AMODE_ABS]=)          0,
  G ([AMODE_INDIX]=)	    1
};


static const int amode_operands[] =
{
  G ([AMODE_NONE]=)         0,
  G ([AMODE_IMM]=)          1,
  G ([AMODE_REG]=)          1,
  G ([AMODE_AREG]=)         1,
  G ([AMODE_IND]=)          1,
  G ([AMODE_POSTINC]=)      1,
  G ([AMODE_PREDEC]=)       1,
  G ([AMODE_INDOFF]=)       2,
  G ([AMODE_ABS]=)          1,
  G ([AMODE_INDIX]=)        1
};


static const char *amode_name[] =
{
  G ([AMODE_NONE]=)         "",
  G ([AMODE_IMM]=)          "imm",
  G ([AMODE_REG]=)          "reg",
  G ([AMODE_AREG]=)         "reg",
  G ([AMODE_IND]=)          "ind",
  G ([AMODE_POSTINC]=)      "postinc",
  G ([AMODE_PREDEC]=)       "predec",
  G ([AMODE_INDOFF]=)       "indoff",
  G ([AMODE_ABS]=)          "abs",
  G ([AMODE_INDIX]=)        "indix",
};


static const value_mapping_t swap_map_for_size[] =
{
  -1,
  G ([B]=) MAP_NATIVE,
  G ([W]=) MAP_SWAP16,
  -1,
  G ([L]=) MAP_SWAP32
};


static const char char_for_size[] =
{
  '?',
  G ([B]=) 'b',
  G ([W]=) 'w',
  '?',
  G ([L]=) 'l'
};

#undef G


static guest_code_descriptor_t *
process_unary (const xlate_descriptor_t *x)
{
  guest_code_descriptor_t *retval;
  BOOL bitwise_p, compare_p;
  size_mask_t size;
  char buf[1024];  /* For scratch stuff. */

  /* Note if this operation is a bitwise one.  If so, we have a little
   * more freedom doing things to memory when we don't need the N bit.
   * These ops are assumed to clear CV and set NZ appropriately.
   */
  bitwise_p = bitwise_op_p (x->i386_op);

  /* Is it a compare?  If so, we don't write any result value back. */
  compare_p = compare_op_p (x->i386_op);

  retval = NULL;

  for (size = B; size <= L; size <<= 1)
    if (x->sizes & size)
      {
	int dst_amode = x->value[0].amode;
	guest_code_descriptor_t def, *with_cc, *without_cc;

	if (dst_amode == AMODE_AREG && size == B)
	  abort ();

	/* Compute some reasonable defaults. */
	default_desc (x, size, &def);

	/* We have two cases: computing cc's, and not computing cc's. */
	with_cc    = alloc_gcd ();
	without_cc = alloc_gcd ();
	*with_cc = *without_cc = def; /* Set to default values. */

	/* Insert them into the chain. */
	with_cc->next = retval;
	without_cc->next = with_cc;
	retval = without_cc;

	/* Set up their names and special attributes. */
	with_cc->static_p = TRUE;
	without_cc->name = create_name (x->name, size,
					AMODE_NONE, dst_amode, 0);
	with_cc->name    = create_name (x->name, size,
					AMODE_NONE, dst_amode, 1);

	if (bitwise_p || compare_p)
	  {
	    /* We still get CVZ for free when we do a bitwise op, regardless
	     * of endianness.  We also get CVZ for free when doing tst
	     * instructions, since they always clear C and V (this wouldn't
	     * work for general compares).
	     */
	    if (size == B)
	      without_cc->cc_out = M68K_CC_CNVZ;
	    else
	      without_cc->cc_out = (M68K_CCC | M68K_CCV | M68K_CCZ);

	    if (REGISTER_AMODE_P (dst_amode))
	      {
		/* For not/tst when we don't need the N bit, we can
		 * tolerate a wider range of initial mappings, since
		 * bit order is irrelevant.
		 */
		if (size == W)
		  without_cc->reg_op_info[0].acceptable_mapping
		    = MAP_NATIVE_MASK | MAP_SWAP16_MASK;
		else if (size == L)
		  without_cc->reg_op_info[0].acceptable_mapping
		    = MAP_NATIVE_MASK | MAP_SWAP16_MASK | MAP_SWAP32_MASK;

		without_cc->reg_op_info[0].output_state
		  = compare_p ? ROS_UNTOUCHED : ROS_UNTOUCHED_DIRTY;
	      }
	  }
	else
	  without_cc->cc_out = M68K_CCZ;

	/* See if we need a scratch register. */
	if (MEMORY_AMODE_P (dst_amode) && size != B)
	  {
	    with_cc->scratch_reg = REGSET_ALL;
	    if (!bitwise_p && !compare_p)
	      without_cc->scratch_reg = REGSET_ALL;
	  }

	if (REGISTER_AMODE_P (dst_amode))
	  {
	    if (!strcmp (x->i386_op, "test"))
	      {
		sprintf (buf, "i386_test%c_reg_reg", char_for_size[size]);
		with_cc->compile_func[0].func = strdup (buf);
		with_cc->compile_func[0].order.operand_num[0]
		  = with_cc->compile_func[0].order.operand_num[1]
		    = x->value[0].operand_num[0];
	      }
	    else if (!strcmp (x->i386_op, "not"))
	      {
		/* Use "xorl %-1,%eax" instead of "notl %eax"; not only
		 * does this give us CC bits, it's also UV pairable
		 * where not is not pairable at all.
		 */
		sprintf (buf, "i386_xor%c_imm_reg", char_for_size[size]);
		with_cc->compile_func[0].func = strdup (buf);
		with_cc->compile_func[0].order.operand_num[0]
		  = USE_MINUS_ONE;
		with_cc->compile_func[0].order.operand_num[1]
		  = x->value[0].operand_num[0];
	      }
	    else
	      {
		sprintf (buf, "i386_%s%c_reg", x->i386_op,
			 char_for_size[size]);
		with_cc->compile_func[0].func = strdup (buf);
		with_cc->compile_func[0].order.operand_num[0]
		  = x->value[0].operand_num[0];
	      }
	    with_cc->reg_op_info[0].output_state
	      = without_cc->reg_op_info[0].output_state
		= compare_p ? ROS_UNTOUCHED : ROS_UNTOUCHED_DIRTY;

	    memcpy (&without_cc->compile_func, &with_cc->compile_func,
		    sizeof without_cc->compile_func);
	  }
	else if (MEMORY_AMODE_P (dst_amode))
	  {
	    if (size == B)
	      {
		if (!strcmp (x->i386_op, "test"))
		  {
		    sprintf (buf, "host_cmpb_imm_%s",
			     amode_name[dst_amode]);
		    with_cc->compile_func[0].func = strdup (buf);
		    with_cc->compile_func[0].order.operand_num[0] = USE_ZERO;
		    with_cc->compile_func[0].order.operand_num[1]
		      = x->value[0].operand_num[0];
		    with_cc->compile_func[0].order.operand_num[2]
		      = x->value[0].operand_num[1];
		  }
		else if (!strcmp (x->i386_op, "not"))
		  {
		    /* Use "xorl %-1,mem" instead of "notl mem"; not only
		     * does this give us CC bits, it's also UV pairable
		     * where "not" is not pairable at all.
		     */
		    sprintf (buf, "host_xorb_imm_%s",
			     amode_name[dst_amode]);
		    with_cc->compile_func[0].func = strdup (buf);
		    with_cc->compile_func[0].order.operand_num[0]
		      = USE_MINUS_ONE;
		    with_cc->compile_func[0].order.operand_num[1]
		      = x->value[0].operand_num[0];
		    with_cc->compile_func[0].order.operand_num[2]
		      = x->value[0].operand_num[1];
		  }
		else
		  {
		    sprintf (buf, "host_%sb_%s",
			     x->i386_op, amode_name[dst_amode]);
		    with_cc->compile_func[0].func = strdup (buf);
		    with_cc->compile_func[0].order.operand_num[0]
		      = x->value[0].operand_num[0];
		    with_cc->compile_func[0].order.operand_num[1]
		      = x->value[0].operand_num[1];
		  }
	    
		memcpy (&without_cc->compile_func, &with_cc->compile_func,
			sizeof without_cc->compile_func);
	      }
	    else		/* size != B */
	      {
		assert (with_cc->scratch_reg != 0);

		/* First load up the value into a scratch reg. */
		sprintf (buf, "host_move%c_%s_reg_swap",
			 char_for_size[size],
			 /* don't offset twice. */
			 (dst_amode == AMODE_POSTINC && !compare_p)
			 ? "ind" : amode_name[dst_amode]);
		with_cc->compile_func[0].func = strdup (buf);

		/* Get up to two operands needed to specify the
		 * source address.  We may overwrite the second
		 * operand below if it's not used.
		 */
		with_cc->compile_func[0].order.operand_num[0] =
		  x->value[0].operand_num[0];
		with_cc->compile_func[0].order.operand_num[1] =
		  x->value[0].operand_num[1];

		/* Put the destination scratch register in the
		 * appropriate operand (either 1 or 2).
		 */
		with_cc->compile_func[0].order
		  .operand_num[amode_operands[dst_amode]]
		    = USE_SCRATCH_REG;

		/* Now perform the operation. */
		if (!strcmp (x->i386_op, "test"))
		  {
		    sprintf (buf, "i386_test%c_reg_reg", char_for_size[size]);
		    with_cc->compile_func[1].func = strdup (buf);
		    with_cc->compile_func[1].order.operand_num[0]
		      = with_cc->compile_func[1].order.operand_num[1]
			= USE_SCRATCH_REG;
		  }
		else if (!strcmp (x->i386_op, "not"))
		  {
		    /* Use "xorl %-1,%eax" instead of "notl %eax"; not only
		     * does this give us CC bits, it's also UV pairable
		     * where not is not pairable at all.
		     */
		    sprintf (buf, "i386_xor%c_imm_reg", char_for_size[size]);
		    with_cc->compile_func[1].func = strdup (buf);
		    with_cc->compile_func[1].order.operand_num[0]
		      = USE_MINUS_ONE;
		    with_cc->compile_func[1].order.operand_num[1]
		      = USE_SCRATCH_REG;
		  }
		else
		  {
		    sprintf (buf, "i386_%s%c_reg", x->i386_op,
			     char_for_size[size]);
		    with_cc->compile_func[1].func = strdup (buf);
		    with_cc->compile_func[1].order.operand_num[0]
		      = USE_SCRATCH_REG;
		  }

		/* If it's not a compare, swap the value and write it back. */
		if (!compare_p)
		  {
		    sprintf (buf, "host_move%c_reg_%s_swap",
			     char_for_size[size],
			     /* don't offset twice. */
			     (dst_amode == AMODE_PREDEC)
			     ? "ind" : amode_name[dst_amode]);
		    with_cc->compile_func[2].func = strdup (buf);
		    with_cc->compile_func[2].order.operand_num[0]
		      = USE_SCRATCH_REG;
		    with_cc->compile_func[2].order.operand_num[1]
		      = x->value[0].operand_num[0];
		    with_cc->compile_func[2].order.operand_num[2]
		      = x->value[0].operand_num[1];
		  }

		if (compare_p || bitwise_p)
		  {
		    assert (!compare_p || !strcmp (x->i386_op, "test"));
		    assert (!bitwise_p || !strcmp (x->i386_op, "not"));

		    sprintf (buf, "host_%s%c_imm_%s",
			     compare_p ? "cmp" : "xor",
			     char_for_size[size], amode_name[dst_amode]);
		    without_cc->compile_func[0].func = strdup (buf);
		    without_cc->compile_func[0].order.operand_num[0]
		      = compare_p ? USE_ZERO : USE_MINUS_ONE;
		    without_cc->compile_func[0].order.operand_num[1]
		      = x->value[0].operand_num[0];
		    without_cc->compile_func[0].order.operand_num[2]
		      = x->value[0].operand_num[1];
		  }
		else
		  {
		    /* without_cc looks the same as the with_cc case. */
		    memcpy (without_cc->compile_func,
			    with_cc->compile_func,
			    sizeof without_cc->compile_func);
		  }
	      }
	  }
	else
	  abort ();

	assert (!(with_cc->cc_out & ~x->cc_to_compute));
	assert (!(without_cc->cc_out & ~x->cc_to_compute));
      }

  return retval;
}


/* Binary ops can have at most one operand in memory.  Supported binary ops:
 * Bitwise:	and, or, xor
 * Arithmetic:	add, sub
 * Compare:	cmp
 * Shift:       lsl, asr, lsr  (not all cc bits computed)
 */
static guest_code_descriptor_t *
process_binary (const xlate_descriptor_t *x)
{
  guest_code_descriptor_t *retval;
  BOOL bitwise_p, compare_p, shift_p;
  size_mask_t size;
  char buf[1024];  /* For scratch stuff. */

  /* Note if this operation is a bitwise one.  If so, we have a little
   * more freedom doing things to memory when we don't need the N bit.
   * These ops are assumed to clear CV and set NZ appropriately.
   */
  bitwise_p = bitwise_op_p (x->i386_op);

  /* Is it a compare?  If so, we don't write any result value back. */
  compare_p = compare_op_p (x->i386_op);

  /* Is it a shift? */
  shift_p = shift_op_p (x->i386_op);

  retval = NULL;

  for (size = B; size <= L; size <<= 1)
    if (x->sizes & size)
      {
	int src_amode = x->value[0].amode;
	int dst_amode = x->value[MAX_XLATE_VALUES - 1].amode;
	guest_code_descriptor_t def, *with_cc, *without_cc;

	/* Compute some reasonable defaults. */
	default_desc (x, size, &def);

	/* We have two cases: computing cc's, and not computing cc's. */
	with_cc    = alloc_gcd ();
	without_cc = alloc_gcd ();
	*with_cc = *without_cc = def;  /* Set to default values. */

	/* Insert them into the chain. */
	with_cc->next = retval;
	without_cc->next = with_cc;
	retval = without_cc;

	/* Set up their names and special attributes. */
	with_cc->static_p = TRUE;
	without_cc->name = create_name (x->name, size,
					src_amode, dst_amode, 0);
	with_cc->name    = create_name (x->name, size,
					src_amode, dst_amode, 1);

	if (bitwise_p || compare_p)
	  {
	    /* We still get CVZ for free when we do a bitwise op, regardless
	     * of endianness.  We get Z for free for compares.
	     */
	    if (size == B)
	      without_cc->cc_out = M68K_CC_CNVZ;
	    else if (bitwise_p)
	      without_cc->cc_out = (M68K_CCC | M68K_CCV | M68K_CCZ);
	    else
	      without_cc->cc_out = M68K_CCZ;

	    if (bitwise_p
		&& src_amode == AMODE_IMM
		&& REGISTER_AMODE_P (dst_amode))
	      {
		/* For bitwise ops when we don't need the N bit, we can
		 * tolerate a wider range of initial mappings.
		 */
		if (size != L)
		  without_cc->reg_op_info[0].acceptable_mapping
		    = MAP_NATIVE_MASK | MAP_SWAP16_MASK;
		else
		  without_cc->reg_op_info[0].acceptable_mapping
		    = MAP_NATIVE_MASK | MAP_SWAP16_MASK | MAP_SWAP32_MASK;
		without_cc->reg_op_info[0].output_state = ROS_UNTOUCHED_DIRTY;

	      }
	    else if (REGISTER_AMODE_P (src_amode)
		     && MEMORY_AMODE_P (dst_amode))
	      {
		if (size == W)
		  without_cc->reg_op_info[0].acceptable_mapping
		    = MAP_SWAP16_MASK;
		else if (size == L)
		  without_cc->reg_op_info[0].acceptable_mapping
		    = MAP_SWAP32_MASK;
	      }
	    else if (MEMORY_AMODE_P (src_amode)
		     && REGISTER_AMODE_P (dst_amode)
		     && (dst_amode != AMODE_AREG || size != W)
		     && size != B)
	      {
		without_cc->reg_op_info[amode_reg_operands[src_amode]]
		  .acceptable_mapping = ((size == W)
					 ? MAP_SWAP16_MASK
					 : MAP_SWAP32_MASK);

		if (compare_p)
		  without_cc->reg_op_info[amode_reg_operands[src_amode]]
		    .output_state = ROS_UNTOUCHED;
		else
		  without_cc->reg_op_info[amode_reg_operands[src_amode]]
		    .output_state = ROS_UNTOUCHED_DIRTY;
	      }
	  }
	else
	  without_cc->cc_out = M68K_CC_NONE;

	/* For add, sub, and cmp with immediate constants, we allow
	 * offset registers, but only when we don't care about
	 * the overflow bit.
	 */
	if (src_amode == AMODE_IMM
	    && REGISTER_AMODE_P (dst_amode)
	    && !bitwise_p
	    && !shift_p)
	  {
	    without_cc->reg_op_info[0].acceptable_mapping
	      = (MAP_NATIVE_MASK | MAP_OFFSET_MASK);
	    without_cc->cc_out = ((M68K_CCC | M68K_CCN | M68K_CCX | M68K_CCZ)
				  & x->cc_to_compute);
	  }

	/* add/sub to address register never affect cc bits. */
	if (dst_amode == AMODE_AREG && !compare_p)
	  {
	    if (bitwise_p)
	      abort ();
	    with_cc->cc_out = without_cc->cc_out = M68K_CC_NONE;
	  }

	/* See if we need a scratch register. */
	if (((MEMORY_AMODE_P (src_amode) || MEMORY_AMODE_P (dst_amode))
	     && size != B)
	    || (size == W
		&& dst_amode == AMODE_AREG
		&& src_amode != AMODE_IMM))
	  {
	    with_cc->scratch_reg = REGSET_ALL;
	    if ((size == W && dst_amode == AMODE_AREG)
		|| (!bitwise_p && !compare_p))
	      without_cc->scratch_reg = REGSET_ALL;
	  }

	/* Compares do not affect the destination register, unlike other
	 * binary ops.
	 */
	if (compare_p && REGISTER_AMODE_P (dst_amode))
	  {
	    without_cc->reg_op_info[amode_reg_operands[src_amode]]
	      .output_state = ROS_UNTOUCHED;
	    with_cc->reg_op_info[amode_reg_operands[src_amode]]
	      .output_state = ROS_UNTOUCHED;
	  }

	if (src_amode == AMODE_IMM)
	  {
	    if (REGISTER_AMODE_P (dst_amode))
	      {
		if (bitwise_p)
		  sprintf (buf, "host_%s%c_imm_reg", x->i386_op,
			   char_for_size[size]);
		else if (size == L && !shift_p)
		  sprintf (buf, "host_%sl_imm_reg", x->i386_op);
		else
		  sprintf (buf, "i386_%s%c_imm_reg", x->i386_op,
			   (dst_amode == AMODE_AREG)
			   ? 'l' : char_for_size[size]);
		with_cc->compile_func[0].func =
		  without_cc->compile_func[0].func = strdup (buf);
	      }
	    else if (MEMORY_AMODE_P (dst_amode))
	      {
		if (size == B)
		  {
		    sprintf (buf, "host_%s%c_imm_%s",
			     x->i386_op,
			     char_for_size[size], amode_name[dst_amode]);
		    with_cc->compile_func[0].func =
		      without_cc->compile_func[0].func = strdup (buf);
		  }
		else  /* size != B */
		  {
		    /* First load up the value into a scratch reg. */
		    sprintf (buf, "host_move%c_%s_reg_swap",
			     char_for_size[size],
			     /* don't offset twice. */
			     (dst_amode == AMODE_POSTINC && !compare_p)
			     ? "ind" : amode_name[dst_amode]);
		    with_cc->compile_func[0].func = strdup (buf);

		    /* Get up to two operands needed to specify the
		     * source address.  We may overwrite the second
		     * operand below if it's not used.
		     */
		    with_cc->compile_func[0].order.operand_num[0] =
		      x->value[1].operand_num[0];
		    with_cc->compile_func[0].order.operand_num[1] =
		      x->value[1].operand_num[1];

		    /* Put the destination scratch register in the
		     * appropriate operand (either 1 or 2).
		     */
		    with_cc->compile_func[0].order
		      .operand_num[amode_operands[dst_amode]]
			= USE_SCRATCH_REG;

		    /* Now perform the operation. */
		    sprintf (buf, "i386_%s%c_imm_reg", x->i386_op,
			     char_for_size[size]);
		    with_cc->compile_func[1].func = strdup (buf);
		    with_cc->compile_func[1].order.operand_num[0]
		      = x->value[0].operand_num[0];
		    with_cc->compile_func[1].order.operand_num[1]
		      = USE_SCRATCH_REG;

		    /* If it's not a compare, swap the value and write
		     * it back.
		     */
		    if (!compare_p)
		      {
			sprintf (buf, "host_move%c_reg_%s_swap",
				 char_for_size[size],
				 /* don't offset twice. */
				 (dst_amode == AMODE_PREDEC)
				 ? "ind" : amode_name[dst_amode]);
			with_cc->compile_func[2].func = strdup (buf);
			with_cc->compile_func[2].order.operand_num[0]
			  = USE_SCRATCH_REG;
			/* defaults are acceptable for other operand_nums. */
		      }

		    if (!bitwise_p && !compare_p)
		      {
			/* without_cc looks the same as the with_cc case. */
			memcpy (without_cc->compile_func,
				with_cc->compile_func,
				sizeof without_cc->compile_func);
		      }
		    else  /* SIZE != B && bitwise_p */
		      {
			/* When we don't need the N bit we can do a much
			 * faster sequence for bitwise ops.  For example,
			 * when the 68k program says: "andw #0x001F,_addr"
			 * we say (in i386-speak) "andw $0x1F00,_addr"
			 * When we are doing a cmp and only need the Z bit,
			 * we can just cmp with the swapped value.
			 */
			sprintf (buf, "host_%s%c_imm_%s",
				 x->i386_op, char_for_size[size],
				 amode_name[dst_amode]);
			without_cc->compile_func[0].func = strdup (buf);
			without_cc->compile_func[1].func = NULL;
		      }
		  }
	      }
	    else
	      abort ();
	  }
	else if (REGISTER_AMODE_P (src_amode))
	  {
	    if (dst_amode == AMODE_AREG && size == W)
	      {
		assert (with_cc->scratch_reg);
		assert (without_cc->scratch_reg);

		/* Sign extend the source register to a long. */
		with_cc->compile_func[0].func =
		  without_cc->compile_func[0].func = "i386_movswl_reg_reg";
		with_cc->compile_func[0].order.operand_num[1]
		  = without_cc->compile_func[0].order.operand_num[1]
		    = USE_SCRATCH_REG;

		/* Perform the actual operation. */
		sprintf (buf, "i386_%sl_reg_reg", x->i386_op);
		with_cc->compile_func[1].func =
		  without_cc->compile_func[1].func = strdup (buf);
		with_cc->compile_func[1].order.operand_num[0] =
		  without_cc->compile_func[1].order.operand_num[0] =
		    USE_SCRATCH_REG;
	      }
	    else if (REGISTER_AMODE_P (dst_amode))
	      {
		sprintf (buf, "i386_%s%c_reg_reg", x->i386_op,
			 char_for_size[size]);
		with_cc->compile_func[0].func =
		  without_cc->compile_func[0].func = strdup (buf);
	      }
	    else if (MEMORY_AMODE_P (dst_amode))
	      {
		if (size == B)
		  {
		    sprintf (buf, "host_%sb_reg_%s",
			     x->i386_op, amode_name[dst_amode]);
		    with_cc->compile_func[0].func =
		      without_cc->compile_func[0].func = strdup (buf);
		  }
		else
		  {
		    /* First load up the value into a scratch reg. */
		    sprintf (buf, "host_move%c_%s_reg_swap",
			     char_for_size[size],
			     /* don't offset twice. */
			     (dst_amode == AMODE_POSTINC && !compare_p)
			     ? "ind" : amode_name[dst_amode]);
		    with_cc->compile_func[0].func = strdup (buf);

		    /* Get up to two operands needed to specify the
		     * source address.  We may overwrite the second
		     * operand below if it's not used.
		     */
		    with_cc->compile_func[0].order.operand_num[0] =
		      x->value[1].operand_num[0];
		    with_cc->compile_func[0].order.operand_num[1] =
		      x->value[1].operand_num[1];

		    /* Put the destination scratch register in the
		     * appropriate operand (either 1 or 2).
		     */
		    with_cc->compile_func[0].order
		      .operand_num[amode_operands[dst_amode]]
			= USE_SCRATCH_REG;

		    /* Now perform the operation. */
		    sprintf (buf, "i386_%s%c_reg_reg", x->i386_op,
			     char_for_size[size]);
		    with_cc->compile_func[1].func = strdup (buf);
		    with_cc->compile_func[1].order.operand_num[0]
		      = x->value[0].operand_num[0];
		    with_cc->compile_func[1].order.operand_num[1]
		      = USE_SCRATCH_REG;

		    /* If it's not a compare, swap the value and write
		     * it back.
		     */
		    if (!compare_p)
		      {
			sprintf (buf, "host_move%c_reg_%s_swap",
				 char_for_size[size],
				 /* don't offset twice. */
				 (dst_amode == AMODE_PREDEC)
				 ? "ind" : amode_name[dst_amode]);
			with_cc->compile_func[2].func = strdup (buf);
			with_cc->compile_func[2].order.operand_num[0]
			  = USE_SCRATCH_REG;
			/* defaults are acceptable for other operand_nums. */
		      }

		    if (!bitwise_p && !compare_p)
		      {
			/* without_cc looks the same as the with_cc case. */
			memcpy (without_cc->compile_func,
				with_cc->compile_func,
				sizeof without_cc->compile_func);
		      }
		    else
		      {
			sprintf (buf, "host_%s%c_reg_%s",
				 x->i386_op, char_for_size[size],
				 amode_name[dst_amode]);
			without_cc->compile_func[0].func = strdup (buf);
		      }
		  }
	      }
	    else
	      abort ();
	  }
	else if (MEMORY_AMODE_P (src_amode))
	  {
	    /* Destination must be a register. */
	    if (!REGISTER_AMODE_P (dst_amode))
	      abort ();
	    
	    if (size == W && dst_amode == AMODE_AREG)
	      {
		assert (with_cc->scratch_reg);
		assert (without_cc->scratch_reg);

		/* First load up the value into a scratch reg. */
		sprintf (buf, "host_move%c_%s_reg_swap",
			 char_for_size[size], amode_name[src_amode]);
		with_cc->compile_func[0].func = strdup (buf);
		with_cc->compile_func[0].order
		  .operand_num[amode_operands[src_amode]]
		    = USE_SCRATCH_REG;

		/* Sign extend the scratch value. */
		with_cc->compile_func[1].func
		  = "i386_movswl_reg_reg";
		with_cc->compile_func[1].order.operand_num[0]
		  = USE_SCRATCH_REG;
		with_cc->compile_func[1].order.operand_num[1]
		  = USE_SCRATCH_REG;

		/* Now perform the operation.  We need to call the
		 * host version here if the src was a predec or a postinc,
		 * since that might have offset the register to which
		 * we are comparing, e.g. "cmpw a0@-,a0".
		 */
		sprintf (buf, "%s_%sl_reg_reg",
			 ((src_amode == AMODE_PREDEC
			   || src_amode == AMODE_POSTINC)
			  ? "host" : "i386"),
			  x->i386_op);
		with_cc->compile_func[2].func = strdup (buf);
		with_cc->compile_func[2].order.operand_num[0]
		  = USE_SCRATCH_REG;
		with_cc->compile_func[2].order.operand_num[1]
		  = x->value[1].operand_num[0];

		/* Make with and without cases identical. */
		memcpy (&without_cc->compile_func,
			&with_cc->compile_func,
			sizeof with_cc->compile_func);
	      }
	    else if (size == B)
	      {
		/* Since it's only a byte op, we don't need a temp
		 * register.
		 */
		sprintf (buf, "host_%sb_%s_reg",
			 x->i386_op, amode_name[src_amode]);
		without_cc->compile_func[0].func
		  = with_cc->compile_func[0].func = strdup (buf);
	      }
	    else
	      {
		assert (with_cc->scratch_reg);

		/* First load up the value into a scratch reg. */
		sprintf (buf, "host_move%c_%s_reg_swap",
			 char_for_size[size], amode_name[src_amode]);
		with_cc->compile_func[0].func = strdup (buf);
		with_cc->compile_func[0].order
		  .operand_num[amode_operands[src_amode]]
		    = USE_SCRATCH_REG;

		/* Now perform the operation. */
		if (size == L
		    && dst_amode == AMODE_AREG
		    && (src_amode == AMODE_PREDEC
			|| src_amode == AMODE_POSTINC)
		    && (!strcmp (x->i386_op, "add")
			|| !strcmp (x->i386_op, "sub")
			|| !strcmp (x->i386_op, "cmp")))
		  {
		    sprintf (buf, "host_%sl_reg_reg", x->i386_op);
		  }
		else
		  {
		    sprintf (buf, "i386_%s%c_reg_reg", x->i386_op,
			     char_for_size[size]);
		  }
		with_cc->compile_func[1].func = strdup (buf);
		with_cc->compile_func[1].order.operand_num[0]
		  = USE_SCRATCH_REG;
		with_cc->compile_func[1].order.operand_num[1]
		  = x->value[1].operand_num[0];

		if (!bitwise_p && !compare_p)
		  {
		    assert (without_cc->scratch_reg);

		    /* without_cc looks the same as the with_cc case. */
		    memcpy (without_cc->compile_func,
			    with_cc->compile_func,
			    sizeof without_cc->compile_func);
		  }
		else
		  {
		    sprintf (buf, "host_%s%c_%s_reg",
			     x->i386_op, char_for_size[size],
			     amode_name[src_amode]);
		    without_cc->compile_func[0].func = strdup (buf);
		  }
	      }
	  }
	else
	  abort ();

	assert (!(with_cc->cc_out & ~x->cc_to_compute));
	assert (!(without_cc->cc_out & ~x->cc_to_compute));
      }

  return retval;
}


static guest_code_descriptor_t *
process_move (const xlate_descriptor_t *x)
{
  guest_code_descriptor_t *retval;
  int i, op;
  size_mask_t size;
  char buf[1024];  /* For scratch stuff. */

  retval = NULL;
  for (size = B; size <= L; size <<= 1)
    if (x->sizes & size)
      {
	int src_amode = x->value[0].amode;
	int dst_amode = x->value[MAX_XLATE_VALUES - 1].amode;
	guest_code_descriptor_t def, *with_cc, *without_cc;

	default_desc (x, size, &def);

	for (i = 0, op = 0; i < MAX_XLATE_VALUES; i++)
	  {
	    int amode;
	    reg_operand_info_t *r;

	    amode = x->value[i].amode;
	    if (amode_reg_operands[amode] == 0)
	      continue;

	    r = &def.reg_op_info[op];
	    
	    /* See if the destination value is a register. */
	    if (i == MAX_XLATE_VALUES - 1 && REGISTER_AMODE_P (amode))
	      {
		/* Figure out which byte orders are acceptable. */
		if (size == B)
		  r->acceptable_mapping = MAP_NATIVE_MASK;
		else if (size == W && amode != AMODE_AREG)
		  r->acceptable_mapping =
		    (MAP_NATIVE_MASK | MAP_SWAP16_MASK);
		else		/* size == L */
		  r->acceptable_mapping = MAP_ALL_MASK;

		/* Figure out if we can just use a spare reg. */
		if (size == L || amode == AMODE_AREG)
		  r->request_type = REQUEST_SPARE_REG;

		/* Compute a likely default for the register
		 * output state.
		 */
		if (size == B || !MEMORY_AMODE_P (src_amode)
		    || (size == W && dst_amode == AMODE_AREG))
		  r->output_state = ROS_NATIVE_DIRTY;
		else if (size == W)
		  r->output_state = ROS_SWAP16_DIRTY;
		else if (size == L)
		  r->output_state = ROS_SWAP32_DIRTY;
	      }
	    else if (i == 0	/* Source operand for reg->mem? */
		     && REGISTER_AMODE_P (amode)
		     && MEMORY_AMODE_P (dst_amode))
	      {
		/* Force it to come in swapped. */
		r->acceptable_mapping = 1L << swap_map_for_size[size];
	      }
	    else if (i == 0	/* Source operand for reg->reg? */
		     && REGISTER_AMODE_P (amode)
		     && REGISTER_AMODE_P (dst_amode))
	      {
		r->acceptable_mapping = MAP_NATIVE_MASK;
	      }
	    else /* Generic memory operand (address register). */
	      {
		r->acceptable_mapping = MAP_NATIVE_MASK | MAP_OFFSET_MASK;
	      }
	    op++;
	  }

	/* We have two cases: computing cc's, and not computing cc's. */
	with_cc    = alloc_gcd ();
	without_cc = alloc_gcd ();
	*with_cc = *without_cc = def;  /* Set to default values. */

	/* Insert them into the chain. */
	with_cc->next = retval;
	without_cc->next = with_cc;
	retval = without_cc;

	/* Set up their names and special attributes. */
	without_cc->cc_out = M68K_CC_NONE;
	with_cc->static_p = TRUE;
	without_cc->name = create_name (x->name, size,
					src_amode, dst_amode, 0);
	with_cc->name    = create_name (x->name, size,
					src_amode, dst_amode, 1);

	/* Memory->memory moves require a scratch register.  imm->memory
	 * moves require a scratch register if we're interested in CC bits.
	 * *->memory moves require a scratch register if we need cc bits
	 * and we're not doing a byte move.  If we're doing a non-byte
	 * move from memory to a register and we want cc's then we also
	 * need a scratch register.  We will demand REGSET_BYTE for the
	 * with_cc case, because if they don't want the Z bit we can compute
	 * CNV by testing the low byte of the temporary swapped value
	 * (instead of swapping the value, testing, and swapping it back).
	 */
	if ((MEMORY_AMODE_P (dst_amode)
	     && (MEMORY_AMODE_P (src_amode)
		 || src_amode == AMODE_IMM
		 || size != B))
	    || (MEMORY_AMODE_P (src_amode) && size != B)
	    || src_amode == AMODE_INDIX
	    || dst_amode == AMODE_INDIX)
	  {
	    with_cc->scratch_reg = REGSET_BYTE;
	  }
	if ((MEMORY_AMODE_P (src_amode) && MEMORY_AMODE_P (dst_amode))
	    || src_amode == AMODE_INDIX || dst_amode == AMODE_INDIX)
	  without_cc->scratch_reg = (size == B) ? REGSET_BYTE : REGSET_ALL;

	if (src_amode == AMODE_IMM)
	  {
	    if (size != B && MEMORY_AMODE_P (dst_amode))
	      assert (with_cc->scratch_reg);

	    /* e.g. movw #5,d0, movl #19,a0@+ */
	    sprintf (buf, "host_move%c_imm_%s",
		     dst_amode == AMODE_AREG ? 'l' : char_for_size[size],
		     amode_name[dst_amode]);
	    without_cc->compile_func[0].func =
	      with_cc->compile_func[0].func = strdup (buf);

	    if (dst_amode == AMODE_INDIX)
	      {
		int t = amode_operands[src_amode] + amode_operands[dst_amode];
		without_cc->compile_func[0].order.operand_num[t]
		  = with_cc->compile_func[0].order.operand_num[t]
		    = ((x->value[0].operand_num[0] == 0)
		       ? USE_M68K_PC : ((size == L)
					? USE_M68K_PC_PLUS_FOUR
					: USE_M68K_PC_PLUS_TWO));
	      }
	  }
	else if (REGISTER_AMODE_P (src_amode))
	  {
	    if (REGISTER_AMODE_P (dst_amode))
	      {
		if (dst_amode == AMODE_AREG && size == W)
		  {
		    without_cc->compile_func[0].func =
		      with_cc->compile_func[0].func =
			"i386_movswl_reg_reg";
		    with_cc->compile_func[1].func = "host_testl_reg";
		    with_cc->compile_func[1].order.operand_num[0]
		      = x->value[1].operand_num[0];
		  }
		else
		  {
		    sprintf (buf, "i386_mov%c_reg_reg", char_for_size[size]);
		    without_cc->compile_func[0].func =
		      with_cc->compile_func[0].func = strdup (buf);
		    sprintf (buf, "host_test%c_reg", char_for_size[size]);
		    with_cc->compile_func[1].func = strdup (buf);
		  }
	      }
	    else  /* dst is a memory amode. */
	      {
		sprintf (buf, "host_move%c_reg_%s",
			 char_for_size[size],
			 amode_name[dst_amode]);
		without_cc->compile_func[0].func =
		  with_cc->compile_func[0].func = strdup (buf);

		if (dst_amode == AMODE_INDIX)
		  {
		    int t = (amode_operands[src_amode]
			     + amode_operands[dst_amode]);
		    without_cc->compile_func[0].order.operand_num[t]
		      = with_cc->compile_func[0].order.operand_num[t]
			= USE_M68K_PC;
		  }

		sprintf (buf, "host_test%c_swapped_reg", char_for_size[size]);
		with_cc->compile_func[1].func = strdup (buf);
	      }
	  }
	else   /* src is a memory amode. */
	  {
	    if (REGISTER_AMODE_P (dst_amode))
	      {
		sprintf (buf, "host_move%c_%s_reg",
			 char_for_size[size],
			 amode_name[src_amode]);

		if (src_amode == AMODE_INDIX)
		  {
		    int t = (amode_operands[src_amode]
			     + amode_operands[dst_amode]);
		    without_cc->compile_func[0].order.operand_num[t]
		      = with_cc->compile_func[0].order.operand_num[t]
			= USE_M68K_PC;
		  }

		with_cc->compile_func[0].func =
		  without_cc->compile_func[0].func = strdup (buf);

		if (size == W && dst_amode == AMODE_AREG)
		  {
		    with_cc->compile_func[1].func =
		      without_cc->compile_func[1].func =
			"host_swap16_sext_test_reg";
		    with_cc->compile_func[1].order.operand_num[0] =
		      without_cc->compile_func[1].order.operand_num[0] =
			x->value[1].operand_num[0];
		  }
		else
		  {
		    /* Create a call to the test function. */
		    sprintf (buf, "host_test%c_swapped_reg",
			     char_for_size[size]);
		    with_cc->compile_func[1].func = strdup (buf);
		    with_cc->compile_func[1].order.operand_num[0] =
		      x->value[1].operand_num[0];
		  }
	      }
	    else  /* Memory -> memory transfer. */
	      {
		compile_func_t comp;

		assert (with_cc->scratch_reg);
		assert (without_cc->scratch_reg);

		sprintf (buf, "host_move%c_%s_reg",
			 char_for_size[size],
			 amode_name[src_amode]);

		/* Move from memory to a scratch register. */
		comp.func = strdup (buf);
		comp.order = def.compile_func[0].order;
		comp.order.operand_num[amode_operands[src_amode]]
		  = USE_SCRATCH_REG;
		if (src_amode == AMODE_INDIX)
		  {
		    int t = amode_operands[src_amode] + 1;
		    comp.order.operand_num[t] = USE_M68K_PC;
		  }
		with_cc->compile_func[0] = without_cc->compile_func[0] = comp;

		/* Test the scratch register, when we need cc bits. */
		sprintf (buf, "host_test%c_swapped_reg", char_for_size[size]);
		with_cc->compile_func[1].func = strdup (buf);
		memset (&with_cc->compile_func[1].order, 0,
			sizeof with_cc->compile_func[1].order);
		with_cc->compile_func[1].order.operand_num[0]
		  = USE_SCRATCH_REG;

		/* Move from scratch register to memory. */
		sprintf (buf, "host_move%c_reg_%s",
			 char_for_size[size],
			 amode_name[dst_amode]);

		comp.func = strdup (buf);
		comp.order.operand_num[0] = USE_SCRATCH_REG;
		comp.order.operand_num[1] = x->value[1].operand_num[0];
		comp.order.operand_num[2] = x->value[1].operand_num[1];
		if (dst_amode == AMODE_INDIX)
		  {
		    int t = 1 + amode_operands[dst_amode];

		    /* We don't know how many m68k words to skip to get
		     * to the indix info if it's an ABS amode (could
		     * be absw or absl).
		     */
		    if (src_amode == AMODE_ABS)
		      abort ();

		    if (src_amode == AMODE_INDOFF || src_amode == AMODE_INDIX)
		      comp.order.operand_num[t] = USE_M68K_PC_PLUS_TWO;
		    else
		      comp.order.operand_num[t] = USE_M68K_PC;
		  }
		with_cc->compile_func[2] = without_cc->compile_func[1] = comp;
	      }
	  }
      }
  
  return retval;
}


static void
default_desc (const xlate_descriptor_t *x, size_mask_t size,
	      guest_code_descriptor_t *g)
{
  oporder_t default_oporder;
  int i, j, op;

  /* Set up a reasonable default order for our operands. */	
  memset (&default_oporder, 0, sizeof default_oporder);
  for (i = op = 0; i < 2; i++)
    for (j = 0; j < amode_operands[x->value[i].amode]; j++)
      default_oporder.operand_num[op++] = x->value[i].operand_num[j];

  /* Set up a default descriptor for all the cases. */
  memset (g, 0, sizeof *g);
  for (i = 0; i < MAX_COMPILE_FUNCS; i++)
    g->compile_func[i].order = default_oporder;
  g->cc_in = M68K_CC_NONE;
  g->cc_out = x->cc_to_compute;

  for (i = 0, op = 0; i < MAX_XLATE_VALUES; i++)
    {
      int amode;
      reg_operand_info_t *r;
      
      amode = x->value[i].amode;
      if (amode_reg_operands[amode] == 0)
	continue;
      
      r = &g->reg_op_info[op];
      r->legitimate_p = TRUE;
      r->add8_p       = (MEMORY_AMODE_P (amode) || amode == AMODE_AREG);
      if (amode == AMODE_INDOFF)
	r->operand_num  = x->value[i].operand_num[1];  /* offset,reg */
      else
	r->operand_num  = x->value[i].operand_num[0];
      
      if (r->add8_p)  /* Address registers can go anywhere. */
	r->regset = REGSET_ALL;
      else
	r->regset = REGSET_BYTE;

      if (MEMORY_AMODE_P (amode))
	r->acceptable_mapping = MAP_NATIVE_MASK | MAP_OFFSET_MASK;
      else
	r->acceptable_mapping = MAP_NATIVE_MASK;
      r->request_type       = REQUEST_REG;

      /* If we're a destination register, we're dirty.  Otherwise,
       * we're untouched.
       */
      if (i == MAX_XLATE_VALUES - 1 && REGISTER_AMODE_P (amode))
	r->output_state = ROS_NATIVE_DIRTY;
      else
	r->output_state = ROS_UNTOUCHED;

      op++;
    }

  g->reg_op_info[op].legitimate_p = FALSE;
  assert (op < NELEM (g->reg_op_info));
}


static guest_code_descriptor_t *
alloc_gcd ()
{
  guest_code_descriptor_t *g;
  g = (guest_code_descriptor_t *)malloc (sizeof *g);
  memset (g, 0, sizeof *g);
  return g;
}

static char *
create_name (const char *n, int size, int src_amode, int dst_amode, int which)
{
  char buf[1024];
  char *s;

  sprintf (buf, "xlate_%s", n);
#if 0
  if (src_amode != AMODE_NONE)
    sprintf (buf + strlen (buf), "_%s", amode_name[src_amode]);
  if (dst_amode != AMODE_NONE)
    sprintf (buf + strlen (buf), "_%s", amode_name[dst_amode]);
#endif
  if (which != 0)
    sprintf (buf + strlen (buf), "_N%d", which);
  for (s = buf; *s != '\0'; s++)
    if (*s == '@')
      *s = char_for_size[size];
  return strdup (buf);
}
