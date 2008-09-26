#include "syn68k_private.h"

#ifdef GENERATE_NATIVE_CODE

#include "native.h"
#include "i386-isa.h"
#include "i386-aux.h"
#include "hash.h"
#include "trap.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef DEBUG
#define CHECK_ADDR_REG(n)				\
assert (c->host_reg_to_guest_reg[n] == NO_REG		\
	|| (c->host_reg_to_guest_reg[n] >= 8		\
	    && c->host_reg_to_guest_reg[n] < 16))
#else
#define CHECK_ADDR_REG(n)
#endif

#ifndef offsetof
#define offsetof(t, f) ((int32) &(((t *) 0)->f))
#endif


#define HOST_TEST_REG(size)					    	    \
int									    \
host_test ## size ## _reg (COMMON_ARGS, int32 reg)			    \
{									    \
  i386_test ## size ## _reg_reg (COMMON_ARG_NAMES, reg, reg);		    \
  return 0;								    \
}

HOST_TEST_REG (b)
HOST_TEST_REG (l)


int
host_testw_reg (COMMON_ARGS, int32 reg)
{
  /* Word ops are slow on the i386, so do a testb if we can;
   * if we don't need Z, testw %ax,%ax -> testb %ah,%ah.
   */
  if (!(cc_to_compute & M68K_CCZ) && reg < 4)
    i386_testb_reg_reg (COMMON_ARG_NAMES, reg + 4, reg + 4);
  else
    i386_testw_reg_reg (COMMON_ARG_NAMES, reg, reg);
  return 0;
}


/* Tests a swapped register.  The register is assumed to be SWAP16 for word
 * tests and SWAP32 for long tests.
 */
#define HOST_TEST_SWAPPED_REG(size, swap, save, restore)		\
int									\
host_test ## size ## _swapped_reg (COMMON_ARGS, int32 reg)		\
{									\
  if (!(cc_to_compute & M68K_CCZ) && reg < 4)				\
    i386_testb_reg_reg (COMMON_ARG_NAMES, reg, reg);			\
  else if (!(cc_to_compute & M68K_CCN))					\
    i386_test ## size ## _reg_reg (COMMON_ARG_NAMES, reg, reg);		\
  else									\
    {									\
      save;								\
      swap (COMMON_ARG_NAMES, reg);					\
      i386_test ## size ## _reg_reg (COMMON_ARG_NAMES, reg, reg); 	\
      restore;								\
    }									\
    									\
  return 0;								\
}


HOST_TEST_SWAPPED_REG (w, host_swap16,
		       i386_pushl (COMMON_ARG_NAMES, reg),
		       i386_popl (COMMON_ARG_NAMES, reg))
HOST_TEST_SWAPPED_REG (l, host_swap32,
		       if (!have_i486_p)
		         i386_pushl (COMMON_ARG_NAMES, reg),
		       (have_i486_p ? i386_bswap (COMMON_ARG_NAMES, reg)
			: i386_popl (COMMON_ARG_NAMES, reg)))


/* This is used when we need to sign extend a SWAP16 address register
 * and possible compute cc bits.  This happens often when we
 * do movew _addr,a0...m68k semantics demand sign extension.
 */
int
host_swap16_sext_test_reg (COMMON_ARGS, int32 reg)
{
  HOST_SWAP16 (reg);
  i386_movswl_reg_reg (COMMON_ARG_NAMES, reg, reg);
  if (cc_to_compute)
    i386_testl_reg_reg (COMMON_ARG_NAMES, reg, reg);
  return 0;
}


/* A special handler for adding, subtracting, or comparing two registers.
 * The problem is that we can get into situations like:
 *    cmpl a0@-,a0
 * where a0 becomes offset after the instruction starts.  This
 * will properly handle offset registers (by unoffsetting them before
 * the compare).
 */
#define ADD_SUB_CMP_OFFSET(op)						\
int									\
host_ ## op ## l_reg_reg (COMMON_ARGS, int32 reg1, int32 reg2)		\
{									\
  int32 gr1, gr2;							\
									\
  gr1 = c->host_reg_to_guest_reg[reg1];					\
  if (gr1 != NO_REG)							\
    host_unoffset_reg (c, codep, cc_spill_if_changed, gr1);		\
									\
  gr2 = c->host_reg_to_guest_reg[reg2];					\
  if (gr2 != NO_REG)							\
    host_unoffset_reg (c, codep, cc_spill_if_changed, gr2);		\
									\
  return i386_ ## op ## l_reg_reg (COMMON_ARG_NAMES, reg1, reg2);	\
}

ADD_SUB_CMP_OFFSET (add)
ADD_SUB_CMP_OFFSET (sub)
ADD_SUB_CMP_OFFSET (cmp)


/* This performs a binary bitwise op on a register when you are
 * either doing a byte sized op, or you don't care about the N bit.
 * This can only handle SWAP32 for long sized ops.
 */
#define HOST_BITWISE_IMM_REG(op, size, mask, size_mask, byte_p)		\
int									\
host_ ## op ## size ## _imm_reg (COMMON_ARGS, int32 val, int32 reg)	\
{									\
  int guest_reg = c->host_reg_to_guest_reg[reg];			\
									\
  val = (val & (size_mask)) | (mask);					\
									\
  switch (c->guest_reg_status[guest_reg].mapping)			\
    {									\
    case MAP_NATIVE:							\
      if (byte_p || (cc_to_compute & (M68K_CCN | M68K_CCZ)))		\
	i386_ ## op ## size ## _imm_reg (COMMON_ARG_NAMES, val, reg);	\
      else								\
	i386_ ## op ## l_imm_reg (COMMON_ARG_NAMES, val | (mask), reg);	\
      break;								\
    case MAP_SWAP16:							\
      /* andb $12,%ah instead of andb $12,%al?  Since we're doing	\
       * a byte op we know we must have a byte reg.			\
       */								\
      if (byte_p)							\
	i386_ ## op ## b_imm_reg (COMMON_ARG_NAMES, val, reg + 4);	\
      else if (cc_to_compute & (M68K_CCN | M68K_CCZ))			\
	i386_ ## op ## size ## _imm_reg (COMMON_ARG_NAMES,		\
					 (SWAPUW_IFLE (val)		\
					  | (val & 0xFFFF0000)),	\
					 reg);				\
      else								\
	i386_ ## op ## l_imm_reg (COMMON_ARG_NAMES,			\
				  (SWAPUW_IFLE (val)			\
				   | (val & 0xFFFF0000)),		\
				  reg);					\
      break;								\
    case MAP_SWAP32:							\
      if (cc_to_compute & (M68K_CCN | M68K_CCZ))			\
	{								\
	  if (byte_p)							\
	    return 1;   /* Can't easily get it. */			\
	  i386_ ## op ## size ## _imm_reg (COMMON_ARG_NAMES,		\
					   SWAPUL_IFLE (val), reg);	\
	}								\
      else								\
	i386_ ## op ## l_imm_reg (COMMON_ARG_NAMES,			\
				  SWAPUL_IFLE (val), reg);		\
      break;								\
    default:								\
      abort ();								\
    }									\
									\
  c->guest_reg_status[guest_reg].dirty_without_offset_p = TRUE;		\
									\
  return 0;								\
}

HOST_BITWISE_IMM_REG (and, b, 0xFFFFFF00, 0x000000FF, TRUE)
HOST_BITWISE_IMM_REG (and, w, 0xFFFF0000, 0x0000FFFF, FALSE)
HOST_BITWISE_IMM_REG (and, l, 0x00000000, 0xFFFFFFFF, FALSE)

HOST_BITWISE_IMM_REG (or, b, 0, 0x000000FF, TRUE)
HOST_BITWISE_IMM_REG (or, w, 0, 0x0000FFFF, FALSE)
HOST_BITWISE_IMM_REG (or, l, 0, 0xFFFFFFFF, FALSE)

HOST_BITWISE_IMM_REG (xor, b, 0, 0x000000FF, TRUE)
HOST_BITWISE_IMM_REG (xor, w, 0, 0x0000FFFF, FALSE)
HOST_BITWISE_IMM_REG (xor, l, 0, 0xFFFFFFFF, FALSE)


#define HOST_BITWISE_IMM_ABS(op, size, swapfunc)			    \
int									    \
host_ ## op ## size ## _imm_abs (COMMON_ARGS, int32 val, int32 addr)	    \
{									    \
  /* Only guaranteed to compute CVZ carry bits; N will of course be	    \
   * botched.  For cmp this only computes Z correctly.			    \
   */									    \
  return i386_ ## op ## size ## _imm_abs (COMMON_ARG_NAMES, swapfunc (val), \
					  (int32) SYN68K_TO_US (addr));	    \
}


HOST_BITWISE_IMM_ABS (and, w, SWAPUW)
HOST_BITWISE_IMM_ABS (and, l, SWAPUL)

HOST_BITWISE_IMM_ABS (or, w, SWAPUW)
HOST_BITWISE_IMM_ABS (or, l, SWAPUL)

HOST_BITWISE_IMM_ABS (xor, w, SWAPUW)
HOST_BITWISE_IMM_ABS (xor, l, SWAPUL)

HOST_BITWISE_IMM_ABS (cmp, w, SWAPUW)
HOST_BITWISE_IMM_ABS (cmp, l, SWAPUL)


#define HOST_BITWISE_IMM_MEM(size, op, amode, swapfunc, preoff, postoff,     \
			     adjust_p)					     \
int									     \
host_ ## op ## size ## _imm_ ## amode (COMMON_ARGS, int32 val, OFF int32 reg)\
{									     \
  int32 regoff;								     \
  int offset_p;								     \
  int guest_reg = c->host_reg_to_guest_reg[reg];			     \
									     \
  CHECK_ADDR_REG (reg);							     \
									     \
  /* Compensate for a7 predec/postinc byte size means +-2 nonsense. */	     \
  if (adjust_p && ((preoff) & 1) && guest_reg == 15)  /* a7? */		     \
    regoff = (preoff) * 2;						     \
  else									     \
    regoff = preoff;							     \
									     \
  offset_p = (c->guest_reg_status[guest_reg].mapping == MAP_OFFSET);	     \
  if (offset_p)								     \
    regoff += c->guest_reg_status[guest_reg].offset;			     \
									     \
  /* Compensate for offset memory. */					     \
  regoff = (int32) SYN68K_TO_US (regoff);				     \
									     \
  /* Actually perform the operation. */					     \
  if (regoff == 0)							     \
    i386_ ## op ## size ## _imm_ind (COMMON_ARG_NAMES,		     	     \
				     swapfunc (val), reg);	    	     \
  else									     \
    i386_ ## op ## size ## _imm_indoff (COMMON_ARG_NAMES, swapfunc (val),    \
					regoff, reg);			     \
  /* Adjust the address register. */					     \
  if (postoff && adjust_p)						     \
    {									     \
      int32 newoff;							     \
									     \
      if (((postoff) & 1) && guest_reg == 15)  /* a7?  Compensate... */	     \
	newoff = (postoff) * 2;						     \
      else								     \
	newoff = postoff;						     \
									     \
      if (offset_p)							     \
	{								     \
	  /* If we're already offset, adjust our offset some more. */	     \
	  if ((c->guest_reg_status[guest_reg].offset += newoff) == 0)	     \
	    c->guest_reg_status[guest_reg].mapping = MAP_NATIVE;	     \
	}								     \
      else								     \
	{								     \
	  /* We now become offset from our original value. */		     \
	  c->guest_reg_status[guest_reg].offset = newoff;		     \
	  c->guest_reg_status[guest_reg].mapping = MAP_OFFSET;		     \
	}								     \
    }									     \
									     \
  return 0;								     \
}


#undef OFF
#define OFF
HOST_BITWISE_IMM_MEM(b, and, ind,     SWAPUB, 0, 0, FALSE)
HOST_BITWISE_IMM_MEM(w, and, ind,     SWAPUW, 0, 0, FALSE)
HOST_BITWISE_IMM_MEM(l, and, ind,     SWAPUL, 0, 0, FALSE)

HOST_BITWISE_IMM_MEM(b, or,  ind,     SWAPUB, 0, 0, FALSE)
HOST_BITWISE_IMM_MEM(w, or,  ind,     SWAPUW, 0, 0, FALSE)
HOST_BITWISE_IMM_MEM(l, or,  ind,     SWAPUL, 0, 0, FALSE)

HOST_BITWISE_IMM_MEM(b, xor, ind,     SWAPUB, 0, 0, FALSE)
HOST_BITWISE_IMM_MEM(w, xor, ind,     SWAPUW, 0, 0, FALSE)
HOST_BITWISE_IMM_MEM(l, xor, ind,     SWAPUL, 0, 0, FALSE)

HOST_BITWISE_IMM_MEM(b, add, ind,     SWAPUB, 0, 0, FALSE)
HOST_BITWISE_IMM_MEM(b, sub, ind,     SWAPUB, 0, 0, FALSE)

HOST_BITWISE_IMM_MEM(b, cmp, ind,     SWAPUB, 0, 0, FALSE)
HOST_BITWISE_IMM_MEM(w, cmp, ind,     SWAPUW, 0, 0, FALSE)
HOST_BITWISE_IMM_MEM(l, cmp, ind,     SWAPUL, 0, 0, FALSE)

HOST_BITWISE_IMM_MEM(b, and, predec,  SWAPUB, -1, -1, TRUE)
HOST_BITWISE_IMM_MEM(w, and, predec,  SWAPUW, -2, -2, TRUE)
HOST_BITWISE_IMM_MEM(l, and, predec,  SWAPUL, -4, -4, TRUE)

HOST_BITWISE_IMM_MEM(b, or,  predec,  SWAPUB, -1, -1, TRUE)
HOST_BITWISE_IMM_MEM(w, or,  predec,  SWAPUW, -2, -2, TRUE)
HOST_BITWISE_IMM_MEM(l, or,  predec,  SWAPUL, -4, -4, TRUE)

HOST_BITWISE_IMM_MEM(b, xor, predec,  SWAPUB, -1, -1, TRUE)
HOST_BITWISE_IMM_MEM(w, xor, predec,  SWAPUW, -2, -2, TRUE)
HOST_BITWISE_IMM_MEM(l, xor, predec,  SWAPUL, -4, -4, TRUE)

HOST_BITWISE_IMM_MEM(b, add, predec,  SWAPUB, -1, -1, TRUE)
HOST_BITWISE_IMM_MEM(b, sub, predec,  SWAPUB, -1, -1, TRUE)

HOST_BITWISE_IMM_MEM(b, cmp, predec,  SWAPUB, -1, -1, TRUE)
HOST_BITWISE_IMM_MEM(w, cmp, predec,  SWAPUW, -2, -2, TRUE)
HOST_BITWISE_IMM_MEM(l, cmp, predec,  SWAPUL, -4, -4, TRUE)

HOST_BITWISE_IMM_MEM(b, and, postinc, SWAPUB, 0, 1, TRUE)
HOST_BITWISE_IMM_MEM(w, and, postinc, SWAPUW, 0, 2, TRUE)
HOST_BITWISE_IMM_MEM(l, and, postinc, SWAPUL, 0, 4, TRUE)

HOST_BITWISE_IMM_MEM(b, or,  postinc, SWAPUB, 0, 1, TRUE)
HOST_BITWISE_IMM_MEM(w, or,  postinc, SWAPUW, 0, 2, TRUE)
HOST_BITWISE_IMM_MEM(l, or,  postinc, SWAPUL, 0, 4, TRUE)

HOST_BITWISE_IMM_MEM(b, xor, postinc, SWAPUB, 0, 1, TRUE)
HOST_BITWISE_IMM_MEM(w, xor, postinc, SWAPUW, 0, 2, TRUE)
HOST_BITWISE_IMM_MEM(l, xor, postinc, SWAPUL, 0, 4, TRUE)

HOST_BITWISE_IMM_MEM(b, add, postinc, SWAPUB, 0, 1, TRUE)
HOST_BITWISE_IMM_MEM(b, sub, postinc, SWAPUB, 0, 1, TRUE)

HOST_BITWISE_IMM_MEM(b, cmp, postinc, SWAPUB, 0, 1, TRUE)
HOST_BITWISE_IMM_MEM(w, cmp, postinc, SWAPUW, 0, 2, TRUE)
HOST_BITWISE_IMM_MEM(l, cmp, postinc, SWAPUL, 0, 4, TRUE)

#undef OFF
#define OFF int32 offset,
HOST_BITWISE_IMM_MEM(b, and, indoff,  SWAPUB, offset, 0, FALSE)
HOST_BITWISE_IMM_MEM(w, and, indoff,  SWAPUW, offset, 0, FALSE)
HOST_BITWISE_IMM_MEM(l, and, indoff,  SWAPUL, offset, 0, FALSE)

