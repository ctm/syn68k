#include "syn68k_public.h"
#include "driver.h"
#include "setup.h"
#include <stdio.h>


#define TEST(f, cc_mask, size, changes_memory, max_calls) void f (uint16 *code)
#define MEM (US_TO_SYN68K (&mem[0]))
#define MEMEND (US_TO_SYN68K (&mem[MEM_SIZE - 1]))
#define MEMMID (US_TO_SYN68K (&mem[(MEM_SIZE) / 2]))

#define GNUC_LOSES_WITH_EXPONENTIAL_GROWTH
#ifdef GNUC_LOSES_WITH_EXPONENTIAL_GROWTH
static inline int
MASK (const char *s, int c)
{
  int i, ret;
  
  for (i = 0, ret = 0; i < 16; i++)
    ret |= ((s[i] == c) << (15 - i));
  return ret;
}
#else
#define MASK(s,c) ((  (s[0 ] == (c)) << 15) | ((s[1 ] == (c)) << 14) \
		   | ((s[2 ] == (c)) << 13) | ((s[3 ] == (c)) << 12) \
		   | ((s[4 ] == (c)) << 11) | ((s[5 ] == (c)) << 10) \
		   | ((s[6 ] == (c)) <<  9) | ((s[7 ] == (c)) <<  8) \
		   | ((s[8 ] == (c)) <<  7) | ((s[9 ] == (c)) <<  6) \
		   | ((s[10] == (c)) <<  5) | ((s[11] == (c)) <<  4) \
		   | ((s[12] == (c)) <<  3) | ((s[13] == (c)) <<  2) \
		   | ((s[14] == (c)) <<  1) | ((s[15] == (c))))
#endif
#define B(s) MASK(s, '1')
#define R(s) (B(s) | (MASK(s, 'r') & randint (0, 65535)))

#define NOP_OPCODE 0x4E71

#define RANDOM_SIZE() (randint (0, 2) << 6)

/* The WRITE4 macro does not write out values in big-endian!  Rather, it
 * writes out the words in big endian format and leaves the bytes within
 * each word in whatever format.  This is because we always byte swap
 * words later, if appropriate.
 */
#define WRITE4(c,n)    \
{                      \
  uint32 tmp = (n);    \
  code[c] = tmp >> 16; \
  code[(c) + 1] = tmp; \
}



TEST (unpk_reg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1000rrr110000rrr");
  code[1] = randnum ();
}


TEST (unpk_mem, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 0);
  code[0] = R ("1000rrr110001rrr");
  code[1] = randnum ();
}


TEST (unlk, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  uint8 *p;

  for (p = mem; p < mem + MEM_SIZE; )
    {
      uint32 addr = randint (MEM + 50, MEMEND - 50) & ~1;
      *p++ = addr >> 24;
      *p++ = addr >> 16;
      *p++ = addr >> 8;
      *p++ = addr;
    }

  /* NOTE: "unlk a7" wedges my 68040 NeXT, so I am skipping that test here.
   * "unlk a7" works on a 68030 NeXT, so it may be worth modifying this
   * code to test that case when we run it on a non-68040 based machine.
   */

  /* WARNING!  Assumes that mem is aligned % 4 */
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 3);
  code[0] = R ("0100111001011000") | randint (0, 6);
}

TEST (movewl_ind_ind, ALL_CCS, 1, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = (R ("001rrrr000000rrr")   /* Randomly either word or long. */
	     | (randint (2, 4) << 6)    /* Random amode [2, 4] */
	     | (randint (2, 4) << 3));  /* Random amode [2, 4] */
}


TEST (divsl_ll_reg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  int divisor_reg = randint (0, 7);
  do { EM_DREG (divisor_reg) = randnum ();} while (EM_DREG (divisor_reg) == 0);
  code[0] = B ("0100110001000000") | divisor_reg;
  code[1] = R ("0rrr1r0000000rrr");
}


TEST (divsl_ll_ind, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  uint16 *p;
  int i;

  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  for (i = 0, p = (uint16 *)mem; i < MEM_SIZE / sizeof (uint16); i++)
    *p++ = SWAPUW_IFLE (randint (1, 65535));
  code[0] = R ("0100110001000rrr") | (randint (2, 4) << 3);
  code[1] = R ("0rrr1r0000000rrr");
}


TEST (divsl_ll_absl, ALL_CCS, 4, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  uint16 *p;
  int i;

  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  for (i = 0, p = (uint16 *)mem; i < MEM_SIZE / sizeof (uint16); i++)
    *p++ = SWAPUW_IFLE (randint (1, 65535));
  code[0] = B ("0100110001111001");
  code[1] = R ("0rrr1r0000000rrr");
  WRITE4 (2, randint (MEM + 50, MEMEND - 50) & ~1);
}


TEST (lea_pc_ind_preix, ALL_CCS, 50, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  int i;
  int bd_size = randint (2, 3), od_size = randint (1, 3);
  int scale = randint (0, 3);

  randomize_mem ();
  randomize_regs (0, 15, 0, 8 >> scale, (4 >> scale) - 1);

  /* Put out some initial NOPs */
  for (i = 0; i < 4; i++)
    code[i] = NOP_OPCODE;  /* nop */

  /* Output the main opcode. */
  code[i] = R ("0100rrr111111011");
  code[i + 1] = (R ("rrrrr0010r000000") | (scale << 9)
		 | (bd_size << 4) | od_size);
  i += 2;

  /* Output the base displacement. */
  switch (bd_size) {
  case 2:
    code[i] = (5 + (od_size - 1)) * 2 + (randint (0, 5) * 4);
    i++;
    break;
  case 3:
    WRITE4 (i, (6 + (od_size - 1)) * 2 + (randint (0, 5) * 4));
    i += 2;
    break;
  }

  /* Output outer displacement (if any). */
  switch (od_size) {
  case 1:
    break;
  case 2:
    code[i] = randint (-20, 20);
    i++;
    break;
  case 3:
    WRITE4 (i, randint (-20, 20));
    i += 2;
    break;
  }

  /* Skip over array of addresses. */
  code[i] = 0x4EF9;  /* jmp absl */
  WRITE4 (i + 1, (unsigned long) (code - (MEMORY_OFFSET / 2) + 50));
  
  for (i += 3; i < 49; i += 2)
    WRITE4 (i, randint (MEM + 50, MEMEND - 50));
}


TEST (lea_pc_ind_postix, ALL_CCS, 50, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  int i;
  int bd_size = randint (2, 3), od_size = randint (1, 3);
  int scale = randint (0, 3);

  randomize_mem ();
  randomize_regs (0, 15, -6, 6, 0);

  /* Put out some initial NOPs */
  for (i = 0; i < 4; i++)
    code[i] = NOP_OPCODE;  /* nop */

  /* Output the main opcode. */
  code[i] = R ("0100rrr111111011");
  code[i + 1] = (R ("rrrrr0010r000100") | (scale << 9)
		 | (bd_size << 4) | od_size);
  i += 2;

  /* Output the base displacement. */
  switch (bd_size) {
  case 2:
    code[i] = (5 + (od_size - 1)) * 2 + (randint (0, 5) * 4);
    i++;
    break;
  case 3:
    WRITE4 (i, (6 + (od_size - 1)) * 2 + (randint (0, 5) * 4));
    i += 2;
    break;
  }

  /* Output outer displacement (if any). */
  switch (od_size) {
  case 1:
    break;
  case 2:
    code[i] = randint (-20, 20);
    i++;
    break;
  case 3:
    WRITE4 (i, randint (-20, 20));
    i += 2;
    break;
  }

  /* Skip over array of addresses. */
  code[i] = 0x4EF9;  /* jmp absl */
  WRITE4 (i + 1, (unsigned long) (code - (MEMORY_OFFSET / 2) + 50));
  
  for (i += 3; i < 49; i += 2)
    WRITE4 (i, randint (MEM + 100, MEMEND - 100));
}


TEST (moveb_pc_ind_preix_dreg, ALL_CCS, 50, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  int i;
  int bd_size = randint (2, 3), od_size = randint (1, 3);
  int scale = randint (0, 3);

  randomize_mem ();
  randomize_regs (0, 15, 0, 8 >> scale, (4 >> scale) - 1);

  /* Put out some initial NOPs */
  for (i = 0; i < 4; i++)
    code[i] = NOP_OPCODE;  /* nop */

  /* Output the main opcode. */
  code[i] = R ("0001rrr000111011");
  code[i + 1] = (R ("rrrrr0010r000000") | (scale << 9)
		 | (bd_size << 4) | od_size);
  i += 2;

  /* Output the base displacement. */
  switch (bd_size) {
  case 2:
    code[i] = (5 + (od_size - 1)) * 2 + (randint (0, 5) * 4);
    i++;
    break;
  case 3:
    WRITE4 (i, (6 + (od_size - 1)) * 2 + (randint (0, 5) * 4));
    i += 2;
    break;
  }

  /* Output outer displacement (if any). */
  switch (od_size) {
  case 1:
    break;
  case 2:
    code[i] = randint (-20, 20);
    i++;
    break;
  case 3:
    WRITE4 (i, randint (-20, 20));
    i += 2;
    break;
  }

  /* Skip over array of addresses. */
  code[i] = 0x4EF9;  /* jmp absl */
  WRITE4 (i + 1, (unsigned long) (code - (MEMORY_OFFSET / 2) + 50));
  
  for (i += 3; i < 49; i += 2)
    WRITE4 (i, randint (MEM + 50, MEMEND - 50));
}


TEST (moveb_pc_ind_postix_dreg, ALL_CCS, 50, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  int i;
  int bd_size = randint (2, 3), od_size = randint (1, 3);
  int scale = randint (0, 3);

  randomize_mem ();
  randomize_regs (0, 15, -6, 6, 0);

  /* Put out some initial NOPs */
  for (i = 0; i < 4; i++)
    code[i] = NOP_OPCODE;  /* nop */

  /* Output the main opcode. */
  code[i] = R ("0001rrr000111011");
  code[i + 1] = (R ("rrrrr0010r000100") | (scale << 9)
		 | (bd_size << 4) | od_size);
  i += 2;

  /* Output the base displacement. */
  switch (bd_size) {
  case 2:
    code[i] = (5 + (od_size - 1)) * 2 + (randint (0, 5) * 4);
    i++;
    break;
  case 3:
    WRITE4 (i, (6 + (od_size - 1)) * 2 + (randint (0, 5) * 4));
    i += 2;
    break;
  }

  /* Output outer displacement (if any). */
  switch (od_size) {
  case 1:
    break;
  case 2:
    code[i] = randint (-20, 20);
    i++;
    break;
  case 3:
    WRITE4 (i, randint (-20, 20));
    i += 2;
    break;
  }

  /* Skip over array of addresses. */
  code[i] = 0x4EF9;  /* jmp absl */
  WRITE4 (i + 1, (unsigned long) (code - (MEMORY_OFFSET / 2) + 50));
  
  for (i += 3; i < 49; i += 2)
    WRITE4 (i, randint (MEM + 100, MEMEND - 100));
}


TEST (moveb_pcd16_dreg, ALL_CCS, 6, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  code[0] = NOP_OPCODE;  /* nop */
  code[1] = NOP_OPCODE;  /* nop */
  code[2] = R ("0001rrr000111010");
  code[3] = randint (-4, 4);
  code[4] = NOP_OPCODE;  /* nop */
  code[5] = NOP_OPCODE;  /* nop */
}


TEST (movewl_pcd16_dreg, ALL_CCS, 6, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  code[0] = NOP_OPCODE;  /* nop */
  code[1] = NOP_OPCODE;  /* nop */
  code[2] = R ("001rrrr000111010");
  code[3] = randint (-4, 4) & ~1;
  code[4] = NOP_OPCODE;  /* nop */
  code[5] = NOP_OPCODE;  /* nop */
}


TEST (moveb_pcd8_dreg, ALL_CCS, 40, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  int i;
  int long_ix = randint (0, 1);    /* Are we using a index reg as a long? */

  randomize_mem ();
  randomize_regs (0, 15, -2, 2, 0);

  /* If we have a word sized index, blow away high word of all regs so
   * that we know the high word is being properly ignored.
   */
  if (!long_ix)
    {
      for (i = 0; i < 16; i++)
	cpu_state.regs[i].ul.n = ((cpu_state.regs[i].ul.n & 0xFFFF)
				  | (randnum () << 16));
    }

  for (i = 0; i < 20; i++)
    code[i] = NOP_OPCODE;  /* nop */
  code[i] = R ("0001rrr000111011");
  code[i + 1] = (R ("rrrr0rr000000000") | (long_ix << 11)
		 | (randint (-4, 4) & 0xFF));
  for (i += 2; i < 40; i++)
    code[i] = NOP_OPCODE;  /* nop */
}


