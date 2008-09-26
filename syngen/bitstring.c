#include "bitstring.h"
#include "error.h"
#include <string.h>


/* This takes a 16 bit number and a bitset (described by a lisp-style
 * expression) and returns TRUE iff the number is a member of that set.
 * A bitset would be described by an expression like:
 *
 * (union "00001111xx0011xx" "mmmm00111011xxx1"
 *    (intersect "1001xxxx0011nnn1" (not "0110001111000101")))
 *
 * The number will be in the set if the usual union, intersect and not
 * properties are met.  A number matches a simple string if each literal
 * '0' and '1' in the string matches a 0 or 1 bit in the number, respectively.
 * All non-'0' and non-'1' characters in the string are assumed to be
 * wildcards; anything in the corresponding bit position in the number will
 * be considered a match.
 */

BOOL
is_member_of_set (unsigned short n, List *set)
{
  List *tmp;
  char buf[512];

  /* If we are looking for a match against a quoted string, efficiently
   * determine if all of the literal bits match.  This could be vastly
   * faster if the quoted string were replaced with a mask of bits that must
   * be valid and a set of values for those bits, but speed just doesn't
   * matter here, and it's handy to deal with the original string (which
   * can contain other information via the specific choice of non-'0'
   * and non-'1' characters).
   */
  if (set->token.type == TOK_QUOTED_STRING)
    {
      const char *s = set->token.u.string;
      int mask;


      for (mask = 1 << 15; mask != 0; s++, mask >>= 1)
	if ((*s == '0' && (n & mask)) || (*s == '1' && !(n & mask)))
	  {
	    return FALSE;
	  }

      return TRUE;
    }

  if (set->token.type != TOK_LIST)
    {
      /* This error will get printed a zillion times, but who cares. */
      parse_error (set, "Malformed bit set; expecting quoted string of bits, "
		   "union, intersect, or not, found %s\n",
		   unparse_token (&set->token, buf));
      return FALSE;
    }

  /* Not a member of the empty list. */
  if (set->car == NULL)
    return FALSE;

  /* See what type of operation we are performing, and do the appropriate
   * thing.
   */
  switch (set->car->token.type) {
  case TOK_UNION:
    if (list_length (set->car) < 2)
      parse_error (set->car, "Missing arguments to union.\n");
    for (tmp = CDAR (set); tmp != NULL; tmp = tmp->cdr)
      if (is_member_of_set (n, tmp))
	return TRUE;
    return FALSE;
  case TOK_INTERSECT:
    if (list_length (set->car) < 2)
      parse_error (set->car, "Missing arguments to intersect.\n");
    for (tmp = CDAR (set); tmp != NULL; tmp = tmp->cdr)
      if (!is_member_of_set (n, tmp))
	return FALSE;
    return TRUE;
  case TOK_NOT:
    if (list_length (set->car) != 2)
      {
	parse_error (set->car, "Must have exactly one argument to not.\n");
	return FALSE;
      }
    return !is_member_of_set (n, CDAR (set));
  default:
    /* This error will get printed a zillion times, but who cares. */
    parse_error (set, "Malformed bit set; expecting quoted string of bits, "
		 "union, intersect, or not.\n");
    return FALSE;
  }
}


BOOL
empty_set (List *set, int literal_bits_mask, int literal_bits)
{
  int i;

  for (i = 0; i < 65536; i++)
    if ((i & literal_bits_mask) == literal_bits && is_member_of_set (i, set))
      return FALSE;
  return TRUE;
}


/* Given a bit pattern like "0010ddd0000ddd001xxx001ppp" and a field number,
 * returns by reference the low index and length of the nth field, where
 * the first field (in this case the first string of 3 d's) is # 1.  Returns
 * TRUE if such a field exists, else FALSE.
 */
BOOL
pattern_range (const char *pattern, int which, PatternRange *range)
{
  const char *p = pattern, *p2;

  do
    {
      if (*p == '\0')
	return FALSE;

      /* Skip to first sequence. */
      for (; *p != '\0' && (*p == '0' || *p == '1'); p++)
	;

      /* Remember where the pattern started. */
      range->index = p - pattern;
      p2 = p;

      /* Find the end of the sequence. */
      for (; *p != '\0' && *p == *p2; p++)
	;

      /* Remember the length of the pattern. */
      range->length = p - p2;
    }
  while (--which > 0);

  return TRUE;
}


int
field_with_index (const char *opcode_bits, int index)
{
  PatternRange range;
  int i;

  for (i = 1; pattern_range (opcode_bits, i, &range); i++)
    {
      if (range.index == index)
	return i;
    }
  return 0;
}


BOOL
field_expanded (int field_number, const char *opcode_bits,
		const char *bits_to_expand)
{
  PatternRange range;
  int i;

  /* If there is no such field, return FALSE. */
  if (!pattern_range (opcode_bits, field_number, &range))
    return FALSE;
  
  /* If the field falls outside the first 16 bits, it can't be expanded. */
  if (range.index >= 16)
    return FALSE;

  for (i = range.index; i < range.index + range.length; i++)
    if (bits_to_expand[i] == '-' && opcode_bits[i] != '0'
	&& opcode_bits[i] != '1')
      return FALSE;

  return TRUE;
}


int
num_fields (const char *opcode_bits)
{
  PatternRange range;
  int i;

  for (i = 1; pattern_range (opcode_bits, i, &range); i++)
    ;
  return i - 1;
}


void
make_unique_field_of_width (const char *opcode_bits, char *where, int width)
{
  int i, x;
  for (x = 'A'; strchr (opcode_bits, x) != NULL; x++)
    ;
  for (i = 0; i < width; i++)
    *where++ = x;
  *where = '\0';
}


/* Handy utility function. */
void
print_16_bits (FILE *stream, unsigned short n)
{
  int shift;
  for (shift = 15; shift >= 0; shift--)
    putc ('0' + ((n >> shift) & 1), stream);
}