HOST_BITWISE_IMM_MEM(b, or,  indoff,  SWAPUB, offset, 0, FALSE)
HOST_BITWISE_IMM_MEM(w, or,  indoff,  SWAPUW, offset, 0, FALSE)
HOST_BITWISE_IMM_MEM(l, or,  indoff,  SWAPUL, offset, 0, FALSE)

HOST_BITWISE_IMM_MEM(b, xor, indoff,  SWAPUB, offset, 0, FALSE)
HOST_BITWISE_IMM_MEM(w, xor, indoff,  SWAPUW, offset, 0, FALSE)
HOST_BITWISE_IMM_MEM(l, xor, indoff,  SWAPUL, offset, 0, FALSE)

HOST_BITWISE_IMM_MEM(b, add, indoff,  SWAPUB, offset, 0, FALSE)
HOST_BITWISE_IMM_MEM(b, sub, indoff,  SWAPUB, offset, 0, FALSE)

HOST_BITWISE_IMM_MEM(b, cmp, indoff,  SWAPUB, offset, 0, FALSE)
HOST_BITWISE_IMM_MEM(w, cmp, indoff,  SWAPUW, offset, 0, FALSE)
HOST_BITWISE_IMM_MEM(l, cmp, indoff,  SWAPUL, offset, 0, FALSE)


#define HOST_UNARY_MEM(size, op, amode, swapfunc, preoff, postoff, adjust_p) \
int									     \
host_ ## op ## size ## _ ## amode (COMMON_ARGS, OFF int32 reg)		     \
{									     \
  int32 regoff;								     \
  int offset_p;								     \
  int guest_reg = c->host_reg_to_guest_reg[reg];			     \
									     \
  CHECK_ADDR_REG (reg);							     \
									     \
  /* Compensate for a7 predec/postinc byte size means +-2 nonsense. */	     \
  if (adjust_p && ((preoff) & 1) && guest_reg == 15)  /* a7? */		     \
    regoff = (preoff) * 2;						     \
  else									     \
    regoff = preoff;							     \
									     \
  offset_p = (c->guest_reg_status[guest_reg].mapping == MAP_OFFSET);	     \
  if (offset_p)								     \
    regoff += c->guest_reg_status[guest_reg].offset;			     \
									     \
  /* Compensate for offset memory. */					     \
  regoff = (int32) SYN68K_TO_US (regoff);				     \
									     \
  /* Actually perform the operation. */					     \
  if (regoff == 0)							     \
    i386_ ## op ## size ## _ind (COMMON_ARG_NAMES, reg);	     	     \
  else									     \
    i386_ ## op ## size ## _indoff (COMMON_ARG_NAMES, regoff, reg);	     \
  /* Adjust the address register. */					     \
  if (postoff && adjust_p)						     \
    {									     \
      int32 newoff;							     \
									     \
      if (((postoff) & 1) && guest_reg == 15)  /* a7?  Compensate... */	     \
	newoff = (postoff) * 2;						     \
      else								     \
	newoff = postoff;						     \
									     \
      if (offset_p)							     \
	{								     \
	  /* If we're already offset, adjust our offset some more. */	     \
	  if ((c->guest_reg_status[guest_reg].offset += newoff) == 0)	     \
	    c->guest_reg_status[guest_reg].mapping = MAP_NATIVE;	     \
	}								     \
      else								     \
	{								     \
	  /* We now become offset from our original value. */		     \
	  c->guest_reg_status[guest_reg].offset = newoff;		     \
	  c->guest_reg_status[guest_reg].mapping = MAP_OFFSET;		     \
	}								     \
    }									     \
									     \
  return 0;								     \
}


#undef OFF
#define OFF
HOST_UNARY_MEM(b, neg, ind,     SWAPUB,  0,     0, FALSE)
HOST_UNARY_MEM(b, neg, predec,  SWAPUB, -1,    -1, TRUE)
HOST_UNARY_MEM(b, neg, postinc, SWAPUB,  0,     1, TRUE)

#undef OFF
#define OFF int32 offset,
HOST_UNARY_MEM(b, neg, indoff,  SWAPUB, offset, 0, FALSE)


static inline void
test_imm (COMMON_ARGS, int32 val, uint32 size_mask)
{
  /* Write out all four cc bits at once. */
  i386_movl_imm_indoff (COMMON_ARG_NAMES,
			((val != 0)
			 | ((val & (size_mask ^ (size_mask >> 1)))
			    ? 0x100 : 0)),
			offsetof (CPUState, ccnz), REG_EBP);
  c->cached_cc &= ~cc_to_compute;
  c->dirty_cc  &= ~cc_to_compute;
}


#define HOST_MOV_IMM_REG(size, size_mask)				\
int									\
host_move ## size ## _imm_reg (COMMON_ARGS, int32 val, int32 reg)	\
{									\
  if (val == 0 && (!cc_spill_if_changed || cc_to_compute))		\
    {									\
      i386_xor ## size ## _reg_reg (COMMON_ARG_NAMES, reg, reg);	\
      if (cc_to_compute)						\
	{								\
	  test_imm (COMMON_ARG_NAMES, 0, size_mask);		        \
	  c->cached_cc |= cc_to_compute;  /* xor cached them. */	\
        }								\
    }									\
  else									\
    {									\
      i386_mov ## size ## _imm_reg (COMMON_ARG_NAMES, val, reg);	\
      if (cc_to_compute)						\
	test_imm (COMMON_ARG_NAMES, val, size_mask);		        \
    }									\
  return 0;								\
}


HOST_MOV_IMM_REG (b, 0xFF)
HOST_MOV_IMM_REG (w, 0xFFFF)
HOST_MOV_IMM_REG (l, 0xFFFFFFFF)


#define HOST_MOV_IMM_ABS(size, swapfunc, size_mask)			    \
int									    \
host_move ## size ## _imm_abs (COMMON_ARGS, int32 val, int32 addr)	    \
{									    \
  if (cc_to_compute)							    \
    test_imm (COMMON_ARG_NAMES, val, size_mask);			    \
  i386_mov ## size ## _imm_abs (COMMON_ARG_NAMES, swapfunc (val),	    \
				(int32) SYN68K_TO_US (addr));		    \
  return 0;								    \
}

HOST_MOV_IMM_ABS (b, SWAPUB, 0xFF)
HOST_MOV_IMM_ABS (w, SWAPUW, 0xFFFF)
HOST_MOV_IMM_ABS (l, SWAPUL, 0xFFFFFFFF)


#define HOST_MOV_REG_ABS_SWAP(size, swap)				    \
int									    \
host_move ## size ## _reg_abs_swap (COMMON_ARGS, int32 src_reg, int32 addr) \
{									    \
  cc_spill_if_changed |= cc_to_compute;					    \
  swap (src_reg);							    \
  return i386_mov ## size ## _reg_abs (COMMON_ARG_NAMES, src_reg,	    \
				       (int32) SYN68K_TO_US (addr));	    \
}


HOST_MOV_REG_ABS_SWAP (w, HOST_SWAP16)
HOST_MOV_REG_ABS_SWAP (l, HOST_SWAP32)


#define HOST_MOV_ABS_REG_SWAP(size, swap, long_p)			    \
int									    \
host_move ## size ## _abs_reg_swap (COMMON_ARGS, int32 addr, int32 dst_reg) \
{									    \
  i386_mov ## size ## _abs_reg (COMMON_ARG_NAMES,			    \
				(int32) SYN68K_TO_US (addr),		    \
				dst_reg);				    \
  swap (dst_reg);							    \
  return 0;								    \
}


HOST_MOV_ABS_REG_SWAP (w, HOST_SWAP16, FALSE)
HOST_MOV_ABS_REG_SWAP (l, HOST_SWAP32, TRUE)


#define HOST_MOV_IMM_MEM(size, amode, swapfunc, preoff, postoff, adjust_p,   \
			 size_mask)					     \
int									     \
host_move ## size ## _imm_ ## amode (COMMON_ARGS, int32 val, OFF int32 reg)  \
{									     \
  int32 regoff;								     \
  int offset_p;								     \
  int guest_reg = c->host_reg_to_guest_reg[reg];			     \
									     \
  CHECK_ADDR_REG (reg);							     \
									     \
  /* Compensate for a7 predec/postinc byte size means +-2 nonsense. */	     \
  if (adjust_p && ((preoff) & 1) && guest_reg == 15)  /* a7? */		     \
    regoff = (preoff) * 2;						     \
  else									     \
    regoff = preoff;							     \
									     \
  offset_p = (c->guest_reg_status[guest_reg].mapping == MAP_OFFSET);	     \
  if (offset_p)								     \
    regoff += c->guest_reg_status[guest_reg].offset;			     \
									     \
  /* Compensate for offset memory. */					     \
  regoff = (int32) SYN68K_TO_US (regoff);				     \
									     \
  /* Set cc bits according to this value, if necessary. */		     \
  if (cc_to_compute)							     \
    {									     \
      test_imm (COMMON_ARG_NAMES, val, size_mask);			     \
      cc_to_compute = M68K_CC_NONE;					     \
    }									     \
									     \
  /* Actually write the value out to memory. */				     \
  if (regoff == 0)							     \
    i386_mov ## size ## _imm_ind (COMMON_ARG_NAMES,		     	     \
				  swapfunc (val), reg);	     		     \
  else									     \
    i386_mov ## size ## _imm_indoff (COMMON_ARG_NAMES, swapfunc (val),	     \
				     regoff, reg);			     \
  /* Adjust the address register. */					     \
  if (postoff && adjust_p)						     \
    {									     \
      int32 newoff;							     \
									     \
      if (((postoff) & 1) && guest_reg == 15)  /* a7?  Compensate... */	     \
	newoff = (postoff) * 2;						     \
      else								     \
	newoff = postoff;						     \
									     \
      if (offset_p)							     \
	{								     \
	  /* If we're already offset, adjust our offset some more. */	     \
	  if ((c->guest_reg_status[guest_reg].offset += newoff) == 0)	     \
	    c->guest_reg_status[guest_reg].mapping = MAP_NATIVE;	     \
	}								     \
      else								     \
	{								     \
	  /* We now become offset from our original value. */		     \
	  c->guest_reg_status[guest_reg].offset = newoff;		     \
	  c->guest_reg_status[guest_reg].mapping = MAP_OFFSET;		     \
	}								     \
    }									     \
									     \
  return 0;								     \
}


#undef OFF
#define OFF
HOST_MOV_IMM_MEM(b, ind,     SWAPUB, 0, 0, FALSE, 0xFF)
HOST_MOV_IMM_MEM(w, ind,     SWAPUW, 0, 0, FALSE, 0xFFFF)
HOST_MOV_IMM_MEM(l, ind,     SWAPUL, 0, 0, FALSE, 0xFFFFFFFF)

HOST_MOV_IMM_MEM(b, predec,  SWAPUB, -1, -1, TRUE, 0xFF)
HOST_MOV_IMM_MEM(w, predec,  SWAPUW, -2, -2, TRUE, 0xFFFF)
HOST_MOV_IMM_MEM(l, predec,  SWAPUL, -4, -4, TRUE, 0xFFFFFFFF)

HOST_MOV_IMM_MEM(b, postinc, SWAPUB, 0, 1, TRUE, 0xFF)
HOST_MOV_IMM_MEM(w, postinc, SWAPUW, 0, 2, TRUE, 0xFFFF)
HOST_MOV_IMM_MEM(l, postinc, SWAPUL, 0, 4, TRUE, 0xFFFFFFFF)

#undef OFF
#define OFF int32 offset,
HOST_MOV_IMM_MEM(b, indoff,  SWAPUB, offset, 0, FALSE, 0xFF)
HOST_MOV_IMM_MEM(w, indoff,  SWAPUW, offset, 0, FALSE, 0xFFFF)
HOST_MOV_IMM_MEM(l, indoff,  SWAPUL, offset, 0, FALSE, 0xFFFFFFFF)


#define COMPLEX_P(n)        ((n) & (1 << 8))
#define LONG_INDEX_REG_P(n) ((n) & (1 << 11))
#define SCALE(n)            (((n) >> 9) & 3)
#define DISPLACEMENT(n)     ((int8) ((n) & 0xFF))
#define INDEX_REG(n)        (((n) >> 12) & 0xF)


#define HOST_OP_IMM_INDIX(size, name, op, swapfunc, test_imm_p,	      	  \
			  size_mask)			  		  \
int									  \
host_ ## name ## size ## _imm_indix (COMMON_ARGS, int32 val,		  \
				     int32 base_addr_reg,		  \
				     uint16 *m68k_addr)			  \
{									  \
  uint16 ext_word;							  \
  int index_reg, guest_index_reg, orig_index_reg;			  \
  int32 disp;								  \
  guest_reg_status_t *base_status, *index_status;			  \
									  \
  /* Fetch the extension word and make sure it's a "simple" reference. */ \
  ext_word = SWAPUW (m68k_addr[1]);					  \
  if (COMPLEX_P (ext_word))						  \
    return 2;								  \
									  \
  /* Set up the index register. */					  \
  guest_index_reg = INDEX_REG (ext_word);				  \
  index_status = &c->guest_reg_status[guest_index_reg];			  \
  if (index_status->host_reg == NO_REG					  \
      && !LONG_INDEX_REG_P (ext_word))					  \
    {									  \
      i386_movswl_indoff_reg (COMMON_ARG_NAMES,				  \
			      offsetof (CPUState,			  \
					regs[guest_index_reg].sw.n),	  \
			      REG_EBP, scratch_reg);			  \
      index_reg = scratch_reg;						  \
    }									  \
  else									  \
    {									  \
      host_reg_mask_t legal_regs;					  \
      legal_regs = ((~((1L << base_addr_reg) | (1L << scratch_reg)))	  \
		    & ((guest_index_reg < 8)				  \
		       ? REGSET_BYTE : REGSET_ALL));			  \
      if (c->guest_reg_status[guest_index_reg].host_reg			  \
	  == base_addr_reg)						  \
	legal_regs |= (1L << base_addr_reg);				  \
      orig_index_reg = host_setup_cached_reg (COMMON_ARG_NAMES,		  \
					      guest_index_reg,		  \
					      MAP_NATIVE_MASK,		  \
					      legal_regs);		  \
      if (orig_index_reg == NO_REG)					  \
	return 3;							  \
      if (LONG_INDEX_REG_P (ext_word))					  \
	index_reg = orig_index_reg;					  \
      else								  \
	{								  \
	  i386_movswl_reg_reg (COMMON_ARG_NAMES, orig_index_reg,	  \
			       scratch_reg);				  \
	  index_reg = scratch_reg;					  \
	}								  \
    }									  \
									  \
  /* Compute the total displacement. */					  \
  disp = DISPLACEMENT (ext_word);					  \
  base_status = &c->guest_reg_status					  \
    [c->host_reg_to_guest_reg[base_addr_reg]];				  \
  if (base_status->mapping == MAP_OFFSET)				  \
    disp += base_status->offset;					  \
									  \
  /* Compensate for offset memory. */					  \
  disp = (int32) SYN68K_TO_US (disp);					  \
									  \
  /* Write the value out. */						  \
  switch (SCALE (ext_word))						  \
    {									  \
    case 0:								  \
      if (disp == 0)							  \
	op ## size ##_imm_indix_no_offset (COMMON_ARG_NAMES,		  \
					   swapfunc (val),		  \
					   base_addr_reg, index_reg);	  \
      else								  \
	op ## size ##_imm_indix (COMMON_ARG_NAMES, swapfunc (val),	  \
				 disp, base_addr_reg, index_reg);	  \
      break;								  \
    case 1:								  \
      if (disp == 0)							  \
	op ## size ##_imm_indix_scale2_no_offset (COMMON_ARG_NAMES,	  \
						  swapfunc (val),	  \
						  base_addr_reg,	  \
						  index_reg);		  \
      else								  \
	op ## size ##_imm_indix_scale2 (COMMON_ARG_NAMES,		  \
					swapfunc (val),			  \
					disp, base_addr_reg,		  \
					index_reg);			  \
      break;								  \
    case 2:								  \
      if (disp == 0)							  \
	op ## size ##_imm_indix_scale4_no_offset (COMMON_ARG_NAMES,	  \
						  swapfunc (val),	  \
						  base_addr_reg,	  \
						  index_reg);		  \
      else								  \
	op ## size ##_imm_indix_scale4 (COMMON_ARG_NAMES,		  \
					swapfunc (val),			  \
					disp, base_addr_reg,		  \
					index_reg);			  \
      break;								  \
    case 3:								  \
      if (disp == 0)							  \
	op ## size ##_imm_indix_scale8_no_offset (COMMON_ARG_NAMES,	  \
						  swapfunc (val),	  \
						  base_addr_reg,	  \
						  index_reg);		  \
      else								  \
	op ## size ##_imm_indix_scale8 (COMMON_ARG_NAMES,		  \
					swapfunc (val),			  \
					disp, base_addr_reg,		  \
					index_reg);			  \
      break;								  \
    default:								  \
      abort ();								  \
    }									  \
									  \
  /* If we need cc bits, compute them. */				  \
  if ((test_imm_p) && cc_to_compute)					  \
    test_imm (COMMON_ARG_NAMES, val, size_mask);			  \
									  \
  return 0;								  \
}


