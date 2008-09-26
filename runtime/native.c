#include "syn68k_private.h"

#ifdef GENERATE_NATIVE_CODE

#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include "native.h"

#ifdef DEBUG
#include <stdio.h>
#endif

static void host_uncache_reg (cache_info_t *c, int guest_reg);


/* This is just a default template to copy whenever you need to set up
 * an empty cache.  Never modify it (except when it's initially set up,
 * of course).
 */
cache_info_t empty_cache_info;


void
native_code_init ()
{
  int i;

  /* Set up an empty cache info struct that we can just copy whenever
   * we need to set up an empty cache.
   */
  memset (&empty_cache_info, 0, sizeof empty_cache_info);
  for (i = 0; i < NUM_GUEST_REGS; i++)
    {
      guest_reg_status_t *r = &empty_cache_info.guest_reg_status[i];
      r->host_reg = NO_REG;
      r->mapping  = MAP_NATIVE;
      r->dirty_without_offset_p = FALSE;
      r->offset = 0;
    }
  for (i = 0; i < NUM_HOST_REGS; i++)
    empty_cache_info.host_reg_to_guest_reg[i] = NO_REG;
  empty_cache_info.cached_cc = M68K_CC_NONE;
  empty_cache_info.dirty_cc = M68K_CC_NONE;

  host_native_code_init ();
}


#ifdef DEBUG
static void
verify_cache_consistency (const cache_info_t *c)
{
  int h, g;

  for (h = 0; h < NUM_HOST_REGS; h++)
    {
      g = c->host_reg_to_guest_reg[h];
      if (g != NO_REG)
	if (c->guest_reg_status[g].host_reg != h)
	  {
	    fprintf (stderr, "Internal error!  register/cc cache internally "
		     "inconsistent.  Host reg %d claims to be in guest reg "
		     "%d, but guest reg %d claims to be cached in host reg "
		     "%d.  Wedging.\n",
		     h, g, g, c->guest_reg_status[g].host_reg);
	    while (1)
	      ;
	  }
    }

  for (g = 0; g < NUM_GUEST_REGS; g++)
    {
      h = c->guest_reg_status[g].host_reg;
      if (h != NO_REG)
	if (c->host_reg_to_guest_reg[h] != g)
	  {
	    fprintf (stderr, "Internal error!  register/cc cache internally "
		     "inconsistent.  Guest reg %d claims to be cached in "
		     "host reg %d, but host reg %d claims to be cacheing "
		     "guest reg %d.  Wedging.\n",
		     g, h, h, c->host_reg_to_guest_reg[h]);
	    while (1)
	      ;
	  }
    }

#ifdef i386
  /* Make sure data registers only go into byte-addressable host regs. */
  for (g = 0; g < 8; g++)
    {
      h = c->guest_reg_status[g].host_reg;
      if (h != NO_REG && !((1L << h) & REGSET_BYTE))
	{
	  fprintf (stderr, "Internal error!  Put data register d%d into "
		   "a non-byte accessible host register (%d).  Wedging.\n",
		   g, h);
	  while (1)
	    ;
	}
    }
#endif  /* i386 */
}
#endif  /* DEBUG */


