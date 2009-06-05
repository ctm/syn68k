#include "syn68k_private.h"

#ifdef GENERATE_NATIVE_CODE

#include "native.h"
#include "i386-isa.h"
#include "interrupt.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifndef offsetof
#define offsetof(t, f) ((int32) &(((t *) 0)->f))
#endif


/* Set to TRUE if we are on an i486 or better, which means we can use
 * the bswap instruction.
 */
uint8 have_i486_p;


/* This stub will get copied to the native code entry point for a block,
 * and then backpatched if it's actually used.
 */
host_code_t native_to_synth_stub[] =
{
  0xBE, 0x78, 0x56, 0x34, 0x12,   /* movl $0x12345678,%esi */
  0xE9, 0x79, 0x56, 0x34, 0x12,   /* jmp $0x12345679       */
};


void
host_backpatch_native_to_synth_stub (Block *b, host_code_t *stub,
				     uint32 *synth_opcode)
{
  assert (!memcmp (stub, native_to_synth_stub, NATIVE_TO_SYNTH_STUB_BYTES));
  assert (sizeof native_to_synth_stub == NATIVE_TO_SYNTH_STUB_BYTES);

  /* Fill in the new synthetic PC so it points to the operands of
   * the first synthetic instruction.
   */
  *(uint32 *)(stub + 1) = ((uint32) synth_opcode) + 4;

  /* Fill in the jmp's relative offset so that it jumps to the
   * handler code for this synthetic opcode.  The branch is relative
   * to the _end_ of the bytes for the jmp instruction.
   */
  *(uint32 *)(stub + 6) = *synth_opcode - (((uint32) stub) + 10);
}


#ifdef SYNCHRONOUS_INTERRUPTS

/* These stubs use comparisons with %ebp as a hack.  Since our
 * interrupt changed flag is either -1 or LONG_MAX, as long as
 * %ebp is somewhere in between it will work.  This makes the code
 * slightly more compact and faster (since the alternative is to use a
 * constant zero, and instructions with two constants take longer to
 * decode).  We verify below that INTERRUPT_STATUS_CHANGED < %ebp and
 * INTERRUPT_STATUS_UNCHANGED >= %ebp.  We overwrite some of the
 * constant operands when we backpatch the stub.
 */
#ifdef USE_BIOS_TIMER
host_code_t check_interrupt_stub[] =
{
  0x64, 0x39, 0x2D, 0x78, 0x56, 0x34, 0x12,   /* cmpl %ebp,%fs:0x12345678 */
  0xB8, 0x78, 0x56, 0x34, 0x12,               /* movl $my_m68k_pc,%eax    */
  0x0F, 0x88, 0x78, 0x56, 0x34, 0x12,         /* js host_interrupt_...    */
};
#else  /* !USE_BIOS_TIMER */
host_code_t check_interrupt_stub[] =
{
#ifdef __CHECKER__
  0x83, 0x3D, 0x78, 0x56, 0x34, 0x12, 0x00, /* cmpl $0,0x12345678     */
  0xB8, 0x78, 0x56, 0x34, 0x12,             /* movl $my_m68k_pc,%eax  */
  0x0F, 0x88, 0x78, 0x56, 0x34, 0x12,       /* js host_interrupt_...  */
#else /* !__CHECKER__ */
  /* The "0x99" here is really "0xNN" and gets replaced below. */
  0x39, 0x6D, 0x99,                     /* cmpl %ebp,0xNN(%ebp)   */
  0xB8, 0x78, 0x56, 0x34, 0x12,         /* movl $my_m68k_pc,%eax  */
  0x0F, 0x88, 0x78, 0x56, 0x34, 0x12,   /* js host_interrupt_...  */
#endif /* !__CHECKER__ */
};
#endif  /* !USE_BIOS_TIMER */


/* This is just here to fool the C compiler. */
extern host_code_t host_interrupt_status_changed[]
  asm ("_host_interrupt_status_changed");

/* This is the stub called from the emulator when the interrupt status
 * changes.  %eax contains the m68k PC of the instruction about to be
 * executed when the interrupt was detected.  This stub calls
 * interrupt_process_any_pending() to handle the interrupt; that function
 * returns the m68k pc at which execution should resume.  Note that we
 * don't recursively enter the emulator here, we just adjust the m68k PC
 * and jump back into the fray.
 */
