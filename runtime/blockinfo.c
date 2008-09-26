#include "syn68k_private.h"
#include "block.h"
#include "mapping.h"
#include "rangetree.h"
#include "blockinfo.h"
#include "alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static void determine_next_block_addresses (const uint16 *code,
					    TempBlockInfo *temp,
					    const OpcodeMappingInfo *map);


/* This function takes a block and a pointer to some m68k code and computes
 * the length of the block and various cc bit information about the block.
 * The info is stored in the block struct.
 */
void
compute_block_info (Block *b, const uint16 *code, TempBlockInfo *temp)
{
  const uint16 *start_code = code, *old_code;
  int clobbered = 0, may_set = 0, may_not_set = ALL_CCS, needed = 0;
  int next_array_size;
  const OpcodeMappingInfo *map = NULL;

  /* Initialize the next offset array.  This lets us step through this
   * code forwards when we actually get around to compiling it.
   */
  temp->num_68k_instrs = 0;
  next_array_size = 16;
  temp->next_instr_offset = (int8 *) xmalloc (next_array_size * sizeof (int8));

  /* Loop over all instructions in the block and determine information
   * about how this block deals with CC bits.
   */
  old_code = code;
  do
    {
      int insn_size;
      unsigned m68k_op;

      m68k_op = READUW (US_TO_SYN68K (code));
      map = &opcode_map_info[opcode_map_index[m68k_op]];

#if 0
      if (opcode_map_index[m68k_op] == 0)
	{
	  fprintf (stderr, "m68kop 0x%04X unimplemented!\n", m68k_op);
	  abort ();
	}
#endif

      insn_size = instruction_size (code, map);
      if (insn_size <= 0)
	{
	  map = &opcode_map_info[opcode_map_index[0x4AFC]];  /* illegal */
	  insn_size = 1;
	}

      /* Update cc bit info for this block. */
      clobbered |= (map->cc_may_set
		    & ~(map->cc_may_not_set | map->cc_needed | needed));
      needed |= map->cc_needed & may_not_set;
      may_set |= map->cc_may_set;
      may_not_set &= map->cc_may_not_set;

      /* Grow next instruction offset array iff necessary.  We'll
       * leave one extra element for after this loop ends.
       */
      if (temp->num_68k_instrs >= next_array_size - 1)
	{
	  next_array_size *= 2;
	  temp->next_instr_offset = (int8 *) xrealloc (temp->next_instr_offset,
						       next_array_size
						       * sizeof (int8));
	}

      /* Remember offset to next instruction. */
      temp->next_instr_offset[temp->num_68k_instrs++] = insn_size;

      /* Move on to the next instruction. */
      old_code = code;
      code += insn_size;
    }
  while (!map->ends_block);

  /* Terminate the array with a 0 offset. */
  temp->next_instr_offset[temp->num_68k_instrs] = 0;

  /* Figure out where this block goes (if possible). */
  determine_next_block_addresses (old_code, temp, map);

  /* Record the block information we've computed. */
  b->cc_clobbered       = clobbered;
  b->cc_may_not_set     = may_not_set;
  b->cc_needed          = needed;
  b->m68k_start_address = US_TO_SYN68K (start_code);
  b->m68k_code_length   = (code - start_code) * sizeof (uint16);
}


