#include "config.h"

#include "syn68k_private.h"
#include "backpatch.h"
#include "alloc.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>


void
backpatch_apply_and_free (Block *b, backpatch_t *p)
{
  ptr_sized_uint value;
  uint32 first_byte;
  int length;
  BOOL found_p;
  backpatch_t **bp;
  char *base;

  /* Find the backpatch in the block's list and remove it. */
  for (bp = &b->backpatch, found_p = FALSE; *bp != NULL; bp = &(*bp)->next)
    if (*bp == p)
      {
	*bp = p->next;
	found_p = TRUE;
	break;
      }
  
#ifdef DEBUG
  /* Make sure we actually found it in the list. */
  if (!found_p)
    abort ();
#endif

  if (p->target == NULL)
    base = (char *)0;
  else
    base = (char *)p->target->compiled_code;

  /* First, compute the value to write out to memory. */
  first_byte = p->offset_location / 8;
  length = p->num_bits;
  if (p->relative_p)
    {
      value = ((base + p->const_offset)
	       - ((char *)b->compiled_code + first_byte));
    }
  else
    {
      value = ((ptr_sized_uint)(base + p->const_offset));
    }  
#ifndef QUADALIGN
  if ((p->offset_location & 7) == 0)
    {
      if (length == 32)
	*(uint32 *)((char *)b->compiled_code + first_byte) = value;
      else if (length == 16)
	*(uint16 *)((char *)b->compiled_code + first_byte) = value;
      else if (length == 8)
	*(uint8 *)((char *)b->compiled_code + first_byte) = value;
#if SIZEOF_CHAR_P == 8
      else if (length == 64)
	*(uint64 *)((char *)b->compiled_code + first_byte) = value;
#endif
      else
	abort ();
    }
  else
    {
      abort ();
    }
#else  /* QUADALIGN */
  /* The whole guy had better fit in one long! */
  if ((p->offset_location & ~31)
      == ((p->offset_location + length - 1) & ~31))
    {
      uint32 *ptr = (uint32 *)((char *)b->compiled_code + first_byte);
      if (length == 32)
	*ptr = value;
      else
	{
	  int offset = p->offset_location & 31;
#error "Write this case"
#ifdef LITTLEENDIAN
	  *ptr = (*ptr & ) | ;
#else  /* !LITTLEENDIAN */
	  *ptr = (*ptr & ) | ;
#endif /* !LITTLEENDIAN */
	}
    }
  else
    abort ();
#endif  /* QUADALIGN */

  free (p);
}


void
backpatch_add (Block *b, int offset_location, int num_bits, BOOL relative_p,
	       int const_offset, Block *target)
{
  backpatch_t *p;

#ifdef DEBUG
  if (num_bits == 0)
    abort ();
#endif

  p = (backpatch_t *) xmalloc (sizeof *p);
  p->next            = b->backpatch;
  p->offset_location = offset_location;
  p->num_bits        = num_bits;
  p->relative_p      = relative_p;
  p->const_offset    = const_offset;
  p->target          = target;

  /* Prepend this guy to the block's list. */
  b->backpatch = p;
}