asm (".text\n\t"
     ".align 4\n"
     "_host_interrupt_status_changed:\n\t"
#ifdef USE_BIOS_TIMER
     "pushl %fs\n\t"
#endif
     "pushl %eax\n\t"
     "call _interrupt_process_any_pending\n\t"
     "pushl %eax\n\t"
     "call _hash_lookup_code_and_create_if_needed\n\t"
     "movl %eax,%esi\n\t"
     "addl $8,%esp\n\t"
     "movl (%eax),%eax\n\t"
     "addl $4,%esi\n\t"
#ifdef USE_BIOS_TIMER
     "popl %fs\n\t"
#endif
     "jmp *%eax");


void
host_backpatch_check_interrupt_stub (Block *b, host_code_t *stub)
{
  assert (!memcmp (stub, check_interrupt_stub, CHECK_INTERRUPT_STUB_BYTES));
  assert (sizeof check_interrupt_stub == CHECK_INTERRUPT_STUB_BYTES);

  /* No need to replace the flag's address; this part of
   * check_interrupt_stub[] was already patched in host_native_code_init.
   */

  /* Move this block's m68k start address into %eax. */
  *(uint32 *)(stub + CHECK_INTERRUPT_STUB_BYTES - 10) = b->m68k_start_address;

  /* Fill in the conditional branch's relative offset to cause it
   * to branch to the interrupt handler.
   */
  *(uint32 *)(stub + CHECK_INTERRUPT_STUB_BYTES - 4)
    = (((int32) host_interrupt_status_changed)
       - (((int32) stub) + CHECK_INTERRUPT_STUB_BYTES));
}
#endif  /* SYNCHRONOUS_INTERRUPTS */


void
host_native_code_init ()
{
  uint32 scratch1, scratch2;

#ifdef __GNUC__
  uint8 i486_p;

  /* Adapted from _Assembly Language: For Real Programmers Only_ p. 561.
   * This code determines if we are on an i486 or higher.  We care because
   * those chips have the "bswap" instruction for byte swapping.
   */
  asm ("pushfl\n\t"
       "pushfl\n\t"
       "popl %0\n\t"
       "xorl $0x40000,%0\n\t"
       "pushl %0\n\t"
       "popfl\n\t"
       "pushfl\n\t"
       "popl %1\n\t"
       "cmpl %0,%1\n\t"
       "jnz 1f\n\t"
       "xorl $0x40000,%0\n\t"
       "pushl %0\n\t"
       "popfl\n\t"
       "pushfl\n\t"
       "popl %1\n\t"
       "cmpl %0,%1\n"
       "1:\n\t"
       "setz %b2\n\t"
       "popfl"
       : "=r" (scratch1), "=r" (scratch2), "=q" (i486_p));

  have_i486_p = i486_p;
#else  /* !__GNUC__ */
#warning "We don't know if we're on an i486!"
  have_i486_p = FALSE;  /* Assume the worst. */
#endif /* !__GNUC__ */

#ifdef SYNCHRONOUS_INTERRUPTS
#ifndef __CHECKER__
  /* This is for the %ebp hack we use in the check_interrupt stub. */
  assert (INTERRUPT_STATUS_CHANGED < ((int32) &cpu_state)
	  && INTERRUPT_STATUS_UNCHANGED >= ((int32) &cpu_state));
#endif

#ifdef USE_BIOS_TIMER
  *(uint32 *)(&check_interrupt_stub[3]) = dos_interrupt_flag_addr;
#else   /* !USE_BIOS_TIMER */
  assert (offsetof (CPUState, interrupt_status_changed) < 128);
# ifdef __CHECKER__
  *(void **)(&check_interrupt_stub[2]) = &cpu_state.interrupt_status_changed;
# else /* !__CHECKER__ */
  check_interrupt_stub[2] = offsetof (CPUState, interrupt_status_changed);
# endif /* !__CHECKER__ */
#endif  /* !USE_BIOS_TIMER */

#endif  /* SYNCHRONOUS_INTERRUPTS */
}


