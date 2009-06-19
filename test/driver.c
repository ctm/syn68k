#include "driver.h"
#include "testbattery.h"
#include "setup.h"
#include "run68k.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "crc.h"

#include <unistd.h>

/* These are set by command-line switches. */
int test_only_non_cc_variants;  /* If 0, test only cc setting variants. */
int generate_crc;

uint32 times_called;
uint16 *ccr_addr;
uint32 *return_address_addr;

static void test_instruction (uint32 times_to_test, const TestInfo *info,
			      DriverMode mode);
#ifdef mc68000
static char *unparse_ccr (int ccr, char *buf);
static void hexdump (FILE *stream, uint16 *addr, int num_words);
#endif

#if	defined (__alpha) || defined (__linux__)
#include <sys/types.h>
#include <sys/mman.h>
#endif	/* __alpha || __linux__ */

#define	SLOP	(sizeof(uint16) + sizeof(uint32)) /* for ccr and return_addr */

void
test_all_instructions (uint32 try_count)
{
  const TestInfo *info;

  /* Try to provide a consistent address across multiple platforms so that
   * crc's of address registers containing ptrs to this space turn out
   * the same.  To this end we malloc a large block of memory and hope
   * the pointer we want is in the middle.  If this doesn't work, you'll
   * have to hand-massage this.
   */
  if (mem == NULL)
    {
      size_t page_size;

#define M_SIZE (40 * 1024 * 1024)
#define CODE_SIZE 512

#if     !defined (__linux__)
#define DESIRED_PTR ((uint8 *) 0x26C0000 + MEMORY_OFFSET)
      uint8 *m = (uint8 *)malloc (M_SIZE + CODE_SIZE + MEMORY_OFFSET);
#else   /* defined (__linux__) */
#if !defined (powerpc) && 0
#define DESIRED_PTR ((uint8 *) 0xAF000000 + MEMORY_OFFSET)
#else
#define DESIRED_PTR ((uint8 *) 0x7F000000 + MEMORY_OFFSET)
#endif
      uint8 *m = mmap (DESIRED_PTR,
                       (M_SIZE + SLOP + 4095) & ~4095,
                       PROT_READ|PROT_WRITE|PROT_EXEC,
                       MAP_ANONYMOUS|MAP_FIXED|MAP_PRIVATE, -1, 0);
#endif   /* defined (__linux__) */

#if !defined (__MINGW32__)
      page_size = getpagesize ();
#else
#warning We currently do not use getpagesize because with the version
#warning of MingW32 that we use it winds up pulling in a routine that
#warning we do not currently have available (VirtualProtect).  This should
#warning be looked into sometime.
      page_size = 4096;
#endif

      if ((DESIRED_PTR < m || DESIRED_PTR >= m + M_SIZE - MEM_SIZE - CODE_SIZE)
	  && generate_crc)
	{
	  /* Print out address here in case assertion fails. */
	  printf ("driver.c: Bad m == 0x%lX, absurd hack didn't work (page_size = %ld, DESIRED_PTR = 0x%lX).\n",
		  (unsigned long) m, (long) page_size, (unsigned long) DESIRED_PTR);
	  abort ();
	}

      if (generate_crc)
	mem = DESIRED_PTR;
      else
	mem = m;
      return_address_addr = (uint32 *)(mem + MEM_SIZE);	/* must be a 32 bit address */
      ccr_addr = (uint16 *) ((char *) return_address_addr +
			     sizeof(*return_address_addr));
    }
  if (generate_crc)
    printf ("mem == 0x%lX\n", (unsigned long) DESIRED_PTR);

  for (info = test_info; info->name; info++)
    {
      uint32 times_to_try = try_count;
      if (info->max_times_to_call != 0
	  && info->max_times_to_call > times_to_try)
	times_to_try = info->max_times_to_call;
      test_instruction (times_to_try, info,
#ifdef mc68000
			generate_crc ? DM_USE_EMULATOR : DM_COMPARE_AS_YOU_GO
#else
			DM_USE_EMULATOR
#endif
			);
    }
}


static uint8 saved_mem[MEM_SIZE];

#ifdef mc68000

/*
 * Under NEXTSTEP 3.1 it is possible for move16 to not move all sixteen bytes.
 */