HOST_OP_IMM_INDIX (b, move, i386_mov, SWAPUB, TRUE, 0xFF)
HOST_OP_IMM_INDIX (w, move, i386_mov, SWAPUW, TRUE, 0xFFFF)
HOST_OP_IMM_INDIX (l, move, i386_mov, SWAPUL, TRUE, 0xFFFFFFFF)

HOST_OP_IMM_INDIX (b, cmp, i386_cmp, SWAPUB, FALSE, 0xFF)
HOST_OP_IMM_INDIX (w, cmp, i386_cmp, SWAPUW, FALSE, 0xFFFF)
HOST_OP_IMM_INDIX (l, cmp, i386_cmp, SWAPUL, FALSE, 0xFFFFFFFF)

HOST_OP_IMM_INDIX (b, add, i386_add, SWAPUB, FALSE, 0xFF)
HOST_OP_IMM_INDIX (b, sub, i386_sub, SWAPUB, FALSE, 0xFF)

#define HOST_ARITH_IMM_INDIX(size, name, op, swapfunc, byte_p, word_p)	   \
int									   \
host_ ## name ## size ## _imm_indix (COMMON_ARGS, int32 val,		   \
				     int32 base_addr_reg,		   \
				     uint16 *m68k_addr)			   \
{									   \
  uint16 ext_word;							   \
  int tmp_val_reg;							   \
									   \
  /* Fetch the extension word and make sure it's a "simple" reference. */  \
  ext_word = SWAPUW (m68k_addr[1]);					   \
  if (COMPLEX_P (ext_word))						   \
    return 1;								   \
									   \
  if (host_leal_indix_reg (COMMON_ARG_NAMES, base_addr_reg,		   \
			   scratch_reg, m68k_addr))			   \
    return 2;								   \
									   \
  /* Grab a temporary register to hold the memory value. */		   \
  tmp_val_reg = host_alloc_reg (c, codep, cc_spill_if_changed,		   \
				((byte_p ? REGSET_BYTE : REGSET_ALL)	   \
				 & ~(1L << scratch_reg)));		   \
  if (tmp_val_reg == NO_REG)						   \
    return 3;								   \
									   \
  /* We've now got the address of the value in scratch_reg.  Load the	   \
   * value, swap it, operate on it, swap it back, and write it out	   \
   * to memory.								   \
   */									   \
  i386_mov ## size ## _indoff_reg (COMMON_ARG_NAMES,			   \
				   (int32) SYN68K_TO_US (0),		   \
				   scratch_reg, tmp_val_reg);		   \
  swapfunc (tmp_val_reg);						   \
  if (word_p && !cc_to_compute)						   \
    op ## size ## _imm_reg (COMMON_ARG_NAMES, val, tmp_val_reg);	   \
  else									   \
    op ## size ## _imm_reg (COMMON_ARG_NAMES, val, tmp_val_reg);	   \
  cc_spill_if_changed |= cc_to_compute;					   \
  swapfunc (tmp_val_reg);						   \
  i386_mov ## size ## _reg_indoff (COMMON_ARG_NAMES, tmp_val_reg,	   \
				   (int32) SYN68K_TO_US (0), scratch_reg); \
									   \
  return 0;								   \
}

HOST_ARITH_IMM_INDIX (w, add, i386_add, HOST_SWAP16, FALSE, TRUE)
HOST_ARITH_IMM_INDIX (l, add, i386_add, HOST_SWAP32, FALSE, FALSE)

HOST_ARITH_IMM_INDIX (w, sub, i386_sub, HOST_SWAP16, FALSE, TRUE)
HOST_ARITH_IMM_INDIX (l, sub, i386_sub, HOST_SWAP32, FALSE, FALSE)


#define HOST_OP_REG_INDIX(size, name, op, swapfunc, test_src_p, byte_p)	  \
int									  \
host_ ## name ## size ## _reg_indix (COMMON_ARGS, int32 src_reg,	  \
				     int32 base_addr_reg,		  \
				     uint16 *m68k_addr)			  \
{									  \
  uint16 ext_word;							  \
  int index_reg, guest_index_reg, orig_index_reg;			  \
  int32 disp;								  \
  guest_reg_status_t *base_status;					  \
									  \
  /* Fetch the extension word and make sure it's a "simple" reference. */ \
  ext_word = SWAPUW (m68k_addr[1]);					  \
  if (COMPLEX_P (ext_word))						  \
    return 1;								  \
									  \
  /* Sometimes we need another scratch register, like for		  \
   * memory->memory moves.  This works around that problem by		  \
   * allocating another one if the source reg is the scratch reg.	  \
   */									  \
  if (src_reg == scratch_reg)						  \
    {									  \
      host_reg_mask_t scratch_regs;					  \
      scratch_regs = ((~((1L << base_addr_reg) | (1L << scratch_reg)	  \
		       | (1L << src_reg)))				  \
		      & REGSET_ALL);					  \
      scratch_reg = host_alloc_reg (c, codep, cc_spill_if_changed,	  \
				    scratch_regs);			  \
      if (scratch_reg == NO_REG)					  \
	return 6;							  \
    }									  \
									  \
  /* Set up the index register. */					  \
  guest_index_reg = INDEX_REG (ext_word);				  \
  if (c->guest_reg_status[guest_index_reg].host_reg == NO_REG		  \
      && !LONG_INDEX_REG_P (ext_word))					  \
    {									  \
      i386_movswl_indoff_reg (COMMON_ARG_NAMES,				  \
			      offsetof (CPUState,			  \
					regs[guest_index_reg].sw.n),	  \
			      REG_EBP, scratch_reg);			  \
      index_reg = scratch_reg;						  \
    }									  \
  else									  \
    {									  \
      int host_index_reg;						  \
      host_reg_mask_t legal_regs;					  \
									  \
      legal_regs = ((~((1L << base_addr_reg) | (1L << scratch_reg)	  \
		       | (1L << src_reg)))				  \
		    & ((guest_index_reg < 8)				  \
		       ? REGSET_BYTE : REGSET_ALL));			  \
      host_index_reg = c->guest_reg_status[guest_index_reg].host_reg;	  \
      if (host_index_reg == base_addr_reg				  \
	  || (byte_p && host_index_reg == src_reg))			  \
	legal_regs |= (1L << host_index_reg);				  \
      if (host_index_reg == src_reg)					  \
	{								  \
	  i386_movl_reg_reg (COMMON_ARG_NAMES, src_reg, scratch_reg);	  \
	  swapfunc (scratch_reg);					  \
	  host_index_reg = orig_index_reg = scratch_reg;		  \
	}								  \
      else								  \
	{								  \
	  if (host_index_reg != NO_REG					  \
	      && !(legal_regs & (1L << host_index_reg)))		  \
		return 5;						  \
	  orig_index_reg = host_setup_cached_reg (COMMON_ARG_NAMES,	  \
						  guest_index_reg,	  \
						  MAP_NATIVE_MASK,	  \
						  legal_regs);		  \
          if (orig_index_reg == NO_REG)					  \
	    return 3;							  \
	}								  \
      if (LONG_INDEX_REG_P (ext_word))					  \
	index_reg = orig_index_reg;					  \
      else								  \
	{								  \
	  i386_movswl_reg_reg (COMMON_ARG_NAMES, orig_index_reg,	  \
			       scratch_reg);				  \
	  index_reg = scratch_reg;					  \
	}								  \
    }									  \
									  \
  /* Compute the total displacement. */					  \
  disp = DISPLACEMENT (ext_word);					  \
  base_status = &c->guest_reg_status					  \
    [c->host_reg_to_guest_reg[base_addr_reg]];				  \
  if (base_status->mapping == MAP_OFFSET)				  \
    disp += base_status->offset;					  \
									  \
  /* Compensate for offset memory. */					  \
  disp = (int32) SYN68K_TO_US (disp);					  \
									  \
  /* Write the value out. */						  \
  switch (SCALE (ext_word))						  \
    {									  \
    case 0:								  \
      if (disp == 0)							  \
	op ## size ##_reg_indix_no_offset (COMMON_ARG_NAMES,		  \
					   src_reg,			  \
					   base_addr_reg, index_reg);	  \
      else								  \
	op ## size ##_reg_indix (COMMON_ARG_NAMES, src_reg,		  \
				 disp, base_addr_reg, index_reg);	  \
      break;								  \
    case 1:								  \
      if (disp == 0)							  \
	op ## size ##_reg_indix_scale2_no_offset (COMMON_ARG_NAMES,	  \
						  src_reg,		  \
						  base_addr_reg,	  \
						  index_reg);		  \
      else								  \
	op ## size ##_reg_indix_scale2 (COMMON_ARG_NAMES,		  \
					src_reg,			  \
					disp, base_addr_reg,		  \
					index_reg);			  \
      break;								  \
    case 2:								  \
      if (disp == 0)							  \
	op ## size ##_reg_indix_scale4_no_offset (COMMON_ARG_NAMES,	  \
						  src_reg,		  \
						  base_addr_reg,	  \
						  index_reg);		  \
      else								  \
	op ## size ##_reg_indix_scale4 (COMMON_ARG_NAMES,		  \
					src_reg,			  \
					disp, base_addr_reg,		  \
					index_reg);			  \
      break;								  \
    case 3:								  \
      if (disp == 0)							  \
	op ## size ##_reg_indix_scale8_no_offset (COMMON_ARG_NAMES,	  \
						  src_reg,		  \
						  base_addr_reg,	  \
						  index_reg);		  \
      else								  \
	op ## size ##_reg_indix_scale8 (COMMON_ARG_NAMES,		  \
					src_reg,			  \
					disp, base_addr_reg,		  \
					index_reg);			  \
      break;								  \
    default:								  \
      abort ();								  \
    }									  \
									  \
  /* If we need cc bits, compute them. */				  \
  if ((test_src_p) && cc_to_compute)					  \
    {									  \
      if ((byte_p) || !(cc_to_compute & M68K_CCN))			  \
	i386_test ## size ## _reg_reg (COMMON_ARG_NAMES, src_reg,	  \
				       src_reg);			  \
      else								  \
	{								  \
	  i386_movl_reg_reg (COMMON_ARG_NAMES, src_reg, scratch_reg);	  \
	  swapfunc (scratch_reg);					  \
	  i386_test ## size ## _reg_reg (COMMON_ARG_NAMES,		  \
					 scratch_reg, scratch_reg);	  \
	}								  \
    }									  \
									  \
  return 0;								  \
}


#define NOSWAP(n)

HOST_OP_REG_INDIX (b, move, i386_mov, NOSWAP,      TRUE, TRUE)
HOST_OP_REG_INDIX (w, move, i386_mov, HOST_SWAP16, TRUE, FALSE)
HOST_OP_REG_INDIX (l, move, i386_mov, HOST_SWAP32, TRUE, FALSE)


#define HOST_OP_INDIX_REG(size, name, op, long_p, cmp_p, lea_p)		  \
int									  \
host_ ## name ## size ## _indix_reg (COMMON_ARGS, int32 base_addr_reg,	  \
				     int32 dst_reg,			  \
				     uint16 *m68k_addr)			  \
{									  \
  uint16 ext_word;							  \
  int index_reg, guest_index_reg, orig_index_reg;			  \
  int32 disp;								  \
  guest_reg_status_t *base_status;					  \
									  \
  /* Fetch the extension word and make sure it's a "simple" reference. */ \
  ext_word = SWAPUW (m68k_addr[1]);					  \
  if (COMPLEX_P (ext_word))						  \
    return 1;								  \
									  \
  /* Set up the index register. */					  \
  guest_index_reg = INDEX_REG (ext_word);				  \
  if (c->guest_reg_status[guest_index_reg].host_reg == NO_REG)		  \
    {									  \
      if (LONG_INDEX_REG_P (ext_word))					  \
        i386_movl_indoff_reg (COMMON_ARG_NAMES,				  \
			      offsetof (CPUState,			  \
					regs[guest_index_reg].ul.n),	  \
			      REG_EBP, scratch_reg);			  \
      else								  \
        i386_movswl_indoff_reg (COMMON_ARG_NAMES,			  \
				offsetof (CPUState,			  \
					  regs[guest_index_reg].sw.n),	  \
				REG_EBP, scratch_reg);			  \
      index_reg = scratch_reg;						  \
    }									  \
  else									  \
    {									  \
      host_reg_mask_t legal_regs;					  \
      int host_index_reg;						  \
      legal_regs = ((~((1L << base_addr_reg) | (1L << scratch_reg)	  \
		       | (1L << dst_reg)))				  \
		    & ((guest_index_reg < 8)				  \
		       ? REGSET_BYTE : REGSET_ALL));			  \
      host_index_reg = c->guest_reg_status[guest_index_reg].host_reg;	  \
      if (host_index_reg == base_addr_reg				  \
	  || (!cmp_p && host_index_reg == dst_reg))			  \
	legal_regs |= (1L << host_index_reg);				  \
      if (cmp_p)							  \
	legal_regs &= ~(1L << dst_reg);					  \
      if (host_index_reg != NO_REG					  \
	  && !(legal_regs & (1L << host_index_reg)))			  \
	return 5;							  \
									  \
      /* When moving a long, the dest register's old value might be	  \
       * invalid (since we are about to smash it).  If that register	  \
       * is also the index register, we don't want to trust the junk	  \
       * that's in it.  To be safe, reload the value.			  \
       */								  \
      if (long_p && !cmp_p && host_index_reg == dst_reg			  \
	  && !c->guest_reg_status[guest_index_reg].dirty_without_offset_p \
	  && c->guest_reg_status[guest_index_reg].mapping == MAP_NATIVE)  \
        {								  \
          i386_movl_indoff_reg (COMMON_ARG_NAMES,			  \
				offsetof (CPUState,			  \
					  regs[guest_index_reg].ul.n),	  \
				REG_EBP, host_index_reg);		  \
	  orig_index_reg = host_index_reg;				  \
	}								  \
      else								  \
        orig_index_reg = host_setup_cached_reg (COMMON_ARG_NAMES,	  \
					        guest_index_reg,	  \
					        MAP_NATIVE_MASK,	  \
					        legal_regs);		  \
									  \
      if (orig_index_reg == NO_REG)					  \
	return 2;							  \
      if (LONG_INDEX_REG_P (ext_word))					  \
	index_reg = orig_index_reg;					  \
      else								  \
	{								  \
	  i386_movswl_reg_reg (COMMON_ARG_NAMES, orig_index_reg,	  \
			       scratch_reg);				  \
	  index_reg = scratch_reg;					  \
	}								  \
    }									  \
									  \
  /* Compute the total displacement. */					  \
  disp = DISPLACEMENT (ext_word);					  \
  base_status = &c->guest_reg_status					  \
    [c->host_reg_to_guest_reg[base_addr_reg]];				  \
  if (base_status->mapping == MAP_OFFSET)				  \
    disp += base_status->offset;					  \
									  \
  /* Compensate for offset memory. */					  \
  if (!(lea_p))								  \
    disp = (int32) SYN68K_TO_US (disp);					  \
									  \
  /* Write the value out. */						  \
  switch (SCALE (ext_word))						  \
    {									  \
    case 0:								  \
      if (disp == 0)							  \
	op ## size ##_indix_reg_no_offset (COMMON_ARG_NAMES,		  \
					   base_addr_reg, index_reg,	  \
					   dst_reg);			  \
      else								  \
	op ## size ##_indix_reg (COMMON_ARG_NAMES,			  \
				 disp, base_addr_reg, index_reg,	  \
				 dst_reg);				  \
      break;								  \
    case 1:								  \
      if (disp == 0)							  \
	op ## size ##_indix_reg_scale2_no_offset (COMMON_ARG_NAMES,	  \
						  base_addr_reg,	  \
						  index_reg,		  \
						  dst_reg);		  \
      else								  \
	op ## size ##_indix_reg_scale2 (COMMON_ARG_NAMES,		  \
					disp, base_addr_reg, index_reg,	  \
					dst_reg);			  \
      break;								  \
    case 2:								  \
      if (disp == 0)							  \
	op ## size ##_indix_reg_scale4_no_offset (COMMON_ARG_NAMES,	  \
						  base_addr_reg,	  \
						  index_reg,		  \
						  dst_reg);		  \
      else								  \
	op ## size ##_indix_reg_scale4 (COMMON_ARG_NAMES,		  \
					disp, base_addr_reg,		  \
					index_reg,			  \
					dst_reg);			  \
      break;								  \
    case 3:								  \
      if (disp == 0)							  \
	op ## size ##_indix_reg_scale8_no_offset (COMMON_ARG_NAMES,	  \
						  base_addr_reg,	  \
						  index_reg,		  \
						  dst_reg);		  \
      else								  \
	op ## size ##_indix_reg_scale8 (COMMON_ARG_NAMES,		  \
					disp, base_addr_reg,		  \
					index_reg,			  \
					dst_reg);			  \
      break;								  \
    default:								  \
      abort ();								  \
    }									  \
									  \
  return 0;								  \
}

