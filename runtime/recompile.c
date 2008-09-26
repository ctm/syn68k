#include "syn68k_private.h"

#ifdef GENERATE_NATIVE_CODE

#include "recompile.h"
#include "translate.h"
#include "destroyblock.h"
#include "hash.h"
#include "alloc.h"
#include "native.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>


static void find_blocks_to_recompile (Block *b, syn68k_addr_t **bad_blocks,
				      unsigned long *num_bad_blocks,
				      unsigned long *max_bad_blocks);


static int
compare_m68k_addrs (const void *p1, const void *p2)
{
  return *(syn68k_addr_t *)p1 - *(syn68k_addr_t *)p2;
}

void
recompile_block_as_native (Block *b)
{
  syn68k_addr_t *bad_blocks, orig_address;
  long n;
  unsigned long num_bad_blocks, max_bad_blocks;
  int old_sigmask;

  BLOCK_INTERRUPTS (old_sigmask);
  
  orig_address = b->m68k_start_address;

#if 0
  fprintf (stderr,
	   "recompiling block %p at address 0x%X.  Block ptr in code is %p.\n",
	   (void *)b, (unsigned)b->m68k_start_address,
	   *(void **)(b->compiled_code + PTR_WORDS));
#endif

  /* Allocate some space for the blocks to recompile. */
  max_bad_blocks = 256;
  num_bad_blocks = 0;
  bad_blocks = (syn68k_addr_t *)xmalloc (max_bad_blocks
					 * sizeof bad_blocks[0]);

  /* Find a bunch of blocks to smash. */
  find_blocks_to_recompile (b, &bad_blocks, &num_bad_blocks, &max_bad_blocks);

  assert (num_bad_blocks != 0);

  /* Sort the blocks by address.  Hopefully this will require fewer
   * passes when recompiling below.
   */
  qsort (bad_blocks, num_bad_blocks, sizeof bad_blocks[0], compare_m68k_addrs);

#if 0
  for (n = 0; n < num_bad_blocks; n++)
    fprintf (stderr, "\t0x%X\n",
	     (unsigned)(hash_lookup (bad_blocks[n])->m68k_start_address));
#endif

  /* Destroy them all. */
  for (n = num_bad_blocks - 1; n >= 0; n--)
    {
      /* We need to do a hash check since this block may have been
       * recursively destroyed on some previous call to destroy_block().
       * That's why we don't just keep an array of Block *'s.
       */
      Block *b = hash_lookup (bad_blocks[n]);
      if (b != NULL)
	destroy_block (b);
    }

  /* Recompile them all with native code enabled. */
  for (n = 0; n < num_bad_blocks; n++)
    {
      Block *junk;
      generate_block (NULL, bad_blocks[n], &junk, TRUE);
    }

  free (bad_blocks);

  /* Smash the jsr stack to be safe. */
  memset (&cpu_state.jsr_stack, -1, sizeof cpu_state.jsr_stack);

  assert ((b = hash_lookup (orig_address)) && NATIVE_CODE_TRIED (b));

  RESTORE_INTERRUPTS (old_sigmask);

#if 0
  fprintf (stderr,
	   "  done compiling.  New block == %p, new block pointer == %p\n",
	   (void *)b, *(void **)(b->compiled_code + PTR_WORDS));
#endif
}


static void
find_parents (Block *b, syn68k_addr_t **bad_blocks,
	      unsigned long *num_bad_blocks,
	      unsigned long *max_bad_blocks)
{
  while (b != NULL && !b->recompile_me)
    {
      int i;

      b->recompile_me = TRUE;

      if (*num_bad_blocks >= *max_bad_blocks)
	{
	  *max_bad_blocks *= 2;
	  *bad_blocks = (syn68k_addr_t *)
			 xrealloc (*bad_blocks, (*max_bad_blocks
						 * sizeof (*bad_blocks)[0]));
	}
      
      /* Append this block to the list of blocks to smash. */
      (*bad_blocks)[(*num_bad_blocks)++] = b->m68k_start_address;
      b->recompile_me = TRUE;

      for (i = b->num_parents - 1; i > 0; i--)
	{
	  Block *p = b->parent[i];
	  if (!p->recompile_me)  /* Check now for speed. */
	    find_parents (p, bad_blocks, num_bad_blocks, max_bad_blocks);
	}
      
      /* Iterate on the last one, for speed. */
      if (b->num_parents)
	b = b->parent[0];
    }
}


static void
find_blocks_to_recompile (Block *b, syn68k_addr_t **bad_blocks,
			  unsigned long *num_bad_blocks,
			  unsigned long *max_bad_blocks)
{
  while (b != NULL
	 && !b->recompile_me
	 && b->num_times_called >= RECOMPILE_CHILD_CUTOFF
	 && !NATIVE_CODE_TRIED (b))
    {
      find_parents (b, bad_blocks, num_bad_blocks, max_bad_blocks);
      
      if (b->child[1] != NULL)
	find_blocks_to_recompile (b->child[1], bad_blocks, num_bad_blocks,
				  max_bad_blocks);

      /* Iterate, for speed. */
      b = b->child[0];
    }
}


/* Just to give us interesting statistics on what fraction was actually
 * recompiled as native.
 */
double
native_fraction ()
{
  Block *b;
  unsigned long native, nonnative;
  double ratio;

  native = nonnative = 0;
  for (b = death_queue_head; b != NULL; b = b->death_queue_next)
    {
      if (NATIVE_CODE_TRIED (b))
	++native;
      else
	++nonnative;
    }

  if (native + nonnative == 0)
    ratio = 0.0;
  else
    ratio = (double)native / (native + nonnative);

  printf ("%lu/%lu native (%.2f%%)\n",  native, native + nonnative,
	  ratio * 100.0);
  
  return ratio;
}


#endif  /* GENERATE_NATIVE_CODE */
