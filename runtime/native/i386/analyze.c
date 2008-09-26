#include "template.h"
#include "asmdata.h"
#include "native.h"
#include "syn68k_private.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


typedef struct
{
  int offset;       /* Offset in bits from the start.      */
  int length;       /* Number of bits long this field is.  */
  int operand_num;  /* Which operand gets plugged in here. */
} oploc_t;


static const uint8 *compute_bits (int size, oploc_t *loc, int *loc_entries,
				  const char legal_p[NUM_SAMPLES]);
static void output_bits (const oploc_t *loc, int loc_entries,
			 const uint8 *bits, int size, const char *extra,
			 int indent);
static boolean_t generate_code (const char legal_p[NUM_SAMPLES],
				const char *postamble,
				boolean_t output_p, int indent);
static void code_for_bits (const char *b, int start, int length,
			   int indent, int offset, const oploc_t *loc);


int
main (int argc, char *argv[])
{
  char legal_p[NUM_SAMPLES];

  memset (legal_p, TRUE, sizeof legal_p);

  /* beginning of ctm hack */

  /* Starting with binutils-2.16.90.0.3, instructions like

       lea 0x0(%eax),%eax

     began compiling into the exact same byte sequence as

       lea (%eax),%eax

     That causes us to see a problem where one doesn't exist.  If the code
     for handling a 0 offset were working properly, there would be no need
     for this hack, but it's not, so we simply go through and invalidate
     all places where we have a 0 as the offset.  This is not guaranteed to
     work, but it does in practice and I'm not going to spend the time needed
     to get proper handling of 0 offsets.

   */

  {
    int op;
    
    for (op = 0; op < NUM_OPERANDS; op++)
      {
	if (TEMPLATE.operand[op].type == CONSTANT &&
	    strcmp(TEMPLATE.operand_name[op], "offset") == 0)
	  {
	    int s;

	    for (s = 0; s < NUM_SAMPLES; s++)
	      {
		if (value[s][op] == 0)
		  legal_p[s] = FALSE;
	      }
	  }
      }
  }
  
  /* end of ctm hack */

  if (!generate_code (legal_p, "", TRUE, 2))
    return EXIT_FAILURE;

  return EXIT_SUCCESS;
}


#define SAMPLE_SIZE(n) (sample[n].end - sample[n].start)


/* Returns TRUE iff some legal samples have values for the specified
 * operand in the given range and some do not.
 */
static boolean_t
operand_variety_p (int operand_num, long val_low, long val_high,
		   const char legal_p[NUM_SAMPLES])
{
  boolean_t has_p, has_not_p;
  int s;

  has_p = has_not_p = FALSE;
  for (s = 0; s < NUM_SAMPLES; s++)
    if (legal_p[s])
      {
	if (value[s][operand_num] >= val_low
	    && value[s][operand_num] <= val_high)
	  has_p = TRUE;
	else
	  has_not_p = TRUE;

	if (has_p && has_not_p)
	  return TRUE;
      }

  return FALSE;
}


static boolean_t
special_operand_p (int operand_num, long val_low, long val_high,
		   const char legal_p[NUM_SAMPLES])
{
  int s;
  boolean_t found_challenger_p;

  found_challenger_p = FALSE;
  for (s = 0; s < NUM_SAMPLES; s++)
    if (legal_p[s]
	&& value[s][operand_num] >= val_low
	&& value[s][operand_num] <= val_high)
      {
	int s2, op;

	/* Now see if every other sample differing _only_ in this value
	 * is of a different size.
	 */
	for (s2 = s + 1; s2 < NUM_SAMPLES; s2++)
	  {
	    if (!legal_p[s2])
	      continue;

	    /* See if s2 differs in operand_num but nothing else. */
	    for (op = 0; op < NUM_OPERANDS; op++)
	      {
		if (op != operand_num)
		  {
		    if (value[s2][op] != value[s][op])
		      break;
		  }
		else
		  {
		    if (value[s2][op] >= val_low
			&& value[s2][op] <= val_high)
		      break;
		  }
	      }

	    if (op >= NUM_OPERANDS)
	      {
		/* Yep, all other operands are the same and the operand is
		 * outside the specified range.  If the size didn't
		 * change, this operand isn't the cause of any special case.
		 */
		if (SAMPLE_SIZE (s2) == SAMPLE_SIZE (s))
		  {
#if 0
		    printf ("operand %d, (%ld, %ld) disqualified by %d:%d\n",
			    operand_num, val_low, val_high, s, s2);
#endif
		    return FALSE;
		  }
		found_challenger_p = TRUE;
	      }
	  }
      }
  
  /* If all or none of the legal samples have this attribute, then there's
   * no reason to call it special.
   */
  return found_challenger_p;
}