TEST (movewl_pcd8_dreg, ALL_CCS, 40, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  int i;

  randomize_mem ();
  randomize_regs (0, 15, -2, 2, 1);

  for (i = 0; i < 20; i++)
    code[i] = NOP_OPCODE;  /* nop */
  code[i] = R ("001rrrr000111011");
  code[i + 1] = R ("rrrrrrr000000000") | (randint (-4, 4) & 0xFE);
  for (i += 2; i < 40; i++)
    code[i] = NOP_OPCODE;  /* nop */
}


TEST (moveb_pc_ind_ix_dreg, ALL_CCS, 40, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  int i;
  int bd_size = randint (1, 3);

  randomize_mem ();
  randomize_regs (0, 15, -2, 2, 0);

  for (i = 0; i < 20; i++)
    code[i] = NOP_OPCODE;  /* nop */
  code[i] = R ("0001rrr000111011");
  code[i + 1] = R ("rrrrrrr10r000000") | (bd_size << 4);
  switch (bd_size) {
  case 1:
    i += 2;
    break;
  case 2:
    code[i + 2] = randint (-4, 4);
    i += 3;
    break;
  case 3:
    WRITE4 (i + 2, randint (-4, 4));
    i += 4;
    break;
  }

  for (; i < 40; i++)
    code[i] = NOP_OPCODE;  /* nop */
}


TEST (movewl_pc_ind_ix_dreg, ALL_CCS, 40, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  int i;
  int bd_size = randint (1, 3);

  randomize_mem ();
  randomize_regs (0, 15, -2, 2, 1);

  for (i = 0; i < 20; i++)
    code[i] = NOP_OPCODE;  /* nop */
  code[i] = R ("001rrrr000111011");
  code[i + 1] = R ("rrrrrrr10r000000") | (bd_size << 4);
  switch (bd_size) {
  case 1:
    i += 2;
    break;
  case 2:
    code[i + 2] = randint (-4, 4) & ~1;
    i += 3;
    break;
  case 3:
    WRITE4 (i + 2, randint (-4, 4) & ~1);
    i += 4;
    break;
  }

  for (; i < 40; i++)
    code[i] = NOP_OPCODE;  /* nop */
}


TEST (lea_ind_preix, ALL_CCS, 50, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  int i;
  int bd_size, od_size = randint (1, 3);
  int scale = randint (0, 3);
  int base_address_reg = randint (0, 7);
  int base_suppress = randint (0, 1);
  int ixreg_specifier;

  randomize_mem ();
  randomize_regs (0, 15, 0, 8 >> scale, (4 >> scale) - 1);

  /* Put out some initial NOPs */
  for (i = 0; i < 4; i++)
    code[i] = NOP_OPCODE;  /* nop */

  /* Determine the index reg.  Make sure it's not the same as our base reg. */
  do
    {
      ixreg_specifier = randint (0, 15);
    }
  while ((ixreg_specifier & 8) && (ixreg_specifier & 7) == base_address_reg);

  if (base_suppress)
    bd_size = 3;
  else
    bd_size = randint (2, 3);

  /* Output the main opcode. */
  code[i] = R ("0100rrr111110000") | base_address_reg;
  code[i + 1] = (R ("0000r0010r000000") | (ixreg_specifier << 12)
		 | (base_suppress << 7) | (scale << 9) | (bd_size << 4)
		 | od_size);
  i += 2;

  /* Put the address of a block of random memory into our base register. */
  cpu_state.regs[8 + base_address_reg].ul.n
    = randint (MEM + 32, MEM + 128) & ~1;

  /* Output the base displacement. */
  if (!base_suppress)
    {
      /* Since we aren't suppressing the base, make this a small offset. */
      switch (bd_size) {
      case 2:
	code[i] = (5 + (od_size - 1)) * 2 + (randint (0, 5) * 4);
	i++;
	break;
      case 3:
	WRITE4 (i, (6 + (od_size - 1)) * 2 + (randint (0, 5) * 4));
	i += 2;
	break;
      }
    }
  else  /* base_suppress */
    {
      /* Since we're suppressing the base, we must make it larger and
       * point directly to the base of the addresses in which we
       * are interested.
       */
      WRITE4 (i, MEM + 32 + (randint (0, 50) * 4));
      i += 2;
    }

  /* Output outer displacement (if any). */
  switch (od_size) {
  case 1:
    break;
  case 2:
    code[i] = randint (-20, 20);
    i++;
    break;
  case 3:
    WRITE4 (i, randint (-20, 20));
    i += 2;
    break;
  }

  /* Skip over array of addresses. */
  code[i] = 0x4EF9;  /* jmp absl */
  WRITE4 (i + 1, (unsigned long) (code - (MEMORY_OFFSET / 2) + 50));
  
  for (i += 3; i < 49; i += 2)
    WRITE4 (i, randint (MEM + 50, MEMEND - 50));
}


TEST (lea_ind_postix, ALL_CCS, 50, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  int i;
  int bd_size, od_size = randint (1, 3);
  int scale = randint (0, 3);
  int base_address_reg = randint (0, 7);
  int base_suppress = randint (0, 1);
  int ixreg_specifier = randint (0, 15);

  randomize_mem ();
  randomize_regs (0, 15, -1024, 1024, 1);

  /* Put out some initial NOPs */
  for (i = 0; i < 4; i++)
    code[i] = NOP_OPCODE;  /* nop */

  if (base_suppress)
    bd_size = 3;
  else
    bd_size = randint (2, 3);

  /* Output the main opcode. */
  code[i] = R ("0100rrr111110000") | base_address_reg;
  code[i + 1] = (R ("0000r0010r000100") | (ixreg_specifier << 12)
		 | (base_suppress << 7) | (scale << 9) | (bd_size << 4)
		 | od_size);
  i += 2;

  /* Put the address of a block of random memory into our base register. */
  cpu_state.regs[8 + base_address_reg].ul.n
    = randint (MEM + 32, MEMEND - 128) & ~1;

  /* Output the base displacement. */
  if (!base_suppress)
    {
      /* Since we aren't suppressing the base, make this a small offset. */
      switch (bd_size) {
      case 2:
	code[i] = (5 + (od_size - 1)) * 2 + (randint (0, 5) * 4);
	i++;
	break;
      case 3:
	WRITE4 (i, (6 + (od_size - 1)) * 2 + (randint (0, 5) * 4));
	i += 2;
	break;
      }
    }
  else  /* base_suppress */
    {
      /* Since we're suppressing the base, we must make it larger and
       * point directly to the base of the addresses in which we
       * are interested.
       */
      WRITE4 (i, MEM + 32 + (randint (0, 50) * 4));
      i += 2;
    }

  /* Output outer displacement (if any). */
  switch (od_size) {
  case 1:
    break;
  case 2:
    code[i] = randint (-20, 20);
    i++;
    break;
  case 3:
    WRITE4 (i, randint (-20, 20));
    i += 2;
    break;
  }

  /* Skip over array of addresses. */
  code[i] = 0x4EF9;  /* jmp absl */
  WRITE4 (i + 1, (unsigned long) (code - (MEMORY_OFFSET / 2) + 50));
  
  for (i += 3; i < 49; i += 2)
    WRITE4 (i, randint (MEM + 50, MEMEND - 50));
}


TEST (add_absl_dreg, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  code[0] = R ("1101rrr000111001") | RANDOM_SIZE ();
  WRITE4 (1, randint (MEM + 8, MEMEND - 8) & ~1);
}


TEST (abcd_reg, C_BIT | Z_BIT | X_BIT, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1100rrr100000rrr");
}


TEST (abcd_mem, C_BIT | Z_BIT | X_BIT, 1, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 0);
  code[0] = R ("1100rrr100001rrr");
}


TEST (addb_dreg_dreg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1101rrr000000rrr");
}


TEST (addw_reg_dreg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1101rrr00100rrrr");
}


TEST (addl_reg_dreg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1101rrr01000rrrr");
}


TEST (add_d16_dreg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("1101rrr000101rrr") | RANDOM_SIZE ();
  code[1] = randint (-290, 290) & ~1;
}


TEST (addb_const_dreg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1101rrr000111100");
  code[1] = randint (0, 65535);
}


TEST (addw_const_dreg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1101rrr001111100");
  code[1] = randint (0, 65535);
}


TEST (add_dreg_ind, ALL_CCS, 1, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = (R ("1101rrr100000rrr")
	     | RANDOM_SIZE ()
	     | (randint (2, 4) << 3));  /* Random amode [2, 4] */
}


TEST (add_ind_dreg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = (R ("1101rrr000000rrr")
	     | RANDOM_SIZE ()
	     | (randint (2, 4) << 3)); /* Random amode [2, 4] */
}

TEST (addl_const_dreg, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("1101rrr010111100");
  WRITE4 (1, randnum ());
}


TEST (add_dreg_d16, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("1101rrr100101rrr") | RANDOM_SIZE ();
  code[1] = randint (-290, 290) & ~1;
}


TEST (add_dreg_absl, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("1101rrr100111001") | RANDOM_SIZE ();
  WRITE4 (1, randint (MEM + 4, MEMEND - 4) & ~1);
}


TEST (addaw_reg_areg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1101rrr01100rrrr");
}


TEST (addal_reg_areg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1101rrr11100rrrr");
}


TEST (adda_ind_areg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = (R ("1101rrrr11000rrr")   /* Random size. */
	     | (randint (2, 4) << 3));  /* Random amode [2, 4]; */
}


TEST (adda_d16_areg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("1101rrrr11101rrr");  /* Random size. */
  code[1] = randint (-290, 290) & ~1;
}


TEST (adda_absl_areg, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("1101rrrr11111001");  /* Random size. */
  WRITE4 (1, randint (MEM + 4, MEMEND - 4) & ~1);
}


TEST (addaw_const_areg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1101rrr011111100");
  code[1] = randint (0, 65535);
}


TEST (addal_const_areg, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1101rrr111111100");
  code[1] = randint (0, 65535);
  code[2] = randint (0, 65535);
}


TEST (addibw_dreg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("000001100r000rrr");  /* Randomly either byte or word */
  code[1] = randnum ();
}


TEST (addil_dreg, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0000011010000rrr");
  WRITE4 (1, randnum ());
}


TEST (addibw_ind, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = (R ("000001100r000rrr")    /* Randomly either byte or word. */
	     | (randint (2, 4) << 3));  /* Random amode [2, 4] */
  code[1] = randnum ();
}


TEST (addil_ind, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = (R ("0000011010000rrr")
	     | (randint (2, 4) << 3));  /* Random amode [2, 4] */
  WRITE4 (1, randnum ());
}


TEST (addibw_d16, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("000001100r101rrr");  /* Randomly either byte or word. */
  code[1] = randnum ();
  code[2] = randint (-290, 290) & ~1;
}


TEST (addil_d16, ALL_CCS, 4, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("0000011010101rrr");
  WRITE4 (1, randnum ());
  code[3] = randint (-290, 290) & ~1;
}


TEST (addibw_absl, ALL_CCS, 4, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("000001100r111001");  /* Randomly either byte or word. */
  code[1] = randnum ();

  WRITE4 (2, randint (MEM + 4, MEMEND - 4) & ~1);
}


TEST (addil_absl, ALL_CCS, 5, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0000011010111001");  /* Randomly either byte or word. */
  WRITE4 (1, randnum ());
  WRITE4 (3, randint (MEM + 4, MEMEND - 4) & ~1);
}


TEST (addq_dreg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0101rrr000000rrr") | RANDOM_SIZE ();
}


TEST (addq_areg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0101rrr000001rrr") | (randint (1, 2) << 6); /* word or long */
}


TEST (addq_ind, ALL_CCS, 1, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = (R ("0101rrr000000rrr")
	     | RANDOM_SIZE ()
	     | (randint (2, 4) << 3));  /* random amode [2, 4] */
}


TEST (addq_d16, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("0101rrr000101rrr") | RANDOM_SIZE ();
  code[1] = randint (-290, 290) & ~1;
}


TEST (addq_absl, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0101rrr000111001") | RANDOM_SIZE ();
  WRITE4 (1, randint (MEM + 4, MEMEND - 4) & ~1);
}


TEST (addx_reg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1101rrr100000rrr") | RANDOM_SIZE ();
}


TEST (addx_mem, ALL_CCS, 1, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = R ("1101rrr100001rrr") | RANDOM_SIZE ();
}


TEST (and_dreg_dreg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1100rrr000000rrr") | RANDOM_SIZE ();
}


TEST (and_ind_dreg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = (R ("1100rrr000000rrr")
	     | RANDOM_SIZE ()
	     | (randint (2, 4) << 3));
}


TEST (and_d16_dreg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("1100rrr000101rrr") | RANDOM_SIZE ();
  code[1] = randint (-290, 290) & ~1;
}


