#ifndef _blockinfo_h_
#define _blockinfo_h_

#include "block.h"

typedef struct {
  uint32 child[2];        /* m68k addresses of m68k code following this blk. */
  int16 num_child_blocks; /* # of child addrs we can know at translate time. */
  uint16 num_68k_instrs;
  int8 *next_instr_offset; /* word offset to next instr; 0 iff last instr.  */
} TempBlockInfo;

extern void compute_block_info (Block *b, const uint16 *code,
				TempBlockInfo *temp);
extern int amode_size (int amode, const uint16 *code, int ref_size);
extern int instruction_size (const uint16 *code, const OpcodeMappingInfo *map);

#endif  /* Not _blockinfo_h_ */