static void
filter (const char *old_legal_p, char *new_legal_p,
	int operand_num, long val_low, long val_high, boolean_t match_p)
{
  int s;

  memset (new_legal_p, 0, NUM_SAMPLES * sizeof new_legal_p[0]);
  for (s = 0; s < NUM_SAMPLES; s++)
    if (old_legal_p[s]
	&& (value[s][operand_num] >= val_low
	    && value[s][operand_num] <= val_high) == match_p)
      new_legal_p[s] = TRUE;
}


static void
spaces (int s)
{
  while (s-- > 0)
    putchar (' ');
}


static boolean_t
generate_code (const char legal_p[NUM_SAMPLES], const char *postamble,
	       boolean_t output_p, int indent)
{
  int i, size;
  boolean_t found_p, problem_p;

  size = 0;

  /* First see if every legal entry is the same size. */
  for (i = 0, found_p = problem_p = FALSE; i < NUM_SAMPLES; i++)
    if (legal_p[i])
      {
	int new_size;
	new_size = SAMPLE_SIZE (i);
	if (new_size != size)
	  {
	    if (found_p)
	      problem_p = TRUE;
	    else
	      {
		size = new_size;
		found_p = TRUE;
	      }
	  }
      }

  /* If they are all the same size, try to generate code for it. */
  if (!problem_p)
    {
      oploc_t *loc;
      int loc_entries;
      const uint8 *example_bits;

      loc = (oploc_t *) alloca (size * 8 * sizeof loc[0]); /* 1 per bit. */
      example_bits = compute_bits (size, loc, &loc_entries, legal_p);
      if (example_bits == NULL)
	problem_p = TRUE;
      else if (output_p)
	{
	  output_bits (loc, loc_entries, example_bits, size, postamble,
		       indent);
	}
    }

  /* problem_p may have changed, so check it again. */
  if (problem_p)
    {
      char new_legal_p[NUM_SAMPLES];
      int op;

      /* For some reason we are unable to find a rule that generates all
       * of the samples marked "legal".  Therefore we need to figure out
       * what is causing the special cases.  There are three reasons for
       * special cases, which may occur in conjunction with one another:
       *
       * (1)  An immediate constant may be in the range -128 to 127.
       *      Most of the time this value can be stored as a sign-extended
       *      8-bit constant.
       * (2)  An immediate constant may be the value 1.
       *      Shift and rotate instructions special case shifting by one bit.
       * (3)  An opcode uses %al/%ax/%eax in some way.
       *      Many opcodes special case this register and have a compact
       *      special version of the opcode that implicitly references it.
       */

      for (op = 0; op < NUM_OPERANDS; op++)
	{
	  if (TEMPLATE.operand[op].type == CONSTANT
	      && special_operand_p (op, 1, 1, legal_p))
	    {
	      if (output_p)
		{
		  spaces (indent);
		  printf ("if (%s == 1) {\n", TEMPLATE.operand_name[op]);
		}
	      filter (legal_p, new_legal_p, op, 1, 1, TRUE);
	      if (!generate_code (new_legal_p, postamble, output_p,
				  indent + 2))
		return FALSE;
	      if (output_p)
		{
		  spaces (indent);
		  puts ("} else {");
		}
	      filter (legal_p, new_legal_p, op, 1, 1, FALSE);
	      if (!generate_code (new_legal_p, postamble, output_p,
				  indent + 2))
		return FALSE;
	      if (output_p)
		{
		  spaces (indent);
		  puts ("}");
		}
	      break;
	    }
	  else if (TEMPLATE.operand[op].type == CONSTANT
	      && special_operand_p (op, 0, 0, legal_p))
	    {
	      if (output_p)
		{
		  spaces (indent);
		  printf ("if (%s == 0) {\n", TEMPLATE.operand_name[op]);
		}
	      filter (legal_p, new_legal_p, op, 0, 0, TRUE);
	      if (!generate_code (new_legal_p, postamble, output_p,
				  indent + 2))
		return FALSE;
	      if (output_p)
		{
		  spaces (indent);
		  puts ("} else {");
		}
	      filter (legal_p, new_legal_p, op, 0, 0, FALSE);
	      if (!generate_code (new_legal_p, postamble, output_p,
				  indent + 2))
		return FALSE;
	      if (output_p)
		{
		  spaces (indent);
		  puts ("}");
		}
	      break;
	    }
	  else if (TEMPLATE.operand[op].type == CONSTANT
		   && TEMPLATE.operand[op].size != SIZE_8
		   && special_operand_p (op, -128, 127, legal_p))
	    {
	      if (output_p)
		{
		  spaces (indent);
		  printf ("if ((unsigned long)(%s + 128) < 256) {\n",
			  TEMPLATE.operand_name[op]);
		}
	      filter (legal_p, new_legal_p, op, -128, 127, TRUE);
	      if (!generate_code (new_legal_p, postamble, output_p,
				  indent + 2))
		return FALSE;
	      if (output_p)
		{
		  spaces (indent);
		  puts ("} else {");
		}
	      filter (legal_p, new_legal_p, op, -128, 127, FALSE);
	      if (!generate_code (new_legal_p, postamble, output_p,
				  indent + 2))
		return FALSE;
	      if (output_p)
		{
		  spaces (indent);
		  puts ("}");
		}
	      break;
	    }
	  else if (TEMPLATE.operand[op].type == REGISTER
		   && special_operand_p (op, 0, 0, legal_p))
	    {
	      if (output_p)
		{
		  spaces (indent);
		  printf ("if (%s == 0) {\n", TEMPLATE.operand_name[op]);
		}
	      filter (legal_p, new_legal_p, op, 0, 0, TRUE);
	      if (!generate_code (new_legal_p, postamble, output_p,
				  indent + 2))
		return FALSE;
	      if (output_p)
		{
		  spaces (indent);
		  puts ("} else {");
		}
	      filter (legal_p, new_legal_p, op, 0, 0, FALSE);
	      if (!generate_code (new_legal_p, postamble, output_p,
				  indent + 2))
		return FALSE;
	      if (output_p)
		{
		  spaces (indent);
		  puts ("}\n");
		}
	      break;
	    }
	}

      if (op >= NUM_OPERANDS)
	{
	  /* Wow, nothing worked.  This happens sometimes when invoking
	   * one special case disables another.  The workaround for this
	   * is to actually try special casing (even though it didn't look
	   * necessary) and see if that works.
	   */
	  for (op = 0; op < NUM_OPERANDS; op++)
	    {
	      if (TEMPLATE.operand[op].type == REGISTER
		  && operand_variety_p (op, 0, 0, legal_p))
		{
		  boolean_t good;
		  
		  /* Test out and see if special casing register %al/%ax/%eax
		   * solves the problem.
		   */
		  filter (legal_p, new_legal_p, op, 0, 0, TRUE);
		  good = generate_code (new_legal_p, postamble, FALSE, indent);
		  filter (legal_p, new_legal_p, op, 0, 0, FALSE);
		  good &= generate_code (new_legal_p, postamble, FALSE,
					 indent);
		  
		  /* If it does, then actually go ahead and crank it out. */
		  if (good)
		    {
		      filter (legal_p, new_legal_p, op, 0, 0, TRUE);
		      if (output_p)
			{
			  spaces (indent);
			  printf ("if (%s == 0) {\n",
				  TEMPLATE.operand_name[op]);
			}
		      if (!generate_code (new_legal_p, postamble, output_p,
					  indent + 2))
			return FALSE;
		      if (output_p)
			{
			  spaces (indent);
			  puts ("} else {");
			}
		      filter (legal_p, new_legal_p, op, 0, 0, FALSE);
		      if (!generate_code (new_legal_p, postamble, output_p,
					  indent + 2))
			return FALSE;
		      if (output_p)
			{
			  spaces (indent);
			  puts ("}");
			}
		      break;
		    }
		}
	      else if (TEMPLATE.operand[op].type == CONSTANT
		       && operand_variety_p (op, -128, 127, legal_p))
		{
		  boolean_t good;
		  
		  /* Test out and see if special casing register %al/%ax/%eax
		   * solves the problem.
		   */
		  filter (legal_p, new_legal_p, op, -128, 127, TRUE);
		  good = generate_code (new_legal_p, postamble, FALSE,
					indent);
		  filter (legal_p, new_legal_p, op, -128, 127, FALSE);
		  good &= generate_code (new_legal_p, postamble, FALSE,
					 indent);
		  
		  /* If it does, then actually go ahead and crank it out. */
		  if (good)
		    {
		      filter (legal_p, new_legal_p, op, -128, 127, TRUE);
		      if (output_p)
			{
			  spaces (indent);
			  printf ("if ((unsigned long)(%s + 128) < 256) {\n",
				  TEMPLATE.operand_name[op]);
			}
		      if (!generate_code (new_legal_p, postamble, output_p,
					  indent + 2))
			return FALSE;
		      if (output_p)
			{
			  spaces (indent);
			  puts ("} else {");
			}
		      filter (legal_p, new_legal_p, op, -128, 127, FALSE);
		      if (!generate_code (new_legal_p, postamble, output_p,
					  indent + 2))
			return FALSE;
		      if (output_p)
			{
			  spaces (indent);
			  puts ("}");
			}
		      break;
		    }
		}
	    }

	  /* If even that failed, complain. */
	  if (op >= NUM_OPERANDS)
	    {
	      if (output_p)
		fprintf (stderr, "Unable to derive rules for %s!\n",
			 TEMPLATE.macro_name);
	      return FALSE;
	    }
	}
    }

  return TRUE;
}


