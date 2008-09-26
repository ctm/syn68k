#include "destroyblock.h"
#include "deathqueue.h"
#include "alloc.h"
#include "hash.h"
#include "rangetree.h"
#include "translate.h"
#define INLINE_CHECKSUM  /* Get the fast, inline version. */
#include "checksum.h"
#include <assert.h>
#include <signal.h>
#include <string.h>


/* Temp hack */
#include <stdio.h>


static Block *current_block_in_death_queue;


/* This routine destroys a block and any known parents of this block (and
 * so on recursively).  Returns the total number of blocks actually destroyed.
 */
unsigned long
destroy_block (Block *b)
{
  unsigned long num_destroyed;
  int i;

  /* If the block is immortal, refuse to destroy it.  FIXME? */
  if (b->immortal)
    return 0;

  /* Be paranoid, to avoid freeing this block multiple times. */
  b->immortal = TRUE;

  /* Call the user-defined function to let them know we're busy. */
  if (call_while_busy_func != NULL)
    call_while_busy_func (1);

  /* Make sure no children claim us as a parent to prevent bad things
   * happening on recursion.
   */
  for (i = b->num_children - 1; i >= 0; i--)
    if (b->child[i] != NULL)
      block_remove_parent (b->child[i], b, FALSE);

  /* Destroy all of our parents.  This should not be able to recurse
   * around to us again since none of our children now claim us as a parent.
   */
  num_destroyed = 0;
  while (b->num_parents != 0)
    {
      --b->num_parents;
      num_destroyed += destroy_block (b->parent[b->num_parents]);
    }

  range_tree_remove (b);
  hash_remove (b);

  /* Maintain current_block_in_death_queue so we can safely traverse the
   * death queue destroying everyone.  Otherwise, we'd lose our place in the
   * queue when we destroyed a bunch of blocks.
   */
  if (b == current_block_in_death_queue)
    current_block_in_death_queue = b->death_queue_next;

  /* Remove this block from the death queue. */
  assert (death_queue_head != NULL);
  death_queue_dequeue (b);

  block_free (b);
  return num_destroyed + 1;  /* Account for the one we just freed. */
}


/* This routine calls destroy_block() for all blocks which came from m68k
 * code intersecting the specified range of addresses.  Returns the total
 * number of blocks destroyed.
 */
#ifdef CHECKSUM_BLOCKS
static unsigned long
destroy_blocks_maybe_checksum (syn68k_addr_t low_m68k_address,
			       uint32 num_bytes, BOOL checksum_blocks)
#else
static unsigned long
destroy_blocks (syn68k_addr_t low_m68k_address, uint32 num_bytes)
#endif
{
  Block *b;
  int old_sigmask;
  unsigned long total_destroyed;

  BLOCK_INTERRUPTS (old_sigmask);

  total_destroyed = 0;

  /* Special case destroying all blocks.  This happens often in HyperCard. */
  if (low_m68k_address == 0 && num_bytes == (uint32)~0)
    {
      if (!checksum_blocks)
	{
	  for (b = death_queue_head; b != NULL; )
	    {
	      unsigned long ndest;
	      current_block_in_death_queue = b;
	      ndest = destroy_block (b);
	      if (ndest != 0)
		{
		  total_destroyed += ndest;
		  b = current_block_in_death_queue;  /* Has advanced. */
		}
	      else
		b = b->death_queue_next;
	    }
	}
      else
	{
	  for (b = death_queue_head; b != NULL; )
	    {
	      if (b->checksum != inline_compute_block_checksum (b))
		{
		  unsigned long ndest;
		  current_block_in_death_queue = b;
		  ndest = destroy_block (b);
		  if (ndest != 0)
		    {
		      total_destroyed += ndest;
		      b = current_block_in_death_queue;  /* Has advanced. */
		    }
		  else
		    b = b->death_queue_next;
		}
	      else
		b = b->death_queue_next;
	    }
	}
    }
  else  /* Destroy only selected range. */
    {
      syn68k_addr_t end = low_m68k_address + num_bytes - 1;
      syn68k_addr_t next_addr;

      /* Loop over blocks in the specified range and destroy them. */
      for (b = range_tree_first_to_intersect (low_m68k_address, end);
	   b != NULL && b->m68k_start_address <= end;
	   b = range_tree_find_first_at_or_after (next_addr))
	{
	  next_addr = b->m68k_start_address + 1;

#ifdef CHECKSUM_BLOCKS
	  if (!checksum_blocks
	      || b->checksum != inline_compute_block_checksum (b))
#endif  /* CHECKSUM_BLOCKS */
	    {
	      total_destroyed += destroy_block (b);
	    }
	}
    }

  /* Smash the jsr stack if we destroyed any blocks. */
  if (total_destroyed > 0)
    memset (&cpu_state.jsr_stack, -1, sizeof cpu_state.jsr_stack);

  RESTORE_INTERRUPTS (old_sigmask);

  /* Call the user-defined function to let them know we're not busy. */
  if (call_while_busy_func != NULL)
    call_while_busy_func (0);

  return total_destroyed;
}


#ifdef CHECKSUM_BLOCKS
/* Always destroys blocks in the specified range. */
unsigned long
destroy_blocks (syn68k_addr_t low_m68k_address, uint32 num_bytes)
{
  return destroy_blocks_maybe_checksum (low_m68k_address, num_bytes, FALSE);
}

/* Only destroys blocks in the specified range if their m68k checksums fail. */
unsigned long
destroy_blocks_with_checksum_mismatch (syn68k_addr_t low_m68k_address,
				       uint32 num_bytes)
{
  return destroy_blocks_maybe_checksum (low_m68k_address, num_bytes, TRUE);
}
#endif  /* CHECKSUM_BLOCKS */


static BOOL
immortal_self_or_ancestor_aux (Block *b)
{
  int i;

  if (!b->recursive_mark)
    {
      if (b->immortal)
	return TRUE;
      b->recursive_mark = TRUE;
      for (i = b->num_parents - 1; i >= 0; i--)
	if (immortal_self_or_ancestor_aux (b->parent[i]))
	  return TRUE;
    }

  return FALSE;
}


static void
clear_recursive_marks (Block *b)
{
  int i;

  if (!b->recursive_mark)
    return;
  b->recursive_mark = FALSE;
  for (i = b->num_parents - 1; i >= 0; i--)
    clear_recursive_marks (b->parent[i]);
}


static BOOL
immortal_self_or_ancestor (Block *b)
{
  BOOL val = immortal_self_or_ancestor_aux (b);
  clear_recursive_marks (b);
  return val;
}


/* Call this only when you are out of memory.  This routine selects
 * one or more blocks to destroy and destroys them, freeing up their
 * memory.  Returns the number of blocks actually freed.  Can only
 * destroy blocks that are not immortal and have no immortal
 * ancestors, and will destroy the oldest such block (and all of its
 * ancestors) that it finds.
 */
unsigned long
destroy_any_block ()
{
  Block *kill;

  /* Find a block to slaughter.  Prefer someone with no parents*/
  for (kill = death_queue_head; kill != NULL; kill = kill->death_queue_next)
    if (!immortal_self_or_ancestor (kill))
      break;

  if (kill != NULL)
    {
      unsigned long num_destroyed;

      num_destroyed = destroy_block (kill);
      assert (num_destroyed != 0);

      memset (&cpu_state.jsr_stack, -1, sizeof cpu_state.jsr_stack);

      /* Call the user-defined function to let them know we're not busy. */
      if (call_while_busy_func != NULL)
	call_while_busy_func (0);

      return num_destroyed;
    }

  return 0;
}
