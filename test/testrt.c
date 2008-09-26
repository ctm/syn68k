/* This file tests the range tree.  It has to include private header files
 * belonging to the runtime system because there is no official outside
 * interface to blocks or to the range tree; don't do this at home.
 */

#define DEBUG
#include "../runtime/include/block.h"
#include "../runtime/include/rangetree.h"
#include "../runtime/include/destroyblock.h"
#include "../runtime/include/hash.h"
#include "../runtime/include/deathqueue.h"
#include "testrt.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_BLOCKS 4096

#if defined (__MINGW32__)

int
random (void)
{
  unsigned int i1, i2, i3;
  int retval;

  i1 = (rand () >> 5) & 0x7ff;
  i2 = (rand () >> 5) & 0x7ff;
  i3 = (rand () >> 5) & 0x7ff;
  retval = (i1 << 22) | (i2 << 11) | i3;
  return retval;
}

#endif

void
test_rangetree ()
{
  int i;

  for (i = 1; i <= 1000000; i++)
    {
      Block *b, *b2;

      if (i % 50 == 0)
	printf ("%d...", i), fflush (stdout);

      if (i % 5 == 0)
	{
	  destroy_blocks (0, ~0);
/*	  range_tree_verify (); */
	}

      b = block_new ();

      b->m68k_code_length = 2;
      do
	{
	  b->m68k_start_address = rand () & ~1;
	}
      while (range_tree_find_first_at_or_after (b->m68k_start_address) != NULL);

      /* Add this block to the end of the death queue. */
      death_queue_enqueue (b);

      b->num_children = 0;

      range_tree_insert (b);
      hash_insert (b);

      if (random () & 1)
	{
	  b2 = range_tree_find_first_at_or_after (random ());
	  if (b2 != NULL && !b2->immortal)
	    {
	      b->child[0] = b2;
	      block_add_parent (b2, b);
	      b->num_children = 1;

	      if (random () & 1)
		{
		  b2 = range_tree_find_first_at_or_after (random ());
		  if (b2 != NULL && b2 != b->child[0] && !b2->immortal)
		    {
		      b->child[1] = b2;
		      block_add_parent (b2, b);
		      b->num_children = 2;
		    }
		}
	    }
	}

/*      range_tree_verify (); */
    }
}
