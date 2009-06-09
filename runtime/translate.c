#include "syn68k_private.h"
#include "block.h"
#include "mapping.h"
#include "rangetree.h"
#include "translate.h"
#include "alloc.h"
#include "blockinfo.h"
#include "hash.h"
#include "diagnostics.h"
#include "destroyblock.h"
#include "callback.h"
#include "deathqueue.h"
#include "checksum.h"
#include "native.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "safe_alloca.h"

/* If != NULL, we call this function periodically while we are busy.  We
 * pass it a 1 if we are busy, and a 0 when we are done.
 */
void (*call_while_busy_func)(int);

#ifdef GENERATE_NATIVE_CODE
/* Boolean:  generate native code? */
int native_code_p;
#else
#define native_code_p FALSE
#endif

/* This keeps track of how nested our executions have gotten.  We don't
 * want to recompile code if we're nested, since that might involve
 * destroying code being run by the interrupted task.
 */
int emulation_depth = 0;


static void compute_child_code_pointers (Block *b);
static void generate_code (Block *b, TempBlockInfo *tbi
/* #ifdef GENERATE_NATIVE_CODE */
			   , BOOL try_native_p
/* #endif */ /* GENERATE_NATIVE_CODE */
			   );
static int translate_instruction (const uint16 *m68k_code,
				  uint16 *synthetic_code,
				  const OpcodeMappingInfo *map,
				  int ccbits_live,
				  int ccbits_to_compute,
				  AmodeFetchInfo amf[2],
				  const TempBlockInfo *tbi,
				  Block *block,
				  int32 *backpatch_request_index
#ifdef GENERATE_NATIVE_CODE
				  , cache_info_t *cache_info,
				  BOOL *prev_native_p, BOOL try_native_p
#endif		       
				  );
static int generate_amode_fetch (uint16 *scode, const uint16 *m68koperand,
				 int amode, BOOL reversed, int size);

typedef struct
{
  const OpcodeMappingInfo *map;
  int live_cc;
} MapAndCC;

static void compute_maps_and_ccs (Block *b, MapAndCC *m,
				  const TempBlockInfo *tbi);


/* Compiles a block at a specified address and returns a mask indicating
 * which cc bits must be valid on entry to this block.  The block is placed
 * in the hashtable and the rangetree.  The block just created is
 * returned by reference in *new.  If a block already exists at the specified
 * address, a new block is not created; cc bit information from the already
 * existing block is returned.
 */
int
generate_block (Block *parent, uint32 m68k_address, Block **new
/* #ifdef GENERATE_NATIVE_CODE */
		, BOOL try_native_p
/* #endif */  /* GENERATE_NATIVE_CODE */
		)
{
  Block *b, *old_block;
  TempBlockInfo tbi;
  int cc_needed_by_this_block, cc_needed_by_children;
  int i;

  /* Call a user-defined function periodically while doing stuff. */
  if (call_while_busy_func != NULL)
    call_while_busy_func (1);

  /* If a block already exists there, just return its info. */
  old_block = hash_lookup (m68k_address);
  if (old_block != NULL)
    {
      *new = old_block;
      if (parent != NULL)
	block_add_parent (old_block, parent);
      return old_block->cc_needed;
    }

  /* See if this is really a magical callback address; if so, compile
   * it as such.
   */
  if (IS_CALLBACK (m68k_address))
    {
      int tmp_cc = callback_compile (parent, m68k_address, new);
      if (*new != NULL)
	return tmp_cc;
    }

  /* Create an entirely new block & gather info about it. */
  b = *new = block_new ();

  compute_block_info (b, SYN68K_TO_US (m68k_address), &tbi);
  if (parent != NULL)
    block_add_parent (b, parent);

  /* Temporarily, additionally demand that all cc bits we might not set
   * be valid on entry into this block.  We'll get a better idea what
   * bits need to be set, but we need to assume the worst for this recursion.
   */
  cc_needed_by_this_block = b->cc_needed;
  b->cc_needed |= b->cc_may_not_set;

  /* Add this block to the universe of blocks. */
  hash_insert (b);
  range_tree_insert (b);

  /* Generate all child blocks & determine what cc bits they need.  If this
   * block has itself as a child, we ignore what cc bits it needs (since
   * it needs exactly what we are now computing and can't add any new bits).
   * Hopefully this should help tight loops compute fewer cc bits.
   */
  b->num_children = tbi.num_child_blocks;
  if (tbi.num_child_blocks == 0)     /* No children -> next_block_dynamic. */
    cc_needed_by_children = ALL_CCS;
  else
    for (i = 0, cc_needed_by_children = 0; i < b->num_children; i++)
      {
	if (tbi.child[i] != m68k_address)
	  cc_needed_by_children |= generate_block (b, US_TO_SYN68K (tbi.child[i]),
						   &b->child[i],
						   try_native_p);
	else
	  {
	    block_add_parent (b, b);
	    b->child[i] = b;
	  }
      }
  
  /* Compute exactly what cc bits must be valid on block entry. */
  b->cc_needed = (cc_needed_by_this_block
		  | (cc_needed_by_children & b->cc_may_not_set));

  /* Generate code for this block. */
  generate_code (b, &tbi,
		 native_code_p && (try_native_p || emulation_depth != 1));

  /* Free up the scratch memory for tbi. */
  free (tbi.next_instr_offset);

  /* Finally, fill in all pointers, offsets, etc. to our children's code. */
  compute_child_code_pointers (b);

  /* Add this block to the end of the death queue. */
  death_queue_enqueue (b);

  return b->cc_needed;
}


/* This function fills in the synthetic operands that point to subsequent
 * blocks with pointers to the compiled code in the child blocks.  Because
 * we have to compile loops, it may not always be possible to get the
 * desired information about the child.  However, we are guaranteed that
 * when that child _does_ get filled in, it will recurse back to us
 * and we'll get our child code pointers filled in then.
 */
