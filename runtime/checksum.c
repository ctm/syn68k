#define INLINE_CHECKSUM
#include "checksum.h"


#ifdef CHECKSUM_BLOCKS
uint32
compute_block_checksum (const Block *b)
{
  return inline_compute_block_checksum (b);
}
#endif  /* CHECKSUM_BLOCKS */
