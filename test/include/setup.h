#ifndef _setup_h_
#define _setup_h_

#include "syn68k_public.h"

#define MEM_SIZE 2048

extern uint8 *mem;

extern void randomize_mem (void);
extern void randomize_regs (int low_reg, int high_reg, uint32 low_val,
			    uint32 high_val, uint32 bits_to_mask);
extern void fully_randomize_regs (void);
extern int32 randint (int32 low, int32 high);
extern int32 randnum ();

#endif  /* Not _setup_h_ */