static void
determine_next_block_addresses (const uint16 *code, TempBlockInfo *temp,
				const OpcodeMappingInfo *map)
{
  uint16 m68kop = READUW (US_TO_SYN68K (code));
  BOOL is_bsr = ((m68kop >> 8) == 0x61);
  BOOL is_fixed_jsr = ((m68kop & 0xFFFE) == 0x4EB8   /* jsr abs{w,l}? */
		       || m68kop == 0x4EBA);         /* jsr pc@d16?   */

#ifdef DEBUG
  temp->child[0] = temp->child[1] = US_TO_SYN68K (NULL);
#endif

  /* First see if we can even tell where the child block is. */
  if (map->next_block_dynamic && !is_bsr && !is_fixed_jsr)
    {
      temp->num_child_blocks = 0;
      return;
    }

  /* Is it a Bcc? */
  if ((m68kop >> 12) == 6)
    {
      uint32 t1, t2;

      t1 = (uint32) (code + 1);
      /* Compute branch target. */
      if ((m68kop & 0xFF) == 0)
	{
	  t1 += READSW (US_TO_SYN68K (code + 1));
	  t2 = (uint32) (code + 2);
	}
      else if ((m68kop & 0xFF) == 0xFF)
	{
	  t1 += READSL (US_TO_SYN68K (code + 1));
	  t2 = (uint32) (code + 3);
	}
      else
	{
	  t1 += ((int8 *)code)[1];
	  t2 = (uint32) (code + 1);
	}

      /* Is it a bsr or bra?  If so, only one destination address.  In the
       * case of bsr, we compute the target address but officially pretend
       * we don't know where it's going.
       */
      if ((m68kop & 0xFE00) == 0x6000)
	{
	  temp->child[0] = t1;
	  temp->num_child_blocks = !is_bsr;
	  return;
	}

      temp->child[0] = t2;
      temp->child[1] = t1;
      temp->num_child_blocks = 2;
      return;
    }

  /* Is it a dbcc? */
  if ((m68kop >> 12) == 5)
    {
      temp->child[0] = (uint32) (code + 2);

      if ((m68kop >> 8) == 0x50)  /* dbt? */
	{
	  temp->num_child_blocks = 1;
	  return;
	}

      temp->child[1] = ((int32 ) (code + 1)
			+ READSW (US_TO_SYN68K (code + 1)));
      temp->num_child_blocks = 2;
      return;
    }

  switch (m68kop)
    {
    case 0x4EF8: /* Is it a jmp _abs.w? */
      temp->child[0] = 
	(uint32) SYN68K_TO_US (READSW (US_TO_SYN68K (code + 1)));
      temp->num_child_blocks = 1;
      return;
    case 0x4EB8: /* Is it a jsr _abs.w? */
      temp->child[0] =
	(uint32) SYN68K_TO_US (READSW (US_TO_SYN68K (code + 1)));
      temp->num_child_blocks = 0;  /* Pretend we don't know the dest. */
      return;
    case 0x4EF9: /* Is it a jmp _abs.l? */
      temp->child[0] = 
	(uint32) SYN68K_TO_US (READUL (US_TO_SYN68K (code + 1)));
      temp->num_child_blocks = 1;
      return;
    case 0x4EB9: /* Is it a jsr _abs.l? */
      temp->child[0] = 
	(uint32) SYN68K_TO_US (READUL (US_TO_SYN68K (code + 1)));
      temp->num_child_blocks = 0;
      return;
    case 0x4EBA: /* Is it a pc-relative jsr? */
      temp->child[0] = (uint32) ((READSW (US_TO_SYN68K (code + 1))
				  + code + 1));
      temp->num_child_blocks = 0;
      return;
    }

  /* Strange, unknown block ender.  Probably something capable of trapping.
   * Assume that the subsequent instruction is the target.
   */
  temp->child[0] = (uint32) (code + map->instruction_words);
  temp->num_child_blocks = 1;
}


int
amode_size (int amode, const uint16 *code, int ref_size)
{
  uint16 c = READUW (US_TO_SYN68K (code));

  switch (amode) {
  case 0x28: case 0x29: case 0x2A: case 0x2B:   /* Addressing mode 5 */
  case 0x2C: case 0x2D: case 0x2E: case 0x2F:
    return 1;
  case 0x38:      /* 111/000 (xxx).W */
    return 1;
  case 0x39:      /* 111/001 (xxx).L */
    return 2;
  case 0x3C:      /* 111/100 #<data> */
    return ref_size == 4 ? 2 : 1;   /* Bytes still take 1 full word. */
  case 0x30: case 0x31: case 0x32: case 0x33:  /* Addressing mode 6. */
  case 0x34: case 0x35: case 0x36: case 0x37:
  case 0x3B:   /* Addressing mode 111/011 */
    if (!(c & 0x100))   /* 8 bit displacement. */
      return 1;
    else if ((c & 0xF) == 0x0)
      return ((c >> 4) & 3);
    return ((c >> 4) & 3) + (c & 3) - 1;
  case 0x3A:      /* 111/010 (d16,PC) */
    return 1;
  default:
    return 0;
  }
}


/* Given a pointer to an m68k opcode, returns the total number of words
 * that instruction occupies, including operands and memory taken by
 * complex addressing modes.
 */
int
instruction_size (const uint16 *code, const OpcodeMappingInfo *map)
{
  int size;
  int m68kop = READUW (US_TO_SYN68K (code));
  
  /* See if we have a conditional branch format operand instruction. */
  if ((m68kop >> 12) == 6)
    {
      if ((m68kop & 0xFF) == 0)
	size = 2;
      else if ((m68kop & 0xFF) == 0xFF)
	size = 3;
      else
	size = 1;
    }
  else
    {
      size = map->instruction_words;
      /* If we have an addressing mode, compute how many words it takes. */
      if (map->amode_size != 0)
	size += amode_size (m68kop & 0x3F, code + size, map->amode_size);
      
      /* If we have a rev. addressing mode, compute how many words it takes. */
      if (map->reversed_amode_size != 0) /* Is there a rev. addressing mode? */
	size += amode_size (((m68kop >> 9) & 0x7) | ((m68kop >> 3) & 0x38),
			    code + size, map->reversed_amode_size);
    }

  return size;
}