static void
output_bits (const oploc_t *loc, int loc_entries, const uint8 *bits,
	     int size, const char *extra, int indent)
{
  int ix, bit, blen, len, offset;
  char b[1024], *n;

  /* Fill in a string with all of the literal bits and operands characters. */
  blen = size * 8;
  n = b;
  for (bit = 0; bit < blen; bit++)
    {
      int op;
      
      for (op = 0; op < loc_entries; op++)
	if (loc[op].offset <= bit
	    && (loc[op].offset + loc[op].length > bit))
	  {
	    if (loc[op].operand_num >= NUM_OPERANDS)
	      abort ();
	    else
	      *n++ = 'a' + op;
	    break;
	  }
      
      /* If we found no operand that overlaps these bits,
       * then output the literal bit we know about.
       */
      if (op >= loc_entries)
	*n++ = ((bits[bit / 8] & (1 << (bit & 7))) ? '1' : '0');
    }
  *n = '\0';

  offset = 0;
  for (ix = 0; ix < blen; ix += len)
    {
      int try;

#ifdef QUADALIGN
      len = MIN (32, HOST_CODE_T_BITS);
#else  /* !QUADALIGN */
      len = 32;
#endif /* !QUADALIGN */
      len = MIN (blen - ix, len);

      /* Make sure it's a power of 2. */
      if (len & (len - 1))
	{
	  if (len > 16)
	    len = 16;
	  else if (len > 8)
	    len = 8;
	}

      /* Choose the largest sequence we can that doesn't split up
       * an operand field.
       */
      try = len;
      while (try >= 8
	     && b[ix + try - 1] == b[ix + try]
	     && b[ix + try] != '0' && b[ix + try] != '1')
	try /= 2;

      /* If we are forced to split an operand anyway, might as well
       * crank out as many bits as possible.  Therefore we'll only
       * override len if we found a size without a split.
       */
      if (b[ix + try - 1] != b[ix + try]
	  || b[ix + try] == '0' || b[ix + try] == '1')
	len = try;

      assert (len % 8 == 0);

      /* Crank out the code for this piece. */
      code_for_bits (b, ix, len, indent, offset, loc);
      offset += len / 8;
    }
  
  if (offset != 0)
    {
      spaces (indent);
      if (offset % sizeof (host_code_t) == 0)
	printf ("code += %d;\n", (int) (offset / sizeof (host_code_t)));
      else
	printf ("code = (host_code_t *)((char *)code + %d);\n", offset);
    }
}


