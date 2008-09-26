#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include "block.h"
#include "alloc.h"
#include "diagnostics.h"
#include "deathqueue.h"
#include "checksum.h"


static Block *free_blocks = NULL;


/* Returns a new, empty Block.  All fields of the block are initialized to
 * zero.  If DEBUG is #define'd, the magic field will be set to
 * BLOCK_MAGIC_VALUE.
 */
Block *
block_new ()
{
  Block *b;

  if (free_blocks != NULL)
    {
      b = free_blocks;
      free_blocks = b->child[0];
    }
  else
    {
#define BLOCK_CHUNK_SIZE (16384 / sizeof (Block))
      static Block *block_chunk;
      static int block_chunk_index = BLOCK_CHUNK_SIZE;

      if (block_chunk_index >= BLOCK_CHUNK_SIZE)
	{
	  block_chunk = xmalloc (BLOCK_CHUNK_SIZE * sizeof (Block));
	  block_chunk_index = 0;
	}

      b = &block_chunk[block_chunk_index++];
    }

  memset (b, 0, sizeof *b);

#ifdef DEBUG
  b->magic = BLOCK_MAGIC_VALUE;
#endif

  return b;
}


/* Free the block and all storage associated with it. */
void
block_free (Block *b)
{
  free (b->parent);
  if (b->compiled_code != NULL)  /* Avoid freeing -2 or anything. */
    free ((void *) (b->compiled_code - b->malloc_code_offset));

  /* Prepend this block to the linked list of free ones. */
  b->child[0] = free_blocks;
  free_blocks = b;
}


/* Adds PARENT to B's parent list.  If PARENT is already a member of B's
 * parent list, no action is taken.
 */
void
block_add_parent (Block *b, Block *parent)
{
  int i;
  BOOL immortal;
  
  for (i = b->num_parents - 1; i >= 0; i--)
    if (b->parent[i] == parent)
      return;

  ++b->num_parents;

  /* Protect this block from getting nuked if the realloc runs out of memory.*/
  immortal = b->immortal;
  b->immortal = TRUE;

  b->parent = (Block **)xrealloc (b->parent,b->num_parents * sizeof (Block *));
  b->parent[b->num_parents - 1] = parent;

  /* Restore immortality state. */
  b->immortal = immortal;
}


/* Removes PARENT from B's parent list.  If PARENT is not already a member
 * of B's parent list, no action is taken.  If RECLAIM_MEMORY is true, then
 * the parents list will be realloc'd to take as little memory as possible.
 */
void
block_remove_parent (Block *b, Block *parent, BOOL reclaim_memory)
{
  int i;

  for (i = b->num_parents - 1; i >= 0; i--)
    if (b->parent[i] == parent)
      {
	--b->num_parents;
	b->parent[i] = b->parent[b->num_parents];
	if (reclaim_memory)
	  {
	    /* Protect this block from getting nuked by xrealloc. */
	    BOOL immortal = b->immortal;
	    b->immortal = TRUE;
	    b->parent = (Block **) xrealloc (b->parent, (b->num_parents
							 * sizeof (Block *)));
	    b->immortal = immortal;
	  }
	break;
      }
}

/* Adds CHILD to B's child list.  If CHILD is already a member of B's
 * child list, no action will be taken.  Note that it is illegal to ever
 * have more than two children at a time.
 */
void
block_add_child (Block *b, Block *child)
{
#ifdef DEBUG
  /* Make sure they aren't trying to add a third child. */
  assert (b->num_children <= 1);
#endif

  b->child[b->num_children] = child;
  ++b->num_children;
}


/* Removes CHILD from B's child list.  If CHILD is not a member of B's
 * child list, no action will be taken.
 */
void
block_remove_child (Block *b, Block *child)
{
  if (b->num_children != 0 && b->child[0] == child)
    b->child[0] = b->child[1];
  else if (b->num_children != 2 || b->child[1] != child)
    return;
  --b->num_children;
  b->child[b->num_children] = NULL;   /* Not strictly necessary. */
}


/* Applies a function F to each of B's parents.  This function will receive
 * a pointer to the parent block and AUX, which you can specify.
 */
void
block_do_for_each_parent (Block *b, void (*f)(Block *, void *), void *aux)
{
  int i;

  for (i = 0; i < b->num_parents; i++)
    f (b->parent[i], aux);
}