int
host_alloc_reg (cache_info_t *c, host_code_t **codep,
		unsigned cc_spill_if_changed,
		host_reg_mask_t legal)
{
  static int last_smashed = 0;  /* Explained below. */
  guest_reg_status_t *r;
  int i, reg;

  /* First do a quick check for a totally free register.  We want
   * to try the higher registers first, because they are less in demand,
   * and if this guy actually wants one, let's give it to him.
   */
  for (i = NUM_HOST_REGS - 1; i >= 0; i--)
    if (legal & (1L << i))
      {
	if (c->host_reg_to_guest_reg[i] == NO_REG)
	  return i;
      }

  /* Nope, didn't find any empty ones!  Look for a non-dirty one.  If we
   * don't find that, take anything we can get.  We keep a static variable
   * around to keep track of the last cached register we took from someone.
   * This way we can cycle through instead of continually hammering
   * one recently used register.
   */
  reg = NO_REG;
  for (i = last_smashed - 1; i >= 0; i--)
    if (legal & (1L << i))
      {
	r = &c->guest_reg_status[c->host_reg_to_guest_reg[i]];
	if (!r->dirty_without_offset_p
	    && (r->mapping != MAP_OFFSET || r->offset == 0))
	  goto found_it;
	if (reg == NO_REG)
	  reg = i;
      }
  for (i = NUM_HOST_REGS - 1; i >= last_smashed; i--)
    if (legal & (1L << i))
      {
	r = &c->guest_reg_status[c->host_reg_to_guest_reg[i]];
	if (!r->dirty_without_offset_p
	    && (r->mapping != MAP_OFFSET || r->offset == 0))
	  goto found_it;
	if (reg == NO_REG)
	  reg = i;
      }

  if (reg == NO_REG)
    return NO_REG;
  i = reg;

 found_it:
  last_smashed = i;
  host_spill_reg (c, codep, cc_spill_if_changed, c->host_reg_to_guest_reg[i]);
  return i;
}


int
host_movel_reg_reg (COMMON_ARGS, int32 src_reg, int32 dst_reg)
{
  return i386_movl_reg_reg (COMMON_ARG_NAMES, src_reg, dst_reg);
}


void
host_cache_reg (cache_info_t *c, host_code_t **codep, 
		unsigned cc_spill_if_changed, int guest_reg,
		int host_reg)
{
  guest_reg_status_t *r;
  
  /* Load up the register. */
  /* NOTE:  you cannot use %ebp as the base without any constant offset
   * on the i386.  That is a special "escape" which means something else.
   * You must use an explicit 0 offset if that's what you want.
   */
  i386_movl_indoff_reg (c, codep, cc_spill_if_changed, M68K_CC_NONE, NO_REG,
			offsetof (CPUState, regs[guest_reg].ul.n),
			REG_EBP, host_reg);

  /* Set up the guest reg info. */
  r = &c->guest_reg_status[guest_reg];
  r->host_reg               = host_reg;
  r->mapping                = MAP_NATIVE;
  r->dirty_without_offset_p = FALSE;

  /* Keep track of what this host register is doing. */
  c->host_reg_to_guest_reg[host_reg] = guest_reg;
}


void
host_unoffset_reg (cache_info_t *c, host_code_t **codep,
		   unsigned cc_spill_if_changed,
		   int guest_reg)
{
  guest_reg_status_t *r;
  int host_reg;

  r = &c->guest_reg_status[guest_reg];
  host_reg = r->host_reg;
  if (host_reg != NO_REG && r->mapping == MAP_OFFSET)
    {
      int32 offset = r->offset;

      if (offset != 0)
	{
	  if (!cc_spill_if_changed)
	    {
	      if (offset == 1)
		i386_incl_reg (c, codep, cc_spill_if_changed, M68K_CC_NONE,
			       NO_REG, host_reg);
	      else if (offset == -1)
		i386_decl_reg (c, codep, cc_spill_if_changed, M68K_CC_NONE,
			       NO_REG, host_reg);
	      else if (offset < 0)  /* Same speed but more readable. */
		i386_subl_imm_reg (c, codep, cc_spill_if_changed, M68K_CC_NONE,
				   NO_REG, -offset, host_reg);
	      else
		i386_addl_imm_reg (c, codep, cc_spill_if_changed, M68K_CC_NONE,
				   NO_REG, offset, host_reg);
	    }
	  else  /* We can offset without spilling any cc's with leal. */
	    {
	      i386_leal_indoff (c, codep, M68K_CC_NONE, M68K_CC_NONE,
				NO_REG, offset, host_reg, host_reg);
	    }
	  r->dirty_without_offset_p = TRUE;
	}
      r->mapping = MAP_NATIVE;
    }
}