HOST_OP_INDIX_REG (b, move, i386_mov, FALSE, FALSE, FALSE)
HOST_OP_INDIX_REG (w, move, i386_mov, FALSE, FALSE, FALSE)
HOST_OP_INDIX_REG (l, move, i386_mov, TRUE,  FALSE, FALSE)

HOST_OP_INDIX_REG (b, cmp, i386_cmp, FALSE, TRUE, FALSE)
HOST_OP_INDIX_REG (w, cmp, i386_cmp, FALSE, TRUE, FALSE)
HOST_OP_INDIX_REG (l, cmp, i386_cmp, TRUE,  TRUE, FALSE)

HOST_OP_INDIX_REG (l, lea, i386_lea, TRUE, FALSE, TRUE)


#define HOST_MOV_REG_MEM(size, amode, preoff, postoff, adjust_p)	      \
int									      \
host_move ## size ## _reg_ ## amode (COMMON_ARGS, int32 src_reg,      	      \
				     OFF int32 dst_addr_reg)		      \
{									      \
  int32 regoff;								      \
  int offset_p;								      \
  int guest_reg = c->host_reg_to_guest_reg[dst_addr_reg];		      \
									      \
  CHECK_ADDR_REG (dst_addr_reg);					      \
									      \
  /* Compensate for a7 predec/postinc byte size means +-2 nonsense. */	      \
  if (adjust_p && ((preoff) & 1) && guest_reg == 15)  /* a7? */	              \
    regoff = (preoff) * 2;						      \
  else									      \
    regoff = preoff;							      \
									      \
  offset_p = (c->guest_reg_status[guest_reg].mapping == MAP_OFFSET);	      \
  if (offset_p)								      \
    regoff += c->guest_reg_status[guest_reg].offset;			      \
									      \
  /* Compensate for offset memory. */					      \
  regoff = (int32) SYN68K_TO_US (regoff);				      \
									      \
  /* Actually write the value out to memory. */				      \
  if (regoff == 0)							      \
    i386_mov ## size ## _reg_ind (COMMON_ARG_NAMES,		     	      \
				  src_reg, dst_addr_reg);		      \
  else									      \
    i386_mov ## size ## _reg_indoff (COMMON_ARG_NAMES, src_reg,		      \
				     regoff, dst_addr_reg);		      \
  /* Adjust the address register. */					      \
  if (postoff && adjust_p)						      \
    {									      \
      int32 newoff;							      \
									      \
      if (((postoff) & 1) && guest_reg == 15)  /* a7?  Compensate... */       \
	newoff = (postoff) * 2;						      \
      else								      \
	newoff = postoff;						      \
									      \
      if (offset_p)							      \
	{								      \
	  /* If we're already offset, adjust our offset some more. */	      \
	  if ((c->guest_reg_status[guest_reg].offset += newoff) == 0)         \
	    c->guest_reg_status[guest_reg].mapping = MAP_NATIVE;	      \
	}								      \
      else								      \
	{								      \
	  /* We now become offset from our original value. */		      \
	  c->guest_reg_status[guest_reg].offset = newoff;		      \
	  c->guest_reg_status[guest_reg].mapping = MAP_OFFSET;	      	      \
	}								      \
    }									      \
									      \
  return 0;								      \
}


#undef OFF
#define OFF
HOST_MOV_REG_MEM(b, ind,    0, 0, FALSE)
HOST_MOV_REG_MEM(w, ind,    0, 0, FALSE)
HOST_MOV_REG_MEM(l, ind,    0, 0, FALSE)

HOST_MOV_REG_MEM(b, predec, -1, -1, TRUE)
HOST_MOV_REG_MEM(w, predec, -2, -2, TRUE)
HOST_MOV_REG_MEM(l, predec, -4, -4, TRUE)

HOST_MOV_REG_MEM(b, postinc, 0, 1, TRUE)
HOST_MOV_REG_MEM(w, postinc, 0, 2, TRUE)
HOST_MOV_REG_MEM(l, postinc, 0, 4, TRUE)

#undef OFF
#define OFF int32 offset,
HOST_MOV_REG_MEM(b, indoff,  offset, 0, FALSE)
HOST_MOV_REG_MEM(w, indoff,  offset, 0, FALSE)
HOST_MOV_REG_MEM(l, indoff,  offset, 0, FALSE)


#define HOST_MOV_REG_MEM_SWAP(size, amode, swap)			 \
int									 \
host_move ## size ## _reg_ ## amode ## _swap (COMMON_ARGS, int32 src_reg,\
					      OFF int32 addr_reg)	 \
{									 \
  /* Bail out if the two registers are the same! */			 \
  if (src_reg == addr_reg)						 \
    return 1;								 \
									 \
  CHECK_ADDR_REG (addr_reg);					         \
									 \
  cc_spill_if_changed |= cc_to_compute;					 \
  swap (src_reg);							 \
  return host_move ## size ## _reg_ ## amode (COMMON_ARG_NAMES, src_reg, \
					      OFFNAME addr_reg);	 \
}


#undef OFF
#define OFF
#undef OFFNAME
#define OFFNAME
HOST_MOV_REG_MEM_SWAP(w, ind, HOST_SWAP16)
HOST_MOV_REG_MEM_SWAP(l, ind, HOST_SWAP32)

HOST_MOV_REG_MEM_SWAP(w, predec, HOST_SWAP16)
HOST_MOV_REG_MEM_SWAP(l, predec, HOST_SWAP32)

HOST_MOV_REG_MEM_SWAP(w, postinc, HOST_SWAP16)
HOST_MOV_REG_MEM_SWAP(l, postinc, HOST_SWAP32)

#undef OFF
#define OFF int32 offset,
#undef OFFNAME
#define OFFNAME offset,
HOST_MOV_REG_MEM_SWAP(w, indoff, HOST_SWAP16)
HOST_MOV_REG_MEM_SWAP(l, indoff, HOST_SWAP32)


#define HOST_MOV_MEM_REG(size, amode, preoff, postoff, adjust_p, byte_p)     \
int									     \
host_move ## size ## _ ## amode ## _reg (COMMON_ARGS, OFF int32 src_addr_reg,\
					int32 dst_reg)			     \
{									     \
  int32 regoff;								     \
  int offset_p;								     \
  int guest_reg = c->host_reg_to_guest_reg[src_addr_reg];		     \
									     \
  CHECK_ADDR_REG (src_addr_reg);					     \
									     \
  /* Compensate for a7 predec/postinc byte size means +-2 nonsense. */	     \
  if (adjust_p && ((preoff) & 1) && guest_reg == 15)  /* a7? */	             \
    regoff = (preoff) * 2;						     \
  else									     \
    regoff = preoff;							     \
									     \
  offset_p = (c->guest_reg_status[guest_reg].mapping == MAP_OFFSET);	     \
  if (offset_p)								     \
    regoff += c->guest_reg_status[guest_reg].offset;			     \
									     \
  /* Compensate for offset memory. */					     \
  regoff = (int32) SYN68K_TO_US (regoff);				     \
									     \
  /* Read the value in from memory. */					     \
  if (regoff == 0)							     \
    i386_mov ## size ## _ind_reg (COMMON_ARG_NAMES, src_addr_reg, dst_reg);  \
  else									     \
    i386_mov ## size ## _indoff_reg (COMMON_ARG_NAMES, regoff, src_addr_reg, \
				     dst_reg);				     \
									     \
  /* Adjust the address register. */					     \
  if (postoff && adjust_p && (byte_p || src_addr_reg != dst_reg))	     \
    {									     \
      int32 newoff;							     \
									     \
      if (((postoff) & 1) && guest_reg == 15)  /* a7?  Compensate... */      \
	newoff = (postoff) * 2;						     \
      else								     \
	newoff = postoff;						     \
									     \
      if (offset_p)							     \
	{								     \
	  /* If we're already offset, adjust our offset some more. */	     \
	  if ((c->guest_reg_status[guest_reg].offset += newoff) == 0)        \
	    c->guest_reg_status[guest_reg].mapping = MAP_NATIVE;	     \
	}								     \
      else								     \
	{								     \
	  /* We now become offset from our original value. */		     \
	  c->guest_reg_status[guest_reg].offset = newoff;		     \
	  c->guest_reg_status[guest_reg].mapping = MAP_OFFSET;		     \
	}								     \
    }									     \
									     \
  return 0;								     \
}


#undef OFF
#define OFF
HOST_MOV_MEM_REG(b, ind,    0, 0, FALSE, TRUE)
HOST_MOV_MEM_REG(w, ind,    0, 0, FALSE, FALSE)
HOST_MOV_MEM_REG(l, ind,    0, 0, FALSE, FALSE)

HOST_MOV_MEM_REG(b, predec, -1, -1, TRUE, TRUE)
HOST_MOV_MEM_REG(w, predec, -2, -2, TRUE, FALSE)
HOST_MOV_MEM_REG(l, predec, -4, -4, TRUE, FALSE)

HOST_MOV_MEM_REG(b, postinc, 0, 1, TRUE, TRUE)
HOST_MOV_MEM_REG(w, postinc, 0, 2, TRUE, FALSE)
HOST_MOV_MEM_REG(l, postinc, 0, 4, TRUE, FALSE)

#undef OFF
#define OFF int32 offset,
HOST_MOV_MEM_REG(b, indoff,  offset, 0, FALSE, TRUE)
HOST_MOV_MEM_REG(w, indoff,  offset, 0, FALSE, FALSE)
HOST_MOV_MEM_REG(l, indoff,  offset, 0, FALSE, FALSE)


#define HOST_MOV_MEM_REG_SWAP(size, amode, swap)			\
int									\
host_move ## size ## _ ## amode ## _reg_swap (COMMON_ARGS,		\
					      OFF int32 addr_reg,	\
					      int32 dst_reg)		\
{									\
  CHECK_ADDR_REG (addr_reg);					        \
									\
  host_move ## size ## _ ## amode ## _reg (COMMON_ARG_NAMES,		\
					   OFFNAME addr_reg, dst_reg);	\
  swap (dst_reg);							\
  return 0;								\
}


#undef OFF
#define OFF
#undef OFFNAME
#define OFFNAME
HOST_MOV_MEM_REG_SWAP(w, ind, HOST_SWAP16)
HOST_MOV_MEM_REG_SWAP(l, ind, HOST_SWAP32)

HOST_MOV_MEM_REG_SWAP(w, predec, HOST_SWAP16)
HOST_MOV_MEM_REG_SWAP(l, predec, HOST_SWAP32)

HOST_MOV_MEM_REG_SWAP(w, postinc, HOST_SWAP16)
HOST_MOV_MEM_REG_SWAP(l, postinc, HOST_SWAP32)

#undef OFF
#define OFF int32 offset,
#undef OFFNAME
#define OFFNAME offset,
HOST_MOV_MEM_REG_SWAP(w, indoff, HOST_SWAP16)
HOST_MOV_MEM_REG_SWAP(l, indoff, HOST_SWAP32)


#define HOST_OP_MEM_REG(size, op, amode, preoff, postoff, adjust_p, byte_p)  \
int									     \
host_ ## op ## size ## _ ## amode ## _reg (COMMON_ARGS,			     \
					   OFF int32 src_addr_reg,	     \
					   int32 dst_reg)		     \
{									     \
  int32 regoff;								     \
  int offset_p;								     \
  int guest_reg = c->host_reg_to_guest_reg[src_addr_reg];		     \
  									     \
  CHECK_ADDR_REG (src_addr_reg);				             \
								  	     \
  /* Compensate for a7 predec/postinc byte size means +-2 nonsense. */	     \
  if (adjust_p && ((preoff) & 1) && guest_reg == 15)  /* a7? */	             \
    regoff = (preoff) * 2;						     \
  else									     \
    regoff = preoff;							     \
									     \
  offset_p = (c->guest_reg_status[guest_reg].mapping == MAP_OFFSET);	     \
  if (offset_p)								     \
    regoff += c->guest_reg_status[guest_reg].offset;			     \
									     \
  /* Compensate for offset memory. */					     \
  regoff = (int32) SYN68K_TO_US (regoff);				     \
									     \
  /* Read the value in from memory. */					     \
  if (regoff == 0)							     \
    i386_ ## op ## size ## _ind_reg (COMMON_ARG_NAMES, src_addr_reg,	     \
				     dst_reg);				     \
  else									     \
    i386_ ## op ## size ## _indoff_reg (COMMON_ARG_NAMES, regoff,	     \
					src_addr_reg, dst_reg);		     \
									     \
  /* Adjust the address register. */					     \
  if (postoff && adjust_p && (byte_p || src_addr_reg != dst_reg))	     \
    {									     \
      int32 newoff;							     \
									     \
      if (((postoff) & 1) && guest_reg == 15)  /* a7?  Compensate... */      \
	newoff = (postoff) * 2;						     \
      else								     \
	newoff = postoff;						     \
									     \
      if (offset_p)							     \
	{								     \
	  /* If we're already offset, adjust our offset some more. */	     \
	  if ((c->guest_reg_status[guest_reg].offset += newoff) == 0)        \
	    c->guest_reg_status[guest_reg].mapping = MAP_NATIVE;	     \
	}								     \
      else								     \
	{								     \
	  /* We now become offset from our original value. */		     \
	  c->guest_reg_status[guest_reg].offset = newoff;		     \
	  c->guest_reg_status[guest_reg].mapping = MAP_OFFSET;		     \
	}								     \
    }									     \
									     \
  return 0;								     \
}


#undef OFF
#define OFF
HOST_OP_MEM_REG(b, and, ind,    0, 0, FALSE, TRUE)
HOST_OP_MEM_REG(w, and, ind,    0, 0, FALSE, FALSE)
HOST_OP_MEM_REG(l, and, ind,    0, 0, FALSE, FALSE)

HOST_OP_MEM_REG(b, and, predec, -1, -1, TRUE, TRUE)
HOST_OP_MEM_REG(w, and, predec, -2, -2, TRUE, FALSE)
HOST_OP_MEM_REG(l, and, predec, -4, -4, TRUE, FALSE)

HOST_OP_MEM_REG(b, and, postinc, 0, 1, TRUE, TRUE)
HOST_OP_MEM_REG(w, and, postinc, 0, 2, TRUE, FALSE)
HOST_OP_MEM_REG(l, and, postinc, 0, 4, TRUE, FALSE)

#undef OFF
#define OFF int32 offset,
HOST_OP_MEM_REG(b, and, indoff,  offset, 0, FALSE, TRUE)
HOST_OP_MEM_REG(w, and, indoff,  offset, 0, FALSE, FALSE)
HOST_OP_MEM_REG(l, and, indoff,  offset, 0, FALSE, FALSE)


#undef OFF
#define OFF
HOST_OP_MEM_REG(b, or, ind,    0, 0, FALSE, TRUE)
HOST_OP_MEM_REG(w, or, ind,    0, 0, FALSE, FALSE)
HOST_OP_MEM_REG(l, or, ind,    0, 0, FALSE, FALSE)

HOST_OP_MEM_REG(b, or, predec, -1, -1, TRUE, TRUE)
HOST_OP_MEM_REG(w, or, predec, -2, -2, TRUE, FALSE)
HOST_OP_MEM_REG(l, or, predec, -4, -4, TRUE, FALSE)

HOST_OP_MEM_REG(b, or, postinc, 0, 1, TRUE, TRUE)
HOST_OP_MEM_REG(w, or, postinc, 0, 2, TRUE, FALSE)
HOST_OP_MEM_REG(l, or, postinc, 0, 4, TRUE, FALSE)

#undef OFF
#define OFF int32 offset,
HOST_OP_MEM_REG(b, or, indoff,  offset, 0, FALSE, TRUE)
HOST_OP_MEM_REG(w, or, indoff,  offset, 0, FALSE, FALSE)
HOST_OP_MEM_REG(l, or, indoff,  offset, 0, FALSE, FALSE)