static void
compute_child_code_pointers (Block *b)
{
  int i;
  backpatch_t *p, *next;
  
  /* Loop over all of our backpatches and fill in what we can. */
  for (p = b->backpatch; p != NULL; p = next)
    {
      next = p->next;
      if (p->target == NULL || p->target->compiled_code != NULL)
	backpatch_apply_and_free (b, p);
    }

  /* Only recurse on those parents interested in our new code location. */
  for (i = b->num_parents - 1; i >= 0; i--)
    {
      for (p = b->parent[i]->backpatch; p != NULL; p = p->next)
	if (p->target == b)
	  break;
      
      /* Is this parent block still interested in where our code ended up? */
      if (p != NULL)
	compute_child_code_pointers (b->parent[i]);
    }
}


#ifdef GENERATE_NATIVE_CODE
/* We use these to keep track of where we need to backpatch transitions
 * from native to synthetic code.
 */
typedef struct
{
  unsigned long stub_offset;
  unsigned long synth_offset;
} ntos_cleanup_t;
#endif  /* GENERATE_NATIVE_CODE */


/* The function generates the synthetic code for a given block.  It does
 * not fill in the pointers to the code in subsequent blocks (if they
 * are even known at translation time).
 */
static void
generate_code (Block *b, TempBlockInfo *tbi, BOOL try_native_p)
{
  const uint16 *m68k_code;
  AmodeFetchInfo amf[2];
  uint8 *code;
  int i;
  MapAndCC *map_and_cc;
  unsigned long max_code_bytes, num_code_bytes;
  uint32 instr_code[256];  /* Space for one instruction. */
#ifdef GENERATE_NATIVE_CODE
  cache_info_t cache_info;
  BOOL prev_native_p;
  ntos_cleanup_t *ntos_cleanup;
  int num_ntos_cleanup;
#ifdef SYNCHRONOUS_INTERRUPTS
  int check_int_stub_offset = -1;
#endif
#endif  /* GENERATE_NATIVE_CODE */
  SAFE_DECL();

  instr_code[(sizeof instr_code / sizeof instr_code[0]) - 1] = 0xFEEBFADE;

#ifdef GENERATE_NATIVE_CODE
  /* Set up appropriate stuff for generating native code. */
  cache_info = empty_cache_info;

  ntos_cleanup = SAFE_alloca ((tbi->num_68k_instrs + 2)
				   * sizeof ntos_cleanup[0]);
  num_ntos_cleanup = 0;
#endif

  /* Allocate space for code.  We'll skip over PTR_BYTES because that
   * space is reserved.
   */
  max_code_bytes = (tbi->num_68k_instrs * 32 + 512);
  code = (uint8 *) xmalloc (PTR_BYTES + max_code_bytes) + PTR_BYTES;
  num_code_bytes = 0;

  /* Start with no backpatches. */
  b->backpatch = NULL;

  /* Compute exactly which cc bits and OpcodeMappingInfo *'s we should
   * use for each m68k instruction in this block.
   */
  map_and_cc = (MapAndCC *) SAFE_alloca ((tbi->num_68k_instrs + 1)
					 * sizeof map_and_cc[0]);
  compute_maps_and_ccs (b, map_and_cc, tbi);

#ifdef GENERATE_NATIVE_CODE
  /* Output the block preamble.  We have separate entry points for
   * incoming native code and incoming synthetic code.  Why?  Incoming
   * native code will not have bothered to update the synthetic PC, and
   * it shouldn't until we actually hit a synthetic opcode.
   *
   * The block preamble looks like this:
   *     [OPCODE_WORDS]      synthetic opcode
   *     [<varies>]          native code
   *
   * If the first m68k instruction in the block was translated to native
   * code, the synthetic opcode will point to the native code (which
   * follows it immediately).  The native code will correspond to that
   * m68k instruction.
   *
   * If the first m68k instruction was translated as synthetic code,
   * the synthetic opcode will be a "skip n words" NOP which skips over
   * the native code stub.  The native code will properly set up the
   * synthetic PC and then start executing the synthetic code, which
   * will immediately follow the native code.
   */

  /* Write out the default preamble. */
  if (try_native_p)
    {
      *(const void **)&code[0] = direct_dispatch_table[0x1];
      *(Block **)&code[sizeof (const void *)] = NULL;
    }
  else
    {
      *(const void **)&code[0] = direct_dispatch_table[0x3];
      *(Block **)&code[sizeof (const void *)] = b;
    }
  num_code_bytes += NATIVE_START_BYTE_OFFSET;

  /* No need to write out the native code preamble here; this will
   * automatically happen if necessary since we'll pretend the previous
   * instruction was native code
   */
#endif

  /* Loop over all instructions, in forwards order, and compile them. */
  m68k_code = SYN68K_TO_US (b->m68k_start_address);
#ifdef GENERATE_NATIVE_CODE
  prev_native_p = TRUE;   /* So n->s stub will be generated if necessary. */
#endif /* GENERATE_NATIVE_CODE */
  for (i = 0; i < tbi->num_68k_instrs; i++)
    {
      int j, main_size;
      int32 backpatch_request_index;
      const OpcodeMappingInfo *map = map_and_cc[i].map;
#ifdef GENERATE_NATIVE_CODE
      BOOL native_p = FALSE;
      backpatch_t *old_backpatch, *native_backpatch;

      old_backpatch = b->backpatch;
      b->backpatch = NULL;
#endif  /* GENERATE_NATIVE_CODE */

      main_size = translate_instruction (m68k_code, (uint16 *)instr_code,
					 map, map_and_cc[i].live_cc,
					 (map_and_cc[i].live_cc
					  & map->cc_may_set),
					 amf, tbi, b,
					 &backpatch_request_index
#ifdef GENERATE_NATIVE_CODE
					 , &cache_info, &native_p,
					 try_native_p
#endif
					 );

      /* Make sure we didn't overrun our temp array. */
      assert (instr_code[sizeof instr_code / sizeof instr_code[0] - 1]
	      == 0xFEEBFADE);

#ifdef GENERATE_NATIVE_CODE

      if (native_p)
	{
	  native_backpatch = b->backpatch;
	  b->backpatch = old_backpatch;

	  if (num_code_bytes == NATIVE_START_BYTE_OFFSET)
	    {
	      /* Smash first synthetic opcode so that it now points
	       * to the native code.
	       */
	      backpatch_add (b, 0, OPCODE_BYTES * 8, FALSE,
			     NATIVE_START_BYTE_OFFSET, b);

#ifdef SYNCHRONOUS_INTERRUPTS
	      memcpy (&code[num_code_bytes], check_interrupt_stub,
		      CHECK_INTERRUPT_STUB_BYTES);
	      check_int_stub_offset = num_code_bytes;
	      num_code_bytes += CHECK_INTERRUPT_STUB_BYTES;
#endif
	    }
	  else if (!prev_native_p)
	    {
	      /* Since the previous instruction wasn't native, and this one
	       * is, we need to throw in a synthetic opcode that will
	       * jump us to the native code.
	       */
	      backpatch_add (b, num_code_bytes * 8,
			     OPCODE_BYTES * 8, FALSE,
			     OPCODE_BYTES + num_code_bytes, b);
	      num_code_bytes += OPCODE_BYTES;
	    }
	}
      else  /* Can only generate amode fetches for non-native code. */
#endif  /* GENERATE_NATIVE_CODE */
	{
#ifdef GENERATE_NATIVE_CODE
	  assert (b->backpatch == NULL);  /* They shouldn't have added any. */
	  native_backpatch = NULL;
	  b->backpatch = old_backpatch;

	  if (prev_native_p)
	    {
	      host_code_t *host_code;
	      BOOL first_p = (num_code_bytes == NATIVE_START_BYTE_OFFSET);

	      /* If the previous code was native, but this isn't,
	       * Generate a stub to pop us back into synthetic code.
	       */
	      host_code = (host_code_t *)&code[num_code_bytes];
	      if (!first_p)
		{
		  host_spill_cc_bits (&cache_info, &host_code,
				      cache_info.cached_cc);
		  host_spill_regs (&cache_info, &host_code, M68K_CC_NONE);
		  cache_info = empty_cache_info;
		}
#ifdef SYNCHRONOUS_INTERRUPTS
	      else
		{
		  /* Write out the check for interrupt stub since this is
		   * the initial native code entry point.
		   */
		  memcpy (host_code, check_interrupt_stub,
			  CHECK_INTERRUPT_STUB_BYTES);
		  check_int_stub_offset = num_code_bytes;
		  host_code = (host_code_t *)((char *)host_code
					      + CHECK_INTERRUPT_STUB_BYTES);
		}
#endif  /* SYNCHRONOUS_INTERRUPTS */

	      /* Write out the transition stub. */
	      memcpy (host_code, native_to_synth_stub,
		      NATIVE_TO_SYNTH_STUB_BYTES);

	      /* Compute the next synthetic opcode address.  It has
	       * to follow the stub, but be aligned mod 4 bytes.
	       */
	      num_code_bytes = (((char *)host_code + NATIVE_TO_SYNTH_STUB_BYTES
				  - (char *)code) + 3) & ~3;

	      /* Add a backpatch to clean up the stub. */
	      ntos_cleanup[num_ntos_cleanup].stub_offset
		= (char *)host_code - (char *)code;
	      
	      /* If we aren't allowed to use native code, and this is the
	       * first instruction, then jump back to the first synthetic
	       * opcode that counts how many times this block has been hit.
	       * Otherwise, gateway directly to the next instruction.
	       */
	      if (!try_native_p && first_p)
		ntos_cleanup[num_ntos_cleanup].synth_offset = 0;
	      else
		ntos_cleanup[num_ntos_cleanup].synth_offset = num_code_bytes;

	      ++num_ntos_cleanup;
	    }
#endif
	  /* Generate instructions to fetch pointer to amode, if necessary. */
	  for (j = 0; j < 2; j++)
	    if (amf[j].valid)
	      {
		int afetch_size;
		afetch_size = generate_amode_fetch (((uint16 *)
						     &code[num_code_bytes]),
						    amf[j].m68koperand,
						    amf[j].amode,
						    amf[j].reversed,
						    amf[j].size);
		num_code_bytes += afetch_size * sizeof (uint16);
	      }

	  /* Set up backpatches for next block pointers, if necessary. */
	  if (backpatch_request_index != -1)
	    {
	      int bln;
	      for (bln = 0; bln < tbi->num_child_blocks; bln++)
		{
		  backpatch_add (b,
				 (backpatch_request_index + num_code_bytes
				  + bln * PTR_BYTES) * 8,
				 PTR_BYTES * 8, FALSE, 0, b->child[bln]);
		}
	    }
	}

#ifdef GENERATE_NATIVE_CODE
      /* If the native code requested any backpatches, correct them now
       * that we know exactly where the native code goes.
       */
      if (native_backpatch != NULL)
	{
	  backpatch_t *n, *next;

	  /* Correct each one and add it to the real backpatch list. */
	  for (n = native_backpatch; n != NULL; n = next)
	    {
	      next = n->next;
	      n->offset_location += 8 * num_code_bytes;
	      n->next = b->backpatch;
	      b->backpatch = n;
	    }
	}
#endif  /* GENERATE_NATIVE_CODE */


      /* Now write out the real code, after any necessary amode fetches. */
      memcpy (&code[num_code_bytes], instr_code, main_size);
      num_code_bytes += main_size;

      /* If we are dangerously close to the end of our allocated code space,
       * xrealloc it to make it bigger.
       */
      if (max_code_bytes - num_code_bytes < 512)
	{
	  max_code_bytes *= 2;
	  code = (uint8 *) xrealloc (code - PTR_BYTES,
				     max_code_bytes + PTR_BYTES);
	  code += PTR_BYTES;  /* Skip over reserved space again. */
	}

      /* Move on to the next instruction. */
      m68k_code += tbi->next_instr_offset[i];
#ifdef GENERATE_NATIVE_CODE
      prev_native_p = native_p;
#endif /* GENERATE_NATIVE_CODE */
    }

  /* Copy the code we just created over to the block.  We allocate a little
   * extra space because we prepend all compiled code with the big-endian
   * 68k PC of the first instruction, in case we hit an interrupt when
   * we are about to start the block.  NOTE: to preserve alignment we
   * allocate PTR_WORDS to hold the 68k PC even though we only need to
   * use 2 (shorts).
   */
  b->compiled_code = (((uint16 *) xrealloc (code - PTR_BYTES,
					    PTR_BYTES + num_code_bytes))
		      + PTR_WORDS);
  b->malloc_code_offset = PTR_WORDS;

  WRITE_LONG (&b->compiled_code[PTR_WORDS], b->m68k_start_address);

#ifdef GENERATE_NATIVE_CODE
  /* Now that the block's code is at a fixed address, patch up any
   * native->synthetic stubs so they do the right thing.
   */
  for (i = 0; i < num_ntos_cleanup; i++)
    {
      host_backpatch_native_to_synth_stub (b,
					   ((host_code_t *)
					    ((char *)b->compiled_code
					     + ntos_cleanup[i].stub_offset)),
					   ((uint32 *)
					    ((char *)b->compiled_code
					     + ntos_cleanup[i].synth_offset)));
    }

#ifdef SYNCHRONOUS_INTERRUPTS
  if (check_int_stub_offset >= 0)
    {
      host_backpatch_check_interrupt_stub (b, ((host_code_t *)
					       ((char *)b->compiled_code
						+ check_int_stub_offset))); 
    }
#endif

#endif  /* GENERATE_NATIVE_CODE */

  /* Checksum the code upon which this was based. */
#ifdef CHECKSUM_BLOCKS
  b->checksum = compute_block_checksum (b);
#endif

  ASSERT_SAFE (map_and_cc);

#ifdef GENERATE_NATIVE_CODE
  ASSERT_SAFE (ntos_cleanup);
#endif
}


