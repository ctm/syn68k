/*
 * hash.c - Routines for manipulating a hash table that maps 68k addresses
 *          to the basic block that begins at that address.  This exists
 *          in addition to the range tree because runtime lookups are much
 *          faster.
 */

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include "hash.h"
#include "alloc.h"
#include "translate.h"


/* Fixed size hash table, indexed by BLOCK_HASH (block->m68k_start_address). */
Block *block_hash_table[NUM_HASH_BUCKETS];


/* Initializes the hash table.  Call this before calling any other hash
 * functions, and call it exactly once.
 */
void
hash_init ()
{
  memset (block_hash_table, 0, sizeof block_hash_table);
}


/* Removes all blocks from the hash table and deallocates all space used by
 * the hash table.
 */
void
hash_destroy ()
{
  Block *b, *next;
  int i;

  /* Remove all Blocks from the hash table. */
  for (i = 0; i < NUM_HASH_BUCKETS; i++)
    for (b = block_hash_table[i]; b != NULL; b = next)
      {
	next = b->next_in_hash_bucket;
	b->next_in_hash_bucket = NULL;
      }

  memset (block_hash_table, 0, sizeof block_hash_table);
}


/* Finds the block associated with a given 68k address and returns a pointer
 * to that block.  Returns NULL iff no block starting at that address exists.
 * Note that it is possible the address specified may fall inside a block of
 * compiled code but not at the beginning; this routine will not detect that
 * situation.  If you want to know what block contains a given address when
 * that address may not refer to the first byte of a block, call the slower
 * range_tree_lookup (addr) function found in rangetree.c
 */
Block *
hash_lookup (syn68k_addr_t addr)
{
  Block *bucket = block_hash_table[BLOCK_HASH (addr)];

  for (; bucket != NULL; bucket = bucket->next_in_hash_bucket)
    if (bucket->m68k_start_address == addr)
      return bucket;

  return NULL;
}


/* Similar to hash_lookup(), but this returns a pointer to the compiled
 * code associated with a given 68k address instead of to the block associated
 * with that code.  If no compiled code exists for that block, code is
 * compiled for that block and a pointer to the new code is returned.  This
 * function is guaranteed to return a non-NULL pointer to valid code.
 */
const uint16 *
hash_lookup_code_and_create_if_needed (syn68k_addr_t addr)
{
  Block *b, *bucket, **bucket_ptr;
  int old_sigmask;

  bucket_ptr = &block_hash_table[BLOCK_HASH (addr)];
  bucket = *bucket_ptr;

  /* If there's anything in this bucket, check for a match. */
  if (bucket != NULL)
    {
      Block *prev, *next;

      /* See if we get a match in the first element. */
      if (bucket->m68k_start_address == addr)
	return bucket->compiled_code;

      /* See if we get a match in a later element.  If we do, move it
       * to the head of the list, since it is likely to be referenced
       * again.
       */
      for (prev = bucket; (next = prev->next_in_hash_bucket) != NULL;
	   prev = next)
	{
	  if (next->m68k_start_address == addr)
	    {
	      BLOCK_INTERRUPTS (old_sigmask);
	      prev->next_in_hash_bucket = next->next_in_hash_bucket;
	      next->next_in_hash_bucket = bucket;
	      *bucket_ptr = next;
	      RESTORE_INTERRUPTS (old_sigmask);
	      return next->compiled_code;
	    }
	}
    }

  BLOCK_INTERRUPTS (old_sigmask);
  generate_block (NULL, addr, &b, FALSE);
  RESTORE_INTERRUPTS (old_sigmask);

  /* Call the user-defined function to let them know we're done. */
  if (call_while_busy_func != NULL)
    call_while_busy_func (0);

  return b->compiled_code;
}


/* Inserts a given block into the hash table based on it's m68k_start_address.
 * Inserting a block twice is illegal but is not checked for.
 */
void
hash_insert (Block *b)
{
  uint32 addr = b->m68k_start_address;
  Block **bucket = &block_hash_table[BLOCK_HASH (addr)];

  /* Prepend this block to the beginning of the list. */
  b->next_in_hash_bucket = *bucket;
  *bucket = b;
}


/* Removes a given block from the hash table.  If it's not in the hash
 * table, no action is taken.
 */
void
hash_remove (Block *b)
{
  Block **bucket = &block_hash_table[BLOCK_HASH (b->m68k_start_address)];
  
  for (; *bucket != NULL; bucket = &(*bucket)->next_in_hash_bucket)
    if (*bucket == b)
      {
	*bucket = b->next_in_hash_bucket;
	break;
      }
}



#ifdef DEBUG
/* Runs through and checks the hash table for consistency.  Returns YES
 * if everything is OK, NO if something is bad (in which case it prints
 * out appropriate errors to stderr.)
 */
BOOL
hash_verify ()
{
  BOOL ok = YES;
  int i;
  Block *b, *b2;

  for (i = 0; i < NUM_HASH_BUCKETS; i++)
    for (b = block_hash_table[i]; b != NULL; b = b->next_in_hash_bucket)
      {
	if (!block_verify (b))
	  ok = NO;
	if (BLOCK_HASH (b->m68k_start_address) != i)
	  {
	    fprintf (stderr, "Internal inconsistency: Block 0x%lX seems to "
		     "have ended up in hash bucket %d instead of hash "
		     "bucket %d where it belongs.\n",
		     b->m68k_start_address, i,
		     BLOCK_HASH (b->m68k_start_address));
	    ok = NO;
	  }

	for (b2 = b->next_in_hash_bucket; b2; b2 = b2->next_in_hash_bucket)
	  if (b->m68k_start_address == b2->m68k_start_address)
	    {
	      fprintf (stderr, "Internal inconsistency: More than one block "
		       "in the hash table has the same m68k_start_address "
		       "(0x%lX).\n", b->m68k_start_address);
	      ok = NO;
	      break;
	    }
      }

  return ok;
}
#endif


#ifdef DEBUG
void
hash_stats ()
{
  int i, max = 0, sum = 0;

  for (i = 0; i < NUM_HASH_BUCKETS; i++)
    {
      int n;
      Block *b;

      for (n = 0, b = block_hash_table[i]; b != NULL; n++,
	   b = b->next_in_hash_bucket);
      if (n > max)
	max = n;
      sum += n;
    }

  printf ("Hash stats: %d entries, %.2f average per bucket, %d in deepest "
	  "bucket.\n",
	  sum, (double) sum / NUM_HASH_BUCKETS, max);
}

#endif