#undef OFF
#define OFF
HOST_OP_MEM_REG(b, add, ind,     0,  0, FALSE, TRUE)
HOST_OP_MEM_REG(b, add, predec, -1, -1, TRUE,  TRUE)
HOST_OP_MEM_REG(b, add, postinc, 0,  1, TRUE,  TRUE)

#undef OFF
#define OFF int32 offset,
HOST_OP_MEM_REG(b, add, indoff,  offset, 0, FALSE, TRUE)


#undef OFF
#define OFF
HOST_OP_MEM_REG(b, sub, ind,     0,  0, FALSE, TRUE)
HOST_OP_MEM_REG(b, sub, predec, -1, -1, TRUE,  TRUE)
HOST_OP_MEM_REG(b, sub, postinc, 0,  1, TRUE,  TRUE)

#undef OFF
#define OFF int32 offset,
HOST_OP_MEM_REG(b, sub, indoff,  offset, 0, FALSE, TRUE)


#undef OFF
#define OFF
HOST_OP_MEM_REG(b, cmp, ind,     0, 0, FALSE, TRUE)
HOST_OP_MEM_REG(w, cmp, ind,     0, 0, FALSE, FALSE)
HOST_OP_MEM_REG(l, cmp, ind,     0, 0, FALSE, FALSE)

HOST_OP_MEM_REG(b, cmp, predec, -1, -1, TRUE, TRUE)
HOST_OP_MEM_REG(w, cmp, predec, -2, -2, TRUE, FALSE)
HOST_OP_MEM_REG(l, cmp, predec, -4, -4, TRUE, FALSE)

HOST_OP_MEM_REG(b, cmp, postinc, 0, 1, TRUE, TRUE)
HOST_OP_MEM_REG(w, cmp, postinc, 0, 2, TRUE, FALSE)
HOST_OP_MEM_REG(l, cmp, postinc, 0, 4, TRUE, FALSE)

#undef OFF
#define OFF int32 offset,
HOST_OP_MEM_REG(b, cmp, indoff,  offset, 0, FALSE, TRUE)
HOST_OP_MEM_REG(w, cmp, indoff,  offset, 0, FALSE, FALSE)
HOST_OP_MEM_REG(l, cmp, indoff,  offset, 0, FALSE, FALSE)


#define HOST_OP_REG_MEM(size, op, amode, preoff, postoff, adjust_p)	     \
int									     \
host_ ## op ## size ## _reg_ ## amode (COMMON_ARGS, int32 src_reg,	     \
				       OFF int32 dst_addr_reg)		     \
{									     \
  int32 regoff;								     \
  int offset_p;								     \
  int guest_reg = c->host_reg_to_guest_reg[dst_addr_reg];		     \
  									     \
  CHECK_ADDR_REG (dst_addr_reg);				             \
								  	     \
  /* Compensate for a7 predec/postinc byte size means +-2 nonsense. */	     \
  if (adjust_p && ((preoff) & 1) && guest_reg == 15)  /* a7? */	             \
    regoff = (preoff) * 2;						     \
  else									     \
    regoff = preoff;							     \
									     \
  offset_p = (c->guest_reg_status[guest_reg].mapping == MAP_OFFSET);	     \
  if (offset_p)								     \
    regoff += c->guest_reg_status[guest_reg].offset;			     \
									     \
  /* Compensate for offset memory. */					     \
  regoff = (int32) SYN68K_TO_US (regoff);				     \
									     \
  /* Read the value in from memory. */					     \
  if (regoff == 0)							     \
    i386_ ## op ## size ## _reg_ind (COMMON_ARG_NAMES, src_reg,		     \
				     dst_addr_reg);			     \
  else									     \
    i386_ ## op ## size ## _reg_indoff (COMMON_ARG_NAMES, src_reg, regoff,   \
					dst_addr_reg);			     \
									     \
  /* Adjust the address register. */					     \
  if (postoff && adjust_p)						     \
    {									     \
      int32 newoff;							     \
									     \
      if (((postoff) & 1) && guest_reg == 15)  /* a7?  Compensate... */      \
	newoff = (postoff) * 2;						     \
      else								     \
	newoff = postoff;						     \
									     \
      if (offset_p)							     \
	{								     \
	  /* If we're already offset, adjust our offset some more. */	     \
	  if ((c->guest_reg_status[guest_reg].offset += newoff) == 0)        \
	    c->guest_reg_status[guest_reg].mapping = MAP_NATIVE;	     \
	}								     \
      else								     \
	{								     \
	  /* We now become offset from our original value. */		     \
	  c->guest_reg_status[guest_reg].offset = newoff;		     \
	  c->guest_reg_status[guest_reg].mapping = MAP_OFFSET;		     \
	}								     \
    }									     \
									     \
  return 0;								     \
}


#undef OFF
#define OFF
HOST_OP_REG_MEM(b, and, ind,    0, 0, FALSE)
HOST_OP_REG_MEM(w, and, ind,    0, 0, FALSE)
HOST_OP_REG_MEM(l, and, ind,    0, 0, FALSE)

HOST_OP_REG_MEM(b, and, predec, -1, -1, TRUE)
HOST_OP_REG_MEM(w, and, predec, -2, -2, TRUE)
HOST_OP_REG_MEM(l, and, predec, -4, -4, TRUE)

HOST_OP_REG_MEM(b, and, postinc, 0, 1, TRUE)
HOST_OP_REG_MEM(w, and, postinc, 0, 2, TRUE)
HOST_OP_REG_MEM(l, and, postinc, 0, 4, TRUE)

#undef OFF
#define OFF int32 offset,
HOST_OP_REG_MEM(b, and, indoff,  offset, 0, FALSE)
HOST_OP_REG_MEM(w, and, indoff,  offset, 0, FALSE)
HOST_OP_REG_MEM(l, and, indoff,  offset, 0, FALSE)


#undef OFF
#define OFF
HOST_OP_REG_MEM(b, or, ind,    0, 0, FALSE)
HOST_OP_REG_MEM(w, or, ind,    0, 0, FALSE)
HOST_OP_REG_MEM(l, or, ind,    0, 0, FALSE)

HOST_OP_REG_MEM(b, or, predec, -1, -1, TRUE)
HOST_OP_REG_MEM(w, or, predec, -2, -2, TRUE)
HOST_OP_REG_MEM(l, or, predec, -4, -4, TRUE)

HOST_OP_REG_MEM(b, or, postinc, 0, 1, TRUE)
HOST_OP_REG_MEM(w, or, postinc, 0, 2, TRUE)
HOST_OP_REG_MEM(l, or, postinc, 0, 4, TRUE)

#undef OFF
#define OFF int32 offset,
HOST_OP_REG_MEM(b, or, indoff,  offset, 0, FALSE)
HOST_OP_REG_MEM(w, or, indoff,  offset, 0, FALSE)
HOST_OP_REG_MEM(l, or, indoff,  offset, 0, FALSE)


#undef OFF
#define OFF
HOST_OP_REG_MEM(b, xor, ind,    0, 0, FALSE)
HOST_OP_REG_MEM(w, xor, ind,    0, 0, FALSE)
HOST_OP_REG_MEM(l, xor, ind,    0, 0, FALSE)

HOST_OP_REG_MEM(b, xor, predec, -1, -1, TRUE)
HOST_OP_REG_MEM(w, xor, predec, -2, -2, TRUE)
HOST_OP_REG_MEM(l, xor, predec, -4, -4, TRUE)

HOST_OP_REG_MEM(b, xor, postinc, 0, 1, TRUE)
HOST_OP_REG_MEM(w, xor, postinc, 0, 2, TRUE)
HOST_OP_REG_MEM(l, xor, postinc, 0, 4, TRUE)

#undef OFF
#define OFF int32 offset,
HOST_OP_REG_MEM(b, xor, indoff,  offset, 0, FALSE)
HOST_OP_REG_MEM(w, xor, indoff,  offset, 0, FALSE)
HOST_OP_REG_MEM(l, xor, indoff,  offset, 0, FALSE)


#undef OFF
#define OFF
HOST_OP_REG_MEM(b, add, ind,     0, 0, FALSE)
HOST_OP_REG_MEM(b, add, predec, -1, -1, TRUE)
HOST_OP_REG_MEM(b, add, postinc, 0, 1, TRUE)

#undef OFF
#define OFF int32 offset,
HOST_OP_REG_MEM(b, add, indoff,  offset, 0, FALSE)


#undef OFF
#define OFF
HOST_OP_REG_MEM(b, sub, ind,     0, 0, FALSE)
HOST_OP_REG_MEM(b, sub, predec, -1, -1, TRUE)
HOST_OP_REG_MEM(b, sub, postinc, 0, 1, TRUE)

#undef OFF
#define OFF int32 offset,
HOST_OP_REG_MEM(b, sub, indoff,  offset, 0, FALSE)


#undef OFF
#define OFF
HOST_OP_REG_MEM(b, cmp, ind,     0, 0, FALSE)
HOST_OP_REG_MEM(w, cmp, ind,     0, 0, FALSE)
HOST_OP_REG_MEM(l, cmp, ind,     0, 0, FALSE)

HOST_OP_REG_MEM(b, cmp, predec, -1, -1, TRUE)
HOST_OP_REG_MEM(w, cmp, predec, -2, -2, TRUE)
HOST_OP_REG_MEM(l, cmp, predec, -4, -4, TRUE)

HOST_OP_REG_MEM(b, cmp, postinc, 0, 1, TRUE)
HOST_OP_REG_MEM(w, cmp, postinc, 0, 2, TRUE)
HOST_OP_REG_MEM(l, cmp, postinc, 0, 4, TRUE)

#undef OFF
#define OFF int32 offset,
HOST_OP_REG_MEM(b, cmp, indoff,  offset, 0, FALSE)
HOST_OP_REG_MEM(w, cmp, indoff,  offset, 0, FALSE)
HOST_OP_REG_MEM(l, cmp, indoff,  offset, 0, FALSE)


int
host_cmpmb_postinc_postinc (COMMON_ARGS, int32 reg1, int32 reg2)
{
  host_moveb_postinc_reg (COMMON_ARG_NAMES, reg1, scratch_reg);
  host_moveb_postinc_reg (COMMON_ARG_NAMES, reg2, scratch_reg + 4);
  host_unoffset_reg (c, codep, cc_spill_if_changed,
		     c->host_reg_to_guest_reg[reg1]);
  host_unoffset_reg (c, codep, cc_spill_if_changed,
		     c->host_reg_to_guest_reg[reg2]);
  i386_cmpb_reg_reg (COMMON_ARG_NAMES, scratch_reg, scratch_reg + 4);
  return 0;
}


int
host_cmpmw_postinc_postinc (COMMON_ARGS, int32 reg1, int32 reg2)
{
  int32 scratch_reg_2;

  scratch_reg_2 = host_alloc_reg (c, codep, cc_spill_if_changed,
				  REGSET_ALL & ~((1L << reg1)
						 | (1L << reg2)
						 | (1L << scratch_reg)));
  if (scratch_reg_2 == NO_REG)
    return 1;

  host_movew_postinc_reg (COMMON_ARG_NAMES, reg1, scratch_reg);
  host_movew_postinc_reg (COMMON_ARG_NAMES, reg2, scratch_reg_2);

  /* We choose to unoffset the registers _now_, because we'll almost
   * certainly need the cc bits after the compare and the adds to unoffset
   * will clobber the i386 cc's.
   */
  
  /* Add and swap back to back, for Pentium pairability. */
  host_unoffset_reg (c, codep, cc_spill_if_changed,
		     c->host_reg_to_guest_reg[reg1]);
  if (cc_to_compute & (M68K_CCN | M68K_CCC | M68K_CCV))
    HOST_SWAP16 (scratch_reg);

  /* Add and swap back to back, for Pentium pairability. */
  host_unoffset_reg (c, codep, cc_spill_if_changed,
		     c->host_reg_to_guest_reg[reg2]);
  if (cc_to_compute & (M68K_CCN | M68K_CCC | M68K_CCV))
    HOST_SWAP16 (scratch_reg_2);

  /* Finally do the compare. */
  i386_cmpw_reg_reg (COMMON_ARG_NAMES, scratch_reg, scratch_reg_2);

  return 0;
}


int
host_cmpml_postinc_postinc (COMMON_ARGS, int32 reg1, int32 reg2)
{
  int32 scratch_reg_2;

  scratch_reg_2 = host_alloc_reg (c, codep, cc_spill_if_changed,
				  REGSET_ALL & ~((1L << reg1)
						 | (1L << reg2)
						 | (1L << scratch_reg)));
  if (scratch_reg_2 == NO_REG)
    return 1;

  host_movel_postinc_reg (COMMON_ARG_NAMES, reg1, scratch_reg);
  host_movel_postinc_reg (COMMON_ARG_NAMES, reg2, scratch_reg_2);

  /* We choose to unoffset the registers _now_, because we'll almost
   * certainly need the cc bits after the compare and the adds to unoffset
   * will clobber the i386 cc's.
   */
  
  /* Adds back to back for Pentium pairability since bswap isn't pairable. */
  host_unoffset_reg (c, codep, cc_spill_if_changed,
		     c->host_reg_to_guest_reg[reg1]);
  host_unoffset_reg (c, codep, cc_spill_if_changed,
		     c->host_reg_to_guest_reg[reg2]);

  /* Swap the two temp guys around to native endian byte order. */
  if (cc_to_compute & (M68K_CCN | M68K_CCC | M68K_CCV))
    {
      host_swap32 (COMMON_ARG_NAMES, scratch_reg);
      host_swap32 (COMMON_ARG_NAMES, scratch_reg_2);
    }

  /* Finally do the compare. */
  i386_cmpl_reg_reg (COMMON_ARG_NAMES, scratch_reg, scratch_reg_2);

  return 0;
}


/* We special case this so we can deal with offset registers efficiently. */
#define ADD_SUB_CMPL_IMM_REG(op, v, adj, compare_p)			\
int									\
host_ ## op ## l_imm_reg (COMMON_ARGS, int32 val, int32 host_reg)	\
{									\
  int guest_reg;							\
  guest_reg_status_t *r;						\
									\
  guest_reg = c->host_reg_to_guest_reg[host_reg];			\
  r = &c->guest_reg_status[guest_reg];					\
									\
  if (!(cc_to_compute & ~(M68K_CCZ | M68K_CCN)))			\
    {									\
      if (r->mapping == MAP_OFFSET)					\
	{								\
	  val adj r->offset;						\
	  r->mapping = MAP_NATIVE;					\
	  if (val == 0 && !cc_to_compute)				\
/*-->*/	    return 0;							\
	}								\
									\
      if (compare_p							\
	  || !(cc_spill_if_changed & ~M68K_CCX)				\
	  || cc_to_compute)						\
	{								\
	  if (!compare_p && (v) == 1)					\
	    i386_incl_reg (COMMON_ARG_NAMES, host_reg);			\
	  else if (!compare_p && (v) == -1)				\
	    i386_decl_reg (COMMON_ARG_NAMES, host_reg);			\
	  else								\
	    i386_ ## op ## l_imm_reg (COMMON_ARG_NAMES, val, host_reg);	\
	}								\
      else								\
	{								\
	  /* We can add without touching any cc's with leal. */		\
	  i386_leal_indoff (c, codep, M68K_CC_NONE, M68K_CC_NONE,	\
			    NO_REG, (v), host_reg, host_reg);		\
	}								\
    }									\
  else  /* We need the tricky cc bits. */				\
    {									\
      if (r->mapping == MAP_OFFSET)					\
	{								\
	  int32 offset = r->offset;					\
	  if (offset != 0)						\
	    {								\
	      if (offset == 1)						\
		i386_incl_reg (COMMON_ARG_NAMES, host_reg);		\
	      else if (offset == -1)					\
		i386_decl_reg (COMMON_ARG_NAMES, host_reg);		\
	      else							\
		i386_addl_imm_reg (COMMON_ARG_NAMES, offset, host_reg);	\
	    }								\
									\
	  r->mapping = MAP_NATIVE;					\
	}								\
									\
      i386_ ## op ## l_imm_reg (COMMON_ARG_NAMES, val, host_reg);	\
    }									\
									\
  return 0;								\
}


ADD_SUB_CMPL_IMM_REG (add,  val, +=, FALSE)
ADD_SUB_CMPL_IMM_REG (sub, -val, -=, FALSE)
ADD_SUB_CMPL_IMM_REG (cmp, -val, -=, TRUE)


int
host_bclr_imm_reg (COMMON_ARGS, int32 bitnum, int32 reg)
{
  return host_andl_imm_reg (c, codep, cc_spill_if_changed, M68K_CC_NONE,
			    scratch_reg, ~(1L << (bitnum & 31)), reg);
}


