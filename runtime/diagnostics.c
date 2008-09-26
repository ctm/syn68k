#include "diagnostics.h"
#include "deathqueue.h"

void
print_cc_bits (FILE *stream, int bits)
{
  int i;

  for (i = 4; i >= 0; i--)
    if (bits & (1 << i))
      putc ("ZXVNC"[i], stream);
    else putc ('-', stream);
}


void
hexdump (const uint16 *addr, int num_words)
{
  while (num_words--)
    printf ("\t0x%04X\n", (unsigned) *addr++);
}


void
dump_cpu_state (void)
{
  int i;

  for (i = 0; i < 16; i++)
    {

#if	!defined(__alpha)
      printf ("%c%d = 0x%08lX ", "da"[i >> 3], i % 8, cpu_state.regs[i].ul.n);
#else
      printf ("%c%d = 0x%08X ", "da"[i >> 3], i % 8, cpu_state.regs[i].ul.n);
#endif

      if ((i & 3) == 3)
	fputs ("\n\t", stdout);
    }
  printf ("c:%d n:%d v:%d x:%d z:%d\n",
	  !!cpu_state.ccc, !!cpu_state.ccn, !!cpu_state.ccv, !!cpu_state.ccx,
	  !cpu_state.ccnz);
  putchar ('\n');
}


/* Given a PC (either synthetic or native), tries to find the m68k
 * block corresponding to that PC and prints it out.  For debugging.
 */
void
m68kaddr (const uint16 *pc)
{
  const Block *b, *best_block;
  unsigned long best_error;

  best_error = 0xFFFFFFFF;
  best_block = NULL;
  for (b = death_queue_head; b != NULL; b = b->death_queue_next)
    {
      if (b->compiled_code <= pc)
	{
	  unsigned long error = pc - b->compiled_code;
	  if (error < best_error)
	    {
	      best_error = error;
	      best_block = b;
	    }
	}
    }

  if (best_block == NULL)
    {
      puts ("No matching m68k block found.");
    }
  else
    {
      printf ("Best guess m68k block start address 0x%lx, offset %lu bytes, "
	      "block (Block *)%p\n",
	      (unsigned long) best_block->m68k_start_address,
	      best_error, (void *) best_block);
    }
}