/* Helper function; writes out the magic bits for an opcode. */
static inline uint16 *
output_opcode (uint16 *code, uint32 opcode)
{
#ifdef USE_DIRECT_DISPATCH
  *(const void **)code = direct_dispatch_table[opcode];
#else
  *(const void **)code = (void *) opcode;
#endif
  code += OPCODE_WORDS;
  return code;
}


#define ROUND_UP(n) ((((n) + (PTR_WORDS - 1)) / PTR_WORDS) * PTR_WORDS)

#define IS_UNEXPANDABLE_AMODE(n) \
  (((n) >> 3) == 6 || (n) == 0x3A  || (n) == 0x3B)

/* Generates synthetic code for the m68k instruction pointed to by m68k_code,
 * placing the synthetic code at the location pointed to by synthetic_code.
 * On entry, ccbits_to_compute specifies a bitmask for the cc bits this
 * instruction must compute (if it can), and ccbits_live specifies all
 * CC bits known to be live at this point (which should be a superset of
 * the bits to compute); the bits are specified CNVXZ, with Z being bit 0 of
 * the mask and so on.  Returns the number of _bytes_ of code generated.
 */
static int
translate_instruction (const uint16 *m68k_code, uint16 *synthetic_code,
		       const OpcodeMappingInfo *map,
		       int ccbits_live,
		       int ccbits_to_compute,
		       AmodeFetchInfo amf[2],
		       const TempBlockInfo *tbi,
		       Block *block,
		       int32 *backpatch_request_index
#ifdef GENERATE_NATIVE_CODE
		       , cache_info_t *cache_info, BOOL *prev_native_p,
		       BOOL try_native_p
#endif		       
		       )
{
  uint16 *scode = synthetic_code;
  const BitfieldInfo *bf;
  const uint16 *opp;
  uint16 m68kop = READUW (US_TO_SYN68K (m68k_code));
  uint16 synop;
  int amode = m68kop & 63;
  int revmode = ((m68kop >> 9) & 7) | ((m68kop >> 3) & 0x38);
  int32 operand[MAX_BITFIELDS];
  int i;

  *backpatch_request_index = -1;  /* default */
  
  /* Grab all of the operands and stick them in our operand array in
   * native endian byte order.  If we generate native code, this is
   * what the native code routines expect.  Otherwise, if we go to
   * synthetic code, we'll later end up byte swapping and so on as
   * necessary.
   */
  for (bf = map->bitfield, i = 0;
       i < MAX_BITFIELDS && !IS_TERMINATING_BITFIELD (bf);
       i++, bf++)
    {
      int index = bf->index, length = bf->length + 1;
      syn68k_addr_t p = (syn68k_addr_t) US_TO_SYN68K (&m68k_code[index >> 4]);
      uint32 val;

      /* A length of 16 or 32 bits implies that the operand is aligned on a
       * word boundary.
       */
      if (length == 16)
	{
	  if (bf->sign_extend)
	    val = READSW (p);      /* Sign extend. */
	  else
	    val = READUW (p);   /* Zero extend. */
	}
      else if (length == 32)
	{
	  val = READUL (p);
	}
      else  /* It's not a nicely aligned word or long. */
	{
	  val = READUW (p) >> (16 - ((index & 0xF) + length));

	  if (bf->rev_amode)
	    {
	      val = ((val >> 3) & 0x7) | ((val & 0x7) << 3);
	    }
	  else
	    {
	      if (bf->sign_extend && (val & (1 << (length - 1))))
		val |= ~((1 << length) - 1);  /* Ones extend. */
	      else
		val &= ((1 << length) - 1);   /* Zero extend. */
	    }
	}

      operand[i] = val;
    }

#ifdef GENERATE_NATIVE_CODE
  {
  host_code_t *hc_scode;

  hc_scode = (host_code_t *) scode;
  if (map->guest_code_descriptor != NULL
      && try_native_p
      && generate_native_code (map->guest_code_descriptor, cache_info,
			       operand, &hc_scode,
			       map->cc_needed, ccbits_live, ccbits_to_compute,
			       map->ends_block, block, m68k_code))
    {
      *prev_native_p = TRUE;
      return (char *)hc_scode - (char *)synthetic_code;
    }
  }
#endif

  /* If we have an unexpanded amode, insert an opcode to compute a pointer
   * to the addressing mode.
   */
  opp = m68k_code + map->instruction_words;  /* Point to 1st amode operand */

  if (map->amode_size != 0
      && (!map->amode_expanded || IS_UNEXPANDABLE_AMODE (amode)))
    {
      amf[0].valid       = TRUE;
      amf[0].reversed    = FALSE;
      amf[0].m68koperand = opp;
      amf[0].amode       = amode;
      amf[0].size        = 1 << (map->amode_size - 1);
      opp += amode_size (amode, opp, map->amode_size);  /* Skip to next oper */
    }
  else
    amf[0].valid = FALSE;

  /* If we have an unexpanded reversed amode, insert an opcode to compute a
   * pointer to the addressing mode.
   */
  if (map->reversed_amode_size != 0
      && (!map->reversed_amode_expanded || IS_UNEXPANDABLE_AMODE (revmode)))
    {
      amf[1].valid       = TRUE;
      amf[1].reversed    = TRUE;
      amf[1].m68koperand = opp;
      amf[1].amode       = revmode;
      amf[1].size        = 1 << (map->reversed_amode_size - 1);
    }
  else
    amf[1].valid = FALSE;

  /* Compute synthetic opcode and place it in synthetic instruction stream. */
  synop = (((m68kop & map->opcode_and_bits) >> map->opcode_shift_count)
	   + map->opcode_add_bits);
  scode = output_opcode (scode, synop);

#ifdef DEBUG
  if (synop == 0
#ifdef USE_DIRECT_DISPATCH
      || direct_dispatch_table[synop] == NULL
#endif
      )
    fprintf (stderr, "Unknown 68k opcode 0x%04X\n", (unsigned) m68kop);
#endif

  /* Reserve space for pointers to next blocks and set up backpatches. */
  if (map->ends_block && !map->next_block_dynamic)
    {
      *backpatch_request_index = (char *)scode - (char *)synthetic_code;
      if (tbi->num_child_blocks > 0)
	{
	  ((uint32 *)scode)[0] = 0x56784321;
	  if (tbi->num_child_blocks > 1)
	    ((uint32 *)scode)[1] = 0x57684321;
	}
      scode += tbi->num_child_blocks * PTR_WORDS;
    }

  /* If we are processing an instruction that requires the big-endian address
   * of the next instruction in the instr stream, do it.  Also insert computed
   * subroutine target for bsr.
   */
  if ((m68kop >> 6) == 0x13A       /* jsr?     */
      || (m68kop >> 8) == 0x61     /* bsr?     */
      )
    {
      uint32 addr;

      /* Write out the big endian return address. */
      addr = SWAPUL_IFLE (US_TO_SYN68K (m68k_code
					+ instruction_size (m68k_code, map)));
      WRITEUL_UNSWAPPED (scode, addr);
      scode += 2;

      /* If it's a bsr, compute the target address. */
      if ((m68kop >> 8) == 0x61)              /* bsr?          */
	{
	  /* Write out the target address. */
	  WRITEUL_UNSWAPPED (scode, tbi->child[0]);
	  scode += 2;
	}
    }

  /* If we need to insert the address of the next instruction in native
   * byte order, do it.  FIXME - when looking for chk or div instructions,
   * we are masking out the addressing mode field.  It's possible that
   * some of the impossible amode combinations actually correspond to
   * entirely different instructions; this would cause the wrong results!
   */
  else if ((m68kop & 0xF0C0) == 0x80C0  /* divs/divu    ditto */
	   || (m68kop & 0xFFC0) == 0x4C40  /* divsl/divul  ditto */
	   || (map->next_block_dynamic
	       && (m68kop == 0x4E76                    /* trapv?   */
		   || (m68kop >> 4) == 0x4E4           /* trap #n? */
		   || ((m68kop & 0xF0FF) >= 0x50FA
		       && (m68kop & 0xF0FF) <= 0x50FC) /* trapcc? */
		   || (m68kop & 0xF140) == 0x4100  /* chk? Not all amodes ok*/
		   || (m68kop & 0xF9C0) == 0x00C0  /* chk2?        ditto */
		   )))
    {
      uint32 addr = US_TO_SYN68K (m68k_code + instruction_size (m68k_code,
								map));
      WRITEUL_UNSWAPPED (scode, addr);
      scode += 2;
    }

  /* If we are processing something that might trap where we need a ptr to
   * the trapping instruction, insert the PC into the instruction stream.
   * See impossible amode warning in previous comment.
   */
  if ((m68kop & 0xF0C0) == 0x80C0     /* divs/divu    ditto */
      || (m68kop & 0xFFC0) == 0x4C40     /* divsl/divul  ditto */
      || (m68kop & 0xFF28) == 0xF428
      || (map->next_block_dynamic
	  && ((m68kop >> 12) == 0xA             /* a-line trap? */
	      || m68kop == 0x4E73               /* rte? */
	      || m68kop == 0x4AFC               /* explicit ILLEGAL?         */
	      || synop == 0                     /* REAL illegal instruction? */
	      || (m68kop >> 3) == (0x4848 >> 3) /* bkpt? */
	      || (m68kop >> 12) == 0xF          /* f-line trap? */
	      || (m68kop & 0xF140) == 0x4100    /* chk?  Not all amodes ok! */
	      || (m68kop & 0xF9C0) == 0x00C0    /* chk2?        ditto */
	      )))
    {
      WRITEUL_UNSWAPPED (scode, US_TO_SYN68K (m68k_code));
      scode += 2;
    }


  /* Extract operands from m68k stream, process them, and place
   * them in the synthetic stream.  16- and 32-bit bitfields must be
   * word-aligned in the 68k stream.  Only 32-bit bitfields are allowed to
   * span multiple words.
   */
  for (bf = map->bitfield, i = 0;
       i < MAX_BITFIELDS && !IS_TERMINATING_BITFIELD (bf);
       i++, bf++)
    {
      int words;
      uint32 val;

      /* Fetch the value (which we already extracted above). */
      val = operand[i];

      /* Put the value back to big endian ordering if we need to. */
      words = bf->words + 1;
#ifdef LITTLEENDIAN
      if (!bf->make_native_endian)
	{
	  if (words == 1)
	    val = SWAPUW (val);
	  else
	    val = SWAPUL (val);
	}
#endif

      /* Write the operand out to the instruction stream. */
      if (words == 1)
	{
	  scode[0] = val;	/* Ideally everything would be a long. */
	  scode[1] = 0;		/* Avoid uninitialized memory complaints. */
	}
      else
	WRITEUL_UNSWAPPED (scode, val);
      scode += 2;
    }
  
  /* Round the size up to occupy an integral number of PTR_WORDS. */
  return ROUND_UP (scode - synthetic_code) * sizeof (uint16);
}