int
host_bset_imm_reg (COMMON_ARGS, int32 bitnum, int32 reg)
{
  return host_orl_imm_reg (c, codep, cc_spill_if_changed, M68K_CC_NONE,
			   scratch_reg, 1L << (bitnum & 31), reg);
}


int
host_bchg_imm_reg (COMMON_ARGS, int32 bitnum, int32 reg)
{
  return host_xorl_imm_reg (c, codep, cc_spill_if_changed, M68K_CC_NONE,
			    scratch_reg, 1L << (bitnum & 31), reg);
}


int
host_btst_imm_reg (COMMON_ARGS, int32 bitnum, int32 reg)
{
  uint32 mask;

  mask = 1L << (bitnum & 31);
  switch (c->guest_reg_status[c->host_reg_to_guest_reg[reg]].mapping)
    {
    case MAP_NATIVE:
      break;
    case MAP_SWAP16:
      mask = (mask & 0xFFFF0000) | SWAPUW_IFLE (mask);
      break;
    case MAP_SWAP32:
      mask = SWAPUL_IFLE (mask);
      break;
    default:
      abort ();
    }

  if (reg < 4 && (mask & 0xFFFF))
    {
      /* Do either "testb $0x4,%al" or "testb $0x8,%ah" (for example). */
      if (mask & 0xFF)
	i386_testb_imm_reg (COMMON_ARG_NAMES, mask, reg);
      else
	i386_testb_imm_reg (COMMON_ARG_NAMES, mask >> 8, reg + 4);
    }
  else
    i386_testl_imm_reg (COMMON_ARG_NAMES, mask, reg);
  return 0;
}


#define CONDL_BRANCH(name, cc_in, i386_insn, i386_reverse_insn,		    \
		     noncached_variant_p,	    			    \
		     two_noncached_cc_p, cc1, njmp1, cc2, njmp2,	    \
		     first_test_child)					    \
int									    \
host_ ## name (COMMON_ARGS, Block *b)					    \
{									    \
  unsigned cc_to_spill, cc[2];						    \
  host_code_t *start_code;						    \
  int reverse_branch_p, cc_diff;					    \
  static const int8 bits_set[32] =					    \
    {									    \
      0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,			    \
      1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5			    \
    };									    \
									    \
  /* Record the start so we can provide accurate backpatch information. */  \
  start_code = *codep;							    \
									    \
  /* First spill any dirty or offset registers. */			    \
  host_spill_regs (c, codep, cc_spill_if_changed | (cc_in));		    \
									    \
  cc[0] = b->child[0]->cc_needed & c->cached_cc & c->dirty_cc;		    \
  cc[1] = b->child[1]->cc_needed & c->cached_cc & c->dirty_cc;		    \
									    \
  /* We'll prefer branching when we don't need to spill ccs, and we'll	    \
   * prefer branching to a block with children (e.g. avoid rts since	    \
   * it's not likely to be in a loop.)					    \
   */									    \
  cc_diff = bits_set[cc[0]] - bits_set[cc[1]];				    \
  reverse_branch_p = (cc_diff < 0					    \
		      || (cc_diff == 0 && b->child[1]->num_parents == 0));  \
									    \
  /* Make sure that the cc bits we need are still alive. */		    \
  if ((c->cached_cc & (cc_in)) == (cc_in))				    \
    {									    \
      /* Spill any cc bits that are live and cached, but only those needed  \
       * if we take the TRUE arm of the branch.  We assume here that our    \
       * SPILL_CC_BITS() routine leaves the cc bits cached in reality, even \
       * though it marks them uncached.					    \
       */								    \
      cc_to_spill = cc[!reverse_branch_p];				    \
      SPILL_CC_BITS (c, codep, cc_to_spill);				    \
									    \
      /* Emit a conditional branch. */					    \
      if (reverse_branch_p)						    \
	i386_reverse_insn (COMMON_ARG_NAMES, 0x12345678);		    \
      else								    \
	i386_insn (COMMON_ARG_NAMES, 0x12345678);			    \
    }									    \
  else if (noncached_variant_p)						    \
    {									    \
      cc_to_spill = ((b->child[0]->cc_needed | b->child[1]->cc_needed	    \
		      | (cc_in))					    \
		     & c->cached_cc & c->dirty_cc);			    \
      SPILL_CC_BITS (c, codep, cc_to_spill);				    \
									    \
      cc_to_compute = M68K_CCZ;						    \
      cc_spill_if_changed = M68K_CC_NONE;				    \
      									    \
      /* We can test up to two cc bits for known values. */		    \
      if (two_noncached_cc_p)						    \
	{								    \
	  Block *child;							    \
									    \
	  i386_cmpb_imm_indoff (COMMON_ARG_NAMES, 0,			    \
				offsetof (CPUState, cc1), REG_EBP);	    \
	  njmp1 (COMMON_ARG_NAMES, 0x12345678);		    	    \
									    \
	  /* Create a backpatch for this conditional branch. */		    \
	  child = b->child[first_test_child];				    \
	  backpatch_add (b, (*codep - start_code - 4) * 8, 32, TRUE,	    \
			 -4       /* Branch is relative to end of jmp.  */  \
			 + NATIVE_START_BYTE_OFFSET /* skip synth opcode */ \
			 + (child->m68k_start_address > b->m68k_start_address \
			    ? CHECK_INTERRUPT_STUB_BYTES : 0),		    \
			 child);					    \
	}								    \
									    \
      /* Check the secondary success condition. */			    \
      i386_cmpb_imm_indoff (COMMON_ARG_NAMES, 0,			    \
			    offsetof (CPUState, cc2), REG_EBP);		    \
      njmp2 (COMMON_ARG_NAMES, 0x12345678);				    \
									    \
      /* Note that we shouldn't try to save the cc bits output by the cmp   \
       * we just generated.						    \
       */								    \
      c->cached_cc = M68K_CC_NONE;					    \
      c->dirty_cc = M68K_CC_NONE;					    \
									    \
      reverse_branch_p = FALSE;						    \
    }									    \
  else									    \
    return 1;								    \
									    \
  /* Create a backpatch for this conditional branch.  Note that this	    \
   * backpatch is only relative to the start of this native code;	    \
   * the main compile loop will adjust the backpatch to be relative	    \
   * to the block start once our code has been placed.			    \
   */									    \
  backpatch_add (b, (*codep - start_code - 4) * 8, 32, TRUE,		    \
		 -4              /* Branch is relative to end of jxx.   */  \
		 + NATIVE_START_BYTE_OFFSET				    \
		 + ((b->child[!reverse_branch_p]->m68k_start_address	    \
		     > b->m68k_start_address)				    \
		    ? CHECK_INTERRUPT_STUB_BYTES : 0),			    \
		 b->child[!reverse_branch_p]);				    \
									    \
  /* Spill any cc bits that are live and cached, but only those needed	    \
   * if we take the FALSE arm of the branch.				    \
   */									    \
  cc_to_spill = (cc[reverse_branch_p]);					    \
  SPILL_CC_BITS (c, codep, cc_to_spill);				    \
									    \
  /* Emit a jmp to the failure block. */				    \
  i386_jmp (COMMON_ARG_NAMES, 0x12345678);				    \
									    \
  /* Create a backpatch for this unconditional jmp. */			    \
  backpatch_add (b, (*codep - start_code - 4) * 8, 32, TRUE,		    \
		 -4              /* Branch is relative to end of jmp.  */   \
		 + NATIVE_START_BYTE_OFFSET /* Skip initial synth opcode.*/ \
		 + ((b->child[reverse_branch_p]->m68k_start_address	    \
		     > b->m68k_start_address)				    \
		    ? CHECK_INTERRUPT_STUB_BYTES : 0),			    \
		 b->child[reverse_branch_p]);				    \
  return 0;								    \
}

CONDL_BRANCH (bcc, M68K_CCC,                       i386_jnc, i386_jc,
	      TRUE, FALSE, ccc, i386_jz, ccc, i386_jz, 0)
CONDL_BRANCH (bcs, M68K_CCC,                       i386_jc, i386_jnc,
	      TRUE, FALSE, ccc, i386_jz, ccc, i386_jnz, 0)
CONDL_BRANCH (beq, M68K_CCZ,                       i386_jz, i386_jnz,
	      TRUE, FALSE, ccc, i386_jz, ccnz, i386_jz, 0)
CONDL_BRANCH (bne, M68K_CCZ,                       i386_jnz, i386_jz,
	      TRUE, FALSE, ccc, i386_jz, ccnz, i386_jnz, 0)
CONDL_BRANCH (bge, M68K_CCN | M68K_CCV,            i386_jge, i386_jl,
	      FALSE, FALSE, ccc, i386_jz, ccnz, i386_jz, 0)
CONDL_BRANCH (blt, M68K_CCN | M68K_CCV,            i386_jl, i386_jge,
	      FALSE, FALSE, ccc, i386_jz, ccnz, i386_jz, 0)
CONDL_BRANCH (bgt, M68K_CCN | M68K_CCV | M68K_CCZ, i386_jnle, i386_jle,
	      FALSE, FALSE, ccc, i386_jz, ccnz, i386_jz, 0)
CONDL_BRANCH (ble, M68K_CCN | M68K_CCV | M68K_CCZ, i386_jle, i386_jnle,
	      FALSE, FALSE, ccc, i386_jz, ccnz, i386_jz, 0)
CONDL_BRANCH (bhi, M68K_CCC | M68K_CCZ,            i386_jnbe, i386_jbe,
	      TRUE, TRUE, ccc, i386_jnz, ccnz, i386_jnz, 0)
CONDL_BRANCH (bls, M68K_CCC | M68K_CCZ,            i386_jbe, i386_jnbe,
	      TRUE, TRUE, ccc, i386_jnz, ccnz, i386_jz, 1)
CONDL_BRANCH (bpl, M68K_CCN,                       i386_jns, i386_js,
	      TRUE, FALSE, ccc, i386_jz, ccn, i386_jz, 0)
CONDL_BRANCH (bmi, M68K_CCN,                       i386_js, i386_jns,
	      TRUE, FALSE, ccc, i386_jz, ccn, i386_jnz, 0)
CONDL_BRANCH (bvc, M68K_CCV,                       i386_jno, i386_jo,
	      TRUE, FALSE, ccc, i386_jz, ccv, i386_jz, 0)
CONDL_BRANCH (bvs, M68K_CCV,                       i386_jo, i386_jno,
	      TRUE, FALSE, ccc, i386_jz, ccv, i386_jnz, 0)


int
host_jmp (COMMON_ARGS, Block *b)
{
  unsigned cc_to_spill;
  host_code_t *start_code;

  /* Record the start so we can provide accurate backpatch information. */
  start_code = *codep;

  /* Spill any needed cc bits. */
  cc_to_spill = (b->child[0]->cc_needed & c->cached_cc & c->dirty_cc);
  SPILL_CC_BITS (c, codep, cc_to_spill);

  /* Spill any dirty or offset registers. */
  host_spill_regs (c, codep, M68K_CC_NONE);

  /* Emit a jmp to the target block. */
  i386_jmp (COMMON_ARG_NAMES, 0x12345678);

  /* Create a backpatch for this jmp. */
  backpatch_add (b, (*codep - start_code - 4) * 8, 32, TRUE,
		 -4		/* Branch is relative to end of jmp.  */
		 + NATIVE_START_BYTE_OFFSET /* Skip initial synth opcode.*/
		 + ((b->child[0]->m68k_start_address > b->m68k_start_address)
		    ? CHECK_INTERRUPT_STUB_BYTES : 0),
		 b->child[0]);

  return 0;
}


int
host_dbra (COMMON_ARGS, int32 guest_reg, Block *b)
{
  unsigned cc_to_spill;
  host_code_t *start_code;
  int32 host_reg;

  /* Record the start so we can provide accurate backpatch information. */
  start_code = *codep;

  /* Spill any cc bits that are live and cached. */
  cc_to_spill = (c->cached_cc & c->dirty_cc);
  SPILL_CC_BITS (c, codep, cc_to_spill);

  /* Canonicalize all registers.  If the loop register is cached,
   * pretend like it's dirty so it gets canonicalized.  It will be
   * decremented below, so we won't be doing any extra work or
   * anything.
   */
  host_reg = c->guest_reg_status[guest_reg].host_reg;
  if (host_reg != NO_REG)
    c->guest_reg_status[guest_reg].dirty_without_offset_p = TRUE;
  make_dirty_regs_native (c, codep, M68K_CC_NONE);

  /* Decrement the loop register, whether it's cached or in memory. */
  if (host_reg == NO_REG)
    {
      /* It's in memory.  We've got to use subw since we need the carry. */
      i386_subw_imm_indoff (c, codep, M68K_CC_NONE, M68K_CCC, NO_REG,
			    1, offsetof (CPUState, regs[guest_reg].uw.n),
			    REG_EBP);
    }
  else
    {
      /* It's in a reg.  We've got to use subw since we need the carry.
       * We already marked the reg dirty, above.
       */
      i386_subw_imm_reg (c, codep, M68K_CC_NONE, M68K_CCC, NO_REG,
			 1, host_reg);
    }
  
  /* Spill all registers back to memory. */
  host_spill_regs (c, codep, M68K_CCC);

  /* Make sure that we've still got the carry from the decw.  Of course,
   * the real m68k doesn't have this carry value, so remember that.
   */
  if (!(c->cached_cc & M68K_CCC))
    return 1;
  c->cached_cc = M68K_CC_NONE;

  /* Emit a conditional branch. */
  i386_jnc (COMMON_ARG_NAMES, 0x12345678);

  /* Create a backpatch for this conditional branch.  Note that this
   * backpatch is only relative to the start of this native code;
   * the main compile loop will adjust the backpatch to be relative
   * to the block start once our code has been placed.
   */
  backpatch_add (b, (*codep - start_code - 4) * 8, 32, TRUE,
		 -4              /* Branch is relative to end of jxx.   */
		 + NATIVE_START_BYTE_OFFSET, /* Skip initial synth opcode. */
		 b->child[1]);

  /* Emit a jmp to the failure block. */
  i386_jmp (COMMON_ARG_NAMES, 0x12345678);

  /* Create a backpatch for this unconditional jmp. */
  backpatch_add (b, (*codep - start_code - 4) * 8, 32, TRUE,
		 -4              /* Branch is relative to end of jmp.  */
		 + NATIVE_START_BYTE_OFFSET /* Skip initial synth opcode.*/
		 + ((b->child[0]->m68k_start_address > b->m68k_start_address)
		    ? CHECK_INTERRUPT_STUB_BYTES : 0),
		 b->child[0]);
  return 0;
}


int
host_swap (COMMON_ARGS, int32 reg)
{
  return i386_rorl_imm_reg (c, codep, cc_spill_if_changed, M68K_CC_NONE,
			    NO_REG, 16, reg);
}


int
host_extbw (COMMON_ARGS, int32 reg)
{
  /* movb %al,%ah
   * sarb $7,%ah
   * testb %al,%al  (optional)
   */
  i386_movb_reg_reg (COMMON_ARG_NAMES, reg, reg + 4);
  i386_sarb_imm_reg (COMMON_ARG_NAMES, 7, reg + 4);
  if (cc_to_compute)
    i386_testb_reg_reg (COMMON_ARG_NAMES, reg, reg);
  return 0;
}


int
host_extwl (COMMON_ARGS, int32 reg)
{
  i386_movswl_reg_reg (COMMON_ARG_NAMES, reg, reg);
  if (cc_to_compute)
    i386_testl_reg_reg (COMMON_ARG_NAMES, reg, reg);
  return 0;
}


int
host_extbl (COMMON_ARGS, int32 reg)
{
  if (cc_to_compute)
    i386_testb_reg_reg (COMMON_ARG_NAMES, reg, reg);
  return i386_movsbl_reg_reg (COMMON_ARG_NAMES, reg, reg);
}


int
host_unlk (COMMON_ARGS, int32 reg, int32 a7_reg)
{
  if (reg == a7_reg)  /* unlk a7 wedges the real 68040...heh */
    return 1;
  if (host_leal_indoff_areg (COMMON_ARG_NAMES, 4, reg, a7_reg))
    return 1;
  if (host_movel_ind_reg (COMMON_ARG_NAMES, reg, reg))
    return 1;
  return 0;
}

int
host_link (COMMON_ARGS, int32 offset, int32 reg, int32 a7_reg)
{
  guest_reg_status_t *ga7;

  ga7 = &c->guest_reg_status[15];

  /* Push the old value on the stack, and move a7 into the frame pointer. */
  host_subl_imm_reg (COMMON_ARG_NAMES, 4, a7_reg);
  assert (ga7->mapping == MAP_NATIVE);
  i386_movl_reg_indoff (COMMON_ARG_NAMES, reg, (int32) SYN68K_TO_US (0),
			a7_reg);
  i386_movl_reg_reg (COMMON_ARG_NAMES, a7_reg, reg);

  /* Add the specified offset to a7. */
  ga7->mapping = MAP_OFFSET;
  ga7->offset = offset;

  return 0;
}


