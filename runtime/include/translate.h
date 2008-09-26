#ifndef _translate_h
#define _translate_h_

#include "block.h"

extern void (*call_while_busy_func)(int);

typedef struct {
  BOOL valid;
  BOOL reversed;
  const uint16 *m68koperand;
  int amode;
  int size;
} AmodeFetchInfo;

extern int generate_block (Block *parent, uint32 m68k_address, Block **new
/* #ifdef GENERATE_NATIVE_CODE */
			   , BOOL try_native_p
/* #endif */ /* GENERATE_NATIVE_CODE */
			   );
extern Block *make_artificial_block (Block *parent, syn68k_addr_t m68k_address,
				     int extra_words, uint16 **extra_start);

#ifdef GENERATE_NATIVE_CODE
extern int native_code_p;
#endif  /* GENERATE_NATIVE_CODE */

extern int emulation_depth;

#endif  /* Not _translate_h_ */
