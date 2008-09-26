#ifndef _diagnostics_h_
#define _diagnostics_h_

#include <stdio.h>
#include "syn68k_private.h"
#include "block.h"

extern void print_cc_bits (FILE *stream, int bits);
extern void hexdump (const uint16 *addr, int num_words);
extern void dump_cpu_state (void);
extern Block *which_block (uint16 *code);

#endif  /* Not _diagnostics_h_ */