/* Generates synthetic code to compute the value for an addressing mode
 * and store it in cpu_state.amode_p or cpu_state.reversed_amode_p;
 * Returns the number of 16-bit words generated (historical; should be
 * bytes).
 */
static int
generate_amode_fetch (uint16 *code, const uint16 *m68koperand, int amode,
		      BOOL reversed, int size)
{
  int mode = amode >> 3, reg = amode & 7;
  uint16 *scode = code;

  switch (mode) {
  case 2:
  case 3:
    scode = output_opcode (scode, 0x4 + (reversed * 8) + reg);
    return ROUND_UP (scode - code);
  case 4:
    {
      static const unsigned char amode_4_base[] = 
	{ 0x00, 0x14, 0x24, 0x00, 0x34 };
      scode = output_opcode (scode, amode_4_base[size] + (reversed * 8) + reg);
      return ROUND_UP (scode - code);
    }
  case 5:
    scode = output_opcode (scode, 0x44 + (reversed * 8) + reg);
    *(int32 *)scode = READSW (US_TO_SYN68K (m68koperand));
    scode += 2;
    return ROUND_UP (scode - code);
  case 6:
    {
      uint16 extword = READUW (US_TO_SYN68K (m68koperand));
      if ((extword & 0x100) == 0)
	{
	  scode = output_opcode (scode, (0x58 - 0x12 - 0x24
					 + (0x12 << (extword >> 15))
					 + (0x24 << ((extword >> 11) & 1))
					 + (reversed * 9) + reg));

	  WRITE_PTR (scode, SYN68K_TO_US ((ptr_sized_uint)
					  (((int8 *) m68koperand)[1])));
	  scode += PTR_WORDS;
	  *(uint32 *)(scode    ) = (extword >> 12) & 7;
	  *(uint32 *)(scode + 2) = (extword >> 9) & 3;
	  return ROUND_UP (scode + 4 - code);
	}
      else if ((extword & 0xF) == 0x0)
	{
	  int32 disp;
	  uint16 synop;

	  /* Base suppress?  Pretend they are using "a8" (== 0 here). */
	  if (extword & 0x80)
	    reg = 8;

	  /* Index suppress? */
	  if (extword & 0x40)
	    synop = 0xA0 + (reversed * 9) + reg;
	  else
	    synop = (0x58 + - 0x12 - 0x24 + (0x12 << (extword >> 15))
		     + (0x24 << ((extword >> 11) & 1))
		     + (reversed * 9) + reg);

	  scode = output_opcode (scode, synop);

	  switch ((extword >> 4) & 0x3) {
	  case 2:
	    disp = READSW (US_TO_SYN68K (m68koperand + 1));
	    break;
	  case 3:
	    disp = READSL (US_TO_SYN68K (m68koperand + 1));
	    break;
	  default:
	    disp = 0;
	    break;
	    }
	  WRITE_PTR (scode, SYN68K_TO_US (disp));
	  scode += PTR_WORDS;

	  if (!(extword & 0x40))
	    {
	      *(uint32 *)(scode    ) = (extword >> 12) & 7;
	      *(uint32 *)(scode + 2) = (extword >> 9) & 3;
	      return ROUND_UP (scode + 4 - code);
	    }
	  return ROUND_UP (scode - code);
	}
      else  /* Memory indirect pre-indexed or memory indirect post-indexed. */
	{
	  int32 base_displacement, outer_displacement;

	  /* Memory indirect pre- or post-indexed. */
	  scode = output_opcode (scode, 0xB2);

	  /* Get base displacement size. */
	  switch ((extword >> 4) & 0x3) {
	  case 2:
	    base_displacement = READSW (US_TO_SYN68K (m68koperand + 1));
	    m68koperand += 1;
	    break;
	  case 3:
	    base_displacement = READSL (US_TO_SYN68K (m68koperand + 1));
	    m68koperand += 2;
	    break;
	  default:
	    base_displacement = 0;
	    break;
	  }

	  /* Get outer displacement size. */
	  switch (extword & 0x3) {
	  case 2:
	    outer_displacement = READSW (US_TO_SYN68K (m68koperand + 1));
	    break;
	  case 3:
	    outer_displacement = READSL (US_TO_SYN68K (m68koperand + 1));
	    break;
	  default:
	    outer_displacement = 0;
	    break;
	  }

	  WRITEUL_UNSWAPPED (scode,     base_displacement);
	  WRITEUL_UNSWAPPED (scode + 2, outer_displacement);
	  
	  /* Toss two magical flags into the flags word.  If the low bit
	   * is set, we are computing reversed_amode_p instead of amode_p.
	   * If the next lowest bit is set, we are in memory indirect
	   * pre-indexed mode instead of memory indirect post-indexed mode.
	   */
	  extword &= ~3;
	  extword |= reversed;
	  if (!(extword & 0x4))   /* Memory indirect pre-indexed? */
	    extword |= 2;
	  *(uint32 *)(scode + 4) = extword;
	  *(uint32 *)(scode + 6) = reg;
	  return ROUND_UP (scode + 8 - code);
	}
    }
    break;
  case 7:
    switch (reg) {
    case 0:
      scode = output_opcode (scode, 0x54 + reversed);
      *(uint32 *)scode = READUW (US_TO_SYN68K (m68koperand));
      return ROUND_UP (scode + 2 - code);
    case 1:
      {
	char *val = (char *) SYN68K_TO_US (CLEAN
					   (READUL
					    (US_TO_SYN68K (m68koperand))));

	/* Specify absolute long address.  Give a real pointer in our space. */
	scode = output_opcode (scode, 0x56 + reversed);
	WRITE_PTR (scode, val);
	return ROUND_UP (scode + PTR_WORDS - code);
      }
    case 2:
      {
	char *val = (READSW (US_TO_SYN68K (m68koperand))
		     + (char *) m68koperand);

	/* Specify absolute long address.  Give a real ptr in our space. */
	scode = output_opcode (scode, 0x56 + reversed);
	WRITE_PTR (scode, val);
	return ROUND_UP (scode + PTR_WORDS - code);
      }
    case 3:      /* 111/011 (d8,PC,Xn), (bd,PC,Xn), ([bd,PC,Xn],od) */
                 /*         ([bd,PC],Xn,od) */
      {
	uint16 extword = READUW (US_TO_SYN68K (m68koperand));
	reg = (extword >> 12) & 0x7;
	if ((extword & 0x100) == 0)
	  {
	    scode = output_opcode (scode, (0x58 - 0x12 - 0x24
					   + (0x12 << (extword >> 15))
					   + (0x24 << ((extword >> 11) & 1))
					   + (reversed * 9) + 8));

	    WRITE_PTR (scode,
		       (char *) ((long) m68koperand
				     + (((int8 *) m68koperand)[1])));
	    scode += PTR_WORDS;
	    *(uint32 *)(scode    ) = reg;
	    *(uint32 *)(scode + 2) = (extword >> 9) & 3;
	    return ROUND_UP (scode + 4 - code);
	  }
	else if ((extword & 0xF) == 0x0)
	  {
	    int32 disp;
	    uint16 synop;
	    
	    /* Index suppress? */
	    if (extword & 0x40)
	      synop = 0xA0 + (reversed * 9) + 8;
	    else
	      synop = (0x58 - 0x12 - 0x24 + (0x12 << (extword >> 15))
		       + (0x24 << ((extword >> 11) & 1))
		       + (reversed * 9) + 8);
	    
	    scode = output_opcode (scode, synop);

	    switch ((extword >> 4) & 0x3) {
	    case 2:
	      disp = READSW (US_TO_SYN68K (m68koperand + 1));
	      break;
	    case 3:
	      disp = READSL (US_TO_SYN68K (m68koperand + 1));
	      break;
	    default:
	      disp = 0;
	      break;
	    }
	    disp += US_TO_SYN68K (m68koperand);/* Add in PC to displacement. */
	    WRITE_PTR (scode, SYN68K_TO_US (disp));
	    scode += PTR_WORDS;

	    if (!(extword & 0x40))
	      {
		*(uint32 *)(scode    ) = (extword >> 12) & 7;
		*(uint32 *)(scode + 2) = (extword >> 9) & 3;
		return ROUND_UP (scode + 4 - code);
	      }
	    return ROUND_UP (scode - code);
	  }
      else  /* PC relative mem indir pre-indexed or mem indir post-indexed. */
	{
	  int32 base_displacement, outer_displacement;

	  /* Memory indirect pre- or post-indexed. */
	  scode = output_opcode (scode, 0xB2);

	  if (extword & 0x80)
	    base_displacement = 0;            /* Suppress PC base */
	  else
	    base_displacement = US_TO_SYN68K (m68koperand);  /* PC is base. */

	  /* Get base displacement size. */
	  switch ((extword >> 4) & 0x3) {
	  case 2:
	    base_displacement += READSW (US_TO_SYN68K (m68koperand + 1));
	    m68koperand += 1;
	    break;
	  case 3:
	    base_displacement += READSL (US_TO_SYN68K (m68koperand + 1));
	    m68koperand += 2;
	    break;
	  default:
	    break;
	  }

	  /* Get outer displacement size. */
	  switch (extword & 0x3) {
	  case 2:
	    outer_displacement = READSW (US_TO_SYN68K (m68koperand + 1));
	    break;
	  case 3:
	    outer_displacement = READSL (US_TO_SYN68K (m68koperand + 1));
	    break;
	  default:
	    outer_displacement = 0;
	    break;
	  }

	  WRITEUL_UNSWAPPED (scode,     base_displacement);
	  WRITEUL_UNSWAPPED (scode + 2, outer_displacement);
	  scode += 4;

	  /* Toss two magical flags into the flags word.  If the low bit
	   * is set, we are computing reversed_amode_p instead of amode_p.
	   * If the next lowest bit is set, we are in memory indirect
	   * pre-indexed mode instead of memory indirect post-indexed mode.
	   */
	  extword &= ~3;
	  extword |= reversed;
	  if (!(extword & 0x4))   /* Memory indirect pre-indexed? */
	    extword |= 2;
	  
	  /* Pretend like base suppress is set. */
	  extword |= 0x80;

	  *(uint32 *)(scode    ) = extword;
	  *(uint32 *)(scode + 2) = reg;
	  return ROUND_UP (scode + 4 - code);
	}
      }
      break;
    }
  }

  return 0;
}