int move16memcmp(const void *emulatedvp, const void *realvp,
						 const void *savedvp, size_t n)
{
    const char *emulatedp, *realp, *savedp;
    int badcount, error;

    emulatedp = emulatedvp;
    realp     = realvp;
    savedp    = savedvp;
    for (badcount = 0, error = 0; !error && n-- != 0; ++emulatedp, ++realp,
								    ++savedp) {
	if (*realp != *emulatedp)
	    if (*realp != *savedp || ++badcount > 16)
		error = 1;
    }
    return error;
}
#endif

static void
test_instruction (uint32 times_to_test, const TestInfo *info, DriverMode mode)
{
  uint32 emulator_return_addr;
  /* We'll use arrays of uint8 to guarantee big-endianness. */
  const uint8 header[] = {
    0x44, 0xF9,  /* movew abs.l,ccr */
    US_TO_SYN68K (ccr_addr) >> 24, US_TO_SYN68K (ccr_addr) >> 16,
    US_TO_SYN68K (ccr_addr) >> 8,  US_TO_SYN68K (ccr_addr),
  };    
  uint8 trailer[] = {

    /* These guys smash the ccr so that we can test the non-cc bit setting
     * versions of different instructions.  To test the cc-bit setting versions
     * of instructions, we clobber these with NOPs below.
     */
    0x44, 0xF9,
    US_TO_SYN68K (ccr_addr) >> 24, US_TO_SYN68K (ccr_addr) >> 16,
    US_TO_SYN68K (ccr_addr) >> 8,  US_TO_SYN68K (ccr_addr),

    /* Here's the main code. */
    0x42, 0xF9,  /* movew ccr,abs.l */
    US_TO_SYN68K (ccr_addr) >> 24, US_TO_SYN68K (ccr_addr) >> 16,
    US_TO_SYN68K (ccr_addr) >> 8,  US_TO_SYN68K (ccr_addr),
    0x4E, 0xF0,  /* jmp memory indirect post-indexed */
    0x01, 0xF5,  /* base suppress + index suppress + LONG bd + no od. */
    US_TO_SYN68K (return_address_addr) >> 24,
    US_TO_SYN68K (return_address_addr) >> 16,
    US_TO_SYN68K (return_address_addr) >> 8,
    US_TO_SYN68K (return_address_addr),
    };
  uint16 *m68k_code = (uint16 *) (mem + MEM_SIZE);
  uint16 *code_dest;
  uint32 entry_point, tc;
  uint16 cc;
#ifdef mc68000
  uint32 errors = 0;
#endif
  uint16 pre_crc = 0, mem_crc = 0, reg_crc = 0, cc_crc = 0;

  /* NOTE: cc_crc is the old reg_crc, where we include condition codes.
           reg_crc now has regs only.  I think the way we handle the "seed"
	   in computer_crc makes having a "crc" of just the condition codes
	   not work w/o some fiddling.  */

  printf ("Testing %s...", info->name), fflush (stdout);

  /* Copy in trailer. */
  if (!test_only_non_cc_variants)   /* NOP out cc clobber if we want cc's. */
    {
      static const unsigned char nop_nop_nop[] = {
	0x4E, 0x71, 0x4E, 0x71, 0x4E, 0x71
	};
      memcpy (trailer, nop_nop_nop, sizeof nop_nop_nop);
    }
  memcpy (((void *)&m68k_code[128]) - sizeof trailer, trailer, sizeof trailer);

  /* Compute where the 68k code should go. */
  code_dest = ((uint16 *) (((void *)&m68k_code[128]) - sizeof trailer)
	       - info->code_words);

  /* Copy in header. */
  memcpy (((void *)code_dest) - sizeof header, header, sizeof header);

  entry_point = US_TO_SYN68K (((void *)code_dest) - sizeof header);

#ifndef mc68000
  if (mode != DM_USE_EMULATOR)
    {
      fprintf (stderr, "Cannot test on real CPU if it's not a 68000!\n");
      exit (-1);
    }
#endif

  emulator_return_addr = SWAPUL_IFLE (MAGIC_EXIT_EMULATOR_ADDRESS);

  for (tc = 0; tc < times_to_test; tc++)
    {
      M68kReg saved_regs[16];

      times_called = tc;

      if (((times_called + 1) % 2000) == 0)
	printf ("%lu...", times_called + 1), fflush (stdout);

      fully_randomize_regs ();

      if (mode != DM_USE_68000)
	{
	  /* Destroy any previously compiled code we may have been testing. */
	  destroy_blocks (US_TO_SYN68K (m68k_code), CODE_SIZE);
	}

      if (info->might_change_memory)
	randomize_mem ();

      /* Create the 68k code. */
      info->code_creator (code_dest);

      /* Byte swap it if appropriate. */
#ifdef LITTLEENDIAN
      {
	int i;
	for (i = info->code_words - 1; i >= 0; i--)
	  code_dest[i] = SWAPUW (code_dest[i]);
      }
#endif

      memcpy (saved_regs, cpu_state.regs, sizeof saved_regs);
      if (info->might_change_memory)
	memcpy (saved_mem, mem, sizeof saved_mem);

#if defined (NeXT) && defined (mc68000)
      if (mode != DM_USE_EMULATOR)
	asm ("trap #2");
#endif

      /* Compute pre_crc if appropriate.  pre_crc refers to the checksum
       * before the code is emulated.
       */
      if (mode != DM_COMPARE_AS_YOU_GO)
	{
	  unsigned char buf[16 * 4];  /* Regs */
	  unsigned char *p;
	  int i;
	  
	  pre_crc = compute_crc (saved_mem, MEM_SIZE, pre_crc);
	  
	  /* Guarantee byte-order independent crc */
	  for (p = buf, i = 0; i < 16; i++)
	    {
	      *p++ = saved_regs[i].ul.n >> 24;
	      *p++ = saved_regs[i].ul.n >> 16;
	      *p++ = saved_regs[i].ul.n >> 8;
	      *p++ = saved_regs[i].ul.n;
	    }
	  pre_crc = compute_crc (buf, p - buf, pre_crc);
	}

      /* Run it. */
      for (cc = 0; cc <= ALL_CCS; cc++)
	{
	  uint16 emulated_cc = 0;
	  M68kReg emulated_regs[16];
	  static uint8 emulated_mem[MEM_SIZE];
	  int mask = info->cc_mask;
#ifdef mc68000
	  uint16 real_cc = 0;
	  M68kReg real_regs[16];
	  static uint8 real_mem[MEM_SIZE];
	  int i;
#endif

	  if (mode != DM_USE_68000)
	    {
	      *ccr_addr = SWAPUW_IFLE (cc);
	      *return_address_addr = emulator_return_addr;
	      memcpy (cpu_state.regs, saved_regs, sizeof saved_regs);
	      if (info->might_change_memory)
		memcpy (mem, saved_mem, sizeof saved_mem);
	      interpret_code (hash_lookup_code_and_create_if_needed
			      (entry_point));
	      memcpy (emulated_regs, cpu_state.regs, sizeof emulated_regs);
	      if (info->might_change_memory)
		memcpy (emulated_mem, mem, sizeof emulated_mem);
	      emulated_cc = SWAPUW_IFLE (*ccr_addr);
	    }

#ifdef mc68000
	  if (mode != DM_USE_EMULATOR)
	    {
	      *ccr_addr = SWAPUW_IFLE (cc);
	      memcpy (cpu_state.regs, saved_regs, sizeof saved_regs);
	      if (info->might_change_memory)
		memcpy (mem, saved_mem, sizeof saved_mem);
	      run_code_on_68000 (SYN68K_TO_US (entry_point), return_address_addr);
	      memcpy (real_regs, cpu_state.regs, sizeof real_regs);
	      if (info->might_change_memory)
		memcpy (real_mem, mem, sizeof real_mem);
	      real_cc = SWAPUW_IFLE (*ccr_addr);
	    }

	  /* If we are doing a divide, and the V bit is set,
	   * let Z and N be undefined.
	   */
	  if (!strncmp (info->name, "div", 3) && (real_cc & V_BIT))
	    mask &= ~(Z_BIT | N_BIT);

	  if (mode == DM_COMPARE_AS_YOU_GO)
	    {
	      char cc1[6], cc2[6];
	      char errbuf[2048] = "";

	      if ((emulated_cc ^ real_cc) & mask)
		{
		  sprintf (errbuf + strlen (errbuf),
			   "\tOutput ccrs different!  emulator = %s, "
			   "real = %s\n", unparse_ccr (emulated_cc, cc1),
			   unparse_ccr (real_cc, cc2));
		}
	      if (memcmp (emulated_regs, real_regs, sizeof emulated_regs))
		{
		  sprintf (errbuf + strlen (errbuf),
			   "\tOutput regs different:\n");
		  for (i = 0; i < 16; i++)
		    {
		      if (emulated_regs[i].ul.n != real_regs[i].ul.n)
			sprintf (errbuf + strlen (errbuf),
				 "\temulator %c%d = 0x%08lX, "
				 "real %c%d = 0x%08lX\n",
				 "da"[i >> 3], i % 8, emulated_regs[i].ul.n,
				 "da"[i >> 3], i % 8, real_regs[i].ul.n);
		    }
		}
	      if ((info->might_change_memory == MIGHT_CHANGE_MEMORY
		  && memcmp (emulated_mem, real_mem, sizeof real_mem)) ||
		  (info->might_change_memory == MOVE16_CHANGE_MEMORY
		  && move16memcmp(emulated_mem, real_mem, saved_mem,
							     sizeof real_mem)))
		{
		  sprintf (errbuf + strlen (errbuf),
			   "\tOutput memory images different!\n");

		  for (i = 0; i < MEM_SIZE; i++)
		    if (emulated_mem[i] != real_mem[i])
		      sprintf (errbuf + strlen (errbuf),
			       "\tmem + 0x%X (%p):\toriginal:0x%02X"
			       "\temulator:0x%02X\treal:0x%02X\n",
			       (unsigned) i, (void *) &mem[i],
			       (unsigned) saved_mem[i],
			       (unsigned) emulated_mem[i],
			       (unsigned) real_mem[i]);
		}
	      if (errbuf[0])
		{
		  fprintf (stderr, "%s failed test for input ccr = %s.\n",
			   info->name, unparse_ccr (cc, cc1));
		  fputs ("Code that caused the problem:\n", stderr);
		  hexdump (stderr, code_dest, info->code_words);
		  fputs ("Input regs:\n\t", stderr);
		  for (i = 0; i < 16; i++)
		    {
		      fprintf (stderr, "%c%d = 0x%08lX ",
			       "da"[i >> 3], i % 8, saved_regs[i].ul.n);
		      if ((i & 3) == 3)
			{
			  putc ('\n', stderr);
			  if (i != 15)
			    putc ('\t', stderr);
			}
		    }

		  fputs ("Error log:\n", stderr);
		  fputs (errbuf, stderr);
		  fputs ("---------------------------------\n", stderr);

		  if (++errors >= MAX_ERRORS)
		    {
		      puts ("aborting.");
		      return;
		    }

		  /* Stop spewing on this code, try new code. */
		  break;
		}
	    }
#endif

	  /* Compute CRC if appropriate. */
	  if (mode != DM_COMPARE_AS_YOU_GO)
	    {
	      unsigned char buf[16 * 4 + 1];  /* Regs + cc bits. */
	      unsigned char *p;
	      int i;

	      if (info->might_change_memory)
		mem_crc = compute_crc (mem, MEM_SIZE, mem_crc);

	      /* Guarantee byte-order independent crc */
	      for (p = buf, i = 0; i < 16; i++)
		{
		  *p++ = cpu_state.regs[i].ul.n >> 24;
		  *p++ = cpu_state.regs[i].ul.n >> 16;
		  *p++ = cpu_state.regs[i].ul.n >> 8;
		  *p++ = cpu_state.regs[i].ul.n;
		}
	      *p++ = emulated_cc & mask;
	      reg_crc = compute_crc (buf, p - buf - 1, reg_crc);
	      cc_crc = compute_crc (buf, p - buf, cc_crc);
	    }
	}
    }

  if (mode != DM_COMPARE_AS_YOU_GO)
    printf ("done.\tpre_crc:0x%04X, mem_crc:0x%04X, reg_crc:0x%04X, cc_crc:0x%04X\n",
	    (unsigned) pre_crc, (unsigned) mem_crc, (unsigned) reg_crc, (unsigned) cc_crc);
  else
    puts ("done");
}


#ifdef mc68000
static char *
unparse_ccr (int ccr, char *buf)
{
  int i;

  if (ccr >= 32)
    {
      printf ("illegal ccr: 0x%X\n", (unsigned) ccr);
      abort ();
    }

  for (i = 0; i < 5; i++)
    buf[i] = (ccr & (1 << i)) ? "CVZNX"[i] : '-';
  buf[5] = '\0';
  return buf;
}


static void
hexdump (FILE *stream, uint16 *addr, int num_words)
{
  while (num_words--)
    {
      unsigned n = *addr++;
      int i;

      fprintf (stream, "\t0x%04X\t(", n);
      for (i = 15; i >= 0; i--)
	putc ((n & (1 << i)) ? '1' : '0', stream);
      fputs (")\n", stream);
    }
}
#endif
