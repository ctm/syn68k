#include "callback.h"
#include "rangetree.h"
#include "block.h"
#include "alloc.h"
#include "hash.h"
#include "destroyblock.h"
#include "deathqueue.h"
#include "checksum.h"
#include "translate.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  callback_handler_t func;
  void *arg;
} CallBackInfo;

CallBackInfo *callback;
int num_callback_slots;
int lowest_free_callback_slot;

/* We just use this array to reserve some dereferenceable address
 * space to hold the callbacks, because we need memory locations we
 * know can never contain 68k code.  We don't actually store anything
 * there.
 */
uint16 callback_dummy_address_space[MAX_CALLBACKS + CALLBACK_SLOP];

void
callback_init (void)
{
  callback = NULL;
  num_callback_slots = 0;
  lowest_free_callback_slot = 0;
}


/* Installs a given callback function at a given 68k address.  If the 68k
 * ever executes code at that address, the callback function will be called
 * and provided with the 68k address of the callback and an arbitrary
 * 32 bit argument.  If some block is already present at the specified
 * address, does nothing and returns 0.  Otherwise, returns 1.
 */
int
callback_compile (Block *parent, syn68k_addr_t m68k_address, Block **new)
{
  Block *b;
  uint16 *code;
  uint32 ix = (m68k_address - CALLBACK_STUB_BASE) / sizeof (uint16);
  const CallBackInfo *callback_info = &callback[ix];

  /* Make sure a callback is actually installed. */
  if (ix >= num_callback_slots || callback_info->func == NULL)
    {
      *new = NULL;
      return 0;
    }

  b = *new = make_artificial_block (parent, m68k_address,
				    OPCODE_WORDS + PTR_WORDS + PTR_WORDS + 2,
				    &code);

  /* Create the synthetic code for the callback. */
#ifdef USE_DIRECT_DISPATCH
  *(const void **)code = direct_dispatch_table[0xB3]; /* callback opcode. */
#else
  *(void **)code = (void *)0xB3;  /* Magical callback synthetic opcode. */
#endif
  *((callback_handler_t *) (code + OPCODE_WORDS)) = callback_info->func;
  *(void **)(code + OPCODE_WORDS + PTR_WORDS)     = callback_info->arg;
  *(syn68k_addr_t *)(code + OPCODE_WORDS + PTR_WORDS + PTR_WORDS)
    = m68k_address;

  /* Insert block into the universe of blocks. */
  hash_insert (b);
  range_tree_insert (b);

  /* Add this block to the end of the death queue. */
  death_queue_enqueue (b);

  b->immortal = FALSE;

  return ALL_CCS;
}


/* Installs a callback stub in the 68k space and returns the 68k address
 * it chose for this stub.  Any 68k code that hits this stub will call
 * the specified callback function.  func must never be NULL.
 */
syn68k_addr_t
callback_install (callback_handler_t func, void *arbitrary_argument)
{
  uint32 slot;
  int old_sigmask;

  BLOCK_INTERRUPTS (old_sigmask);
  slot = lowest_free_callback_slot;

  if (lowest_free_callback_slot >= num_callback_slots)
    {
      if (num_callback_slots >= MAX_CALLBACKS)
	{
	  fprintf (stderr, "Internal error: syn68k out of callback slots!\n");
	  abort ();
	}

      ++num_callback_slots;
      callback = (CallBackInfo *) xrealloc (callback, (num_callback_slots
						       * sizeof callback[0]));
    }

  /* Remember the callback they are specifying. */
  callback[slot].func = func;
  callback[slot].arg  = arbitrary_argument;

  /* Move lowest free callback slot to next lowest slot. */
  for (lowest_free_callback_slot++; ; lowest_free_callback_slot++)
    if (lowest_free_callback_slot >= num_callback_slots
	|| callback[lowest_free_callback_slot].func == NULL)
      break;

  /* Reenable interrupts. */
  RESTORE_INTERRUPTS (old_sigmask);

  return CALLBACK_STUB_BASE + (slot * sizeof (uint16));
}


void *
callback_argument (syn68k_addr_t callback_address)
{
  uint32 ix = (callback_address - CALLBACK_STUB_BASE) / sizeof (uint16);

  if (ix >= num_callback_slots)
    return NULL;
  return callback[ix].arg;
}


callback_handler_t
callback_function (syn68k_addr_t callback_address)
{
  uint32 ix = (callback_address - CALLBACK_STUB_BASE) / sizeof (uint16);

  if (ix >= num_callback_slots)
    return NULL;
  return callback[ix].func;
}


void
callback_remove (syn68k_addr_t m68k_address)
{
  uint32 ix;
  Block *b;
  int old_sigmask;

  BLOCK_INTERRUPTS (old_sigmask);

  /* Fetch the block at that address. */
  b = hash_lookup (m68k_address);
  if (b != NULL)
    destroy_block (b);

  ix = (m68k_address - CALLBACK_STUB_BASE) / sizeof (uint16);
  if (ix < num_callback_slots)
    {
      if (ix == num_callback_slots - 1)
	{
	  --num_callback_slots;
	  callback = (CallBackInfo *) xrealloc (callback,
						(num_callback_slots
						 * sizeof callback[0]));
	}
      else
	{
	  callback[ix].func = NULL;
	  if (ix < lowest_free_callback_slot)
	    lowest_free_callback_slot = ix;
	}
    }

  RESTORE_INTERRUPTS (old_sigmask);
}