void
host_unoffset_regs (cache_info_t *c, host_code_t **codep,
		    unsigned cc_spill_if_changed)
{
  int i;
  for (i = NUM_GUEST_REGS - 1; i >= 0; i--)
    host_unoffset_reg (c, codep, cc_spill_if_changed, i);
}


/* This canonicalizes all registers so they are ready to simply
 * be written out to memory.
 */
inline void
make_dirty_regs_native (cache_info_t *c, host_code_t **codep,
			unsigned cc_spill_if_changed)
{
  int i;

  for (i = NUM_GUEST_REGS - 1; i >= 0; i--)
    {
      guest_reg_status_t *r;
      int host_reg;

      r = &c->guest_reg_status[i];
      host_reg = r->host_reg;
      if (host_reg != NO_REG)
	{
	  switch (r->mapping)
	    {
	    case MAP_NATIVE:
	      break;
	    case MAP_OFFSET:
	      if (r->offset != 0)
		{
		  if (!cc_spill_if_changed)
		    {
		      if (r->offset == 1)
			i386_incl_reg (c, codep, cc_spill_if_changed,
				       M68K_CC_NONE, NO_REG, host_reg);
		      else if (r->offset == -1)
			i386_decl_reg (c, codep, cc_spill_if_changed,
				       M68K_CC_NONE, NO_REG, host_reg);
		      else
			i386_addl_imm_reg (c, codep, cc_spill_if_changed,
					   M68K_CC_NONE, NO_REG, r->offset,
					   host_reg);
		    }
		  else /* We can offset without spilling any cc's with leal. */
		    {
		      i386_leal_indoff (c, codep, M68K_CC_NONE,
					M68K_CC_NONE, NO_REG, r->offset,
					host_reg, host_reg);
		    }

		  r->dirty_without_offset_p = TRUE;
		}
	      break;
	    case MAP_SWAP16:
	      if (r->dirty_without_offset_p)
		HOST_SWAP16 (host_reg);
	      break;
	    case MAP_SWAP32:
	      if (r->dirty_without_offset_p)
		HOST_SWAP32 (host_reg);
	      break;
	    default:
	      abort ();
	    }

	  r->mapping = MAP_NATIVE;
	}
    }
}


inline void
host_spill_reg (cache_info_t *c, host_code_t **codep,
		unsigned cc_spill_if_changed,
		int guest_reg)
{
  guest_reg_status_t *r;
  int host_reg;

  r = &c->guest_reg_status[guest_reg];
  host_reg = r->host_reg;
  if (host_reg != NO_REG)
    {
      if (r->dirty_without_offset_p
	  || (r->mapping == MAP_OFFSET && r->offset != 0))
	{
	  /* Canonicalize the cached register before spilling it out. */
	  switch (r->mapping)
	    {
	    case MAP_NATIVE:
	      break;
	    case MAP_OFFSET:
	      host_unoffset_reg (c, codep, cc_spill_if_changed, guest_reg);
	      break;
	    case MAP_SWAP16:
	      HOST_SWAP16 (host_reg);
	      break;
	    case MAP_SWAP32:
	      host_swap32 (c, codep, cc_spill_if_changed, M68K_CC_NONE, NO_REG,
			   host_reg);
	      break;
	    default:
	      abort ();
	    }
	  
	  /* NOTE: you cannot use %ebp as the base without any
	   * constant offset on the i386.  That is a special "escape"
	   * which means something else.  You must use an explicit 0
	   * offset if that's what you want.
	   */
	  i386_movl_reg_indoff (c, codep, cc_spill_if_changed, M68K_CC_NONE,
				NO_REG, host_reg,
				offsetof (CPUState, regs[guest_reg].ul.n),
				REG_EBP);
	  r->mapping = MAP_NATIVE;
	  r->dirty_without_offset_p = FALSE;
	}
      c->host_reg_to_guest_reg[host_reg] = NO_REG;
      r->host_reg = NO_REG;
    }
}