int
host_setup_cached_reg (COMMON_ARGS, int guest_reg,
		       unsigned acceptable_mappings,
		       host_reg_mask_t acceptable_regs)
{
  guest_reg_status_t *r;
  int host_reg;

  r = &c->guest_reg_status[guest_reg];
  host_reg = r->host_reg;

  if (host_reg == NO_REG)
    {
      host_reg = host_alloc_reg (c, codep, cc_spill_if_changed,
				 acceptable_regs);
      if (host_reg == NO_REG)
	return NO_REG;
      host_cache_reg (c, codep, cc_spill_if_changed, guest_reg, host_reg);
    }
  else if (!((1L << host_reg) & acceptable_regs))
    {
      int new_reg;

      /* If they are demanding a register other than the one it's in,
       * move it to an acceptable one.
       */
      new_reg = host_alloc_reg (c, codep, cc_spill_if_changed,
				acceptable_regs);
      if (new_reg == NO_REG)
	return NO_REG;
      if (host_movel_reg_reg (COMMON_ARG_NAMES, host_reg, new_reg))
	return NO_REG;

      r->host_reg = new_reg;
      c->host_reg_to_guest_reg[host_reg] = NO_REG;
      c->host_reg_to_guest_reg[new_reg] = guest_reg;
      host_reg = new_reg;
    }

  /* If the register's mapping is already acceptable, then we're done. */
  if ((1L << r->mapping) & acceptable_mappings)
    return host_reg;

  switch (r->mapping)
    {
    case MAP_NATIVE:
      if (acceptable_mappings & MAP_OFFSET_MASK)
	{
	  r->offset = 0;
	  r->mapping = MAP_OFFSET;
	}
#ifdef LITTLEENDIAN
      else if (acceptable_mappings & MAP_SWAP16_MASK)
	{
	  host_swap16 (c, codep, cc_spill_if_changed, 
		       M68K_CC_NONE, NO_REG, host_reg);
	  r->mapping = MAP_SWAP16;
	}
      else if (acceptable_mappings & MAP_SWAP32_MASK)
	{
	  host_swap32 (c, codep, cc_spill_if_changed, 
		       M68K_CC_NONE, NO_REG, host_reg);
	  r->mapping = MAP_SWAP32;
	}
#endif /* LITTLEENDIAN */
      else
	return NO_REG;
      break;
    case MAP_OFFSET:
      if (acceptable_mappings
	  & (MAP_NATIVE_MASK
#ifdef LITTLEENDIAN
	     | MAP_SWAP16_MASK | MAP_SWAP32_MASK
	     ))
#endif /* LITTLEENDIAN */
	{
	  host_unoffset_reg (c, codep, cc_spill_if_changed,
			     guest_reg);
	  if (acceptable_mappings & MAP_NATIVE_MASK)
	    r->mapping = MAP_NATIVE;
#ifdef LITTLEENDIAN
	  else if (acceptable_mappings & MAP_SWAP16_MASK)
	    {
	      host_swap16 (c, codep, cc_spill_if_changed, 
			   M68K_CC_NONE, NO_REG, host_reg);
	      r->mapping = MAP_SWAP16;
	    }
	  else	/* if (acceptable_mappings & MAP_SWAP32_MASK) */
	    {
	      host_swap32 (c, codep, cc_spill_if_changed, 
			   M68K_CC_NONE, NO_REG, host_reg);
	      r->mapping = MAP_SWAP32;
	    }
#endif /* LITTLEENDIAN */
	}
      else
	return NO_REG;
      break;
#ifdef LITTLEENDIAN
    case MAP_SWAP16:
      if (acceptable_mappings & MAP_NATIVE_MASK)
	{
	  host_swap16 (c, codep, cc_spill_if_changed, 
		       M68K_CC_NONE, NO_REG, host_reg);
	  r->mapping = MAP_NATIVE;
	}
      else if (acceptable_mappings & MAP_OFFSET_MASK)
	{
	  host_swap16 (c, codep, cc_spill_if_changed,
		       M68K_CC_NONE, NO_REG, host_reg);
	  r->offset = 0;
	  r->mapping = MAP_OFFSET;
	}
      else if (acceptable_mappings & MAP_SWAP32_MASK)
	{
	  host_swap16_to_32 (c, codep, cc_spill_if_changed, 
			     M68K_CC_NONE, NO_REG, host_reg);
	  r->mapping = MAP_SWAP32;
	}
      else
	return NO_REG;
      break;
    case MAP_SWAP32:
      if (acceptable_mappings & MAP_NATIVE_MASK)
	{
	  host_swap32 (c, codep, cc_spill_if_changed, 
		       M68K_CC_NONE, NO_REG, host_reg);
	  r->mapping = MAP_NATIVE;
	}
      else if (acceptable_mappings & MAP_OFFSET_MASK)
	{
	  host_swap32 (c, codep, cc_spill_if_changed, 
		       M68K_CC_NONE, NO_REG, host_reg);
	  r->offset = 0;
	  r->mapping = MAP_OFFSET;
	}
      else if (acceptable_mappings & MAP_SWAP16_MASK)
	{
	  host_swap32_to_16 (c, codep, cc_spill_if_changed, 
			     M68K_CC_NONE, NO_REG, host_reg);
	  r->mapping = MAP_SWAP16;
	}
      else
	return NO_REG;
      break;
#endif /* LITTLEENDIAN */
    default:
      abort ();
    }

  return host_reg;
}