/* Given a string like "10000101aaaa10100" and the character 'a', this
 * returns the index into the string where the first 'a' is found, and
 * the number of contiguous 'a's at that index.  Returns TRUE if such
 * a sequence is found, else FALSE.
 */
static int
find_field (const char *string, int c, int *first, int *length)
{
  const char *s;
  int l;

  for (s = string; *s != '\0' && *s != c; s++)
    ;
  if (*s == '\0')
    return FALSE;
  *first = s - string;
  for (l = 1; s[l] == c; l++)
    ;
  *length = l;

  return TRUE;
}


/* This generates a sequence of cstmts to create the code for a given
 * bit string and set of operands.
 */
static void
code_for_bits (const char *b, int start, int length, int indent,
	       int offset, const oploc_t *loc)
{
  uint32 literal_bits;
  int n, end;
  uint32 length_mask;
  int first_done_p;

  /* Sanity check. */
  assert (length >= HOST_CODE_T_BITS && length % HOST_CODE_T_BITS == 0
	  && length > 0 && length <= 32);

  /* Construct a mask that indicates which bits will actually be output. */
  length_mask = 0xFFFFFFFF >> (32 - length);

  /* Compute the literal bits (1's and 0's) and put them in a mask.
   * All non-literal bits will be treated as zeros.
   */
  for (n = 0, literal_bits = 0; n < length; n++)
    {
#ifndef LITTLEENDIAN
      literal_bits = (literal_bits << 1) | (b[start + n] == '1');
#else  /* LITTLEENDIAN */
      literal_bits |= (uint32) (b[start + n] == '1') << n;
#endif /* LITTLEENDIAN */
    }

  /* Output the assignment operator. */
  spaces (indent);
  if (length == HOST_CODE_T_BITS)
    {
      if (offset % sizeof (host_code_t) == 0)
	printf ("code[%d] =", (int) (offset / sizeof (host_code_t)));
      else
	printf ("*(host_code_t *)(%scode + %d) =",
		(sizeof (host_code_t) == 1) ? "" : "(char *)", offset);
    }
  else
    {
      if (offset == 0)
	printf ("*(uint%d *)code =", length);
      else
	printf ("*(uint%d *)(%scode + %d) =", length,
		(sizeof (host_code_t) == 1) ? "" : "(char *)", offset);
    }

  /* Output the base constant. */
  if (literal_bits != 0)
    {
      printf (" 0x%lX", (unsigned long) literal_bits);
      first_done_p = TRUE;
    }
  else
    {
      putchar (' ');
      first_done_p = FALSE;
    }

  /* Loop over all of the operands. */
  end = start + length;
  for (n = 0; n < 26; n++)
    {
      int field_start, field_length, field_end;

      /* See if this operand overlaps the bits we are cranking out. */
      if (find_field (b, n + 'a', &field_start, &field_length)
	  && field_start + field_length > start
	  && field_start < start + length)
	{
	  uint32 mask;
	  int shift;

	  field_end = field_start + field_length;
	  if (first_done_p)
	    fputs (" | ", stdout);

	  /* Compute a mask for the field of interest. */
	  mask = 0xFFFFFFFF >> (32 - field_length);

#ifndef LITTLEENDIAN
	  shift = end - field_end;
#else  /* LITTLEENDIAN */
	  shift = field_start - start;
#endif /* LITTLEENDIAN */

	  if (shift != 0)
	    fputs ("(", stdout);

	  /* Mask out all but the relevant bits if necessary.  There's
	   * no need to mask if we are shifting the value so far that
	   * the masked bits are shifted out of the resulting value
	   * anyway.
	   */
	  if ((shift >= 0 && ((~mask << shift) & length_mask) != 0)
	      || (shift < 0 && ((~mask >> -shift) & length_mask) != 0))
	    {
	      assert (TEMPLATE.operand_name[loc[n].operand_num] != NULL);
	      printf ("(%s & 0x%lX)",
		      TEMPLATE.operand_name[loc[n].operand_num],
		      (unsigned long) mask);
	    }
	  else
	    {
	      fputs (TEMPLATE.operand_name[loc[n].operand_num], stdout);
	    }

	  /* Shift the bits to where they belong (computed above). */
	  if (shift != 0)
	    {
	      if (shift > 0)
		printf (" << %d)", shift);
	      else
		printf (" >> %d)", -shift);
	    }
	  
	  first_done_p = TRUE;
	}
    }

  /* If all bits are zero, actually output a 0. */
  if (literal_bits == 0 && !first_done_p)
    fputs ("0", stdout);

  puts (";");
}