TEST (and_absl_dreg, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  code[0] = R ("1100rrr000111001") | RANDOM_SIZE ();
  WRITE4 (1, randint (MEM + 4, MEMEND - 4) & ~1);
}


TEST (andbw_const_dreg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1100rrr00r111100");  /* randomly byte or word */
  code[1] = randnum ();
}


TEST (andl_const_dreg, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1100rrr010111100");
  WRITE4 (1, randnum ());
}


TEST (and_dreg_ind, ALL_CCS, 1, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = (R ("1100rrr100000rrr")
	     | RANDOM_SIZE ()
	     | (randint (2, 4) << 3));
}


TEST (and_dreg_d16, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("1100rrr100101rrr") | RANDOM_SIZE ();
  code[1] = randint (-290, 290) & ~1;
}


TEST (and_dreg_absl, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1100rrr100111001") | RANDOM_SIZE ();
  WRITE4 (1, randint (MEM + 4, MEMEND - 4) & ~1);
}


TEST (andibw_dreg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("000000100r000rrr");
  code[1] = randnum ();
}


TEST (andil_dreg, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0000001010000rrr");
  WRITE4 (1, randnum ());
}


TEST (andibw_ind, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("000000100r000rrr") | (randint (2, 4) << 3);
  code[1] = randnum ();
}


TEST (andil_ind, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("0000001010000rrr") | (randint (2, 4) << 3);
  WRITE4 (1, randnum ());
}


TEST (andibw_d16, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("000000100r101rrr");  /* randomly either byte or word. */
  code[1] = randnum ();
  code[2] = randint (-290, 290) & ~1;
}


TEST (andil_d16, ALL_CCS, 4, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("0000001010101rrr");
  WRITE4 (1, randnum ());
  code[3] = randint (-290, 290) & ~1;
}


TEST (andibw_absl, ALL_CCS, 4, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("000000100r111001");
  code[1] = randnum ();
  WRITE4 (2, randint (MEM + 4, MEMEND - 4) & ~1);
}


TEST (andil_absl, ALL_CCS, 5, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = B ("0000001010111001");
  WRITE4 (1, randnum ());
  WRITE4 (3, randint (MEM + 4, MEMEND - 4) & ~1);
}


TEST (andi_to_ccr, ALL_CCS, 2, WONT_CHANGE_MEMORY, 32)
{
  code[0] = B ("0000001000111100");
  code[1] = times_called;
}


TEST (asl_dx_dy, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1110rrr100100rrr") | RANDOM_SIZE ();
}


TEST (asl_const_dy, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1110rrr100000rrr") | RANDOM_SIZE ();
}


TEST (asl_ind, ALL_CCS, 1, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = R ("1110000111000rrr") | (randint (2, 4) << 3);
}


TEST (asl_d16, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("1110000111101rrr");
  code[1] = randint (-290, 290) & ~1;
}


TEST (asl_absl, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = B ("1110000111111001");
  WRITE4 (1, randint (MEM + 4, MEMEND - 4) & ~1);
}


TEST (asr_dx_dy, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1110rrr000100rrr") | RANDOM_SIZE ();
}


TEST (asr_const_dy, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1110rrr000000rrr") | RANDOM_SIZE ();
}


TEST (asr_ind, ALL_CCS, 1, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = R ("1110000011000rrr") | (randint (2, 4) << 3);
}


TEST (asr_d16, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("1110000011101rrr");
  code[1] = randint (-290, 290) & ~1;
}


TEST (asr_absl, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = B ("1110000011111001");
  WRITE4 (1, randint (MEM + 4, MEMEND - 4) & ~1);
}


TEST (lsl_dx_dy, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1110rrr100101rrr") | RANDOM_SIZE ();
}


TEST (lsl_const_dy, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1110rrr100001rrr") | RANDOM_SIZE ();
}


TEST (lsl_ind, ALL_CCS, 1, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = R ("1110001111000rrr") | (randint (2, 4) << 3);
}


TEST (lsl_d16, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("1110001111101rrr");
  code[1] = randint (-290, 290) & ~1;
}


TEST (lsl_absl, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = B ("1110001111111001");
  WRITE4 (1, randint (MEM + 4, MEMEND - 4) & ~1);
}


TEST (lsr_dx_dy, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1110rrr000101rrr") | RANDOM_SIZE ();
}


TEST (lsr_const_dy, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1110rrr000001rrr") | RANDOM_SIZE ();
}


TEST (lsr_ind, ALL_CCS, 1, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = R ("1110001011000rrr") | (randint (2, 4) << 3);
}


TEST (lsr_d16, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("1110001011101rrr");
  code[1] = randint (-290, 290) & ~1;
}


TEST (lsr_absl, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = B ("1110001011111001");
  WRITE4 (1, randint (MEM + 4, MEMEND - 4) & ~1);
}


#define BCC_TEST_B(bits) \
  code[0] = 0x6004 | ((bits) << 8);  /* b?? * + 4 */   \
  code[1] = B ("0111000000000000");  /* moveq #0,d0 */ \
  code[2] = 0x6002;                  /* bra * + 2 */   \
  code[3] = B ("0111000000000001");  /* moveq #1,d0 */ \
  code[4] = NOP_OPCODE               /* nop */

TEST (bra_b, ALL_CCS, 5, WONT_CHANGE_MEMORY, 2) { BCC_TEST_B (0x0); }
/* 0x1 is reserved for bsr */
TEST (bhi_b, ALL_CCS, 5, WONT_CHANGE_MEMORY, 2) { BCC_TEST_B (0x2); }
TEST (bls_b, ALL_CCS, 5, WONT_CHANGE_MEMORY, 2) { BCC_TEST_B (0x3); }
TEST (bcc_b, ALL_CCS, 5, WONT_CHANGE_MEMORY, 2) { BCC_TEST_B (0x4); }
TEST (bcs_b, ALL_CCS, 5, WONT_CHANGE_MEMORY, 2) { BCC_TEST_B (0x5); }
TEST (bne_b, ALL_CCS, 5, WONT_CHANGE_MEMORY, 2) { BCC_TEST_B (0x6); }
TEST (beq_b, ALL_CCS, 5, WONT_CHANGE_MEMORY, 2) { BCC_TEST_B (0x7); }
TEST (bvc_b, ALL_CCS, 5, WONT_CHANGE_MEMORY, 2) { BCC_TEST_B (0x8); }
TEST (bvs_b, ALL_CCS, 5, WONT_CHANGE_MEMORY, 2) { BCC_TEST_B (0x9); }
TEST (bpl_b, ALL_CCS, 5, WONT_CHANGE_MEMORY, 2) { BCC_TEST_B (0xA); }
TEST (bmi_b, ALL_CCS, 5, WONT_CHANGE_MEMORY, 2) { BCC_TEST_B (0xB); }
TEST (bge_b, ALL_CCS, 5, WONT_CHANGE_MEMORY, 2) { BCC_TEST_B (0xC); }
TEST (blt_b, ALL_CCS, 5, WONT_CHANGE_MEMORY, 2) { BCC_TEST_B (0xD); }
TEST (bgt_b, ALL_CCS, 5, WONT_CHANGE_MEMORY, 2) { BCC_TEST_B (0xE); }
TEST (ble_b, ALL_CCS, 5, WONT_CHANGE_MEMORY, 2) { BCC_TEST_B (0xF); }

#undef BCC_TEST_B

#define BCC_TEST_W(bits) \
  code[0] = 0x6000 | ((bits) << 8);    /* b?? * + 8   */ \
  code[1] = 0x0006;                    /* (offset)    */ \
  code[2] = B ("0111000000000000");    /* moveq #0,d0 */ \
  code[3] = 0x6002;                    /* bra * + 2   */ \
  code[4] = B ("0111000000000001");    /* moveq #1,d0 */ \
  code[5] = NOP_OPCODE                 /* nop         */

TEST (bra_w, ALL_CCS, 6, WONT_CHANGE_MEMORY, 2) { BCC_TEST_W (0x0); }
/* 0x1 is reserved for bsr */
TEST (bhi_w, ALL_CCS, 6, WONT_CHANGE_MEMORY, 2) { BCC_TEST_W (0x2); }
TEST (bls_w, ALL_CCS, 6, WONT_CHANGE_MEMORY, 2) { BCC_TEST_W (0x3); }
TEST (bcc_w, ALL_CCS, 6, WONT_CHANGE_MEMORY, 2) { BCC_TEST_W (0x4); }
TEST (bcs_w, ALL_CCS, 6, WONT_CHANGE_MEMORY, 2) { BCC_TEST_W (0x5); }
TEST (bne_w, ALL_CCS, 6, WONT_CHANGE_MEMORY, 2) { BCC_TEST_W (0x6); }
TEST (beq_w, ALL_CCS, 6, WONT_CHANGE_MEMORY, 2) { BCC_TEST_W (0x7); }
TEST (bvc_w, ALL_CCS, 6, WONT_CHANGE_MEMORY, 2) { BCC_TEST_W (0x8); }
TEST (bvs_w, ALL_CCS, 6, WONT_CHANGE_MEMORY, 2) { BCC_TEST_W (0x9); }
TEST (bpl_w, ALL_CCS, 6, WONT_CHANGE_MEMORY, 2) { BCC_TEST_W (0xA); }
TEST (bmi_w, ALL_CCS, 6, WONT_CHANGE_MEMORY, 2) { BCC_TEST_W (0xB); }
TEST (bge_w, ALL_CCS, 6, WONT_CHANGE_MEMORY, 2) { BCC_TEST_W (0xC); }
TEST (blt_w, ALL_CCS, 6, WONT_CHANGE_MEMORY, 2) { BCC_TEST_W (0xD); }
TEST (bgt_w, ALL_CCS, 6, WONT_CHANGE_MEMORY, 2) { BCC_TEST_W (0xE); }
TEST (ble_w, ALL_CCS, 6, WONT_CHANGE_MEMORY, 2) { BCC_TEST_W (0xF); }

#undef BCC_TEST_W

#define BCC_TEST_L(bits) \
  code[0] = 0x60FF | ((bits) << 8);    /* b?? * + 8   */ \
  code[1] = 0x0000;                    /* (offset hi) */ \
  code[2] = 0x0008;                    /* (offset lo) */ \
  code[3] = B ("0111000000000000");    /* moveq #0,d0 */ \
  code[4] = 0x6002;                    /* bra * + 2   */ \
  code[5] = B ("0111000000000001");    /* moveq #1,d0 */ \
  code[6] = NOP_OPCODE                 /* nop         */

TEST (bra_l, ALL_CCS, 7, WONT_CHANGE_MEMORY, 2) { BCC_TEST_L (0x0); }
/* 0x1 is reserved for bsr */
TEST (bhi_l, ALL_CCS, 7, WONT_CHANGE_MEMORY, 2) { BCC_TEST_L (0x2); }
TEST (bls_l, ALL_CCS, 7, WONT_CHANGE_MEMORY, 2) { BCC_TEST_L (0x3); }
TEST (bcc_l, ALL_CCS, 7, WONT_CHANGE_MEMORY, 2) { BCC_TEST_L (0x4); }
TEST (bcs_l, ALL_CCS, 7, WONT_CHANGE_MEMORY, 2) { BCC_TEST_L (0x5); }
TEST (bne_l, ALL_CCS, 7, WONT_CHANGE_MEMORY, 2) { BCC_TEST_L (0x6); }
TEST (beq_l, ALL_CCS, 7, WONT_CHANGE_MEMORY, 2) { BCC_TEST_L (0x7); }
TEST (bvc_l, ALL_CCS, 7, WONT_CHANGE_MEMORY, 2) { BCC_TEST_L (0x8); }
TEST (bvs_l, ALL_CCS, 7, WONT_CHANGE_MEMORY, 2) { BCC_TEST_L (0x9); }
TEST (bpl_l, ALL_CCS, 7, WONT_CHANGE_MEMORY, 2) { BCC_TEST_L (0xA); }
TEST (bmi_l, ALL_CCS, 7, WONT_CHANGE_MEMORY, 2) { BCC_TEST_L (0xB); }
TEST (bge_l, ALL_CCS, 7, WONT_CHANGE_MEMORY, 2) { BCC_TEST_L (0xC); }
TEST (blt_l, ALL_CCS, 7, WONT_CHANGE_MEMORY, 2) { BCC_TEST_L (0xD); }
TEST (bgt_l, ALL_CCS, 7, WONT_CHANGE_MEMORY, 2) { BCC_TEST_L (0xE); }
TEST (ble_l, ALL_CCS, 7, WONT_CHANGE_MEMORY, 2) { BCC_TEST_L (0xF); }

#undef BCC_TEST_L


TEST (bchg_reg_reg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0000rrr101000rrr");
}


TEST (bchg_const_reg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0000100001000rrr");
  code[1] = randint (0, 65535);
}