#ifdef DEBUG
/* Debugging function, prints out all blocks whose checksums have changed
 * and their addresses.
 */
unsigned long
block_changed_checksum ()
{
  Block *b;
  unsigned long num_diff;

  num_diff = 0;
  for (b = death_queue_head; b != NULL; b = b->death_queue_next)
    if (compute_block_checksum (b) != b->checksum)
      {
	++num_diff;
	printf ("m68k code checksum differs for block [0x%lX, 0x%lX]\n",
		(unsigned long) b->m68k_start_address,
		(unsigned long) (b->m68k_start_address
				 + b->m68k_code_length - 1));
      }

  return num_diff;
}


/* Checks a block for internal consistency.  If it appears to be OK,
 * returns YES.  Else it prints out appropriate error messages to stderr
 * and returns NO.
 */
BOOL
block_verify (Block *b)
{
  BOOL ok = YES;

#ifdef DEBUG
  if (b->magic != BLOCK_MAGIC_VALUE)
    {
      fprintf (stderr, "Internal inconsistency: Block 0x%lX magic value "
	       "incorrect: 0x%lX, should be 0x%lX.\n",
	       b->m68k_start_address, b->magic, BLOCK_MAGIC_VALUE);
      ok = NO;
    }
#endif

#ifdef BLOCK_CHECKSUM
  if (b->checksum != compute_block_checksum (b))
    {
      fprintf (stderr, "m68k checksum for block 0x%lX failed; "
	       "self-modifying code?\n",
	       b->m68k_start_address);
    }
#endif

  /* Check to see if child pointers are OK. */
  switch (b->num_children) {
  case 0:
    break;
  case 1:
    if (b->child[0] == NULL)
      {
	fprintf (stderr, "Internal inconsistency: Block 0x%lX has 1 child, "
		 "child ptr 0 is NULL!\n", b->m68k_start_address);
	ok = NO;
      }
    break;
  case 2:
    if (b->child[0] == NULL || b->child[1] == NULL)
      {
	fprintf (stderr, "Internal inconsistency: Block 0x%lX has two "
		 "children (%p, %p) yet both are not non-NULL!\n",
		 b->m68k_start_address, (void *) b->child[0],
		 (void *) b->child[1]);
	ok = NO;
      }
    break;
  default:
    fprintf (stderr, "Internal inconsistency: Block 0x%lX has an illegal "
	     "# of children (%d)!\n", b->m68k_start_address, b->num_children);
    ok = NO;
    break;
  }

  {
    int i, j;
    for (i = 0; i < b->num_parents; i++)
      {
	Block *parent = b->parent[i];

	if (parent == NULL)
	  {
	    fprintf (stderr, "Internal inconsistency: Block 0x%lX has "
		     "parent[%d] == NULL!\n",
		     b->m68k_start_address, i);
	    ok = NO;
	  }
	else
	  {
	    for (j = i + 1; j < b->num_parents; j++)
	      if (parent == b->parent[j])
		{
		  fprintf (stderr, "Internal inconsistency: Block 0x%lX "
			   "has parent[%d] == parent[%d] == %p!\n",
			   b->m68k_start_address, i, j,
			   (void *) b->parent[i]);
		  ok = NO;
		}

	    /* Make sure our parent claims us as a child. */
	    for (j = parent->num_children - 1; j >= 0; j--)
	      if (parent->child[j] == b)
		break;

	    if (j < 0)
	      {
		fprintf (stderr, "Internal inconsistency: Block 0x%lX "
			 "has a parent that doesn't claims it as a child!\n",
			 b->m68k_start_address);
		ok = NO;
	      }
	  }
      }

    for (i = 0; i < b->num_children; i++)
      {
	Block *child = b->child[i];

	for (j = child->num_parents - 1; j >= 0; j--)
	  if (child->parent[j] == b)
	    break;

	if (j < 0)
	  {
	    fprintf (stderr, "Internal inconsistency: Block 0x%lX has a child "
		     "that doesn't claim it as a parent!\n",
		     b->m68k_start_address);
	    ok = NO;
	  }
      }
  }

  if (!ok)
    abort ();

  return ok;
}
#endif