int
host_moveml_reg_predec (COMMON_ARGS, int32 host_addr_reg, int32 reg_mask)
{
  int i, guest_addr_reg;
  guest_reg_status_t *g;
  int32 off;

  guest_addr_reg = c->host_reg_to_guest_reg[host_addr_reg];

  g = &c->guest_reg_status[guest_addr_reg];
  if (g->mapping == MAP_OFFSET)
    off = g->offset;
  else
    off = 0;

  /* Compensate for offset memory. */
  off = (int32) SYN68K_TO_US (off);

  for (i = 15; i >= 0; i--)
    if (reg_mask & (0x8000 >> i))
      {
	int reg_to_move, host_reg;

	g = &c->guest_reg_status[i];
	host_reg = g->host_reg;
	if (host_reg == host_addr_reg)
	  {
	    /* Here we move the real value of the register minus 4;
	     * this behavior is correct for the 68020 and up.
	     */
	    i386_leal_indoff (COMMON_ARG_NAMES,
			      ((g->mapping == MAP_OFFSET)
			       ? g->offset : 0) - 4,
			      host_addr_reg, scratch_reg);
	    host_swap32 (COMMON_ARG_NAMES, scratch_reg);
	    reg_to_move = scratch_reg;
	  }
	else if (host_reg == NO_REG)
	  {
	    i386_movl_indoff_reg (COMMON_ARG_NAMES,
				  offsetof (CPUState, regs[i].ul.n), REG_EBP,
				  scratch_reg);
	    host_swap32 (COMMON_ARG_NAMES, scratch_reg);
	    reg_to_move = scratch_reg;
	  }
	else
	  {
	    switch (g->mapping)
	      {
	      case MAP_OFFSET:
		host_unoffset_reg (c, codep, cc_spill_if_changed, i);
		/* Fall through to MAP_NATIVE case... */
	      case MAP_NATIVE:
		host_swap32 (COMMON_ARG_NAMES, host_reg);
		g->mapping = MAP_SWAP32;
		break;
	      case MAP_SWAP16:
		host_swap16_to_32 (COMMON_ARG_NAMES, host_reg);
		g->mapping = MAP_SWAP32;
		break;
	      case MAP_SWAP32:
		break;
	      default:
		abort ();
	      }
	    reg_to_move = host_reg;
	  }

	off -= 4;
	if (off != 0)
	  i386_movl_reg_indoff (COMMON_ARG_NAMES, reg_to_move, off,
				host_addr_reg);
	else
	  i386_movl_reg_ind (COMMON_ARG_NAMES, reg_to_move, host_addr_reg);
      }

  g = &c->guest_reg_status[guest_addr_reg];

  /* Undo fake memory offset. */
  off = US_TO_SYN68K (off);

  if (off == 0)
    g->mapping = MAP_NATIVE;
  else
    {
      g->mapping = MAP_OFFSET;
      g->offset = off;
    }

  return 0;
}


int
host_moveml_postinc_reg (COMMON_ARGS, int32 host_addr_reg, int32 reg_mask)
{
  int i, guest_addr_reg, off;
  guest_reg_status_t *g;
  host_reg_mask_t locked_host_regs;

  guest_addr_reg = c->host_reg_to_guest_reg[host_addr_reg];

  locked_host_regs = ((~ALLOCATABLE_REG_MASK) | (1L << host_addr_reg)
		      | (1L << scratch_reg));

  g = &c->guest_reg_status[guest_addr_reg];
  if (g->mapping == MAP_OFFSET)
    off = g->offset;
  else
    off = 0;

  /* Compensate for offset memory. */
  off = (int32) SYN68K_TO_US (off);

  for (i = 0; i < 16; i++)
    if (reg_mask & (1L << i))
      {
	int host_reg;

	if (i == guest_addr_reg)
	  {
	    /* Don't load up our address register. */
	    off += 4;
	    continue;
	  }

	g = &c->guest_reg_status[i];
	host_reg = g->host_reg;
	if (host_reg == NO_REG)
	  {
	    host_reg = host_alloc_reg (c, codep, cc_spill_if_changed,
				       ((i < 8)
					? (REGSET_BYTE & ~locked_host_regs)
					: ~locked_host_regs));
	    if (host_reg == NO_REG)
	      host_reg = scratch_reg;
	    else
	      {
		locked_host_regs |= (1L << host_reg);
		c->host_reg_to_guest_reg[host_reg] = i;
		g->host_reg = host_reg;
	      }
	  }

	if (off == 0)
	  i386_movl_ind_reg (COMMON_ARG_NAMES, host_addr_reg, host_reg);
	else
	  i386_movl_indoff_reg (COMMON_ARG_NAMES, off, host_addr_reg,
				host_reg);

	if (host_reg == scratch_reg)
	  {
	    host_swap32 (COMMON_ARG_NAMES, scratch_reg);
	    i386_movl_reg_indoff (COMMON_ARG_NAMES, scratch_reg,
				  offsetof (CPUState, regs[i].ul.n), REG_EBP);
	  }
	else
	  {
	    g->mapping = MAP_SWAP32;
	    g->dirty_without_offset_p = TRUE;
	  }
	off += 4;
      }

  g = &c->guest_reg_status[guest_addr_reg];

  /* Undo fake memory offset. */
  off = US_TO_SYN68K (off);

  if (off == 0)
    g->mapping = MAP_NATIVE;
  else
    {
      g->mapping = MAP_OFFSET;
      g->offset = off;
    }

  return 0;
}


int
host_pea_indoff (COMMON_ARGS, int32 offset, int32 host_reg, int32 a7_reg)
{
  int guest_reg;
  guest_reg_status_t *g;

  guest_reg = c->host_reg_to_guest_reg[host_reg];
  g = &c->guest_reg_status[guest_reg];
  if (g->mapping == MAP_OFFSET)
    offset += g->offset;
  
  if (offset == 0)
    i386_movl_reg_reg (COMMON_ARG_NAMES, host_reg, scratch_reg);
  else
    i386_leal_indoff (COMMON_ARG_NAMES, offset, host_reg, scratch_reg);
  host_swap32 (COMMON_ARG_NAMES, scratch_reg);
  return host_movel_reg_predec (COMMON_ARG_NAMES, scratch_reg, a7_reg);
}


int
host_leal_indoff_areg (COMMON_ARGS, int32 offset, int32 base_reg,
		       int32 dst_reg)
{
  guest_reg_status_t *g;

  g = &c->guest_reg_status[c->host_reg_to_guest_reg[base_reg]];
  if (g->mapping == MAP_OFFSET)
    offset += g->offset;
  
  if (offset == 0)
    i386_movl_reg_reg (COMMON_ARG_NAMES, base_reg, dst_reg);
  else
    i386_leal_indoff (COMMON_ARG_NAMES, offset, base_reg, dst_reg);

  return 0;
}


int
host_rts (COMMON_ARGS, Block *b, int32 a7_reg)
{
  int jsr_sp_reg;
  host_code_t *jnz_end_addr, *start_code;

  /* Keep track of original codep so we can backpatch, below. */
  start_code = *codep;

  /* Spill all dirty cc bits. */
  host_spill_cc_bits (c, codep, M68K_CC_ALL);
  cc_spill_if_changed = M68K_CC_NONE;

  /* Grab the return address. */
  host_movel_postinc_reg (COMMON_ARG_NAMES, a7_reg, scratch_reg);

  /* Spill all dirty regs. */
  host_spill_regs (c, codep, M68K_CC_NONE);

  /* Allocate a register to hold the jsr stack pointer. */
  jsr_sp_reg = host_alloc_reg (c, codep, M68K_CC_NONE,
			       ALLOCATABLE_REG_MASK & ~(1L << scratch_reg));
  if (jsr_sp_reg == NO_REG)
    return 1;

  /* Grab the jsr stack pointer. */
  i386_movl_indoff_reg (COMMON_ARG_NAMES,
			offsetof (CPUState, jsr_stack_byte_index), REG_EBP,
			jsr_sp_reg);

  /* Compare the return address to the tag at the top of the jsr
   * stack cache.  The tags are kept in big endian order, so we don't
   * need to byte swap here.
   */
  i386_cmpl_reg_indoff (COMMON_ARG_NAMES, scratch_reg,
			(int32)&cpu_state.jsr_stack[0].tag, jsr_sp_reg);

  /* If we don't get a hit, skip this code. */
  i386_jnz (COMMON_ARG_NAMES, 5);  /* One byte offset patched below. */
  jnz_end_addr = *codep;

  /* Fetch the code pointer.  Clobber scratch_reg. */
  i386_movl_indoff_reg (COMMON_ARG_NAMES,
			(int32)&cpu_state.jsr_stack[0].code, jsr_sp_reg,
			scratch_reg);

  /* Pop the jsr stack and jump to the target address. */
  i386_addl_imm_reg (COMMON_ARG_NAMES, sizeof (jsr_stack_elt_t), jsr_sp_reg);
  i386_addl_imm_reg (COMMON_ARG_NAMES, NATIVE_START_BYTE_OFFSET, scratch_reg);
  i386_andl_imm_reg (COMMON_ARG_NAMES, (sizeof cpu_state.jsr_stack) - 1,
		     jsr_sp_reg);
  i386_movl_reg_indoff (COMMON_ARG_NAMES, jsr_sp_reg,
			offsetof (CPUState, jsr_stack_byte_index), REG_EBP);
  i386_jmp_reg (COMMON_ARG_NAMES, scratch_reg);

  /* Patch up the jnz to jump to here. */
  ((int8 *)jnz_end_addr)[-1] = *codep - jnz_end_addr;
  
  /* Call hash_lookup_code_and_create_if_needed(). */
  host_swap32 (COMMON_ARG_NAMES, scratch_reg);
  i386_pushl (COMMON_ARG_NAMES, scratch_reg);
  i386_call_abs (COMMON_ARG_NAMES, 0x1234567A); /* hash_lookup_code_and... */

  /* Note a backpatch for the call, since it's a relative displacement. */
  backpatch_add (b, (*codep - start_code - 4) * 8, 32, TRUE,
		 -4              /* Branch is relative to end of call.  */
		 + (int32)hash_lookup_code_and_create_if_needed,
		 NULL);

  /* Jump to the native code entry point for the code returned by
   * hash_lookup_code_and_create_if_needed ().
   */
  i386_addl_imm_reg (COMMON_ARG_NAMES, NATIVE_START_BYTE_OFFSET, REG_EAX);
  i386_addl_imm_reg (COMMON_ARG_NAMES, 4, REG_ESP); /* Pop off call arg. */
  i386_jmp_reg (COMMON_ARG_NAMES, REG_EAX);

  return 0;
}


static void
host_recompile_jsr (Block *b, host_code_t *code_start,
		    int32 target_addr, int32 rts_addr,
		    int32 dynamic_address_reg)
{
  int old_immortal, old_target_immortal, old_rts_immortal;
  int jsr_sp_reg;
  Block *target, *rts;
  cache_info_t empty, *c;
  host_code_t *code, **codep;
  int cc_spill_if_changed, cc_to_compute, scratch_reg;

  /* Set up some bogus COMMON_ARGS for the native code generation
   * we're about to do.
   */
  empty = empty_cache_info;
  c = &empty;
  code = code_start;
  codep = &code;
  cc_spill_if_changed = cc_to_compute = M68K_CC_NONE;
  scratch_reg = NO_REG;

  /* Make the jsr block temporarily immortal so it doesn't accidentally
   * get trashed while compiling the target block.
   */
  old_immortal = b->immortal;
  b->immortal = TRUE;

  /* Actually create the target code and then grab the block. */
  hash_lookup_code_and_create_if_needed (target_addr);
  target = hash_lookup (target_addr);
  assert (target != NULL);
  old_target_immortal = target->immortal;
  target->immortal = TRUE;

  /* Actually create the rts addr code and then grab the block. */
  hash_lookup_code_and_create_if_needed (rts_addr);
  rts = hash_lookup (rts_addr);
  assert (rts != NULL);
  old_rts_immortal = rts->immortal;
  rts->immortal = TRUE;

  /* Tie the blocks together. */
  b->num_children = 2;
  b->child[0] = target;
  b->child[1] = rts;
  block_add_parent (target, b);
  block_add_parent (rts, b);

  /* Replace all of the recompile gunk with some code that adjusts the
   * jsr stack and then jumps to the target address.  We can use any
   * i386 registers we want here as long as we don't clobber
   * dynamic_address_reg.
   */

  jsr_sp_reg = (dynamic_address_reg == REG_EAX) ? REG_EBX : REG_EAX;

  /* Grab the jsr stack pointer. */
  i386_movl_indoff_reg (COMMON_ARG_NAMES,
			offsetof (CPUState, jsr_stack_byte_index), REG_EBP,
			jsr_sp_reg);

  /* Push the return code onto the jsr stack. */
  i386_subl_imm_reg (COMMON_ARG_NAMES, sizeof (jsr_stack_elt_t), jsr_sp_reg);
  i386_andl_imm_reg (COMMON_ARG_NAMES, (sizeof cpu_state.jsr_stack) - 1,
		     jsr_sp_reg);
  i386_movl_reg_indoff (COMMON_ARG_NAMES, jsr_sp_reg,
			offsetof (CPUState, jsr_stack_byte_index), REG_EBP);
  i386_movl_imm_indoff (COMMON_ARG_NAMES, SWAPUL (rts_addr),
			(int32)&cpu_state.jsr_stack[0].tag, jsr_sp_reg);
  i386_movl_imm_indoff (COMMON_ARG_NAMES, (int32)rts->compiled_code,
			(int32)&cpu_state.jsr_stack[0].code, jsr_sp_reg);

  if (dynamic_address_reg != NO_REG)
    {
      cc_to_compute = M68K_CCZ;
      i386_cmpl_imm_reg (COMMON_ARG_NAMES, target_addr, dynamic_address_reg);
      cc_to_compute = M68K_CC_NONE;
      i386_jz (COMMON_ARG_NAMES, 0x1234567D);
      *(int32 *)(code - 4) = (((char *)target->compiled_code
			       + NATIVE_START_BYTE_OFFSET)
			      - (char *)code);
      i386_pushl (COMMON_ARG_NAMES, dynamic_address_reg);

      /* Call hash_lookup_code_and_create_if_needed(). */
      i386_call_abs (COMMON_ARG_NAMES, 0x1234567A);
      *(int32 *)(code - 4) = ((char *)hash_lookup_code_and_create_if_needed
			      - (char *)code);
      /* Jump to the native code entry point for the code returned by
       * hash_lookup_code_and_create_if_needed ().
       */
      i386_addl_imm_reg (COMMON_ARG_NAMES, NATIVE_START_BYTE_OFFSET, REG_EAX);
      i386_addl_imm_reg (COMMON_ARG_NAMES, 4, REG_ESP); /* Pop off call arg. */
      i386_jmp_reg (COMMON_ARG_NAMES, REG_EAX);
    }
  else
    {
      /* jmp to the target address. */
      i386_jmp (COMMON_ARG_NAMES, 0x1234567D);
      *(int32 *)(code - 4) = (((char *)target->compiled_code
			       + NATIVE_START_BYTE_OFFSET)
			      - (char *)code);
    }

  b->immortal      = old_immortal;
  rts->immortal    = old_rts_immortal;
  target->immortal = old_target_immortal;
}