TEST (bchg_reg_ind, ALL_CCS, 1, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 0);
  code[0] = R ("0000rrr101000rrr") | (randint (2, 4) << 3);
}


TEST (bchg_const_ind, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 0);
  code[0] = R ("0000100001000rrr") | (randint (2, 4) << 3);
  code[1] = randint (0, 65535);
}


TEST (bchg_reg_d16, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0);
  code[0] = R ("0000rrr101101rrr");
  code[1] = randint (-290, 290);
}


TEST (bchg_const_d16, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0);
  code[0] = R ("0000100001101rrr");
  code[1] = randint (0, 65535);
  code[2] = randint (-290, 290);
}


TEST (bchg_reg_absl, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0000rrr101111001");
  WRITE4 (1, randint (MEM + 4, MEMEND - 4));
}


TEST (bchg_const_absl, ALL_CCS, 4, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0000100001111001");
  code[1] = randint (0, 65535);
  WRITE4 (2, randint (MEM + 4, MEMEND - 4));
}


TEST (bclr_reg_reg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0000rrr110000rrr");
}


TEST (bclr_const_reg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0000100010000rrr");
  code[1] = randint (0, 65535);
}


TEST (bclr_reg_ind, ALL_CCS, 1, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 0);
  code[0] = R ("0000rrr110000rrr") | (randint (2, 4) << 3);
}


TEST (bclr_const_ind, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 0);
  code[0] = R ("0000100010000rrr") | (randint (2, 4) << 3);
  code[1] = randint (0, 65535);
}


TEST (bclr_reg_d16, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0);
  code[0] = R ("0000rrr110101rrr");
  code[1] = randint (-290, 290);
}


TEST (bclr_const_d16, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0);
  code[0] = R ("0000100010101rrr");
  code[1] = randint (0, 65535);
  code[2] = randint (-290, 290);
}


TEST (bclr_reg_absl, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0000rrr110111001");
  WRITE4 (1, randint (MEM + 4, MEMEND - 4));
}


TEST (bclr_const_absl, ALL_CCS, 4, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0000100010111001");
  code[1] = randint (0, 65535);
  WRITE4 (2, randint (MEM + 4, MEMEND - 4));
}


TEST (bset_reg_reg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0000rrr111000rrr");
}


TEST (bset_const_reg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0000100011000rrr");
  code[1] = randint (0, 65535);
}


TEST (bset_reg_ind, ALL_CCS, 1, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 0);
  code[0] = R ("0000rrr111000rrr") | (randint (2, 4) << 3);
}


TEST (bset_const_ind, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 0);
  code[0] = R ("0000100011000rrr") | (randint (2, 4) << 3);
  code[1] = randint (0, 65535);
}


TEST (bset_reg_d16, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0);
  code[0] = R ("0000rrr111101rrr");
  code[1] = randint (-290, 290);
}


TEST (bset_const_d16, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0);
  code[0] = R ("0000100011101rrr");
  code[1] = randint (0, 65535);
  code[2] = randint (-290, 290);
}


TEST (bset_reg_absl, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0000rrr111111001");
  WRITE4 (1, randint (MEM + 4, MEMEND - 4));
}


TEST (bset_const_absl, ALL_CCS, 4, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0000100011111001");
  code[1] = randint (0, 65535);
  WRITE4 (2, randint (MEM + 4, MEMEND - 4));
}


TEST (btst_reg_reg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0000rrr100000rrr");
}


TEST (btst_const_reg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0000100000000rrr");
  code[1] = randint (0, 65535);
}


TEST (btst_reg_ind, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 0);
  code[0] = R ("0000rrr100000rrr") | (randint (2, 4) << 3);
}


TEST (btst_const_ind, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 0);
  code[0] = R ("0000100000000rrr") | (randint (2, 4) << 3);
  code[1] = randint (0, 65535);
}


TEST (btst_reg_d16, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0);
  code[0] = R ("0000rrr100101rrr");
  code[1] = randint (-290, 290);
}


TEST (btst_const_d16, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0);
  code[0] = R ("0000100000101rrr");
  code[1] = randint (0, 65535);
  code[2] = randint (-290, 290);
}


TEST (btst_reg_absl, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  code[0] = R ("0000rrr100111001");
  WRITE4 (1, randint (MEM + 4, MEMEND - 4));
}


TEST (btst_const_absl, ALL_CCS, 4, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  code[0] = B ("0000100000111001");
  code[1] = randint (0, 65535);
  WRITE4 (2, randint (MEM + 4, MEMEND - 4));
}


TEST (bfchg_reg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1110101011000rrr");
  code[1] = randint (0, 65535);
}


TEST (bfchg_ind, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (0, 7, -250 * 8, 250 * 8, 0);           /* Offset/width */
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0); /* Base */
  code[0] = R ("1110101011010rrr");
  code[1] = randint (0, 65535);
}


TEST (bfchg_d16, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (0, 7, -130 * 8, 130 * 8, 0);           /* Offset/width */
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0); /* Base */
  code[0] = R ("1110101011101rrr");
  code[1] = randint (0, 65535);
  code[2] = randint (-130, 130);
}


TEST (bfchg_absl, ALL_CCS, 4, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (0, 7, -130 * 8, 130 * 8, 0);           /* Offset/width */
  code[0] = B ("1110101011111001");
  code[1] = randint (0, 65535);
  WRITE4 (2, randint (MEM + 200, MEMEND - 200));
}


TEST (bfclr_reg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1110110011000rrr");
  code[1] = randint (0, 65535);
}


TEST (bfclr_ind, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (0, 7, -250 * 8, 250 * 8, 0);           /* Offset/width */
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0); /* Base */
  code[0] = R ("1110110011010rrr");
  code[1] = randint (0, 65535);
}


TEST (bfclr_d16, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (0, 7, -130 * 8, 130 * 8, 0);           /* Offset/width */
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0); /* Base */
  code[0] = R ("1110110011101rrr");
  code[1] = randint (0, 65535);
  code[2] = randint (-130, 130);
}


TEST (bfclr_absl, ALL_CCS, 4, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (0, 7, -130 * 8, 130 * 8, 0);           /* Offset/width */
  code[0] = B ("1110110011111001");
  code[1] = randint (0, 65535);
  WRITE4 (2, randint (MEM + 200, MEMEND - 200));
}


TEST (bfset_reg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1110111011000rrr");
  code[1] = randint (0, 65535);
}


TEST (bfset_ind, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (0, 7, -250 * 8, 250 * 8, 0);           /* Offset/width */
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0); /* Base */
  code[0] = R ("1110111011010rrr");
  code[1] = randint (0, 65535);
}


TEST (bfset_d16, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (0, 7, -130 * 8, 130 * 8, 0);           /* Offset/width */
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0); /* Base */
  code[0] = R ("1110111011101rrr");
  code[1] = randint (0, 65535);
  code[2] = randint (-130, 130);
}


TEST (bfset_absl, ALL_CCS, 4, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (0, 7, -130 * 8, 130 * 8, 0);           /* Offset/width */
  code[0] = B ("1110111011111001");
  code[1] = randint (0, 65535);
  WRITE4 (2, randint (MEM + 200, MEMEND - 200));
}


TEST (bftst_reg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1110100011000rrr");
  code[1] = randint (0, 65535);
}


TEST (bftst_ind, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (0, 7, -250 * 8, 250 * 8, 0);           /* Offset/width */
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0); /* Base */
  code[0] = R ("1110100011010rrr");
  code[1] = randint (0, 65535);
}


TEST (bftst_d16, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (0, 7, -130 * 8, 130 * 8, 0);           /* Offset/width */
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0); /* Base */
  code[0] = R ("1110100011101rrr");
  code[1] = randint (0, 65535);
  code[2] = randint (-130, 130);
}


TEST (bftst_absl, ALL_CCS, 4, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (0, 7, -130 * 8, 130 * 8, 0);           /* Offset/width */
  code[0] = B ("1110100011111001");
  code[1] = randint (0, 65535);
  WRITE4 (2, randint (MEM + 200, MEMEND - 200));
}


TEST (bfexts_reg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1110101111000rrr");
  code[1] = randint (0, 65535);
}


TEST (bfexts_ind, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (0, 7, -250 * 8, 250 * 8, 0);           /* Offset/width */
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0); /* Base */
  code[0] = R ("1110101111010rrr");
  code[1] = randint (0, 65535);
}


TEST (bfexts_d16, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (0, 7, -130 * 8, 130 * 8, 0);           /* Offset/width */
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0); /* Base */
  code[0] = R ("1110101111101rrr");
  code[1] = randint (0, 65535);
  code[2] = randint (-130, 130);
}


TEST (bfexts_absl, ALL_CCS, 4, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (0, 7, -130 * 8, 130 * 8, 0);           /* Offset/width */
  code[0] = B ("1110101111111001");
  code[1] = randint (0, 65535);
  WRITE4 (2, randint (MEM + 200, MEMEND - 200));
}


TEST (bfextu_reg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1110100111000rrr");
  code[1] = randint (0, 65535);
}


TEST (bfextu_ind, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (0, 7, -250 * 8, 250 * 8, 0);           /* Offset/width */
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0); /* Base */
  code[0] = R ("1110100111010rrr");
  code[1] = randint (0, 65535);
}


TEST (bfextu_d16, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (0, 7, -130 * 8, 130 * 8, 0);           /* Offset/width */
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0); /* Base */
  code[0] = R ("1110100111101rrr");
  code[1] = randint (0, 65535);
  code[2] = randint (-130, 130);
}


TEST (bfextu_absl, ALL_CCS, 4, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (0, 7, -130 * 8, 130 * 8, 0);           /* Offset/width */
  code[0] = B ("1110100111111001");
  code[1] = randint (0, 65535);
  WRITE4 (2, randint (MEM + 200, MEMEND - 200));
}


TEST (bfffo_reg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1110110111000rrr");
  code[1] = randint (0, 65535);
}


TEST (bfffo_ind, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (0, 7, -250 * 8, 250 * 8, 0);           /* Offset/width */
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0); /* Base */
  code[0] = R ("1110110111010rrr");
  code[1] = randint (0, 65535);
}


TEST (bfffo_d16, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (0, 7, -130 * 8, 130 * 8, 0);           /* Offset/width */
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0); /* Base */
  code[0] = R ("1110110111101rrr");
  code[1] = randint (0, 65535);
  code[2] = randint (-130, 130);
}


TEST (bfffo_absl, ALL_CCS, 4, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (0, 7, -130 * 8, 130 * 8, 0);           /* Offset/width */
  randomize_mem ();
  code[0] = B ("1110110111111001");
  code[1] = randint (0, 65535);
  WRITE4 (2, randint (MEM + 200, MEMEND - 200));
}


TEST (bfins_reg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1110111111000rrr");
  code[1] = randint (0, 65535);
}


TEST (bfins_ind, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (0, 7, -250 * 8, 250 * 8, 0);           /* Offset/width */
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0); /* Base */
  code[0] = R ("1110111111010rrr");
  code[1] = randint (0, 65535);
}


TEST (bfins_d16, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (0, 7, -130 * 8, 130 * 8, 0);           /* Offset/width */
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0); /* Base */
  code[0] = R ("1110111111101rrr");
  code[1] = randint (0, 65535);
  code[2] = randint (-130, 130);
}


TEST (bfins_absl, ALL_CCS, 4, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (0, 7, -130 * 8, 130 * 8, 0);           /* Offset/width */
  code[0] = B ("1110111111111001");
  code[1] = randint (0, 65535);
  WRITE4 (2, randint (MEM + 200, MEMEND - 200));
}


TEST (bsr_b, ALL_CCS, 6, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (15, 15, MEM + 50, MEMEND - 50, 1);  /* random sp */
  code[0] = 0x6104;                  /* bsr * + 4   */
  code[1] = 0x5280;                  /* addql #1,d0 */
  code[2] = 0x6004;                  /* bra * + 4 */
  code[3] = R ("01110000rrrrrrrr");  /* moveq #<random>,d0 */
  code[4] = 0x4E75;                  /* rts */
  code[5] = NOP_OPCODE;              /* nop */
}


TEST (bsr_w, ALL_CCS, 7, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (15, 15, MEM + 50, MEMEND - 50, 1);  /* random sp */
  code[0] = 0x6100;                  /* bsr * + 6   */
  code[1] = 0x0006;
  code[2] = 0x5280;                  /* addql #1,d0 */
  code[3] = 0x6004;                  /* bra * + 4 */
  code[4] = R ("01110000rrrrrrrr");  /* moveq #<random>,d0 */
  code[5] = 0x4E75;                  /* rts */
  code[6] = NOP_OPCODE;              /* nop */
}