static const uint8 *
compute_bits (int size, oploc_t *loc, int *loc_entries,
	      const char legal_p[NUM_SAMPLES])
{
  uint8 *fixed;
  const uint8 *bits;
  int s, bit, op, num_bits, entries;

  /* Set up defaults. */
  *loc_entries = entries = 0;

  /* Allocate a bit array for which bits change as the operands change. */
  fixed = (uint8 *) alloca (size);
  memset (fixed, ~0, size);

  /* Identify which bits change and which remain fixed. */
  bits = NULL;
  for (s = 0; s < NUM_SAMPLES; s++)
    if (legal_p[s])
      {
	int byte;
	if (bits == NULL)
	  bits = sample[s].start;
	else
	  for (byte = 0; byte < size; byte++)
	    fixed[byte] &= ~(bits[byte] ^ sample[s].start[byte]);
      }

  /* If we found nothing of this size, return NULL. */
  if (bits == NULL)
    return NULL;

  /* Now we must fill in the changed bits with the operands we've got. */
  for (bit = 0, num_bits = size * 8; bit < num_bits; )
    {
      int most_consecutive, best_op;

      /* Find the next non-fixed bit. */
      for (; bit < num_bits && (fixed[bit / 8] & (1 << (bit & 7))); bit++)
	;
      /* Find an operand whose values account for the most consecutive
       * bits starting at this offset.
       */
      if (bit >= num_bits)
	break;
	  
      most_consecutive = best_op = 0;
      for (op = 0; op < NUM_OPERANDS; op++)
	{
	  int consec = 40;
	  for (s = 0; consec > 0 && s < NUM_SAMPLES; s++)
	    {
	      int c;

	      if (!legal_p[s])
		continue;

	      for (c = 0; c < 32 && bit + c < num_bits; c++)
		if ((((sample[s].start[(bit + c) / 8] >> ((bit + c) & 7)) & 1)
		     != ((value[s][op] >> c) & 1))
		    || (fixed[(bit + c) / 8] & (1 << ((bit + c) & 7))))
		  break;
	      if (c < consec)
		consec = c;
	    }

	  if (consec <= 32 && consec > most_consecutive)
	    {
	      most_consecutive = consec;
	      best_op = op;
	    }
	}

      if (most_consecutive == 0)
	{
#if 0
	  fprintf (stderr, "Unable to account for bit %d of %s "
		   "(size == %d)!\n",
		   bit, TEMPLATE.macro_name, size);
	  most_consecutive = 1;  /* Keep going. */
	  best_op = 1000;
#else
	  return NULL;
#endif
	}

      assert (best_op < NUM_OPERANDS);

      loc[entries].offset = bit;
      loc[entries].length = most_consecutive;
      loc[entries].operand_num = best_op;
      entries++;
      bit += most_consecutive;
    }

  *loc_entries = entries;
  return bits;
}
