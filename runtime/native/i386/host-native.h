#ifndef _HOST_NATIVE_H_
#define _HOST_NATIVE_H_

#define REG_EAX 0
#define REG_ECX 1
#define REG_EDX 2
#define REG_EBX 3
#define REG_ESP 4
#define REG_EBP 5
#define REG_ESI 6
#define REG_EDI 7

#define REG_AX 0
#define REG_CX 1
#define REG_DX 2
#define REG_BX 3
#define REG_SP 4
#define REG_BP 5
#define REG_SI 6
#define REG_DI 7

#define REG_AL 0
#define REG_CL 1
#define REG_DL 2
#define REG_BL 3
#define REG_AH 4
#define REG_CH 5
#define REG_DH 6
#define REG_BH 7

/* Don't allow %ebp or %esp.  These registers are strange because in
 * some situations they are used as "escape" registers on the i386.
 * We could work around this by only allowing them when they are legal,
 * but legend has it that we can't use either under DPMI because of
 * a strange stack segment value.
 */
#define ALLOCATABLE_REG_MASK ((host_reg_mask_t) (  (1 << REG_EAX)	\
						 | (1 << REG_EBX)	\
						 | (1 << REG_ECX)	\
						 | (1 << REG_EDX)	\
						 | (1 << REG_ESI)	\
						 | (1 << REG_EDI)))


/* Any of the 6 regs we free up to use. */
#define REGSET_ALL    ALLOCATABLE_REG_MASK

/* Only those regs which are byte-addressable: %eax, %ebx, %ecx, %edx. */
#define REGSET_BYTE  ((host_reg_mask_t) (  (1 << REG_EAX)	\
					 | (1 << REG_EBX)	\
					 | (1 << REG_ECX)	\
					 | (1 << REG_EDX)))
#define REGSET_EMPTY 0

#define NUM_HOST_REGS 8

typedef uint8 host_code_t;

extern uint8 have_i486_p;

#define NATIVE_TO_SYNTH_STUB_BYTES 10
#define NATIVE_TO_SYNTH_STUB_WORDS \
(((NATIVE_TO_SYNTH_STUB_BYTES + 1) / 2 + 1) & ~1)/* Must be even # of words. */

#ifdef SYNCHRONOUS_INTERRUPTS
# ifdef USE_BIOS_TIMER
#  define CHECK_INTERRUPT_STUB_BYTES 18
# else   /* !USE_BIOS_TIMER */
#  if defined (__CHECKER__)
#   define CHECK_INTERRUPT_STUB_BYTES 18
#  else /* !__CHECKER__ */
#   define CHECK_INTERRUPT_STUB_BYTES 14
#  endif /* !__CHECKER__ */
# endif  /* !USE_BIOS_TIMER */
#else
# define CHECK_INTERRUPT_STUB_BYTES 0
#endif  /* !SYNCHRONOUS_INTERRUPTS */

#define HOST_SWAP16(r) \
i386_rorw_imm_reg (c, codep, cc_spill_if_changed, M68K_CC_NONE, NO_REG, 8, (r))
#define HOST_SWAP32(r) \
host_swap32 (c, codep, cc_spill_if_changed, M68K_CC_NONE, NO_REG, (r))

#endif /* !_HOST_NATIVE_H_ */