TEST (bsr_l, ALL_CCS, 8, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (15, 15, MEM + 50, MEMEND - 50, 1);  /* random sp */
  code[0] = 0x61FF;                  /* bsr * + 8   */
  code[1] = 0x0000;
  code[2] = 0x0008;
  code[3] = 0x5280;                  /* addql #1,d0 */
  code[4] = 0x6004;                  /* bra * + 4 */
  code[5] = R ("01110000rrrrrrrr");  /* moveq #<random>,d0 */
  code[6] = 0x4E75;                  /* rts */
  code[7] = NOP_OPCODE;              /* nop */
}


TEST (cas_ind, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (7, 7 + 8, MEM + 50, MEMEND - 50, 1);
  code[0] = (R ("0000100011000rrr")
	     | (randint (1, 3) << 9)    /* Random size */
	     | (randint (2, 4) << 3));  /* Random amode [2, 4] */
  code[1] = randint (0, 65535);
}


TEST (cas2_areg, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (7, 7 + 8, MEM + 50, MEMEND - 50, 1);
  code[0] = R ("000011r011111100");  /* Randomly either word or long. */
  code[1] = R ("1rrr000rrr000rrr");
  code[2] = R ("1rrr000rrr000rrr");
}


TEST (cas2_anyreg, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (0, 7 + 8, MEM + 50, MEMEND - 50, 1);
  code[0] = R ("000011r011111100");  /* Randomly either word or long. */
  code[1] = R ("rrrr000rrr000rrr");
  code[2] = R ("rrrr000rrr000rrr");
}


TEST (clr_reg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0100001000000rrr") | RANDOM_SIZE ();
}


TEST (clrb_ind, ALL_CCS, 1, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (7, 7 + 8, MEM + 50, MEMEND - 50, 0);
  code[0] = R ("0100001000000rrr") | (randint (2, 4) << 3);
}


TEST (clrwl_ind, ALL_CCS, 1, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (7, 7 + 8, MEM + 50, MEMEND - 50, 1);
  code[0] = (R ("0100001000000rrr")
	     | (randint (1, 2) << 6)   /* Randomly either word or long. */
	     | (randint (2, 4) << 3)); /* Random amode [2, 4] */
}


TEST (clrb_absl, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = B ("0100001000111001");
  WRITE4 (1, randint (MEM + 50, MEMEND - 50));
}


TEST (clrwl_absl, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = B ("0100001000111001") | RANDOM_SIZE ();
  WRITE4 (1, randint (MEM + 50, MEMEND - 50) & ~1);
}


TEST (cmp_dreg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1011rrr000000rrr") | RANDOM_SIZE ();
}


TEST (cmp_areg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1011rrr000001rrr") | RANDOM_SIZE ();
}


TEST (cmpb_ind, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (7, 7 + 8, MEM + 50, MEMEND - 50, 0);
  code[0] = R ("1011rrr000000rrr") | (randint (2, 4) << 3);
}


TEST (cmpwl_ind, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (7, 7 + 8, MEM + 50, MEMEND - 50, 1);
  code[0] = (R ("1011rrr000000rrr")
	     | (randint (1, 2) << 6)   /* Randomly either word or long. */
	     | (randint (2, 4) << 3)); /* Random amode [2, 4] */
}


TEST (cmpb_d16, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0);
  code[0] = R ("1011rrr000101rrr");
  code[1] = randint (-250, 250);
}


TEST (cmpwl_d16, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("1011rrr000101rrr") | (randint (1, 2) << 6);
  code[1] = randint (-250, 250) & ~1;
}


TEST (cmpa_reg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1011rrrr1100rrrr");
}


TEST (cmpa_ind, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = R ("1011rrrr11000rrr") | (randint (2, 4) << 3);
}


/* This compares an arbitrary value in a0 vs. memory indirected through
 * some odd register, as opposed to comparing a legitimate address with memory.
 */
TEST (cmpa_ind2, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (9, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = R ("1011000r11000rr1") | (randint (2, 4) << 3);
}


TEST (cmpa_d16, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("1011rrrr11101rrr");
  code[1] = randint (-250, 250) & ~1;
}


/* This compares an arbitrary value in a0 vs. memory indirected through
 * some odd register, as opposed to comparing a legitimate address with memory.
 */
TEST (cmpa_d16_2, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (9, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("1011000r11101rr1");
  code[1] = randint (-250, 250) & ~1;
}


TEST (cmpa_absl, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  code[0] = R ("1011rrrr11111001");
  WRITE4 (1, randint (MEM + 50, MEMEND - 50) & ~1);
}


TEST (cmpibw_reg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("000011000r000rrr");  /* Randomly either byte or word. */
  code[1] = randnum ();
}


TEST (cmpil_reg, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0000110010000rrr");
  WRITE4 (1, randnum ());
}


TEST (cmpib_ind, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 0);
  randomize_mem ();
  code[0] = R ("0000110000000rrr") | (randint (2, 4) << 3);
  code[1] = randnum ();
}


TEST (cmpiw_ind, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  randomize_mem ();
  code[0] = R ("0000110001000rrr") | (randint (2, 4) << 3);
  code[1] = randnum ();
}


TEST (cmpil_ind, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  randomize_mem ();
  code[0] = R ("0000110010000rrr") | (randint (2, 4) << 3);
  WRITE4 (1, randnum ());
}


TEST (cmpmb, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 0);
  code[0] = R ("1011rrr100001rrr");
}


TEST (cmpmwl, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = R ("1011rrr100001rrr") | (randint (1, 2) << 6);  /* Word or long */
}


TEST (cmp2b_ind, X_BIT | Z_BIT | C_BIT, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 0);
  randomize_mem ();
  code[0] = R ("0000000011010rrr");
  code[1] = randint (0, 15) << 12;
}


TEST (cmp2wl_ind, X_BIT | Z_BIT | C_BIT, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  randomize_mem ();
  code[0] = R ("0000000011010rrr") | (randint (1, 2) << 9);
  code[1] = randint (0, 15) << 12;
}


TEST (dbcc, ALL_CCS, 3, WONT_CHANGE_MEMORY, 16)
{
  code[0] = 0x5480;   /* addql #2,d0 */
  code[1] = (R ("0101000011001000") /* dbra *-4 */
	     | randint (1, 7)
	     | ((times_called & 0xF) << 8));
  code[2] = -4;
}


/* NOTE!  All div's are special cased in the test driver so that if the V bit
 * is set by the real CPU, the N and Z bits are ignored.
 */
TEST (divs_reg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  int r1, r2;

  r1 = randint (0, 7);
  r2 = (r1 + randint (1, 7)) & 7;
  EM_DREG (r1) = randnum ();
  do { EM_DREG (r2) = randnum (); } while (cpu_state.regs[r2].uw.n == 0);
  code[0] = B ("1000000111000000") | (r1 << 9) | r2;
}


TEST (divs_same_reg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  int r1;
  r1 = randint (0, 7);

  do { EM_DREG (r1) = randnum (); } while (cpu_state.regs[r1].uw.n == 0);
  code[0] = B ("1000000111000000") | (r1 << 9) | r1;
}


TEST (divs_ind, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  uint16 *p;
  int i;

  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  for (i = 0, p = (uint16 *)mem; i < MEM_SIZE / sizeof (uint16); i++)
    *p++ = SWAPUW_IFLE (randint (1, 65535));
  code[0] = R ("1000rrr111000rrr") | (randint (2, 4) << 3);
}


TEST (divs_absl, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  uint16 *p;
  int i;

  for (i = 0, p = (uint16 *)mem; i < MEM_SIZE / sizeof (uint16); i++)
    *p++ = SWAPUW_IFLE (randint (1, 65535));
  code[0] = R ("1000rrr111111001") | (randint (2, 4) << 3);
  WRITE4 (1, randint (MEM + 50, MEMEND - 50) & ~1);
}


TEST (divu_reg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  int r1, r2;

  r1 = randint (0, 7);
  r2 = (r1 + randint (1, 7)) & 7;   /* Assure r1 != r2 */
  EM_DREG (r1) = randnum ();
  do { EM_DREG (r2) = randnum (); } while (cpu_state.regs[r2].uw.n == 0);
  code[0] = B ("1000000011000000") | (r1 << 9) | r2;
}


TEST (divu_same_reg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  int r1;
  r1 = randint (0, 7);
  do { EM_DREG (r1) = randnum (); } while (cpu_state.regs[r1].uw.n == 0);
  code[0] = B ("1000000011000000") | (r1 << 9) | r1;
}


TEST (divu_ind, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  uint16 *p;
  int i;

  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  for (i = 0, p = (uint16 *)mem; i < MEM_SIZE / sizeof (uint16); i++)
    *p++ = SWAPUW_IFLE (randint (1, 65535));
  code[0] = R ("1000rrr011000rrr") | (randint (2, 4) << 3);
}


TEST (divu_absl, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  uint16 *p;
  int i;

  for (i = 0, p = (uint16 *)mem; i < MEM_SIZE / sizeof (uint16); i++)
    *p++ = SWAPUW_IFLE (randint (1, 65535));
  code[0] = R ("1000rrr011111001") | (randint (2, 4) << 3);
  WRITE4 (1, randint (MEM + 50, MEMEND - 50) & ~1);
}


TEST (divul_ll_reg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  int divisor_reg = randint (0, 7);
  do
    {
      EM_DREG (divisor_reg) = randnum ();
    }
  while (EM_DREG (divisor_reg) == 0);
  code[0] = B ("0100110001000000") | divisor_reg;
  code[1] = R ("0rrr0r0000000rrr");
}


TEST (divul_ll_ind, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  uint16 *p;
  int i;

  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  for (i = 0, p = (uint16 *)mem; i < MEM_SIZE / sizeof (uint16); i++)
    *p++ = SWAPUW_IFLE (randint (1, 65535));
  code[0] = R ("0100110001000rrr") | (randint (2, 4) << 3);
  code[1] = R ("0rrr0r0000000rrr");
}


TEST (divul_ll_absl, ALL_CCS, 4, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  uint16 *p;
  int i;

  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  for (i = 0, p = (uint16 *)mem; i < MEM_SIZE / sizeof (uint16); i++)
    *p++ = SWAPUW_IFLE (randint (1, 65535));
  code[0] = B ("0100110001111001");
  code[1] = R ("0rrr0r0000000rrr");
  WRITE4 (2, randint (MEM + 50, MEMEND - 50) & ~1);
}


TEST (eor_reg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1011rrr100000rrr") | RANDOM_SIZE ();
}


TEST (eor_ind, ALL_CCS, 1, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = (R ("1011rrr100000rrr")
	     | RANDOM_SIZE ()
	     | randint (2, 4) << 3);
}


TEST (eor_d16, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("1011rrr100101rrr") | RANDOM_SIZE ();
  code[1] = randint (-250, 250) & ~1;
}


TEST (eor_absl, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1011rrr100111001") | RANDOM_SIZE ();
  WRITE4 (1, randint (MEM + 50, MEMEND - 50) & ~1);
}


TEST (eoribw_dreg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("000010100r000rrr");
  code[1] = randnum ();
}


TEST (eoril_dreg, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0000101010000rrr");
  WRITE4 (1, randnum ());
}


TEST (eoribw_ind, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("000010100r000rrr") | (randint (2, 4) << 3);
  code[1] = randnum ();
}


TEST (eoril_ind, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("0000101010000rrr") | (randint (2, 4) << 3);
  WRITE4 (1, randnum ());
}


TEST (eoribw_d16, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("000010100r101rrr");  /* randomly either byte or word. */
  code[1] = randnum ();
  code[2] = randint (-290, 290) & ~1;
}


TEST (eoril_d16, ALL_CCS, 4, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("0000101010101rrr");
  WRITE4 (1, randnum ());
  code[3] = randint (-290, 290) & ~1;
}


TEST (eoribw_absl, ALL_CCS, 4, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("000010100r111001");
  code[1] = randnum ();
  WRITE4 (2, randint (MEM + 4, MEMEND - 4) & ~1);
}


TEST (eoril_absl, ALL_CCS, 5, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = B ("0000101010111001");
  WRITE4 (1, randnum ());
  WRITE4 (3, randint (MEM + 4, MEMEND - 4) & ~1);
}


TEST (eori_to_ccr, ALL_CCS, 2, WONT_CHANGE_MEMORY, 32)
{
  code[0] = B ("0000101000111100");
  code[1] = times_called;
}


TEST (exg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  static int mode[] = { 8, 9, 17 };
  code[0] = R ("1100rrr100000rrr") | (mode[randint (0, 2)] << 3);
}


TEST (ext, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  static int mode[] = { 2, 3, 7 };
  code[0] = R ("0100100000000rrr") | (mode[randint (0, 2)] << 6);
}


TEST (jmp_absl, ALL_CCS, 5, WONT_CHANGE_MEMORY, 2)
{
  code[0] = B ("0100111011111001");
  WRITE4 (1, US_TO_SYN68K (&code[4]));
  code[3] = 0x5480;   /* addql #2,d0 */
  code[4] = 0x5480;   /* addql #2,d0 */
}


