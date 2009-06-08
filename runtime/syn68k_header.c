/* Global register to hold the PC.  We put this up here so that
 * the global register will be defined before any of the inline functions
 * in syn68k_public.h are hit, but after uint16 is typedef'd.
 */
#ifdef i386
# ifdef __CHECKER__
#  define GLOBAL_REGISTER_DECLS \
         register const uint16 *code asm ("%si");
# else /* !__CHECKER__ */
#  define GLOBAL_REGISTER_DECLS \
         register const uint16 *code asm ("%si"); \
         register CPUState *cpu_state_ptr asm ("%bp");
# endif /* !__CHECKER__ */
#elif defined (mc68000)
# define GLOBAL_REGISTER_DECLS register const uint16 *code asm ("a4");
#elif defined (__alpha__)
# define GLOBAL_REGISTER_DECLS register const uint16 *code asm ("$9");
#elif defined (powerpc) || defined (__ppc__)
# define GLOBAL_REGISTER_DECLS register const uint16 *code asm ("%r13");
#elif defined(__x86_64)
# define GLOBAL_REGISTER_DECLS register const uint16 *code asm ("%rsi");
#else
# error "Choose a global register to hold the current synthetic PC.  Make sure it is saved by the normal calling convention."
#endif


/* #define this value so headers can detect that syn68k.c is including them. */
#define SYN68K_C


#include "syn68k_private.h"
#include "interrupt.h"
#include "hash.h"
#include "trap.h"
#include "profile.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "rangetree.h"
#include "native.h"
#include "translate.h"
#include "recompile.h"
#include <stdlib.h>

#if defined (i386) && !defined (__CHECKER__)
#define cpu_state (*cpu_state_ptr)  /* To provide more concise code. */
#endif
#include "ccfuncs.h"
#undef cpu_state

#ifdef DEBUG
# define IFDEBUG(x) x
#else
# define IFDEBUG(x)
#endif



#ifdef USE_BIOS_TIMER
# define RESTORE_FS() asm volatile ("movw %0,%%fs" \
				 : : "g" (dos_memory_selector))
#else
# define RESTORE_FS()
#endif


/* Do an efficient inline code lookup.  Whenever we get a hit on a hash table
 * entry, we move it to the head of the linked list.  We can just check the
 * head here; if that fails, we can do the slower check and possible compile.
 */

static __inline__ const uint16 *code_lookup (uint32 addr) __attribute__((always_inline));

static __inline__ const uint16 *
code_lookup (uint32 addr)
{
  Block *b = block_hash_table[BLOCK_HASH (addr)];
  const uint16 *c;
  if (b != NULL && b->m68k_start_address == addr)
    c = b->compiled_code;
  else
    {
      c = hash_lookup_code_and_create_if_needed (addr);
      RESTORE_FS ();
    }
  return c;
}

/* #define FREQUENCY(n) do { if (profile_p) ++frequency[n].freq; } while (0) */

#ifdef FREQUENCY
#warning "Frequency counting code in place; expect a performance hit."
int profile_p = 0;

struct _freq
{
  unsigned long freq;
  int opcode;
} frequency[65536];

static int
compare_freq (const void *p1, const void *p2)
{
  int diff = (((const struct _freq *)p2)->freq
	      - ((const struct _freq *)p1)->freq);
  if (diff != 0)
    return diff;

  /* Break ties with the opcode. */
  return (((const struct _freq *)p1)->opcode
	  - ((const struct _freq *)p2)->opcode);
}


static int
compare_opcode (const void *p1, const void *p2)
{
  return (((const struct _freq *)p1)->opcode
	  - ((const struct _freq *)p2)->opcode);
}


void
dump_frequency ()
{
  int i;
  double total_freq;

  total_freq = 0.0;
  for (i = 0; i < 65536; i++)
    {
      frequency[i].opcode = i;
      total_freq += frequency[i].freq;
    }
  qsort (frequency, 65536, sizeof frequency[0], compare_freq);
  for (i = 0; i < 65536 && frequency[i].freq != 0; i++)
    printf ("0x%04X\t%lu\t%.2f%%\n", (unsigned)frequency[i].opcode,
	    frequency[i].freq, frequency[i].freq * 100.0 / total_freq);
  qsort (frequency, 65536, sizeof frequency[0], compare_opcode);
}


void
reset_frequency ()
{
  memset (frequency, 0, sizeof frequency);
}


#else
#define FREQUENCY(n)
#endif


#ifdef SYNCHRONOUS_INTERRUPTS
# define CHECK_FOR_INTERRUPT(pc)			\
do							\
{							\
  if (INTERRUPT_PENDING ())				\
    {							\
      syn68k_addr_t __pc;				\
      syn68k_addr_t new_addr;				\
							\
      __pc = (pc);					\
      new_addr = interrupt_process_any_pending (__pc);	\
      if (new_addr != (__pc))				\
	{						\
	  code = code_lookup (new_addr);		\
	  NEXT_INSTRUCTION (PTR_WORDS);			\
	}						\
    }							\
} while (0)
#else  /* !SYNCHRONOUS_INTERRUPTS */
# define CHECK_FOR_INTERRUPT(pc)
#endif /* !SYNCHRONOUS_INTERRUPTS */


#ifdef USE_DIRECT_DISPATCH

#define	INSTR_DEBUG_HOLD_SIZE 0  /* DO NOT CHECK IN WITH NON-ZERO VALUE */

#if	INSTR_DEBUG_HOLD_SIZE > 0

#warning "Instructing tracing code in place; expect a performance hit."

void *instr_debug_addr[INSTR_DEBUG_HOLD_SIZE];
void *instr_debug_hold[INSTR_DEBUG_HOLD_SIZE];
int instr_debug_index;

static void next_instruction_hook(const void *vp)
{
    ++instr_debug_index;
    instr_debug_addr[instr_debug_index % INSTR_DEBUG_HOLD_SIZE] = vp;
    instr_debug_hold[instr_debug_index % INSTR_DEBUG_HOLD_SIZE] =
							        * (void **) vp;
}

#define NEXT_INSTRUCTION_HOOK(n) next_instruction_hook(code + (n) - PTR_WORDS)

#else

#define NEXT_INSTRUCTION_HOOK(n)

#endif

