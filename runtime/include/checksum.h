#ifndef _checksum_h_
#define _checksum_h_

#include "syn68k_private.h"

#ifdef CHECKSUM_BLOCKS

#include "block.h"

extern uint32 compute_block_checksum (const Block *b);

#ifdef INLINE_CHECKSUM
#include "callback.h"

static __inline__ uint32
inline_compute_block_checksum (const Block *b)
{
  const uint16 *code;
  uint32 sum;
  long n;
  syn68k_addr_t start;

  /* Don't actually checksum the bytes at a callback! */
  start = b->m68k_start_address;
  if (IS_CALLBACK (start))
    return (start
	    + (unsigned long) callback_argument (start)
	    + (unsigned long) callback_function (start));
  else if (start >= MAGIC_ADDRESS_BASE
	   && start < CALLBACK_STUB_BASE)
    return 0;

  code = SYN68K_TO_US (start);
  for (sum = 0, n = b->m68k_code_length / sizeof (uint16); --n >= 0; )
    {
      /* Bizarre but fast checksum.  A good checksum like CRC is
       * probably too slow.
       */
#if defined (__GNUC__) && defined(i386)
      uint32 tmp1, tmp2;

      /* This is taking lots of CPU time for HyperCard, and gcc was generating
       * lousy code, so I've hand-coded and hand-scheduled this for the
       * Pentium.  This should do the same thing as the C code below.
       */
      asm ("movl %0,%2\n\t"
	   "movl %0,%1\n\t"
	   "sall $5,%2\n\t"
	   "shrl $14,%1\n\t"
	   "addl %0,%2\n\t"
	   "xorl %2,%1\n\t"
	   "xorw (%4,%5,2),%w0\n\t"
	   "xorl %1,%0"
	   : "=r" (sum), "=&r" (tmp1), "=&r" (tmp2)
	   : "0" (sum), "r" (code), "r" (n)
	   : "cc");
#else   /* !__GNUC__ || !i386 */
      sum = (sum >> 14) ^ (sum * 33) ^ code[n];
#endif  /* !__GNUC__ || !i386 */
    }

  return sum;
}
#endif  /* INLINE_CHECKSUM */

#endif  /* CHECKSUM_BLOCKS */

#endif  /* Not _checksum_h_ */