void
host_spill_regs (cache_info_t *c, host_code_t **codep,
		 unsigned cc_spill_if_changed)
{
  int i;

  /* Canonicalize all of the registers, and then spill all the dirty ones.
   * Doing it this way gives us slightly better Pentium pairability,
   * since instead of code like:
   *    addl $4,%edi
   *    movl %edi,_a0
   *    addl $4,%esi
   *    movl %esi,_a1
   *    rorw $8,%eax
   *    movl %eax,_d0
   *
   * we get:
   *
   *    addl $4,%edi
   *    addl $4,%esi
   *    rorw $8,%eax
   *    movl %edi,_a0
   *    movl %esi,_a1
   *    movl %eax,_d0
   *
   * Not a huge win, since the spill of the previous guy can overlap with
   * the canonicalization of the next one, but this is a relatively
   * cheap optimization.
   */
  make_dirty_regs_native (c, codep, cc_spill_if_changed);
  for (i = NUM_GUEST_REGS - 1; i >= 0; i--)
    {
      guest_reg_status_t *r;
      int host_reg;

      r = &c->guest_reg_status[i];
      host_reg = r->host_reg;
      
      if (host_reg != NO_REG)
	{
	  if (r->dirty_without_offset_p)
	    {
	      /* NOTE: you cannot use %ebp as the base without any
	       * constant offset on the i386.  That is a special "escape"
	       * which means something else.  You must use an explicit 0
	       * offset if that's what you want.
	       */
	      i386_movl_reg_indoff (c, codep, cc_spill_if_changed,
				    M68K_CC_NONE, NO_REG, host_reg,
				    offsetof (CPUState, regs[i].ul.n),
				    REG_EBP);
	      r->dirty_without_offset_p = FALSE;
	    }

	  c->host_reg_to_guest_reg[host_reg] = NO_REG;
	  r->host_reg = NO_REG;
	}
    }
}


void
host_spill_cc_bits (cache_info_t *c, host_code_t **codep, unsigned spill_cc)
{
  unsigned cc;

  assert (!(spill_cc & ~M68K_CC_ALL));

  cc = spill_cc & c->cached_cc & c->dirty_cc;

  if (cc)
    {
      if (cc & M68K_CCZ)
	i386_setnz_indoff (c, codep, 0, 0, NO_REG,
			   offsetof (CPUState, ccnz), REG_EBP);
      if (cc & M68K_CCN)
	i386_sets_indoff  (c, codep, 0, 0, NO_REG,
			   offsetof (CPUState, ccn), REG_EBP);
      if (cc & M68K_CCC)
	i386_setc_indoff  (c, codep, 0, 0, NO_REG,
			   offsetof (CPUState, ccc), REG_EBP);
      if (cc & M68K_CCV)
	i386_seto_indoff  (c, codep, 0, 0, NO_REG,
			   offsetof (CPUState, ccv), REG_EBP);
      if (cc & M68K_CCX)
	i386_setc_indoff  (c, codep, 0, 0, NO_REG,
			   offsetof (CPUState, ccx), REG_EBP);

      c->dirty_cc &= ~cc;
    }

  /* We typically call this when we are about to clobber cc bits, so we
   * should mark them as not being cached.  In special circumstances,
   * like conditional branches, we'll want to remember that the cc bits
   * _are_ actually cached.
   */
  c->cached_cc &= ~spill_cc;
}


int
host_swap16 (COMMON_ARGS, int32 host_reg)
{
  HOST_SWAP16 (host_reg);
  return 0;
}


int
host_swap16_to_32 (COMMON_ARGS, int32 host_reg)
{
  i386_rorl_imm_reg (c, codep, cc_spill_if_changed, M68K_CC_NONE, NO_REG, 16,
		     host_reg);
  return i386_rorw_imm_reg (c, codep, 0, M68K_CC_NONE, NO_REG, 8,
			    host_reg);
}


int
host_swap32 (COMMON_ARGS, int32 host_reg)
{
  if (have_i486_p)
    {
      i386_bswap (c, codep, cc_spill_if_changed, M68K_CC_NONE, NO_REG,
		  host_reg);
    }
  else
    {
      i386_rorw_imm_reg (c, codep, cc_spill_if_changed, M68K_CC_NONE, NO_REG,
			 8, host_reg);
      i386_rorl_imm_reg (c, codep, M68K_CC_NONE,        M68K_CC_NONE, NO_REG,
			 16, host_reg);
      i386_rorw_imm_reg (c, codep, M68K_CC_NONE,        M68K_CC_NONE, NO_REG,
			 8, host_reg);
    }

  return 0;
}


int
host_swap32_to_16 (COMMON_ARGS, int32 host_reg)
{
  i386_rorw_imm_reg (c, codep, cc_spill_if_changed, M68K_CC_NONE, NO_REG,
		     8,  host_reg);
  return i386_rorl_imm_reg (c, codep, 0, M68K_CC_NONE, NO_REG,
			    16, host_reg);
}

#endif