TEST (jmp_ind, ALL_CCS, 3, WONT_CHANGE_MEMORY, 8)
{
  EM_AREG (times_called & 7) = US_TO_SYN68K (&code[2]);
  code[0] = B ("0100111011010000") | (times_called & 7);
  code[1] = 0x5480;   /* addql #2,d0 */
  code[2] = 0x5480;   /* addql #2,d0 */
}


TEST (jmp_d16, ALL_CCS, 4, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  EM_AREG (times_called & 7) = randint (US_TO_SYN68K (&code[3] - 100),
					US_TO_SYN68K (&code[3] + 100)) & ~1;
  code[0] = B ("0100111011101000") | (times_called & 7);
  code[1] = US_TO_SYN68K (&code[3]) - EM_AREG (times_called & 7);
  code[2] = 0x5480;   /* addql #2,d0 */
  code[3] = 0x5480;   /* addql #2,d0 */
}


TEST (jsr_absl, ALL_CCS, 7, MIGHT_CHANGE_MEMORY, 2)
{
  EM_A7 = randint (MEM + 50, MEMEND - 50) & ~1;
  code[0] = B ("0100111010111001");
  WRITE4 (1, US_TO_SYN68K (&code[6]));
  code[3] = 0x5480;   /* addql #2,d0 */
  code[4] = 0x5480;   /* addql #2,d0 */
  code[5] = 0x6002;   /* bra *+2 */
  code[6] = 0x4E75;   /* rts */
}


TEST (jsr_ind, ALL_CCS, 5, MIGHT_CHANGE_MEMORY, 8)
{
  EM_AREG (times_called % 7) = US_TO_SYN68K (&code[4]);
  EM_A7 = randint (MEM + 50, MEMEND - 50) & ~1;
  code[0] = B ("0100111010010000") | (times_called % 7);
  code[1] = 0x5480;   /* addql #2,d0 */
  code[2] = 0x5480;   /* addql #2,d0 */
  code[3] = 0x6002;   /* bra *+2 */
  code[4] = 0x4E75;   /* rts */
}


TEST (jsr_d16, ALL_CCS, 6, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  EM_AREG (times_called % 7) = randint (US_TO_SYN68K (&code[5] - 100),
					US_TO_SYN68K (&code[5] + 100)) & ~1;
  EM_A7 = randint (MEM + 50, MEMEND - 50) & ~1;
  code[0] = B ("0100111010101000") | (times_called % 7);
  code[1] = US_TO_SYN68K (&code[5]) - EM_AREG (times_called % 7);
  code[2] = 0x5480;   /* addql #2,d0 */
  code[3] = 0x5480;   /* addql #2,d0 */
  code[4] = 0x6002;   /* bra *+2 */
  code[5] = 0x4E75;   /* rts */
}


TEST (lea_ind, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0100rrr111010rrr");
}


TEST (lea_d16, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0100rrr111101rrr");
  code[1] = randnum ();
}


TEST (lea_absw, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0100rrr111111000");
  code[1] = randnum ();
}


TEST (lea_absl, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0100rrr111111001");
  WRITE4 (1, randnum ());
}


TEST (linkw, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  EM_A7 = randint (MEM + 50, MEMEND - 50) & ~1;
  code[0] = R ("0100111001010rrr");
  code[1] = randnum ();
}


TEST (linkl, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  EM_A7 = randint (MEM + 50, MEMEND - 50) & ~1;
  code[0] = R ("0100100000001rrr");
  WRITE4 (1, randnum ());
}


TEST (move16_postinc_postinc, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 0);
  code[0] = R ("1111011000100rrr");
  code[1] = R ("1rrr000000000000");
}


TEST (move16_absl_ind, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 0);
  code[0] = R ("1111011000011rrr");
  WRITE4 (1, randint (MEM + 50, MEMEND - 50));
}


TEST (move16_absl_postinc, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 0);
  code[0] = R ("1111011000001rrr");
  WRITE4 (1, randint (MEM + 50, MEMEND - 50));
}


TEST (move16_ind_absl, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 0);
  code[0] = R ("1111011000010rrr");
  WRITE4 (1, randint (MEM + 50, MEMEND - 50));
}


TEST (move16_postinc_absl, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 0);
  code[0] = R ("1111011000000rrr");
  WRITE4 (1, randint (MEM + 50, MEMEND - 50));
}


TEST (moveb_dreg_dreg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0001rrr000000rrr");
}


TEST (movewl_reg_dreg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("001rrrr00000rrrr");   /* Randomly either word or long. */
}


TEST (moveb_ind_dreg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 0);
  code[0] = (R ("0001rrr000000rrr")
	     | (randint (2, 4) << 3));  /* Random amode [2, 4] */
}


TEST (movewl_ind_dreg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = (R ("001rrrr000000rrr")       /* Randomly word or long. */
	     | (randint (2, 4) << 3));  /* Random amode [2, 4] */
}


TEST (moveb_d16_dreg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0);
  code[0] = R ("0001rrr000101rrr");
  code[1] = randint (-290, 290);
}


TEST (movewl_d16_dreg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("001rrrr000101rrr");  /* Randomly either word or long. */
  code[1] = randint (-290, 290) & ~1;
}


TEST (moveb_absl_dreg, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  code[0] = R ("0001rrr000111001");
  WRITE4 (1, randint (MEM + 50, MEMEND - 50));
}


TEST (movewl_absl_dreg, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  code[0] = R ("001rrrr000111001");  /* Randomly either word or long. */
  WRITE4 (1, randint (MEM + 50, MEMEND - 50) & ~1);
}


TEST (movebw_const_dreg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("00r1rrr000111100");   /* Randomly either byte or word. */
  code[1] = randnum ();
}


TEST (movel_const_dreg, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0010rrr000111100");
  WRITE4 (1, randnum ());
}


TEST (moveb_dreg_ind, ALL_CCS, 1, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 0);
  code[0] = (R ("0001rrr000000rrr")
	     | (randint (2, 4) << 6));  /* Random amode [2, 4] */
}


TEST (movewl_reg_ind, ALL_CCS, 1, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = (R ("001rrrr00000rrrr")       /* Randomly word or long. */
	     | (randint (2, 4) << 6));  /* Random amode [2, 4] */
}


TEST (moveb_dreg_d16, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0);
  code[0] = R ("0001rrr101000rrr");
  code[1] = randint (-290, 290);
}


TEST (movewl_reg_d16, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("001rrrr10100rrrr");  /* Randomly either word or long. */
  code[1] = randint (-290, 290) & ~1;
}


TEST (moveb_dreg_absl, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0001001111000rrr");
  WRITE4 (1, randint (MEM + 50, MEMEND - 50));
}


TEST (movewl_reg_absl, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("001r00111100rrrr");  /* Randomly either word or long. */
  WRITE4 (1, randint (MEM + 50, MEMEND - 50) & ~1);
}


TEST (moveb_ind_ind, ALL_CCS, 1, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 0);
  code[0] = (R ("0001rrr000000rrr")
	     | (randint (2, 4) << 6)
	     | (randint (2, 4) << 3));
}


TEST (moveb_ind_d16, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0);
  code[0] = R ("0001rrr101000rrr") | (randint (2, 4) << 3);
  code[1] = randint (-290, 290);
}


TEST (moveb_d16_ind, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0);
  code[0] = R ("0001rrr000101rrr") | (randint (2, 4) << 6);
  code[1] = randint (-290, 290);
}


TEST (movewl_ind_d16, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("001rrrr101000rrr") | (randint (2, 4) << 3);
  code[1] = randint (-290, 290) & ~1;
}


TEST (movewl_d16_ind, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("001rrrr000101rrr") | (randint (2, 4) << 6);
  code[1] = randint (-290, 290) & ~1;
}


TEST (moveb_d16_d16, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0);
  code[0] = R ("0001rrr101101rrr");
  code[1] = randint (-290, 290);
  code[2] = randint (-290, 290);
}


TEST (movewl_d16_d16, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("001rrrr101101rrr");
  code[1] = randint (-290, 290) & ~1;
  code[2] = randint (-290, 290) & ~1;
}


TEST (moveb_ind_absl, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 0);
  code[0] = R ("0001001111000rrr") | (randint (2, 4) << 3);
  WRITE4 (1, randint (MEM + 50, MEMEND - 50));
}


TEST (movewl_ind_absl, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = R ("001r001111000rrr") | (randint (2, 4) << 3);
  WRITE4 (1, randint (MEM + 50, MEMEND - 50) & ~1);
}


TEST (moveb_absl_ind, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 0);
  code[0] = R ("0001rrr000111001") | (randint (2, 4) << 6);
  WRITE4 (1, randint (MEM + 50, MEMEND - 50));
}


TEST (movewl_absl_ind, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = R ("001rrrr000111001") | (randint (2, 4) << 6);
  WRITE4 (1, randint (MEM + 50, MEMEND - 50) & ~1);
}


TEST (moveb_absl_absl, ALL_CCS, 5, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = B ("0001001111111001");
  WRITE4 (1, randint (MEM + 50, MEMEND - 50));
  WRITE4 (3, randint (MEM + 50, MEMEND - 50));
}


TEST (movewl_absl_absl, ALL_CCS, 5, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("001r001111111001");
  WRITE4 (1, randint (MEM + 50, MEMEND - 50) & ~1);
  WRITE4 (3, randint (MEM + 50, MEMEND - 50) & ~1);
}


TEST (moveb_d16_absl, ALL_CCS, 4, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0);
  code[0] = R ("0001001111101rrr");
  code[1] = randint (-290, 290);
  WRITE4 (2, randint (MEM + 50, MEMEND - 50));
}


TEST (movewl_d16_absl, ALL_CCS, 4, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("001r001111101rrr");
  code[1] = randint (-290, 290) & ~1;
  WRITE4 (2, randint (MEM + 50, MEMEND - 50) & ~1);
}


TEST (moveb_absl_d16, ALL_CCS, 4, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0);
  code[0] = R ("0001rrr101111001");
  WRITE4 (1, randint (MEM + 50, MEMEND - 50));
  code[3] = randint (-290, 290);
}


TEST (movewl_absl_d16, ALL_CCS, 4, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("001rrrr101111001");
  WRITE4 (1, randint (MEM + 50, MEMEND - 50) & ~1);
  code[3] = randint (-290, 290) & ~1;
}


TEST (moveb_const_ind, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 0);
  code[0] = R ("0001rrr000111100") | (randint (2, 4) << 6);
  code[1] = randnum ();
}


TEST (moveb_const_d16, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0);
  code[0] = R ("0001rrr101111100");
  code[1] = randnum ();
  code[2] = randint (-290, 290);
}


TEST (moveb_const_absl, ALL_CCS, 4, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = B ("0001001111111100");
  code[1] = randnum ();
  WRITE4 (2, randint (MEM + 50, MEMEND - 50) & ~1);
}


TEST (movew_const_ind, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = R ("0011rrr000111100") | (randint (2, 4) << 6);
  code[1] = randnum ();
}


TEST (movew_const_d16, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("0011rrr101111100");
  code[1] = randnum ();
  code[2] = randint (-290, 290) & ~1;
}


TEST (movew_const_absl, ALL_CCS, 4, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = B ("0011001111111100");
  code[1] = randnum ();
  WRITE4 (2, randint (MEM + 50, MEMEND - 50) & ~1);
}


TEST (movel_const_ind, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = R ("0010rrr000111100") | (randint (2, 4) << 6);
  WRITE4 (1, randnum ());
}


TEST (movel_const_d16, ALL_CCS, 4, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("0010rrr101111100");
  WRITE4 (1, randnum ());
  code[3] = randint (-290, 290) & ~1;
}


TEST (movel_const_absl, ALL_CCS, 5, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = B ("0010001111111100");
  WRITE4 (1, randnum ());
  WRITE4 (3, randint (MEM + 50, MEMEND - 50) & ~1);
}


TEST (movea_reg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("001rrrr00100rrrr");
}


TEST (movea_ind, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = R ("001rrrr001000rrr") | (randint (2, 4) << 3);
}


TEST (movea_d16, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("001rrrr001101rrr");
  code[1] = randint (-290, 290) & ~1;
}


TEST (movea_absl, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  code[0] = R ("001rrrr001111001");
  WRITE4 (1, randint (MEM + 50, MEMEND - 50) & ~1);
}


TEST (moveaw_const, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0011rrr001111100");
  code[1] = randnum ();
}


TEST (moveal_const, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0010rrr001111100");
  WRITE4 (1, randnum ());
}


TEST (move_from_ccr_dreg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0100001011000rrr");
}


TEST (move_from_ccr_ind, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = R ("0100001011000rrr") | (randint (2, 4) << 3);
}