# define CASE(n) \
void \
s68k_handle_opcode_ ## n () \
{ \
  asm volatile ("\n_S68K_HANDLE_" #n ":"); \
  FREQUENCY (n);
# define CASE_PREAMBLE(name,bits,ms,mns,n) {
extern void s68k_handle_opcode_dummy (void);
# ifdef i386
#  define NEXT_INSTRUCTION(words_to_inc) \
{ \
  register void *next_code asm ("%edi");   /* Little-used register. */ \
  NEXT_INSTRUCTION_HOOK(words_to_inc); \
  asm volatile ("movl %3,%0\n\t" \
		"addl %4,%1\n\t" \
		"jmp *%0\n" \
		"/* _S68K_DONE_WITH_THIS: */"                 \
		: "=r" (next_code), "=r" (code) : "1" (code), \
		"g" (*(void **)(code + (words_to_inc) - PTR_WORDS)), \
		"g" (words_to_inc * sizeof (uint16))); \
  s68k_handle_opcode_dummy (); \
}

# else   /* !i386 */

#  define NEXT_INSTRUCTION(words_to_inc)	\
{				 \
  void *next_code;	         \
  NEXT_INSTRUCTION_HOOK(words_to_inc);	 \
  next_code = *(void **)(code + (words_to_inc) - PTR_WORDS); \
  INCREMENT_CODE (words_to_inc); \
  goto *next_code; \
}
# endif  /* !i386 */
# define CASE_POSTAMBLE(words_to_inc) } NEXT_INSTRUCTION (words_to_inc); }
#else
# define CASE(n) case n:
# define NEXT_INSTRUCTION(ignored) break
# define CASE_PREAMBLE(name,bits,ms,mns,n) {
# define CASE_POSTAMBLE(words_to_inc) } NEXT_INSTRUCTION (words_to_inc);
#endif


#if defined(__GNUC__) && !defined(__alpha) && __GNUC__ < 3
/* Work around poor gcc code generated when adding to a global reg var. */

/* NOTE: if we want to use this trick on the alpha, we'll need to cast
         v up to 64 bits, first, but this trick may not be needed on
	 the alpha, so we don't hassle with changes yet */

/* NOTE: I hope we don't need this trick with GCC 3 or greater.  I certainly
         haven't timed the change though.  I just put the test for __GNUC__
         in above to get rid of some warnings */

# define INCREMENT_CODE(n) (++((typeof (*code) (*)[n])code))

/* It seems that gcc is generating poor code for ++'s to memory as well. */
# define INC_VAR(v, n) ((typeof (v)) ++((char (*)[n])(v)))
# define DEC_VAR(v, n) ((typeof (v)) --((char (*)[n])(v)))
#else
# define INCREMENT_CODE(n) (code += (n))
# define INC_VAR(v, n) ((v) += (n))
# define DEC_VAR(v, n) ((v) -= (n))
#endif


/* This macro rounds a size up to some integral multiple of PTR_WORDS. */
#define ROUND_UP(n) ((((n) + (PTR_WORDS - 1)) / PTR_WORDS) * PTR_WORDS)


#if	0 && !defined(GO32)
extern void abort (void);
#endif


#ifdef M68K_REGS_IN_ARRAY
# define LOAD_CPU_STATE()
#define SAVE_CPU_STATE()
#else  /* !M68K_REGS_IN_ARRAY */
# define LOAD_CPU_STATE() \
  d0 = cpu_state.regs[0],  d1 = cpu_state.regs[1],  \
  d2 = cpu_state.regs[2],  d3 = cpu_state.regs[3],  \
  d4 = cpu_state.regs[4],  d5 = cpu_state.regs[5],  \
  d6 = cpu_state.regs[6],  d7 = cpu_state.regs[7],  \
  a0 = cpu_state.regs[8],  a1 = cpu_state.regs[9],  \
  a2 = cpu_state.regs[10], a3 = cpu_state.regs[11], \
  a4 = cpu_state.regs[12], a5 = cpu_state.regs[13], \
  a6 = cpu_state.regs[14], a7 = cpu_state.regs[15]
#define SAVE_CPU_STATE() \
  cpu_state.regs[0]  = d0, cpu_state.regs[1]  = d1, \
  cpu_state.regs[2]  = d2, cpu_state.regs[3]  = d3, \
  cpu_state.regs[4]  = d4, cpu_state.regs[5]  = d5, \
  cpu_state.regs[6]  = d6, cpu_state.regs[7]  = d7, \
  cpu_state.regs[8]  = a0, cpu_state.regs[9]  = a1, \
  cpu_state.regs[10] = a2, cpu_state.regs[11] = a3, \
  cpu_state.regs[12] = a4, cpu_state.regs[13] = a5, \
  cpu_state.regs[14] = a6, cpu_state.regs[15] = a7
#endif  /* !M68K_REGS_IN_ARRAY */

#ifdef M68K_REGS_IN_ARRAY
#define GENERAL_REGISTER(n,TYPE) (cpu_state.regs[n] TYPE)
#define DATA_REGISTER(n,TYPE) (cpu_state.regs[n] TYPE)
#define ADDRESS_REGISTER(n,TYPE) (cpu_state.regs[8 + (n)] TYPE)

/* We use these macros to compensate for gcc brain damage when referencing
 * arrays of structs on the i386.
 */
#ifndef offsetof
# define offsetof(s, t) ((int) &((s *) 0)->t)
#endif

#define GENERAL_REGISTER_SB(reg) \
(*((int8  *)((int32 *)&cpu_state.regs[0] + (reg)) + offsetof (M68kReg, ub.n)))
#define GENERAL_REGISTER_UB(reg) \
(*((uint8 *)((int32 *)&cpu_state.regs[0] + (reg)) + offsetof (M68kReg, sb.n)))
#define GENERAL_REGISTER_SW(reg) \
(*(int16  *)((int8  *)((int32 *)&cpu_state.regs[0] + (reg)) \
	     + offsetof (M68kReg, sw.n)))
#define GENERAL_REGISTER_UW(reg) \
(*(uint16 *)((int8  *)((int32 *)&cpu_state.regs[0] + (reg)) \
	     + offsetof (M68kReg, uw.n)))
#define GENERAL_REGISTER_SL(reg) (*((int32  *)&cpu_state.regs[0] + (reg)))
#define GENERAL_REGISTER_UL(reg) (*((uint32 *)&cpu_state.regs[0] + (reg)))

#define DATA_REGISTER_UB(n) GENERAL_REGISTER_UB (n)
#define DATA_REGISTER_SB(n) GENERAL_REGISTER_SB (n)
#define DATA_REGISTER_UW(n) GENERAL_REGISTER_UW (n)
#define DATA_REGISTER_SW(n) GENERAL_REGISTER_SW (n)
#define DATA_REGISTER_UL(n) GENERAL_REGISTER_UL (n)
#define DATA_REGISTER_SL(n) GENERAL_REGISTER_SL (n)

#define ADDRESS_REGISTER_UB(n) GENERAL_REGISTER_UB ((n) + 8)
#define ADDRESS_REGISTER_SB(n) GENERAL_REGISTER_SB ((n) + 8)
#define ADDRESS_REGISTER_UW(n) GENERAL_REGISTER_UW ((n) + 8)
#define ADDRESS_REGISTER_SW(n) GENERAL_REGISTER_SW ((n) + 8)
#define ADDRESS_REGISTER_UL(n) GENERAL_REGISTER_UL ((n) + 8)
#define ADDRESS_REGISTER_SL(n) GENERAL_REGISTER_SL ((n) + 8)

#else
# define GENERAL_REGISTER(n,TYPE) \
  ((n) >= 8 ? ADDRESS_REGISTER ((n) - 8, TYPE) : DATA_REGISTER (n, TYPE))
# ifdef __GNUC__
#  define DATA_REGISTER(n,TYPE)                   \
   ({ int _tmp = (n);                             \
        (((_tmp) > 3)                             \
	 ? (((_tmp) > 5)                          \
	    ? (((_tmp) > 6) ? d7 TYPE : d6 TYPE)  \
	    : (((_tmp) < 5) ? d4 TYPE : d5 TYPE)) \
	 : (((_tmp) > 1)                          \
	    ? (((_tmp) > 2) ? d3 TYPE : d2 TYPE)  \
	    : (((_tmp) < 1) ? d0 TYPE : d1 TYPE))); })
#  define ADDRESS_REGISTER(n,TYPE)                \
   ({ int _tmp = (n);                             \
        (((_tmp) > 3)                             \
	 ? (((_tmp) > 5)                          \
	    ? (((_tmp) > 6) ? a7 TYPE : a6 TYPE)  \
	    : (((_tmp) < 5) ? a4 TYPE : a5 TYPE)) \
	 : (((_tmp) > 1)                          \
	    ? (((_tmp) > 2) ? a3 TYPE : a2 TYPE)  \
	    : (((_tmp) < 1) ? a0 TYPE : a1 TYPE))); })

# else  /* Not M68K_REGS_IN_ARRAY and Not __GNUC__ */

#  define DATA_REGISTER(n,TYPE)           \
   (((n) > 3)                             \
    ? (((n) > 5)                          \
       ? (((n) > 6) ? d7 TYPE : d6 TYPE)  \
       : (((n) < 5) ? d4 TYPE : d5 TYPE)) \
    : (((n) > 1)                          \
       ? (((n) > 2) ? d3 TYPE : d2 TYPE)  \
       : (((n) < 1) ? d0 TYPE : d1 TYPE)))

#  define ADDRESS_REGISTER(n,TYPE)        \
   (((n) > 3)                             \
    ? (((n) > 5)                          \
       ? (((n) > 6) ? a7 TYPE : a6 TYPE)  \
       : (((n) < 5) ? a4 TYPE : a5 TYPE)) \
    : (((n) > 1)                          \
       ? (((n) > 2) ? a3 TYPE : a2 TYPE)  \
      : (((n) < 1) ? a0 TYPE : a1 TYPE)))

# endif  /* Not __GNUC__ */
#endif  /* Not M68K_REGS_IN_ARRAY */

#ifdef M68K_REGS_IN_ARRAY
typedef struct {
  uint32 reg;   /* Could make these uint8/int8, but that requires */
  int32 delta;  /* movzbl/movsbl, which are slow and non-pairable. */
} AmodeCleanupInfo;
#endif


static const uint8 neg_bcd_table[16] = {
  0x9A, -6, -6, -6, -6, -6, -6, -6, -6, -6, 0xFA, -6, -6, -6, -6, -6
};
#define NEGBCD_TABLE(n) neg_bcd_table[n]


#ifdef M68K_REGS_IN_ARRAY
  static const AmodeCleanupInfo amode_cleanup_info[3][64] = {
    { {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},
      {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},
      {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},
      {8, 1}, {9, 1}, {10, 1},{11, 1},{12, 1},{13, 1},{14, 1},{15, 2},
      {8,-1}, {9,-1}, {10,-1},{11,-1},{12,-1},{13,-1},{14,-1},{15,-2},
      {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},
      {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},
      {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0} },
    { {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},
      {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},
      {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},
      {8, 2}, {9, 2}, {10, 2},{11, 2},{12, 2},{13, 2},{14, 2},{15,2},
      {8,-2}, {9,-2}, {10,-2},{11,-2},{12,-2},{13,-2},{14,-2},{15,-2},
      {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},
      {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},
      {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0} },
    { {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},
      {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},
      {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},
      {8, 4}, {9, 4}, {10, 4},{11, 4},{12, 4},{13, 4},{14, 4},{15, 4},
      {8,-4}, {9,-4}, {10,-4},{11,-4},{12,-4},{13,-4},{14,-4},{15,-4},
      {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},
      {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},
      {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0},  {0,0} }
  };
#endif


#if defined (__GNUC__) && __GNUC__ > 2
#  define NOINLINE __attribute__((noinline))
#else
#  define NOINLINE
#endif

static void threaded_gateway (void) NOINLINE;


void
interpret_code (const uint16 *start_code)
{
  jmp_buf setjmp_buf;
  char *saved_amode_p, *saved_reversed_amode_p;  /* Used in interpreter. */
  jmp_buf *saved_setjmp_buf;
  const uint16 *saved_code;
#if defined (i386) && !defined (__CHECKER__)
  CPUState *saved_cpu_state_ptr;
#endif
#ifdef USE_BIOS_TIMER
  volatile uint16 saved_fs;
#endif

/* #define	CODE_HISTORY	10 */
#if	defined(CODE_HISTORY)
  uint16 lastcodes[CODE_HISTORY];
  uint16 *lastcodeps[CODE_HISTORY];
#endif

#ifndef M68K_REGS_IN_ARRAY
#error "Regs need to be in an array; this is totally broken right now."
#endif

#define d0 cpu_state.regs[0]
#define d1 cpu_state.regs[1]
#define d2 cpu_state.regs[2]
#define d3 cpu_state.regs[3]
#define d4 cpu_state.regs[4]
#define d5 cpu_state.regs[5]
#define d6 cpu_state.regs[6]
#define d7 cpu_state.regs[7]
#define a0 cpu_state.regs[8]
#define a1 cpu_state.regs[9]
#define a2 cpu_state.regs[10]
#define a3 cpu_state.regs[11]
#define a4 cpu_state.regs[12]
#define a5 cpu_state.regs[13]
#define a6 cpu_state.regs[14]
#define a7 cpu_state.regs[15]

  /* Save stuff so we are reentrant and don't smash registers illegally. */
#if defined (i386) && !defined (__CHECKER__)
  saved_cpu_state_ptr = cpu_state_ptr;
  cpu_state_ptr = &cpu_state;
#define cpu_state (*cpu_state_ptr)  /* To provide more concise code. */
#endif
  saved_amode_p          = cpu_state.amode_p;
  saved_reversed_amode_p = cpu_state.reversed_amode_p;
  saved_setjmp_buf       = cpu_state.setjmp_buf;
  saved_code             = code;

#ifdef USE_BIOS_TIMER
  asm volatile ("movw %%fs,%0\n\t"
		"movw %1,%%fs"
		: "=m" (saved_fs)
		: "g" (dos_memory_selector));
#endif  /* USE_BIOS_TIMER */

  /* Note that we are currently busy. */
  ++emulation_depth;

  /* Grab all information from the CPUState. */
  LOAD_CPU_STATE ();

  code = start_code;

  /* Skip over various hacks. */
  goto main_loop;

#ifndef M68K_REGS_IN_ARRAY
 cleanup_amode_for_size_1:
  switch (amode) {
  case 24: a0.ul.n += 1; break;
  case 25: a1.ul.n += 1; break;
  case 26: a2.ul.n += 1; break;
  case 27: a3.ul.n += 1; break;
  case 28: a4.ul.n += 1; break;
  case 29: a5.ul.n += 1; break;
  case 30: a6.ul.n += 1; break;
  case 31: a7.ul.n += 2; break;
  case 32: a0.ul.n -= 1; break;
  case 33: a1.ul.n -= 1; break;
  case 34: a2.ul.n -= 1; break;
  case 35: a3.ul.n -= 1; break;
  case 36: a4.ul.n -= 1; break;
  case 37: a5.ul.n -= 1; break;
  case 38: a6.ul.n -= 1; break;
  case 39: a7.ul.n -= 2; break;
  }
  goto main_loop;

 cleanup_amode_for_size_2:
  switch (amode) {
  case 24: a0.ul.n += 2; break;
  case 25: a1.ul.n += 2; break;
  case 26: a2.ul.n += 2; break;
  case 27: a3.ul.n += 2; break;
  case 28: a4.ul.n += 2; break;
  case 29: a5.ul.n += 2; break;
  case 30: a6.ul.n += 2; break;
  case 31: a7.ul.n += 2; break;
  case 32: a0.ul.n -= 2; break;
  case 33: a1.ul.n -= 2; break;
  case 34: a2.ul.n -= 2; break;
  case 35: a3.ul.n -= 2; break;
  case 36: a4.ul.n -= 2; break;
  case 37: a5.ul.n -= 2; break;
  case 38: a6.ul.n -= 2; break;
  case 39: a7.ul.n -= 2; break;
  }
  goto main_loop;

 cleanup_amode_for_size_4:
  switch (amode) {
  case 24: a0.ul.n += 4; break;
  case 25: a1.ul.n += 4; break;
  case 26: a2.ul.n += 4; break;
  case 27: a3.ul.n += 4; break;
  case 28: a4.ul.n += 4; break;
  case 29: a5.ul.n += 4; break;
  case 30: a6.ul.n += 4; break;
  case 31: a7.ul.n += 4; break;
  case 32: a0.ul.n -= 4; break;
  case 33: a1.ul.n -= 4; break;
  case 34: a2.ul.n -= 4; break;
  case 35: a3.ul.n -= 4; break;
  case 36: a4.ul.n -= 4; break;
  case 37: a5.ul.n -= 4; break;
  case 38: a6.ul.n -= 4; break;
  case 39: a7.ul.n -= 4; break;
  }
  goto main_loop;

#define CLEANUP_AMODE(mode, size) goto cleanup_amode_for_size_ ## size
#else  /* M68K_REGS_IN_ARRAY */

/* We use ugly, idiotic code here to compensate for gcc 2.6.0 brain damage
 * when referencing arrays of structs on the i386.
 */
#define CLEANUP_REG(mode, ix) \
  (*(uint32 *) ((int8 *) &amode_cleanup_info[ix][0] \
		+ (sizeof amode_cleanup_info[0][0] * (mode)) \
		+ offsetof (AmodeCleanupInfo, reg)))
#define CLEANUP_DELTA(mode, ix) \
  (*(int32 *) ((int8 *) &amode_cleanup_info[ix][0] \
	       + (sizeof amode_cleanup_info[0][0] * (mode)) \
	       + offsetof (AmodeCleanupInfo, delta)))
#define CLEANUP_AMODE(mode, size) \
  { \
    /* C compiler should do good things here since "size" is a constant. */ \
    if ((size) == 1)					\
      GENERAL_REGISTER_SL (CLEANUP_REG (mode, 0)) 	\
	+= CLEANUP_DELTA (mode, 0);			\
    else if ((size) == 2)				\
      GENERAL_REGISTER_SL (CLEANUP_REG (mode, 1)) 	\
	+= CLEANUP_DELTA (mode, 1);			\
    else			 			\
      GENERAL_REGISTER_SL (CLEANUP_REG (mode, 2)) 	\
	+= CLEANUP_DELTA (mode, 2);			\
  }
#endif

  /* Extract out increment of code and put it before the loop; this
   * should save some memory as arms of the switch that used to increment
   * this and branch back to the top can now just branch back to the top.
   * Will probably also help instruction cache hits.  Falls through to
   * the main loop...
   */
 main_loop:
#ifdef USE_DIRECT_DISPATCH
  cpu_state.setjmp_buf = &setjmp_buf;
  if (!setjmp (setjmp_buf))
    threaded_gateway ();

  SAVE_CPU_STATE ();

  /* Restore stuff so (for reentrancy). */
  cpu_state.amode_p          = saved_amode_p;
  cpu_state.reversed_amode_p = saved_reversed_amode_p;
  cpu_state.setjmp_buf       = saved_setjmp_buf;
  code                       = saved_code;

#ifdef USE_BIOS_TIMER
  asm volatile ("movw %0,%%fs"
		: : "m" (saved_fs));
#endif

#if defined (i386) && !defined (__CHECKER__)
  cpu_state_ptr              = saved_cpu_state_ptr;
#endif
}
#else

  while (1)
    {
      /* This can't be used with USE_DIRECT_DISPATCH enabled. */
#if	defined(CODE_HISTORY)
    memmove(lastcodes +1, lastcodes ,
				   sizeof(lastcodes ) - sizeof(lastcodes [0]));
    memmove(lastcodeps+1, lastcodeps,
				   sizeof(lastcodeps) - sizeof(lastcodeps[0]));
    lastcodes [0] = (int) *(void **)code;
    lastcodeps[0] =  code;
#endif

      switch ((int) *(((void **)code)++))
	{
#endif

#ifndef USE_DIRECT_DISPATCH
      /* Default to printing error message. */
      default:
	fprintf (stderr, "Internal error: unknown synthetic opcode 0x%04X; "
		 "code = %p\n", (unsigned) (((void **) code)[-1]),
		 (void *) code);
	abort ();
	break;
#endif  /* !USE_DIRECT_DISPATCH */

      /* Reserved - exit emulator. */
      CASE (0x0000)
	CASE_PREAMBLE ("Reserved - exit emulator", "", "", "", "")
#ifdef USE_DIRECT_DISPATCH
	--emulation_depth;
	assert (emulation_depth >= 0);
	longjmp (*cpu_state.setjmp_buf, 1);
#else
	SAVE_CPU_STATE ();
	/* Restore stuff (for reentrancy). */
	cpu_state.amode_p          = saved_amode_p;
	cpu_state.reversed_amode_p = saved_reversed_amode_p;
	cpu_state.setjmp_buf       = saved_setjmp_buf;
	code                       = saved_code;
	--emulation_depth;
	assert (emulation_depth >= 0);
	return;
#endif
	CASE_POSTAMBLE (ROUND_UP (PTR_WORDS));
	
      /* Reserved - one word NOP. */
      CASE (0x0001)
#ifdef GENERATE_NATIVE_CODE
	CASE_PREAMBLE ("Reserved: skip native preamble NOP", "", "", "", "")
#ifdef SYNCHRONOUS_INTERRUPTS
	  {
	    /* Each block's code is prefaced by its address in big
	     * endian order.  Since we know we're at the beginning of
	     * a block, we can check for the interrupt here.
	     */
#if !defined (__alpha) /* FIXME -- TODO -- just use __alpha case for everyone */
	    syn68k_addr_t addr = READUL (US_TO_SYN68K (code - PTR_WORDS - PTR_WORDS));
#else
	    syn68k_addr_t addr = READUL_US (code - PTR_WORDS - PTR_WORDS);
#endif
	    CHECK_FOR_INTERRUPT (addr);
	  }
#endif  /* SYNCHRONOUS_INTERRUPTS */
	CASE_POSTAMBLE (ROUND_UP (PTR_WORDS + PTR_WORDS
				  + NATIVE_PREAMBLE_WORDS));
#else  /* !GENERATE_NATIVE_CODE */
	/* Historical cruft. */
	CASE_PREAMBLE ("Reserved: 1 word NOP", "", "", "", "")
	  abort ();
	CASE_POSTAMBLE (ROUND_UP (PTR_WORDS + 1));
#endif /* !GENERATE_NATIVE_CODE */

      /* Reserved - two word NOP. */
      CASE (0x0002)
	/* Historical cruft. */
	CASE_PREAMBLE ("Reserved: 2 word NOP", "", "", "", "")
	CASE_POSTAMBLE (ROUND_UP (PTR_WORDS + 1));
	
      /* Reserved - skip stub NOP. */
      CASE (0x0003)
#ifdef GENERATE_NATIVE_CODE
	CASE_PREAMBLE ("Reserved: count block freq NOP", "", "", "", "")
	{
	  Block *b = *((Block **)code);
	  if (b != NULL)
	    {
	      syn68k_addr_t addr = b->m68k_start_address;

	      CHECK_FOR_INTERRUPT (addr);

	      if (native_code_p
		  && ++b->num_times_called >= RECOMPILE_CUTOFF
		  && emulation_depth == 1)
		{
		  recompile_block_as_native (b);
		  code = (hash_lookup_code_and_create_if_needed (addr)
			  /* Compensate for the add we do below. */
			  - ROUND_UP (PTR_WORDS + PTR_WORDS
				      + NATIVE_PREAMBLE_WORDS)
			  + OPCODE_WORDS);
		}
	    }
	}
	CASE_POSTAMBLE (ROUND_UP (PTR_WORDS + PTR_WORDS
				  + NATIVE_PREAMBLE_WORDS));
#else  /* !GENERATE_NATIVE_CODE */
	/* Historical cruft. */
	CASE_PREAMBLE ("Reserved: 3 word NOP", "", "", "", "")
	CASE_POSTAMBLE (ROUND_UP (PTR_WORDS + 2));
#endif /* !GENERATE_NATIVE_CODE */

#define AMODE_2_3(casenum, reg, p) \
      CASE (casenum) \
        CASE_PREAMBLE ("Reserved - compute " #p " for mode == 2/3, reg == " \
		       #reg, "", "", "", "") \
	p = (char *) SYN68K_TO_US (CLEAN (reg)); \
	IFDEBUG (printf ("\t" #p " == %p\n", p)); \
	CASE_POSTAMBLE (ROUND_UP (PTR_WORDS))

	AMODE_2_3 (0x0004, a0.ul.n, cpu_state.amode_p);
	AMODE_2_3 (0x0005, a1.ul.n, cpu_state.amode_p);
	AMODE_2_3 (0x0006, a2.ul.n, cpu_state.amode_p);
	AMODE_2_3 (0x0007, a3.ul.n, cpu_state.amode_p);
	AMODE_2_3 (0x0008, a4.ul.n, cpu_state.amode_p);
	AMODE_2_3 (0x0009, a5.ul.n, cpu_state.amode_p);
	AMODE_2_3 (0x000A, a6.ul.n, cpu_state.amode_p);
	AMODE_2_3 (0x000B, a7.ul.n, cpu_state.amode_p);
	AMODE_2_3 (0x000C, a0.ul.n, cpu_state.reversed_amode_p);
	AMODE_2_3 (0x000D, a1.ul.n, cpu_state.reversed_amode_p);
	AMODE_2_3 (0x000E, a2.ul.n, cpu_state.reversed_amode_p);
	AMODE_2_3 (0x000F, a3.ul.n, cpu_state.reversed_amode_p);
	AMODE_2_3 (0x0010, a4.ul.n, cpu_state.reversed_amode_p);
	AMODE_2_3 (0x0011, a5.ul.n, cpu_state.reversed_amode_p);
	AMODE_2_3 (0x0012, a6.ul.n, cpu_state.reversed_amode_p);
	AMODE_2_3 (0x0013, a7.ul.n, cpu_state.reversed_amode_p);

#undef AMODE_2_3
#define AMODE_4(casenum, reg, size, p) \
      CASE (casenum) \
        CASE_PREAMBLE ("Reserved - compute " #p " for mode == 4, reg == " \
		       #reg ", size == " #size, "", "", "", "") \
	p = (char *) SYN68K_TO_US (CLEAN (reg - size)); \
	IFDEBUG (printf ("\t" #p " == %p\n", p)); \
	CASE_POSTAMBLE (ROUND_UP (PTR_WORDS))
	
	AMODE_4 (0x0014, a0.ul.n, 1, cpu_state.amode_p);
	AMODE_4 (0x0015, a1.ul.n, 1, cpu_state.amode_p);
	AMODE_4 (0x0016, a2.ul.n, 1, cpu_state.amode_p);
	AMODE_4 (0x0017, a3.ul.n, 1, cpu_state.amode_p);
	AMODE_4 (0x0018, a4.ul.n, 1, cpu_state.amode_p);
	AMODE_4 (0x0019, a5.ul.n, 1, cpu_state.amode_p);
	AMODE_4 (0x001A, a6.ul.n, 1, cpu_state.amode_p);
	AMODE_4 (0x001B, a7.ul.n, 2, cpu_state.amode_p);
	AMODE_4 (0x001C, a0.ul.n, 1, cpu_state.reversed_amode_p);
	AMODE_4 (0x001D, a1.ul.n, 1, cpu_state.reversed_amode_p);
	AMODE_4 (0x001E, a2.ul.n, 1, cpu_state.reversed_amode_p);
	AMODE_4 (0x001F, a3.ul.n, 1, cpu_state.reversed_amode_p);
	AMODE_4 (0x0020, a4.ul.n, 1, cpu_state.reversed_amode_p);
	AMODE_4 (0x0021, a5.ul.n, 1, cpu_state.reversed_amode_p);
	AMODE_4 (0x0022, a6.ul.n, 1, cpu_state.reversed_amode_p);
	AMODE_4 (0x0023, a7.ul.n, 2, cpu_state.reversed_amode_p);

	AMODE_4 (0x0024, a0.ul.n, 2, cpu_state.amode_p);
	AMODE_4 (0x0025, a1.ul.n, 2, cpu_state.amode_p);
	AMODE_4 (0x0026, a2.ul.n, 2, cpu_state.amode_p);
	AMODE_4 (0x0027, a3.ul.n, 2, cpu_state.amode_p);
	AMODE_4 (0x0028, a4.ul.n, 2, cpu_state.amode_p);
	AMODE_4 (0x0029, a5.ul.n, 2, cpu_state.amode_p);
	AMODE_4 (0x002A, a6.ul.n, 2, cpu_state.amode_p);
	AMODE_4 (0x002B, a7.ul.n, 2, cpu_state.amode_p);
	AMODE_4 (0x002C, a0.ul.n, 2, cpu_state.reversed_amode_p);
	AMODE_4 (0x002D, a1.ul.n, 2, cpu_state.reversed_amode_p);
	AMODE_4 (0x002E, a2.ul.n, 2, cpu_state.reversed_amode_p);
	AMODE_4 (0x002F, a3.ul.n, 2, cpu_state.reversed_amode_p);
	AMODE_4 (0x0030, a4.ul.n, 2, cpu_state.reversed_amode_p);
	AMODE_4 (0x0031, a5.ul.n, 2, cpu_state.reversed_amode_p);
	AMODE_4 (0x0032, a6.ul.n, 2, cpu_state.reversed_amode_p);
	AMODE_4 (0x0033, a7.ul.n, 2, cpu_state.reversed_amode_p);

	AMODE_4 (0x0034, a0.ul.n, 4, cpu_state.amode_p);
	AMODE_4 (0x0035, a1.ul.n, 4, cpu_state.amode_p);
	AMODE_4 (0x0036, a2.ul.n, 4, cpu_state.amode_p);
	AMODE_4 (0x0037, a3.ul.n, 4, cpu_state.amode_p);
	AMODE_4 (0x0038, a4.ul.n, 4, cpu_state.amode_p);
	AMODE_4 (0x0039, a5.ul.n, 4, cpu_state.amode_p);
	AMODE_4 (0x003A, a6.ul.n, 4, cpu_state.amode_p);
	AMODE_4 (0x003B, a7.ul.n, 4, cpu_state.amode_p);
	AMODE_4 (0x003C, a0.ul.n, 4, cpu_state.reversed_amode_p);
	AMODE_4 (0x003D, a1.ul.n, 4, cpu_state.reversed_amode_p);
	AMODE_4 (0x003E, a2.ul.n, 4, cpu_state.reversed_amode_p);
	AMODE_4 (0x003F, a3.ul.n, 4, cpu_state.reversed_amode_p);
	AMODE_4 (0x0040, a4.ul.n, 4, cpu_state.reversed_amode_p);
	AMODE_4 (0x0041, a5.ul.n, 4, cpu_state.reversed_amode_p);
	AMODE_4 (0x0042, a6.ul.n, 4, cpu_state.reversed_amode_p);
	AMODE_4 (0x0043, a7.ul.n, 4, cpu_state.reversed_amode_p);

#undef AMODE_4
#define AMODE_5(casenum, reg, p) \
      CASE (casenum) \
        CASE_PREAMBLE ("Reserved - compute " #p " for mode == 5, "\
		       "reg == " #reg, "", "", "", "") \
	p = (char *) SYN68K_TO_US (CLEAN (reg + (*(int32 *)code))); \
	IFDEBUG (printf ("\t" #p " == %p\n", p)); \
	CASE_POSTAMBLE (ROUND_UP (PTR_WORDS + 2))

	AMODE_5 (0x0044, a0.ul.n, cpu_state.amode_p);
	AMODE_5 (0x0045, a1.ul.n, cpu_state.amode_p);
	AMODE_5 (0x0046, a2.ul.n, cpu_state.amode_p);
	AMODE_5 (0x0047, a3.ul.n, cpu_state.amode_p);
	AMODE_5 (0x0048, a4.ul.n, cpu_state.amode_p);
	AMODE_5 (0x0049, a5.ul.n, cpu_state.amode_p);
	AMODE_5 (0x004A, a6.ul.n, cpu_state.amode_p);
	AMODE_5 (0x004B, a7.ul.n, cpu_state.amode_p);
	AMODE_5 (0x004C, a0.ul.n, cpu_state.reversed_amode_p);
	AMODE_5 (0x004D, a1.ul.n, cpu_state.reversed_amode_p);
	AMODE_5 (0x004E, a2.ul.n, cpu_state.reversed_amode_p);
	AMODE_5 (0x004F, a3.ul.n, cpu_state.reversed_amode_p);
	AMODE_5 (0x0050, a4.ul.n, cpu_state.reversed_amode_p);
	AMODE_5 (0x0051, a5.ul.n, cpu_state.reversed_amode_p);
	AMODE_5 (0x0052, a6.ul.n, cpu_state.reversed_amode_p);
	AMODE_5 (0x0053, a7.ul.n, cpu_state.reversed_amode_p);

#undef AMODE_5

      CASE (0x0054)
	CASE_PREAMBLE ("Reserved - compute cpu_state.amode_p for (xxx).W",
		       "", "", "", "")
	cpu_state.amode_p = (char *) SYN68K_TO_US (CLEAN (*(int32 *)code));
#ifdef DEBUG
	printf ("\tcpu_state.amode_p = %p\n", (void *) cpu_state.amode_p);
#endif
	CASE_POSTAMBLE (ROUND_UP (PTR_WORDS + 2));

      CASE (0x0055)
	CASE_PREAMBLE ("Reserved - compute cpu_state.reversed_amode_p for (xxx).W",
		       "", "", "", "")
	cpu_state.reversed_amode_p = (char *) SYN68K_TO_US (CLEAN (*(int32 *)code));
#ifdef DEBUG
	printf ("\tcpu_state.reversed_amode_p = %p\n", (void *) cpu_state.reversed_amode_p);
#endif
	CASE_POSTAMBLE (ROUND_UP (PTR_WORDS + 2));

      CASE (0x0056)
	CASE_PREAMBLE ("Reserved - compute cpu_state.amode_p for (xxx).L",
		       "", "", "", "")
	cpu_state.amode_p = (char *) *(signed char **)code;
#ifdef DEBUG
	printf ("\tcpu_state.amode_p = %p\n", (void *) cpu_state.amode_p);
#endif
	CASE_POSTAMBLE (ROUND_UP (PTR_WORDS + PTR_WORDS));

      CASE (0x0057)
	CASE_PREAMBLE ("Reserved - compute cpu_state.reversed_amode_p for (xxx).L",
		       "", "", "", "")
	cpu_state.reversed_amode_p = (char *) *(signed char **)code;
#ifdef DEBUG
	printf ("\tcpu_state.reversed_amode_p = %p\n",
		(void *) cpu_state.reversed_amode_p);
#endif
	CASE_POSTAMBLE (ROUND_UP (PTR_WORDS + PTR_WORDS));

#define AMODE_6_SIMPLE(casenum, areg, ixreg, size, p) \
      CASE (casenum) \
	CASE_PREAMBLE ("Reserved - compute " #p " for mode == 6, areg == " \
		       #areg ", ixreg == " #ixreg, "", "", "", "") \
	p = (char *) (CLEAN ((areg) + (((int32) (ixreg)) << \
				       *(uint32 *)(code + PTR_WORDS + 2)) \
			     + *(signed char **)code)); \
	IFDEBUG (printf ("\t" #p " = %p\n", (void *) p)); \
	CASE_POSTAMBLE (ROUND_UP (PTR_WORDS + (size)))

#define ALL_AREG_AMODE_6_SIMPLE(base0,  base1,  base2,  base3,  \
				base4,  base5,  base6,  base7,  \
				base8,  base9,  base10, base11, \
				base12, base13, base14, base15, \
				base16, base17, ixreg, size) \
	AMODE_6_SIMPLE (base0,  a0.ul.n, ixreg, size, cpu_state.amode_p); \
	AMODE_6_SIMPLE (base1,  a1.ul.n, ixreg, size, cpu_state.amode_p); \
	AMODE_6_SIMPLE (base2,  a2.ul.n, ixreg, size, cpu_state.amode_p); \
	AMODE_6_SIMPLE (base3,  a3.ul.n, ixreg, size, cpu_state.amode_p); \
	AMODE_6_SIMPLE (base4,  a4.ul.n, ixreg, size, cpu_state.amode_p); \
	AMODE_6_SIMPLE (base5,  a5.ul.n, ixreg, size, cpu_state.amode_p); \
	AMODE_6_SIMPLE (base6,  a6.ul.n, ixreg, size, cpu_state.amode_p); \
	AMODE_6_SIMPLE (base7,  a7.ul.n, ixreg, size, cpu_state.amode_p); \
	AMODE_6_SIMPLE (base8,  0,       ixreg, size, cpu_state.amode_p); \
	AMODE_6_SIMPLE (base9,  a0.ul.n, ixreg, size, \
			cpu_state.reversed_amode_p);  \
	AMODE_6_SIMPLE (base10, a1.ul.n, ixreg, size, \
			cpu_state.reversed_amode_p);  \
	AMODE_6_SIMPLE (base11, a2.ul.n, ixreg, size, \
			cpu_state.reversed_amode_p);  \
	AMODE_6_SIMPLE (base12, a3.ul.n, ixreg, size, \
			cpu_state.reversed_amode_p);  \
	AMODE_6_SIMPLE (base13, a4.ul.n, ixreg, size, \
			cpu_state.reversed_amode_p);  \
	AMODE_6_SIMPLE (base14, a5.ul.n, ixreg, size, \
			cpu_state.reversed_amode_p);  \
	AMODE_6_SIMPLE (base15, a6.ul.n, ixreg, size, \
			cpu_state.reversed_amode_p);  \
	AMODE_6_SIMPLE (base16, a7.ul.n, ixreg, size, \
			cpu_state.reversed_amode_p);  \
	AMODE_6_SIMPLE (base17, 0,       ixreg, size, \
			cpu_state.reversed_amode_p)

	/* Actual case statements. */
	ALL_AREG_AMODE_6_SIMPLE (0x0058, 0x0059, 0x005A, 0x005B, 0x005C,
				 0x005D, 0x005E, 0x005F, 0x0060, 0x0061,
				 0x0062, 0x0063, 0x0064, 0x0065, 0x0066,
				 0x0067, 0x0068, 0x0069,
				 DATA_REGISTER_SW (*(uint32 *)(code
							       + PTR_WORDS)),
				 4 + PTR_WORDS);
	ALL_AREG_AMODE_6_SIMPLE (0x006A, 0x006B, 0x006C, 0x006D, 0x006E,
				 0x006F, 0x0070, 0x0071, 0x0072, 0x0073,
				 0x0074, 0x0075, 0x0076, 0x0077, 0x0078,
				 0x0079, 0x007A, 0x007B,
				 ADDRESS_REGISTER_SW (*(uint32 *)(code
								  + PTR_WORDS)),
				 4 + PTR_WORDS);
	ALL_AREG_AMODE_6_SIMPLE (0x007C, 0x007D, 0x007E, 0x007F, 0x0080,
				 0x0081, 0x0082, 0x0083, 0x0084, 0x0085,
				 0x0086, 0x0087, 0x0088, 0x0089, 0x008A,
				 0x008B, 0x008C, 0x008D,
				 DATA_REGISTER_UL (*(uint32 *)(code + PTR_WORDS)),
				 4 + PTR_WORDS);
	ALL_AREG_AMODE_6_SIMPLE (0x008E, 0x008F, 0x0090, 0x0091, 0x0092,
				 0x0093, 0x0094, 0x0095, 0x0096, 0x0097,
				 0x0098, 0x0099, 0x009A, 0x009B, 0x009C,
				 0x009D, 0x009E, 0x009F,
				 ADDRESS_REGISTER_UL (*(uint32 *)(code
								  + PTR_WORDS)),
				 4 + PTR_WORDS);
	ALL_AREG_AMODE_6_SIMPLE (0x00A0, 0x00A1, 0x00A2, 0x00A3, 0x00A4,
				 0x00A5, 0x00A6, 0x00A7, 0x00A8, 0x00A9,
				 0x00AA, 0x00AB, 0x00AC, 0x00AD, 0x00AE,
				 0x00AF, 0x00B0, 0x00B1,
				 0, PTR_WORDS);/* ixreg suppressed */

#undef ALL_AREG_AMODE_6_SIMPLE
#undef AMODE_6_SIMPLE

	/* These addressing modes (memory indirect pre- and post-indexed)
	 * are ridiculous so I don't care about performance.  There is only
	 * one case for all variants of this addressing mode, and only one
	 * variant for amode_p and reversed_amode_p.  It wouldn't be difficult
	 * to speed this up by expanding out this case and doing more work at
	 * translation time.
	 */
      CASE (0x00B2)
	CASE_PREAMBLE ("Reserved - compute [reversed_]amode_p for memory "
		       "indirect pre/post-indexed", "", "", "", "")
	int32 base_displacement, outer_displacement;
	uint32 flags = *(uint32 *)(code + 4);
	uint32 areg = ((flags & 0x80) ? 0
		       : ADDRESS_REGISTER_UL (*(uint32 *)(code + 6)));
	int32 index;
	char *temp;

	if (flags & 0x40)  /* Index suppress? */
	  index = 0;
	else
	  {
	    if (flags & (1 << 11))
	      index = GENERAL_REGISTER_SL (flags >> 12);
	    else
	      index = GENERAL_REGISTER_SW (flags >> 12);
	    index <<= (flags >> 9) & 3;
	  }

	base_displacement  = ((int32 *)code)[0];
	outer_displacement = ((int32 *)code)[1];

	if (flags & 2)
	  temp = (char *) SYN68K_TO_US (READSL (areg + base_displacement
						+ index)
					+ outer_displacement);
	else
	  temp = (char *) SYN68K_TO_US (READSL (areg + base_displacement)
					+ index + outer_displacement);

	if (flags & 1)
	  {
	    cpu_state.reversed_amode_p = temp;
#ifdef DEBUG
	    printf ("\tcpu_state.reversed_amode_p = %p\n",
		    (void *) reversed_amode_p);
#endif
	  }
	else
	  {
	    cpu_state.amode_p = temp;
#ifdef DEBUG
	    printf ("\tcpu_state.amode_p = %p\n", (void *) cpu_state.amode_p);
#endif
	  }

	CASE_POSTAMBLE (ROUND_UP (PTR_WORDS + 8));

      CASE (0x00B3)
	CASE_PREAMBLE ("Reserved - callback", "", "", "", "")
	SAVE_CPU_STATE ();
	code = code_lookup ((*((uint32 (**)(uint32, void *)) code))
			    (*(uint32 *)(code + PTR_WORDS + PTR_WORDS),
			     *(void **)(code + PTR_WORDS)));
	LOAD_CPU_STATE ();
	RESTORE_FS ();
	CASE_POSTAMBLE (ROUND_UP (PTR_WORDS));

      CASE (0x00B4)
	CASE_PREAMBLE ("Reserved - fast jsr", "", "", "", "")
	unsigned ix;
	jsr_stack_elt_t *j;
	syn68k_addr_t retaddr;

	ix = ((cpu_state.jsr_stack_byte_index - sizeof (jsr_stack_elt_t))
	      % sizeof (cpu_state.jsr_stack));
	cpu_state.jsr_stack_byte_index = ix;
	/* Note: retaddr is in big-endian byte order. */
	retaddr = *(const uint32 *)(code + PTR_WORDS + PTR_WORDS); 
	j = (jsr_stack_elt_t *)((char *)&cpu_state.jsr_stack + ix);
	j->tag = retaddr;
	j->code = *(const uint16 **)(code + PTR_WORDS);
	code = *(const uint16 **)code;
	a7.ul.n -= 4;
	WRITEUL_UNSWAPPED (SYN68K_TO_US (CLEAN (a7.ul.n)), retaddr);
	CASE_POSTAMBLE (ROUND_UP (PTR_WORDS));