#ifdef GCD_RANGE
#warning "Temp debugging stuff!"
const guest_code_descriptor_t *lowest_gcd = 0;
const guest_code_descriptor_t *highest_gcd = (guest_code_descriptor_t *)~0;
#endif

/* Generates native code for the specified list of guest_code_descriptor_t's
 * at the address pointed to by *code if possible.  If successful, returns
 * TRUE and updates *code to point just past the end of the newly generated
 * code.  Otherwise, returns FALSE and *code is unaffected.  cc_to_compute
 * should always be a subset of cc_live.
 */
BOOL
generate_native_code (const guest_code_descriptor_t *first_desc,
		      cache_info_t *cache_info,
		      const int32 *orig_operands,
		      host_code_t **code,
		      uint8 cc_input, uint8 cc_live, uint8 cc_to_compute,
		      BOOL ends_block_p,
		      Block *block,
		      const uint16 *m68k_code)
{
  const guest_code_descriptor_t *desc;
  const reg_operand_info_t *op;
  host_code_t *orig_code;
  cache_info_t orig_cache_info;
  guest_reg_status_t *r;
  int i, guest_reg, host_reg, scratch_reg;
  unsigned cc_dont_touch, cc_dont_touch_during_setup;
  backpatch_t *orig_backpatch, *bp, *bp_next, *old_first_backpatch;

#ifdef GCD_RANGE
#warning "Temp debugging stuff!"
  if (first_desc < lowest_gcd || first_desc > highest_gcd)
    return 0;
#endif

  /* Save these away in case we realize a mistake and we have to start over. */
  orig_code = *code;
  orig_cache_info = *cache_info;
  orig_backpatch = block->backpatch;
  block->backpatch = NULL;

#ifdef DEBUG
      verify_cache_consistency (cache_info);
#endif

  /* Compute those cc bits we are not allowed to modify (without spilling). */
  cc_dont_touch = cc_live & cache_info->cached_cc & ~cc_to_compute;
  cc_dont_touch_during_setup = ((cc_dont_touch | cc_input)
				& cache_info->cached_cc);

  /* See if we find any match in the list of code descriptors, and fill
   * in the operands array.
   */
  for (desc = first_desc; desc != NULL; desc = desc->next)
    {
      int32 operands[MAX_BITFIELDS], canonical_operands[MAX_BITFIELDS];
      host_reg_mask_t locked_host_reg;
      uint8 acceptable_mapping[NUM_GUEST_REGS];

      /* If insufficient cc bits are cached, bail out quickly. */
      if ((cache_info->cached_cc & desc->cc_in) != desc->cc_in)
	goto failure_no_copy;

      /* If this guy doesn't compute enough cc bits, bail out quickly. */
      if ((cc_to_compute & desc->cc_out) != cc_to_compute)
	goto failure_no_copy;

      /* Create the "canonical" set of operands to use.  Right now this
       * means just adding 8 to address register operands.
       */
      memcpy (canonical_operands, orig_operands, sizeof canonical_operands);
      for (op = desc->reg_op_info; op->legitimate_p; op++)
	{
	  if (op->add8_p)
	    {
	      int opnum = op->operand_num;
	      canonical_operands[opnum] = (orig_operands[opnum] & 7) + 8;
	    }
	}

      /* Figure out exactly which mappings are acceptable for every
       * guest register mentioned.  If one of them has no acceptable
       * mapping, bail out.
       */
      memset (acceptable_mapping, MAP_ALL_MASK, sizeof acceptable_mapping);
      for (op = desc->reg_op_info; op->legitimate_p; op++)
	{
	  int guest_reg = canonical_operands[op->operand_num];
	  assert (guest_reg >= 0 && guest_reg < NUM_GUEST_REGS);
	  acceptable_mapping[guest_reg] &= op->acceptable_mapping;
	  if (acceptable_mapping[guest_reg] == 0)
	    goto failure;
	}

      /* Default to new operands being the same as the old operands. */
      memcpy (operands, canonical_operands, sizeof operands);
      
      locked_host_reg = (host_reg_mask_t)~ALLOCATABLE_REG_MASK;

#if 0
/* I think this is causing extra unoffsetting. */

      /* If this instruction actually computes live cc bits, un-offset
       * registers NOW.  Otherwise, we might be forced to un-offset the
       * registers between a compare and a conditional branch (for example),
       * and the process of offsetting is less efficient if we can't
       * step on cc bits.
       */
      if (cc_live & cc_to_compute & desc->cc_out)
	host_unoffset_regs (cache_info, code, cc_dont_touch);
#endif

      /* Cache registers as necessary and with the appropriate byte order. */
      for (op = desc->reg_op_info; op->legitimate_p; op++)
	{
	  unsigned acceptable;

	  guest_reg = canonical_operands[op->operand_num];
	  assert (guest_reg >= 0 && guest_reg < NUM_GUEST_REGS);
	  r = &cache_info->guest_reg_status[guest_reg];
	  host_reg = r->host_reg;

	  switch (op->request_type)
	    {
	    case REQUEST_REG:
	      acceptable = acceptable_mapping[guest_reg];

	      /* Allocate a register, if necessary. */
	      if (host_reg == NO_REG)
		{
		  host_reg = host_alloc_reg (cache_info, code,
					     cc_dont_touch_during_setup,
					     op->regset & ~locked_host_reg);
		  if (host_reg == NO_REG)
		    goto failure;
		  operands[op->operand_num] = host_reg;
		  host_cache_reg (cache_info, code, cc_dont_touch_during_setup,
				  guest_reg, host_reg);
		}

	      /* Map to an acceptable byte order or offset and
	       * particular register.
	       */
	      if (!((1L << r->mapping) & acceptable)
		  || !(op->regset & (1L << host_reg)))
		{
		  host_reg = host_setup_cached_reg (cache_info, code,
						    cc_dont_touch_during_setup,
						    M68K_CC_NONE, NO_REG,
						    guest_reg, acceptable,
						    (op->regset
						     & ~locked_host_reg));
		  if (host_reg == NO_REG)
		    goto failure;
		}
	      operands[op->operand_num] = host_reg;
	      locked_host_reg |= (1L << host_reg);
	      break;
	    case REQUEST_SPARE_REG:
	      /* Allocate a register, if necessary. */
	      if (host_reg == NO_REG)
		{
		  host_reg = host_alloc_reg (cache_info, code,
					     cc_dont_touch_during_setup,
					     op->regset & ~locked_host_reg);
		  /* If we failed to find one, bail out! */
		  if (host_reg == NO_REG)
		    goto failure;
		  r->host_reg = host_reg;
		  r->mapping = MAP_NATIVE;  /* default */
		  r->dirty_without_offset_p = FALSE;
		  cache_info->host_reg_to_guest_reg[host_reg] = guest_reg;
		}
	      locked_host_reg |= (1L << host_reg);
	      operands[op->operand_num] = host_reg;
	      break;
	    default:
	      abort ();
	    }
	}
      
      /* If insufficient cc bits are cached at this point, we failed. */
      if ((cache_info->cached_cc & desc->cc_in) != desc->cc_in)
	goto failure;

#ifdef LITTLEENDIAN
      /* If they are computing live cc bits, and we have swapped dirty
       * registers lying around, let's swap them back now.  That way we
       * won't be forced to swap right before a conditional branch and
       * throw away our hard-earned cc bits.  This is a heuristic; it
       * is not necessary for correct behavior.
       */
      if (cc_live & cc_to_compute & desc->cc_out)
	{
	  int hr;

	  for (hr = NUM_HOST_REGS - 1; hr >= 0; hr--)
	    if (!(locked_host_reg & (1L << hr)))
	      {
		int gr = cache_info->host_reg_to_guest_reg[hr];
		if (gr != NO_REG)
		  {
		    guest_reg_status_t *grs;
		    grs = &cache_info->guest_reg_status[gr];
		    if (grs->dirty_without_offset_p)
		      {
			if (grs->mapping == MAP_SWAP16)
			  {
			    host_swap16 (cache_info, code,
					 cc_dont_touch_during_setup,
					 M68K_CC_NONE, NO_REG, hr);
			    grs->mapping = MAP_NATIVE;
			  }
			else if (grs->mapping == MAP_SWAP32
#ifdef i386
				 && !have_i486_p  /* bswap doesn't touch ccs */
#endif
				 )
			  {
			    host_swap32 (cache_info, code,
					 cc_dont_touch_during_setup,
					 M68K_CC_NONE, NO_REG, hr);
			    grs->mapping = MAP_NATIVE;
			  }
		      }
		  }
	      }
	}
#endif  /* LITTLEENDIAN */

      /* Allocate a scratch register if one was requested. */
      if (desc->scratch_reg != 0)
	{
	  scratch_reg = host_alloc_reg (cache_info, code,
					cc_dont_touch_during_setup,
					desc->scratch_reg & ~locked_host_reg);
	  if (scratch_reg == NO_REG)
	    goto failure;
	  locked_host_reg |= 1L << scratch_reg;
	}
      else
	scratch_reg = NO_REG;

      /* Crank out the native code.  Note that we may pass in more
       * arguments than the function is expecting because our function
       * pointer can point to handlers that care about different numbers
       * of operands; technically that's invalid C, but it will work on
       * any reasonable machine.  If any of the compiler functions
       * return nonzero, we abort this compile and try something else.
       */
      old_first_backpatch = NULL;
      for (i = 0; i < MAX_COMPILE_FUNCS; i++)
	{
	  const oporder_t *order;
	  void *ops[4];
	  int j, (*f)();
	  host_code_t *code_start;
	  int32 offset;

	  f = desc->compile_func[i].func;
	  if (f == NULL)
	    break;
	  
	  /* Scramble the operands appropriately. */
	  order = &desc->compile_func[i].order;
	  for (j = 0; j < MAX_M68K_OPERANDS; j++)
	    {
	      int n = order->operand_num[j];
	      if (n >= 0)
		ops[j] = (void *)operands[n];
	      else
		{
		  switch (n)
		    {
		    case USE_SCRATCH_REG:
		      assert (scratch_reg != NO_REG);
		      ops[j] = (void *)scratch_reg;
		      break;
		    case USE_BLOCK:
		      ops[j] = (void *)block;
		      break;
		    case USE_MINUS_ONE:
		      ops[j] = (void *)-1;
		      break;
		    case USE_ZERO:
		      ops[j] = (void *)0;
		      break;
		    case USE_0xFFFF:
		      ops[j] = (void *)0xFFFF;
		      break;
		    case USE_M68K_PC:
		      ops[j] = (void *)m68k_code;
		      break;
		    case USE_M68K_PC_PLUS_TWO:
		      ops[j] = (void *)m68k_code + 2;
		      break;
		    case USE_M68K_PC_PLUS_FOUR:
		      ops[j] = (void *)m68k_code + 4;
		      break;
		    default:
		      abort ();
		    }
		}
	    }

	  code_start = *code;
	  if ((*f)(cache_info, code, cc_dont_touch, cc_to_compute,
		   scratch_reg, ops[0], ops[1], ops[2], ops[3]))
	    goto failure;

	  /* Check for new backpatches.  If we found any, adjust their
	   * starting addresses to be relative to the start of all native
	   * code generated here, rather than just the native code generated
	   * by the last function call.
	   */
	  offset = ((char *)code_start - (char *)orig_code) * 8;
	  for (bp = block->backpatch; bp != old_first_backpatch; bp = bp->next)
	    bp->offset_location += offset;
	  old_first_backpatch = block->backpatch;
	}
      
      /* Update the cache_info to reflect the new cached register status. */
      if (!ends_block_p)  /* Don't bother if the cache is dead anyway. */
	{
	  /* Update the cached register information. */
	  for (op = desc->reg_op_info; op->legitimate_p; op++)
	    {
	      guest_reg = canonical_operands[op->operand_num];
	      assert (guest_reg >= 0 && guest_reg < NUM_GUEST_REGS);
	      r = &cache_info->guest_reg_status[guest_reg];
	      host_reg = r->host_reg;

	      switch (op->output_state)
		{
		case ROS_UNTOUCHED:
		  break;
		case ROS_UNTOUCHED_DIRTY:
		  r->dirty_without_offset_p = TRUE;
		  break;
		case ROS_INVALID:
		  host_uncache_reg (cache_info, guest_reg);
		  break;
		case ROS_NATIVE:
		case ROS_OFFSET:
		case ROS_NATIVE_DIRTY:
		case ROS_OFFSET_DIRTY:
#ifdef LITTLEENDIAN
		case ROS_SWAP16:
		case ROS_SWAP32:
		case ROS_SWAP16_DIRTY:
		case ROS_SWAP32_DIRTY:
#endif  /* LITTLEENDIAN */
		  r->mapping = (op->output_state & 3);
		  r->dirty_without_offset_p = !!(op->output_state
						 & ROS_DIRTY_BIT);
		  break;
		default:
		  abort ();
		}
	    }
	}

#ifdef DEBUG
      verify_cache_consistency (cache_info);
#endif

      /* Prepend all new backpatches to the final list. */
      for (bp = block->backpatch; bp != NULL; bp = bp_next)
	{
	  bp_next = bp->next;
	  bp->next = orig_backpatch;
	  orig_backpatch = bp;
	}
      block->backpatch = orig_backpatch;

      /* Success! */
      return TRUE;

    failure:
      /* No luck with this description; reset everything and try the next.
       * Note that we have to restore everything even if we have no
       * descriptions left to try, since our caller needs to know what
       * they should spill if they transition to synthetic code.
       */
      *code = orig_code;
      *cache_info = orig_cache_info;
    failure_no_copy:
      for (bp = block->backpatch; bp != NULL; bp = bp_next)
	{
	  bp_next = bp->next;
	  free (bp);
	}
      block->backpatch = NULL;
    }

  
#ifdef DEBUG
      verify_cache_consistency (cache_info);
#endif

  /* We failed to generate the desired native code. */
  block->backpatch = orig_backpatch;
  return FALSE;
}


static void
host_uncache_reg (cache_info_t *c, int guest_reg)
{
  guest_reg_status_t *r;
  int host_reg;

  r = &c->guest_reg_status[guest_reg];
  host_reg = r->host_reg;
  if (host_reg != NO_REG)
    {
      c->host_reg_to_guest_reg[host_reg] = NO_REG;
      r->host_reg = NO_REG;
    }
}

#endif  /* GENERATE_NATIVE_CODE */