static int
host_jsr_abs_common (COMMON_ARGS, int32 a7_reg, int32 target_addr,
		     Block *b, int32 return_addr, int jsr_d16_base_reg)
{
  host_code_t *orig_code, *replaced_code, *end_call;
  int temp_reg;

  orig_code = *codep;

  /* Spill all dirty cc bits. */
  host_spill_cc_bits (c, codep, M68K_CC_ALL);
  cc_spill_if_changed = M68K_CC_NONE;

  /* Push the m68k return address on the stack. */
  host_movel_imm_predec (COMMON_ARG_NAMES, return_addr, a7_reg);

  if (jsr_d16_base_reg != NO_REG)
    {
      if (host_leal_indoff_areg (COMMON_ARG_NAMES, target_addr,
				 jsr_d16_base_reg, scratch_reg))
	return 1;
    }

  /* Spill all registers back to their memory homes. */
  host_spill_regs (c, codep, M68K_CC_NONE);

  /* NOTE!!!!  All code from this point on is about to smash itself
   * with a faster jsr that knows where its target block is.  We do it
   * this way so we can lazily compile through jsrs.
   */

  replaced_code = *codep;

  /* Save the scratch register. */
  if (scratch_reg != NO_REG)
    i386_pushl (COMMON_ARG_NAMES, scratch_reg);

  /* Push call args on the stack.  If you add more args or delete any,
   * be sure to adjust ESP differently, below, to pop off the args.
   */
  i386_pushl_imm (COMMON_ARG_NAMES, scratch_reg);
  i386_pushl_imm (COMMON_ARG_NAMES, return_addr);
  if (jsr_d16_base_reg != NO_REG)
    i386_pushl (COMMON_ARG_NAMES, scratch_reg);
  else
    i386_pushl_imm (COMMON_ARG_NAMES, target_addr);
  i386_call_abs (COMMON_ARG_NAMES, 0);  /* Push current PC on the stack. */
  end_call = *codep;
  temp_reg = (scratch_reg == REG_EAX) ? REG_EBX : REG_EAX;
  i386_popl (COMMON_ARG_NAMES, temp_reg);
  i386_subl_imm_reg (COMMON_ARG_NAMES, end_call - replaced_code, temp_reg);
  i386_pushl (COMMON_ARG_NAMES, temp_reg);
  i386_pushl_imm (COMMON_ARG_NAMES, (int32)b);

  /* Add some more dead space so there will be room for this code
   * to be overwritten.
   */
  memset (*codep, 0x90  /* NOP */, 48);
  *codep += 48;

  /* Call a routine that will rewrite this jsr into a direct one. */
  i386_call_abs (COMMON_ARG_NAMES, 0x1234567C);
  
  /* Note a backpatch for the call, since it's a relative displacement. */
  backpatch_add (b, (*codep - orig_code - 4) * 8, 32, TRUE,
		 -4              /* Branch is relative to end of call.  */
		 + (int32)host_recompile_jsr,
		 NULL);
  
  /* Pop off all call args. */
  i386_addl_imm_reg (COMMON_ARG_NAMES, 5 * 4, REG_ESP);

  /* Restore the scratch reg. */
  if (scratch_reg != NO_REG)
    i386_popl (COMMON_ARG_NAMES, scratch_reg);

  /* Jump back to the beginning and do the fast jsr this time
   * (and every subsequent time).
   */
  i386_jmp (COMMON_ARG_NAMES, 5);
  ((int8 *)*codep)[-1] = replaced_code - *codep;

  return 0;
}


int
host_jsr_abs (COMMON_ARGS, int32 a7_reg, int32 target_addr,
	      Block *b, uint16 *m68k_code)
{
  int32 return_addr;

  /* Compute the real return address. */
  switch (SWAPUW (*m68k_code) & 63)
    {
    case 0x38:  /* absw */
      return_addr = (syn68k_addr_t) (m68k_code + 2);
      break;
    case 0x39:  /* absl */
      return_addr = (syn68k_addr_t) (m68k_code + 3);
      break;
    default:
      return_addr = 0;  /* Avoid compiler warnings. */
      abort ();
      break;
    }

  return host_jsr_abs_common (COMMON_ARG_NAMES, a7_reg, target_addr, b,
			      US_TO_SYN68K (return_addr), NO_REG);
}


int
host_jsr_pcd16 (COMMON_ARGS, int32 a7_reg, Block *b, uint16 *m68k_code)
{
  /* Map this to a jsr_abs. */
  return host_jsr_abs_common (COMMON_ARG_NAMES, a7_reg,
			      (int32) (US_TO_SYN68K (m68k_code) + 2
				       + SWAPSW (m68k_code[1])),
			      b, US_TO_SYN68K (m68k_code + 2), NO_REG);
}


int
host_jsr_d16 (COMMON_ARGS, int32 a7_reg, int32 base_addr_reg,
	      Block *b, uint16 *m68k_code)
{
  return host_jsr_abs_common (COMMON_ARG_NAMES, a7_reg, SWAPSW (m68k_code[1]),
			      b, US_TO_SYN68K (m68k_code + 2), base_addr_reg);
}


int
host_bsr (COMMON_ARGS, int32 a7_reg, Block *b, uint16 *m68k_code)
{
  int32 addr, return_addr;

  /* Compute the bsr destination address. */
  switch (((const uint8 *)m68k_code)[1])
    {
    case 0x00:
      addr = (int32)(m68k_code + 1) + SWAPSW (m68k_code[1]);
      return_addr = (int32) (m68k_code + 2);
      break;
    case 0xFF:
      addr = (int32)(m68k_code + 1) + READSL (US_TO_SYN68K (m68k_code + 1));
      return_addr = (int32) (m68k_code + 3);
      break;
    default:
      addr = (int32)(m68k_code + 1) + ((const int8 *)m68k_code)[1];
      return_addr = (int32) (m68k_code + 1);
      break;
    }

  return host_jsr_abs_common (COMMON_ARG_NAMES, a7_reg,
			      US_TO_SYN68K (addr),
			      b, US_TO_SYN68K (return_addr), NO_REG);
}


/* Divides %eax by scratch_reg, and stores the quotient in
 * %ax and the remainder in the high 16 bits of %eax, unless there
 * is overflow in which case no action is taken.  Division by zero
 * causes a trap.
 */
int
host_divsw (COMMON_ARGS, uint16 *m68k_addr, uint16 *next_addr, Block *b,
	    int32 might_overflow_i386_p)
{
  host_code_t *overflow_branch_end, *jmp_end, *br_end_1, *br_end_2;
  int real_cc_to_compute;
  host_code_t *start_code;

  /* Record the start so we can provide accurate backpatch information. */
  start_code = *codep;

  real_cc_to_compute = cc_to_compute;
  cc_to_compute = M68K_CC_NONE;
  
  assert (scratch_reg != -1 && scratch_reg != REG_EDX);

  /* Pick up %edx as another scratch register. */
  if (host_alloc_reg (c, codep, cc_spill_if_changed, 1L << REG_EDX)
      != REG_EDX)
    return 3;

  /* Check for division by zero. */
  if (m68k_addr != NULL)
    {
      host_code_t *divzero_branch_end;
      cache_info_t save;

      save = *c;

      i386_testl_reg_reg (COMMON_ARG_NAMES, scratch_reg, scratch_reg);
      i386_jnz (COMMON_ARG_NAMES, 5);
      divzero_branch_end = *codep;

      /* Ack!  Division by zero! */
      host_spill_cc_bits (c, codep,
			  M68K_CCN | M68K_CCV | M68K_CCX | M68K_CCZ);
      i386_movb_imm_indoff (COMMON_ARG_NAMES, 0, offsetof (CPUState, ccc),
			    REG_EBP);
      host_spill_regs (c, codep, M68K_CC_NONE);
      
      i386_pushl_imm (COMMON_ARG_NAMES, US_TO_SYN68K (m68k_addr));
      i386_pushl_imm (COMMON_ARG_NAMES, US_TO_SYN68K (next_addr));
      i386_pushl_imm (COMMON_ARG_NAMES, 5);  /* Division by zero exception. */

      /* Trap. */
      i386_call_abs (COMMON_ARG_NAMES, 0x1234567A); /* trap_direct. */
      backpatch_add (b, (*codep - start_code - 4) * 8, 32, TRUE,
		     -4              /* Branch is relative to end of call.  */
		     + (int32)trap_direct,
		     NULL);

      /* Jump to the specified address. */
      i386_pushl (COMMON_ARG_NAMES, REG_EAX);
      i386_call_abs (COMMON_ARG_NAMES, 0x1234567A); /* hash_lookup_code... */
      backpatch_add (b, (*codep - start_code - 4) * 8, 32, TRUE,
		     -4              /* Branch is relative to end of call.  */
		     + (int32)hash_lookup_code_and_create_if_needed,
		     NULL);

      /* Jump to the native code entry point for the code returned by
       * hash_lookup_code_and_create_if_needed ().
       */
      i386_addl_imm_reg (COMMON_ARG_NAMES, NATIVE_START_BYTE_OFFSET, REG_EAX);
      i386_addl_imm_reg (COMMON_ARG_NAMES, 4 * 4, REG_ESP); /* Pop off args. */
      i386_jmp_reg (COMMON_ARG_NAMES, REG_EAX);

      /* Here's where we go if there was no problem. */
      divzero_branch_end[-1] = *codep - divzero_branch_end;

      *c = save;
    }

  /* Save the dividend in case we overflow. */
  i386_pushl (COMMON_ARG_NAMES, REG_EAX);
  
  /* The i386 cannot handle 0x80000000 / -1 without crashing, so test for
   * that case and signal overflow.
   */
  if (might_overflow_i386_p)
    {
      i386_cmpl_imm_reg (COMMON_ARG_NAMES, -1, scratch_reg);
      i386_jnz (COMMON_ARG_NAMES, 5);
      br_end_1 = *codep;
      i386_cmpl_imm_reg (COMMON_ARG_NAMES, 0x80000000, REG_EAX);
      i386_jz (COMMON_ARG_NAMES, 5);
      br_end_2 = *codep;

      br_end_1[-1] = *codep - br_end_1;
    }
  else
    br_end_2 = NULL;

  /* Do the divide. */
  i386_cltd (COMMON_ARG_NAMES);
  i386_idivl (COMMON_ARG_NAMES, scratch_reg);  /* Implicitly uses %eax */

  if (br_end_2 != NULL)
    br_end_2[-1] = *codep - br_end_2;

  /* The result of the divide is now in %eax:%edx.  First we have to
   * check for overflow, since the m68k leaves the operand unaffected
   * if there is overflow.
   */
  i386_addl_imm_reg (COMMON_ARG_NAMES, 32768, REG_EAX);
  i386_testl_imm_reg (COMMON_ARG_NAMES, 0xFFFF0000, REG_EAX);
  i386_jz (COMMON_ARG_NAMES, 5);  /* Filled in below to skip overflow code. */
  overflow_branch_end = *codep;
  
  /* Handle the overflow. */
  if (real_cc_to_compute & M68K_CC_CNVZ)
    {
      cc_to_compute = real_cc_to_compute;

      /* N and Z are technically undefined here, but they seem to actually
       * be set based on the result on the 68040.
       */
      if (real_cc_to_compute & (M68K_CCC | M68K_CCN | M68K_CCZ))
	{
	  i386_testl_reg_reg (COMMON_ARG_NAMES, REG_EAX, REG_EAX);
	  i386_lahf (COMMON_ARG_NAMES);
	}

      /* Set the V flag, if so requested. */
      if (real_cc_to_compute & M68K_CCV)
	{
	  i386_movl_imm_reg (COMMON_ARG_NAMES, 0x80000000, REG_EDX);
	  i386_addl_reg_reg (COMMON_ARG_NAMES, REG_EDX, REG_EDX);
	}

      if (real_cc_to_compute & (M68K_CCC | M68K_CCN | M68K_CCZ))
	i386_sahf (COMMON_ARG_NAMES);

      cc_to_compute = M68K_CC_NONE;
    }

  /* Restore the original dividend. */
  i386_popl (COMMON_ARG_NAMES, REG_EAX);
  i386_jmp (COMMON_ARG_NAMES, 5);
  jmp_end = *codep;

  /* Backpatch the conditional branch. */
  overflow_branch_end[-1] = *codep - overflow_branch_end;

  /* Now we place the low-order 16 bits of %edx into the high-order 16
   * bits of %eax.
   */
  i386_subl_imm_reg (COMMON_ARG_NAMES, 32768, REG_EAX);
  i386_rorl_imm_reg (COMMON_ARG_NAMES, 16, REG_EAX);
  i386_movw_reg_reg (COMMON_ARG_NAMES, REG_DX, REG_AX);
  i386_rorl_imm_reg (COMMON_ARG_NAMES, 16, REG_EAX);

  /* Pop off the saved overflow value (we didn't need it). */
  i386_addl_imm_reg (COMMON_ARG_NAMES, 4, REG_ESP);

  /* Set the CC bits appropriately, if so desired. */
  if (real_cc_to_compute)
    {
      cc_to_compute = real_cc_to_compute;
      i386_testw_reg_reg (COMMON_ARG_NAMES, REG_AX, REG_AX);
    }

  /* Backpatch the jump. */
  jmp_end[-1] = *codep - jmp_end;

  c->cached_cc |= real_cc_to_compute;
  c->dirty_cc  |= real_cc_to_compute;

  return 0;
}


int
host_divsw_imm_reg (COMMON_ARGS, int32 val)
{
  if (val == 0)
    return 1;
  i386_movl_imm_reg (COMMON_ARG_NAMES, val, scratch_reg);
  return host_divsw (COMMON_ARG_NAMES, NULL, NULL, NULL, val == -1);
}


int
host_negb_abs (COMMON_ARGS, int32 dst_addr)
{
  return i386_negb_abs(COMMON_ARG_NAMES, (int32) SYN68K_TO_US (dst_addr));
}


#define HOST_OP_IMM_ABS(size, op, x86_op)				  \
int									  \
host_ ## op ## size ## _imm_abs (COMMON_ARGS, int32 src, int32 dst_addr)  \
{									  \
  return i386_ ## x86_op ## size ## _imm_abs (COMMON_ARG_NAMES, src,	  \
					      ((int32)			  \
					       SYN68K_TO_US (dst_addr))); \
}

HOST_OP_IMM_ABS(b, add, add)
HOST_OP_IMM_ABS(b, sub, sub)


#define HOST_OP_REG_ABS(size, op, x86_op)				  \
int									  \
host_ ## op ## size ## _reg_abs (COMMON_ARGS, int32 src, int32 dst_addr)  \
{									  \
  return i386_ ## x86_op ## size ## _reg_abs (COMMON_ARG_NAMES, src,	  \
					      ((int32)			  \
					       SYN68K_TO_US (dst_addr))); \
}


HOST_OP_REG_ABS(b, and, and)
HOST_OP_REG_ABS(w, and, and)
HOST_OP_REG_ABS(l, and, and)

HOST_OP_REG_ABS(b, or, or)
HOST_OP_REG_ABS(w, or, or)
HOST_OP_REG_ABS(l, or, or)

HOST_OP_REG_ABS(b, xor, xor)
HOST_OP_REG_ABS(w, xor, xor)
HOST_OP_REG_ABS(l, xor, xor)

HOST_OP_REG_ABS(b, cmp, cmp)
HOST_OP_REG_ABS(w, cmp, cmp)
HOST_OP_REG_ABS(l, cmp, cmp)

HOST_OP_REG_ABS(b, add, add)
HOST_OP_REG_ABS(b, sub, sub)

HOST_OP_REG_ABS(b, move, mov)
HOST_OP_REG_ABS(w, move, mov)
HOST_OP_REG_ABS(l, move, mov)


#define HOST_OP_ABS_REG(size, op, x86_op)				 \
int									 \
host_ ## op ## size ## _abs_reg (COMMON_ARGS, int32 src_addr, int32 dst) \
{									 \
  return i386_ ## x86_op ## size ## _abs_reg (COMMON_ARG_NAMES,		 \
					      ((int32)			 \
					       SYN68K_TO_US (src_addr)), \
					      dst);			 \
}

HOST_OP_ABS_REG(b, and, and)
HOST_OP_ABS_REG(w, and, and)
HOST_OP_ABS_REG(l, and, and)

HOST_OP_ABS_REG(b, or, or)
HOST_OP_ABS_REG(w, or, or)
HOST_OP_ABS_REG(l, or, or)

HOST_OP_ABS_REG(b, xor, xor)
HOST_OP_ABS_REG(w, xor, xor)
HOST_OP_ABS_REG(l, xor, xor)

HOST_OP_ABS_REG(b, cmp, cmp)
HOST_OP_ABS_REG(w, cmp, cmp)
HOST_OP_ABS_REG(l, cmp, cmp)

HOST_OP_ABS_REG(b, add, add)
HOST_OP_ABS_REG(b, sub, sub)

HOST_OP_ABS_REG(b, move, mov)
HOST_OP_ABS_REG(w, move, mov)
HOST_OP_ABS_REG(l, move, mov)

#define HOST_OPB_IMM_ABS(op, x86_op)                                     \
int									 \
host_ ## op ## b_imm_abs (COMMON_ARGS, int32 val, int32 addr)            \
{									 \
  return i386_ ## x86_op ## b_imm_abs (COMMON_ARG_NAMES,		 \
                                              val,                       \
					      ((int32)			 \
					       SYN68K_TO_US (addr)));    \
}

HOST_OPB_IMM_ABS(and, and)
HOST_OPB_IMM_ABS(or , or )
HOST_OPB_IMM_ABS(xor, xor)
HOST_OPB_IMM_ABS(cmp, cmp)

#endif  /* GENERATE_NATIVE_CODE */