TEST (move_from_ccr_d16, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("0100001011101rrr");
  code[1] = randint (-290, 290) & ~1;
}


TEST (move_to_ccr_reg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0100010011000rrr");
}


TEST (move_to_ccr_ind, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = R ("0100010011000rrr") | (randint (2, 4) << 3);
}


TEST (movem_to_ind, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  int indreg = randint (0, 7);
  EM_AREG (indreg) = randint (MEM + 100, MEMEND - 100) & ~1;
  code[0] = R ("010010001r010000") | indreg;
  code[1] = randnum ();
}


TEST (movem_to_predec, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  int indreg = randint (0, 7);
  EM_AREG (indreg) = randint (MEM + 100, MEMEND - 100) & ~1;
  code[0] = R ("010010001r100000") | indreg;
  code[1] = randnum ();
}


TEST (movem_to_d16, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  int indreg = randint (0, 7);
  EM_AREG (indreg) = randint (MEM + 500, MEMEND - 500) & ~1;
  code[0] = R ("010010001r101000") | indreg;
  code[1] = randnum ();
  code[2] = randint (-290, 290) & ~1;
}


TEST (movem_to_absl, ALL_CCS, 4, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("010010001r111001");
  code[1] = randnum ();
  WRITE4 (2, randint (MEM + 100, MEMEND - 100) & ~1);
}


TEST (movem_from_d16, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  int indreg = randint (0, 7);
  randomize_mem ();
  EM_AREG (indreg) = randint (MEM + 500, MEMEND - 500) & ~1;
  code[0] = R ("010011001r101000") | indreg;
  code[1] = randnum ();
  code[2] = randint (-290, 290) & ~1;
}


TEST (movem_from_ind, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  int indreg = randint (0, 7);
  randomize_mem ();
  EM_AREG (indreg) = randint (MEM + 100, MEMEND - 100) & ~1;
  code[0] = R ("010011001r01r000") | indreg;  /* Randomly an@ or an@+ */
  code[1] = randnum ();
}


TEST (movem_from_absl, ALL_CCS, 4, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  code[0] = R ("010011001r111001");
  code[1] = randnum ();
  WRITE4 (2, randint (MEM + 100, MEMEND - 100) & ~1);
}


TEST (movep_to_mem, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0);
  code[0] = R ("0000rrr11r001rrr");
  code[1] = randint (-290, 290);
}


TEST (movep_from_mem, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0);
  code[0] = R ("0000rrr10r001rrr");
  code[1] = randint (-290, 290);
}


TEST (moveq, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0111rrr0rrrrrrrr");
}


TEST (mulsw_dreg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1100rrr111000rrr");
}


TEST (mulsw_ind, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = R ("1100rrr111000rrr") | (randint (2, 4) << 3);
}


TEST (mulsw_d16, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("1100rrr111101rrr");
  code[1] = randint (-290, 290) & ~1;
}


TEST (mulsw_absl, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  code[0] = R ("1100rrr111111001");
  WRITE4 (1, randint (MEM + 50, MEMEND - 50) & ~1);
}


TEST (muluw_dreg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1100rrr011000rrr");
}


TEST (muluw_ind, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = R ("1100rrr011000rrr") | (randint (2, 4) << 3);
}


TEST (muluw_d16, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("1100rrr011101rrr");
  code[1] = randint (-290, 290) & ~1;
}


TEST (muluw_absl, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  code[0] = R ("1100rrr011111001");
  WRITE4 (1, randint (MEM + 50, MEMEND - 50) & ~1);
}


TEST (mulsl_dreg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  int r1 = randint (0, 7);
  int r2 = (r1 + randint (1, 7)) & 7;    /* Assure r1 != r2 */
  code[0] = R ("0100110000000rrr");
  code[1] = R ("00001r0000000000") | (r1 << 12) | r2;
}


TEST (mulsl_ind, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  int r1 = randint (0, 7);
  int r2 = (r1 + randint (1, 7)) & 7;    /* Assure r1 != r2 */
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = R ("0100110000000rrr") | (randint (2, 4) << 3);
  code[1] = R ("00001r0000000000") | (r1 << 12) | r2;
}


TEST (mulsl_d16, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  int r1 = randint (0, 7);
  int r2 = (r1 + randint (1, 7)) & 7;    /* Assure r1 != r2 */
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("0100110000101rrr");
  code[1] = R ("00001r0000000000") | (r1 << 12) | r2;
  code[2] = randint (-290, 290) & ~1;
}


TEST (mulsl_absl, ALL_CCS, 4, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  int r1 = randint (0, 7);
  int r2 = (r1 + randint (1, 7)) & 7;    /* Assure r1 != r2 */
  randomize_mem ();
  code[0] = B ("0100110000111001");
  code[1] = R ("00001r0000000000") | (r1 << 12) | r2;
  WRITE4 (2, randint (MEM + 50, MEMEND - 50) & ~1);
}


TEST (mulul_dreg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  int r1 = randint (0, 7);
  int r2 = (r1 + randint (1, 7)) & 7;    /* Assure r1 != r2 */
  code[0] = R ("0100110000000rrr");
  code[1] = R ("00000r0000000000") | (r1 << 12) | r2;
}


TEST (mulul_ind, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  int r1 = randint (0, 7);
  int r2 = (r1 + randint (1, 7)) & 7;    /* Assure r1 != r2 */
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = R ("0100110000000rrr") | (randint (2, 4) << 3);
  code[1] = R ("00000r0000000000") | (r1 << 12) | r2;
}


TEST (mulul_d16, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  int r1 = randint (0, 7);
  int r2 = (r1 + randint (1, 7)) & 7;    /* Assure r1 != r2 */
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("0100110000101rrr");
  code[1] = R ("00000r0000000000") | (r1 << 12) | r2;
  code[2] = randint (-290, 290) & ~1;
}


TEST (mulul_absl, ALL_CCS, 4, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  int r1 = randint (0, 7);
  int r2 = (r1 + randint (1, 7)) & 7;    /* Assure r1 != r2 */
  randomize_mem ();
  code[0] = R ("0100110000111001");
  code[1] = R ("00000r0000000000") | (r1 << 12) | r2;
  WRITE4 (2, randint (MEM + 50, MEMEND - 50) & ~1);
}


TEST (nbcd_reg, X_BIT | Z_BIT | C_BIT, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0100100000000rrr");
}


TEST (nbcd_ind, X_BIT | Z_BIT | C_BIT, 1, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 0);
  code[0] = R ("0100100000000rrr") | (randint (2, 4) << 3);
}


TEST (nbcd_d16, X_BIT | Z_BIT | C_BIT, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0);
  code[0] = R ("0100100000101rrr");
  code[1] = randint (-290, 290);
}


TEST (nbcd_absl, X_BIT | Z_BIT | C_BIT, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = B ("0100100000111001");
  WRITE4 (1, randint (MEM + 50, MEMEND - 50));
}


TEST (neg_reg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0100010000000rrr") | RANDOM_SIZE ();
}


TEST (negb_ind, ALL_CCS, 1, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 0);
  code[0] = R ("0100010000000rrr") | (randint (2, 4) << 3);
}


TEST (negwl_ind, ALL_CCS, 1, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = (R ("0100010000000rrr")
	     | (randint (2, 4) << 3)
	     | (randint (1, 2) << 6));
}


TEST (negb_d16, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0);
  code[0] = R ("0100010000101rrr");
  code[1] = randint (-290, 290);
}


TEST (negwl_d16, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = (R ("0100010000101rrr") | (randint (1, 2) << 6));
  code[1] = randint (-290, 290) & ~1;
}


TEST (neg_absl, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = B ("0100010000111001") | RANDOM_SIZE ();
  WRITE4 (1, randint (MEM + 50, MEMEND - 50) & ~1);
}


TEST (negx_reg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0100000000000rrr") | RANDOM_SIZE ();
}


TEST (negxb_ind, ALL_CCS, 1, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 0);
  code[0] = R ("0100000000000rrr") | (randint (2, 4) << 3);
}


TEST (negxwl_ind, ALL_CCS, 1, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = (R ("0100000000000rrr")
	     | (randint (2, 4) << 3)
	     | (randint (1, 2) << 6));
}


TEST (negxb_d16, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0);
  code[0] = R ("0100000000101rrr");
  code[1] = randint (-290, 290);
}


TEST (negxwl_d16, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = (R ("0100000000101rrr") | (randint (1, 2) << 6));
  code[1] = randint (-290, 290) & ~1;
}


TEST (negx_absl, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = B ("0100000000111001") | RANDOM_SIZE ();
  WRITE4 (1, randint (MEM + 50, MEMEND - 50) & ~1);
}


TEST (nop, ALL_CCS, 1, WONT_CHANGE_MEMORY, 1)
{
  code[0] = B ("0100111001110001");
}


TEST (not_reg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0100011000000rrr") | RANDOM_SIZE ();
}


TEST (notb_ind, ALL_CCS, 1, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 0);
  code[0] = R ("0100011000000rrr") | (randint (2, 4) << 3);
}


TEST (notwl_ind, ALL_CCS, 1, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = (R ("0100011000000rrr")
	     | (randint (2, 4) << 3)
	     | (randint (1, 2) << 6));
}


TEST (notb_d16, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0);
  code[0] = R ("0100011000101rrr");
  code[1] = randint (-290, 290);
}


TEST (notwl_d16, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = (R ("0100011000101rrr") | (randint (1, 2) << 6));
  code[1] = randint (-290, 290) & ~1;
}


TEST (not_absl, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = B ("0100011000111001") | RANDOM_SIZE ();
  WRITE4 (1, randint (MEM + 50, MEMEND - 50) & ~1);
}


TEST (or_dreg_dreg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1000rrr000000rrr") | RANDOM_SIZE ();
}


TEST (or_ind_dreg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = (R ("1000rrr000000rrr")
	     | RANDOM_SIZE ()
	     | (randint (2, 4) << 3));
}


TEST (or_d16_dreg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("1000rrr000101rrr") | RANDOM_SIZE ();
  code[1] = randint (-290, 290) & ~1;
}


TEST (or_absl_dreg, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  code[0] = R ("1000rrr000111001") | RANDOM_SIZE ();
  WRITE4 (1, randint (MEM + 4, MEMEND - 4) & ~1);
}


TEST (orbw_const_dreg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1000rrr00r111100");  /* randomly byte or word */
  code[1] = randnum ();
}


TEST (orl_const_dreg, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1000rrr010111100");
  WRITE4 (1, randnum ());
}


TEST (or_dreg_ind, ALL_CCS, 1, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = (R ("1000rrr100000rrr")
	     | RANDOM_SIZE ()
	     | (randint (2, 4) << 3));
}


TEST (or_dreg_d16, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("1000rrr100101rrr") | RANDOM_SIZE ();
  code[1] = randint (-290, 290) & ~1;
}


TEST (or_dreg_absl, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1000rrr100111001") | RANDOM_SIZE ();
  WRITE4 (1, randint (MEM + 4, MEMEND - 4) & ~1);
}


TEST (oribw_dreg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("000000000r000rrr");
  code[1] = randnum ();
}


TEST (oril_dreg, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0000000010000rrr");
  WRITE4 (1, randnum ());
}


TEST (oribw_ind, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("000000000r000rrr") | (randint (2, 4) << 3);
  code[1] = randnum ();
}


TEST (oril_ind, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("0000000010000rrr") | (randint (2, 4) << 3);
  WRITE4 (1, randnum ());
}


TEST (oribw_d16, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("000000000r101rrr");  /* randomly either byte or word. */
  code[1] = randnum ();
  code[2] = randint (-290, 290) & ~1;
}


TEST (oril_d16, ALL_CCS, 4, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("0000000010101rrr");
  WRITE4 (1, randnum ());
  code[3] = randint (-290, 290) & ~1;
}


TEST (oribw_absl, ALL_CCS, 4, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("000000000r111001");
  code[1] = randnum ();
  WRITE4 (2, randint (MEM + 4, MEMEND - 4) & ~1);
}


TEST (oril_absl, ALL_CCS, 5, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = B ("0000000010111001");
  WRITE4 (1, randnum ());
  WRITE4 (3, randint (MEM + 4, MEMEND - 4) & ~1);
}


TEST (ori_to_ccr, ALL_CCS, 2, WONT_CHANGE_MEMORY, 32)
{
  code[0] = B ("0000000000111100");
  code[1] = times_called;
}


TEST (pack_reg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1000rrr101000rrr");
  code[1] = randnum ();
}


TEST (pack_mem, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 0);
  code[0] = R ("1000rrr101001rrr");
  code[1] = randnum ();
}


TEST (pea_ind, ALL_CCS, 1, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  EM_A7 = randint (MEM + 50, MEMEND - 50) & ~1;
  code[0] = R ("0100100001010rrr");
}


