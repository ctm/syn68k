#ifndef _bitset_h_
#define _bitset_h_

#include "common.h"
#include "list.h"

typedef struct {
  int index, length;
} PatternRange;

extern BOOL is_member_of_set (unsigned short n, List *set);
extern void print_16_bits (FILE *stream, unsigned short n);
extern BOOL pattern_range (const char *pattern, int which,
			   PatternRange *range);
extern int num_fields (const char *opcode_bits);
extern void make_unique_field_of_width (const char *opcode_bits, char *where,
					int width);
extern int field_with_index (const char *opcode_bits, int index);
extern BOOL field_expanded (int field_number, const char *opcode_bits,
			    const char *bits_to_expand);
extern BOOL empty_set (List *set, int literal_bits_mask, int literal_bits);

#endif  /* Not _bitset_h_ */
