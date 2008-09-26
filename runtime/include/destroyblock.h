#ifndef _destroyblock_h_
#define _destroyblock_h_

#include "block.h"

extern unsigned long destroy_block (Block *b);
extern unsigned long destroy_blocks (syn68k_addr_t low_m68k_address, uint32 num_bytes);
extern unsigned long destroy_any_block (void);

#endif  /* Not _destroyblock_h_ */