TEST (pea_d16, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  EM_A7 = randint (MEM + 50, MEMEND - 50) & ~1;
  code[0] = R ("0100100001101rrr");
  code[1] = randnum ();
}


TEST (pea_absw, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  EM_A7 = randint (MEM + 50, MEMEND - 50) & ~1;
  code[0] = B ("0100100001111000");
  code[1] = randnum ();
}


TEST (pea_absl, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  EM_A7 = randint (MEM + 50, MEMEND - 50) & ~1;
  code[0] = B ("0100100001111001");
  WRITE4 (1, randnum ());
}


TEST (rol_dx_dy, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1110rrr100111rrr") | RANDOM_SIZE ();
}


TEST (rol_const_dy, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1110rrr100011rrr") | RANDOM_SIZE ();
}


TEST (rol_ind, ALL_CCS, 1, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = R ("1110011111000rrr") | (randint (2, 4) << 3);
}


TEST (rol_d16, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("1110011111101rrr");
  code[1] = randint (-290, 290) & ~1;
}


TEST (rol_absl, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = B ("1110011111111001");
  WRITE4 (1, randint (MEM + 4, MEMEND - 4) & ~1);
}


TEST (ror_dx_dy, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1110rrr000111rrr") | RANDOM_SIZE ();
}


TEST (ror_const_dy, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1110rrr000011rrr") | RANDOM_SIZE ();
}


TEST (ror_ind, ALL_CCS, 1, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = R ("1110011011000rrr") | (randint (2, 4) << 3);
}


TEST (ror_d16, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("1110011011101rrr");
  code[1] = randint (-290, 290) & ~1;
}


TEST (ror_absl, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = B ("1110011011111001");
  WRITE4 (1, randint (MEM + 4, MEMEND - 4) & ~1);
}


TEST (roxl_dx_dy, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1110rrr100110rrr") | RANDOM_SIZE ();
}


TEST (roxl_const_dy, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1110rrr100010rrr") | RANDOM_SIZE ();
}


TEST (roxl_ind, ALL_CCS, 1, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = R ("1110010111000rrr") | (randint (2, 4) << 3);
}


TEST (roxl_d16, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("1110010111101rrr");
  code[1] = randint (-290, 290) & ~1;
}


TEST (roxl_absl, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = B ("1110010111111001");
  WRITE4 (1, randint (MEM + 4, MEMEND - 4) & ~1);
}


TEST (roxr_dx_dy, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1110rrr000110rrr") | RANDOM_SIZE ();
}


TEST (roxr_const_dy, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1110rrr000010rrr") | RANDOM_SIZE ();
}


TEST (roxr_ind, ALL_CCS, 1, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = R ("1110010011000rrr") | (randint (2, 4) << 3);
}


TEST (roxr_d16, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("1110010011101rrr");
  code[1] = randint (-290, 290) & ~1;
}


TEST (roxr_absl, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = B ("1110010011111001");
  WRITE4 (1, randint (MEM + 4, MEMEND - 4) & ~1);
}


TEST (rtd, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  uint32 addr;
  randomize_mem ();
  EM_A7 = randint (MEM + 50, MEMEND - 50) & ~1;
  addr = US_TO_SYN68K (&code[2]);
  ((uint8 *)(SYN68K_TO_US (EM_A7)))[0] = addr >> 24;
  ((uint8 *)(SYN68K_TO_US (EM_A7)))[1] = addr >> 16;
  ((uint8 *)(SYN68K_TO_US (EM_A7)))[2] = addr >> 8;
  ((uint8 *)(SYN68K_TO_US (EM_A7)))[3] = addr;
  code[0] = B ("0100111001110100");
  code[1] = randnum ();
}


TEST (rtr, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  uint32 addr, randcc;
  randomize_mem ();
  EM_A7 = randint (MEM + 50, MEMEND - 50) & ~1;

  addr = US_TO_SYN68K (&code[1]);
  randcc = randnum ();

  ((uint8 *)(SYN68K_TO_US (EM_A7)))[0] = randcc >> 8;
  ((uint8 *)(SYN68K_TO_US (EM_A7)))[1] = randcc;
  ((uint8 *)(SYN68K_TO_US (EM_A7)))[2] = addr >> 24;
  ((uint8 *)(SYN68K_TO_US (EM_A7)))[3] = addr >> 16;
  ((uint8 *)(SYN68K_TO_US (EM_A7)))[4] = addr >> 8;
  ((uint8 *)(SYN68K_TO_US (EM_A7)))[5] = addr;
  code[0] = B ("0100111001110111");
}


TEST (rts, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  uint32 addr;
  randomize_mem ();
  EM_A7 = randint (MEM + 50, MEMEND - 50) & ~1;
  addr = US_TO_SYN68K (&code[1]);
  ((uint8 *)(SYN68K_TO_US (EM_A7)))[0] = addr >> 24;
  ((uint8 *)(SYN68K_TO_US (EM_A7)))[1] = addr >> 16;
  ((uint8 *)(SYN68K_TO_US (EM_A7)))[2] = addr >> 8;
  ((uint8 *)(SYN68K_TO_US (EM_A7)))[3] = addr;
  code[0] = B ("0100111001110101");
}


TEST (sbcd_reg, X_BIT | Z_BIT | C_BIT, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1000rrr100000rrr");
}


TEST (sbcd_mem, X_BIT | Z_BIT | C_BIT, 1, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 0);
  code[0] = R ("1000rrr100001rrr");
}


TEST (Scc_reg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0101rrrr11000rrr");
}


TEST (Scc_ind, ALL_CCS, 1, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 0);
  code[0] = R ("0101rrrr11000rrr") | (randint (2, 4) << 3);
}


TEST (Scc_d16, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0);
  code[0] = R ("0101rrrr11101rrr");
  code[1] = randint (-290, 290);
}


TEST (Scc_absl, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0101rrrr11111001");
  WRITE4 (1, randint (MEM + 50, MEMEND - 50));
}


TEST (subb_dreg_dreg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1001rrr000000rrr");
}


TEST (subw_reg_dreg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1001rrr00100rrrr");
}


TEST (subl_reg_dreg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1001rrr01000rrrr");
}


TEST (sub_ind_dreg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = (R ("1001rrr000000rrr")
	     | RANDOM_SIZE ()
	     | (randint (2, 4) << 3)); /* Random amode [2, 4] */
}


TEST (sub_d16_dreg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("1001rrr000101rrr") | RANDOM_SIZE ();
  code[1] = randint (-290, 290) & ~1;
}


TEST (sub_absl_dreg, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("1001rrr000111001") | RANDOM_SIZE ();
  WRITE4 (1, randint (MEM + 4, MEMEND - 4) & ~1);
}


TEST (subb_const_dreg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1001rrr000111100");
  code[1] = randint (0, 65535);
}


TEST (subw_const_dreg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1001rrr001111100");
  code[1] = randint (0, 65535);
}


TEST (subl_const_dreg, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("1001rrr010111100");
  code[1] = randint (0, 65535);
  code[2] = randint (0, 65535);
}


TEST (sub_dreg_ind, ALL_CCS, 1, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = (R ("1001rrr100000rrr")
	     | RANDOM_SIZE ()
	     | (randint (2, 4) << 3));  /* Random amode [2, 4] */
}


TEST (sub_dreg_d16, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("1001rrr100101rrr") | RANDOM_SIZE ();
  code[1] = randint (-290, 290) & ~1;
}


TEST (sub_dreg_absl, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("1001rrr100111001") | RANDOM_SIZE ();
  WRITE4 (1, randint (MEM + 4, MEMEND - 4) & ~1);
}


TEST (subaw_reg_areg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1001rrr01100rrrr");
}


TEST (subal_reg_areg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1001rrr11100rrrr");
}


TEST (suba_ind_areg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = (R ("1001rrrr11000rrr")   /* Random size. */
	     | (randint (2, 4) << 3));  /* Random amode [2, 4]; */
}


TEST (suba_d16_areg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("1001rrrr11101rrr");  /* Random size. */
  code[1] = randint (-290, 290) & ~1;
}


TEST (suba_absl_areg, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("1001rrrr11111001");  /* Random size. */
  WRITE4 (1, randint (MEM + 4, MEMEND - 4) & ~1);
}


TEST (subaw_const_areg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1001rrr011111100");
  code[1] = randint (0, 65535);
}


TEST (subal_const_areg, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1001rrr111111100");
  code[1] = randint (0, 65535);
  code[2] = randint (0, 65535);
}


TEST (subibw_dreg, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("000001000r000rrr");  /* Randomly either byte or word */
  code[1] = randnum ();
}


TEST (subil_dreg, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0000010010000rrr");
  WRITE4 (1, randnum ());
}


TEST (subibw_ind, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = (R ("000001000r000rrr")    /* Randomly either byte or word. */
	     | (randint (2, 4) << 3));  /* Random amode [2, 4] */
  code[1] = randnum ();
}


TEST (subil_ind, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = (R ("0000010010000rrr")
	     | (randint (2, 4) << 3));  /* Random amode [2, 4] */
  WRITE4 (1, randnum ());
}


TEST (subibw_d16, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("000001000r101rrr");  /* Randomly either byte or word. */
  code[1] = randnum ();
  code[2] = randint (-290, 290) & ~1;
}


TEST (subil_d16, ALL_CCS, 4, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("0000010010101rrr");
  WRITE4 (1, randnum ());
  code[3] = randint (-290, 290) & ~1;
}


TEST (subibw_absl, ALL_CCS, 4, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("000001000r111001");  /* Randomly either byte or word. */
  code[1] = randnum ();

  WRITE4 (2, randint (MEM + 4, MEMEND - 4) & ~1);
}


TEST (subil_absl, ALL_CCS, 5, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0000010010111001");  /* Randomly either byte or word. */
  WRITE4 (1, randnum ());
  WRITE4 (3, randint (MEM + 4, MEMEND - 4) & ~1);
}


TEST (subq_dreg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0101rrr100000rrr") | RANDOM_SIZE ();
}


TEST (subq_areg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0101rrr100001rrr") | (randint (1, 2) << 6); /* word or long */
}


TEST (subq_ind, ALL_CCS, 1, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = (R ("0101rrr100000rrr")
	     | RANDOM_SIZE ()
	     | (randint (2, 4) << 3));  /* random amode [2, 4] */
}


TEST (subq_d16, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("0101rrr100101rrr") | RANDOM_SIZE ();
  code[1] = randint (-290, 290) & ~1;
}


TEST (subq_absl, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0101rrr100111001") | RANDOM_SIZE ();
  WRITE4 (1, randint (MEM + 4, MEMEND - 4) & ~1);
}


TEST (subx_reg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("1001rrr100000rrr") | RANDOM_SIZE ();
}


TEST (subx_mem, ALL_CCS, 1, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = R ("1001rrr100001rrr") | RANDOM_SIZE ();
}


TEST (swap, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0100100001000rrr");
}


TEST (tas_dreg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = R ("0100101011000rrr");
}


TEST (tas_ind, ALL_CCS, 1, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 0);
  code[0] = R ("0100101011000rrr") | (randint (2, 4) << 3);
}


TEST (tas_d16, ALL_CCS, 2, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0);
  code[0] = R ("0100101011101rrr");
  code[1] = randint (-290, 290);
}


TEST (tas_absl, ALL_CCS, 3, MIGHT_CHANGE_MEMORY, NO_LIMIT)
{
  code[0] = B ("0100101011111001");
  WRITE4 (1, randint (MEM + 50, MEMEND - 50));
}


TEST (tstb_dreg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  code[0] = R ("0100101000000rrr");
}


TEST (tstwl_reg, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  code[0] = R ("010010100000rrrr") | (randint (1, 2) << 6);
}


TEST (tstb_ind, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 0);
  code[0] = R ("0100101000000rrr") | (randint (2, 4) << 3);
}


TEST (tstwl_ind, ALL_CCS, 1, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 50, MEMEND - 50, 1);
  code[0] = (R ("0100101000000rrr")
	     | (randint (1, 2) << 6)  /* Randomly word or long */
	     | (randint (2, 4) << 3)); /* Random amode [2, 4]   */
}


TEST (tstb_d16, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 0);
  code[0] = R ("0100101000101rrr");
  code[1] = randint (-290, 290);
}


TEST (tstwl_d16, ALL_CCS, 2, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  randomize_regs (8, 8 + 7, MEM + 300, MEMEND - 300, 1);
  code[0] = R ("0100101000101rrr");
  code[1] = randint (-290, 290);
}


TEST (tst_absl, ALL_CCS, 3, WONT_CHANGE_MEMORY, NO_LIMIT)
{
  randomize_mem ();
  code[0] = B ("0100101000111001") | RANDOM_SIZE ();
  WRITE4 (1, randint (MEM + 50, MEMEND - 50) & ~1);
}