static void
compute_maps_and_ccs (Block *b, MapAndCC *m, const TempBlockInfo *tbi)
{
  const uint16 *m68k_code;
  int cc_needed;
  int i;

  /* Determine what cc bits must be valid after the last instruction. */
  if (b->num_children == 0)
    cc_needed = ALL_CCS;
  else
    {
      cc_needed = b->child[0]->cc_needed;
      if (b->num_children > 1)
	cc_needed |= b->child[1]->cc_needed;
    }

  /* Loop over all instructions, in backwards order, and compute
   * their live cc bits and optimal OcpodeMappingInfo *'s.
   */
  m68k_code = (SYN68K_TO_US (b->m68k_start_address)
	       + (b->m68k_code_length / sizeof (uint16)));

  /* Save the cc bits live at the end at the end of the array. */
  m[tbi->num_68k_instrs].map     = NULL;
  m[tbi->num_68k_instrs].live_cc = cc_needed;

  for (i = tbi->num_68k_instrs - 1; i >= 0; i--)
    {
      const OpcodeMappingInfo *map;
      int parity, best_cc;

      m68k_code -= tbi->next_instr_offset[i];

      /* Grab first struct in opcode mapping sequence. */
      map = &opcode_map_info[opcode_map_index[READUW (US_TO_SYN68K (m68k_code))]];

      /* Grab the parity of this sequence and max cc bits computable. */
      parity = map->sequence_parity;
      best_cc = cc_needed & map->cc_may_set;

      /* Locate the mapping that computes as few cc bits as we legally can. */
      while (map[1].sequence_parity == parity
	     && (cc_needed & map[1].cc_may_set) == best_cc)
	map++;

      /* Record the best map and the cc bits we need here. */
      m[i].map = map;
      m[i].live_cc = cc_needed;

      cc_needed = (cc_needed & map->cc_may_not_set) | map->cc_needed;
    }
}


