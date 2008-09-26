#ifdef MSDOS

#include "syn68k_private.h"

#ifndef SYNCHRONOUS_INTERRUPTS

int
dos_block_interrupts ()
{
  unsigned long iflag;
  asm ("pushfl\n\t" /* We save the old eflags, which has interrupt mask bit. */
       "popl %0\n\t"
       "shrl $9,%0\n\t"
       "andl $1,%0\n\t"
       "jz 1f\n\t"     /* Avoid CLI overhead under DPMI. */
       "cli\n"
       "1:"
       : "=r" (iflag)
       : : "cc");
  return iflag;
}


void
dos_restore_interrupts (int onoff)
{
  unsigned long current_iflag;
  
  asm ("pushfl\n\t" /* We save the old eflags, which has interrupt mask bit. */
       "popl %0\n\t"
       "shrl $9,%0\n\t"
       "andl $1,%0"
       : "=r" (current_iflag)
       : : "cc");
  
  if (onoff != current_iflag)
    {
      /* Only execute these when necessary, to avoid DPMI overhead. */
      if (onoff)
	asm ("sti");
      else
	asm ("cli");
    }
}

#endif  /* !SYNCHRONOUS_INTERRUPTS */

#endif  /* MSDOS */