/* We use artificial blocks when we need to do something for which there
 * is no m68k opcode, like exit the emulator or call a callback routine.
 */
Block *
make_artificial_block (Block *parent, syn68k_addr_t m68k_address,
		       int extra_words, uint16 **extra_start)
{
  Block *b;
  uint16 *code;

  b = block_new ();
  b->m68k_start_address = m68k_address;
  b->m68k_code_length   = 1;
  b->cc_may_not_set     = ALL_CCS;
  b->cc_needed          = ALL_CCS;
  b->immortal           = TRUE;
  if (parent != NULL)
    block_add_parent (b, parent);

  b->malloc_code_offset = PTR_WORDS;
  code = (((uint16 *) xcalloc (PTR_WORDS
#ifdef GENERATE_NATIVE_CODE
			       + NATIVE_START_BYTE_OFFSET / sizeof (uint16)
			       + NATIVE_PREAMBLE_WORDS
#endif
			       + extra_words,
			       sizeof (uint16)))
	  + PTR_WORDS);  /* Skip over prepended m68k address. */
  WRITE_LONG (&code[-2], m68k_address);
  b->compiled_code = code;
  b->checksum = compute_block_checksum (b);

#ifdef GENERATE_NATIVE_CODE
  *(const void **)code = direct_dispatch_table[0x1];
  *(Block **)(code + sizeof (const void *)) = NULL;
  *extra_start = (uint16 *) ((char *)code
			     + NATIVE_START_BYTE_OFFSET
			     + NATIVE_PREAMBLE_WORDS * sizeof (uint16));

  /* Write out the transition stub.  It will just jump back to the
   * synthetic NOP, which will skip over this.  Fun, huh?
   */
#ifdef SYNCHRONOUS_INTERRUPTS
  memcpy ((char *)code + NATIVE_START_BYTE_OFFSET, check_interrupt_stub,
	  CHECK_INTERRUPT_STUB_BYTES);
#endif
  memcpy ((char *)code + NATIVE_START_BYTE_OFFSET + CHECK_INTERRUPT_STUB_BYTES,
	  native_to_synth_stub, NATIVE_TO_SYNTH_STUB_BYTES);
  host_backpatch_check_interrupt_stub (b, ((host_code_t *)
					   ((char *)code
					    + NATIVE_START_BYTE_OFFSET))); 
  host_backpatch_native_to_synth_stub (b,
				       ((host_code_t *)
					((char *)code
					 + NATIVE_START_BYTE_OFFSET
					 + CHECK_INTERRUPT_STUB_BYTES)),
				       (uint32 *)code);
#else   /* !GENERATE_NATIVE_CODE */
  *extra_start = code;
#endif  /* !GENERATE_NATIVE_CODE */

  return b;
}
