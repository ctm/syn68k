#ifndef _block_h_
#define _block_h_

#define BLOCK_MAGIC_VALUE 0xDEADBABEUL

#include "syn68k_private.h"    /* To typedef uint16 and uint32. */
#include "backpatch.h"

struct _Block {
  struct _Block *next_in_hash_bucket;   /* Next Block in this hash bucket.   */
  const uint16 *compiled_code;      /* Memory containing compiled code.      */
#ifdef GENERATE_NATIVE_CODE
  uint32 num_times_called;          /* # of times nonnative code called.     */
#endif
  struct _Block *death_queue_prev;  /* Prev block to be nuked if mem needed. */
  struct _Block *death_queue_next;  /* Next block to be nuked if mem needed. */
  struct _Block *range_tree_left;   /* Left child in range tree.             */
  struct _Block *range_tree_right;  /* Right " " " "  (see rangetree.c)      */
  struct _Block *range_tree_parent; /* Parent in r-tree  (see rangetree.c)   */
  struct _Block **parent;           /* Array of ptrs to parent blocks.       */
  struct _Block *child[2];          /* Array of ptrs to child blocks.        */
  syn68k_addr_t m68k_start_address; /* Starting address of 68k code.         */
#ifdef CHECKSUM_BLOCKS
  uint32 checksum;                  /* Checksum of m68k code for this block. */
#endif
  uint32 m68k_code_length;          /* Length of 68k code, in _bytes_.       */
  uint32 range_tree_color  :1;      /* Either RED or BLACK.                  */
  uint32 cc_clobbered      :5;      /* CC bits modified before use.          */
  uint32 cc_may_not_set    :5;      /* CC bits that may be changed.          */
  uint32 cc_needed         :5;      /* CC bits whose values we need.         */
  uint32 num_children      :2;      /* # of blocks that this feeds to.       */
  uint32 immortal          :1;      /* Can't be freed to save space.         */
  uint32 recursive_mark    :1;      /* 1 means hit during this recursion.    */
#ifdef GENERATE_NATIVE_CODE
  uint32 recompile_me      :1;      /* Recompile me as native (temp. flag).  */
#endif  /* GENERATE_NATIVE_CODE */
  uint16 malloc_code_offset:3;      /* Pass compiled_code - this to free().  */
  uint16 num_parents       :13;     /* # of blocks that feed into this one.  */
  backpatch_t *backpatch;           /* Linked list of backpatches to apply.  */
#ifdef DEBUG                        /*  to ptr to the specified child block. */
  uint32 magic;
#endif
};

typedef struct _Block Block;


/* Function prototypes. */
extern Block *block_new (void);
extern void block_free (Block *b);
extern void block_add_parent (Block *b, Block *parent);
extern void block_remove_parent (Block *b, Block *parent, BOOL reclaim_memory);
extern void block_add_child (Block *b, Block *child);
extern void block_remove_child (Block *b, Block *child);
extern void block_do_for_each_parent (Block *b, void (*f)(Block *, void *),
				      void *aux);
#ifdef DEBUG
extern BOOL block_verify (Block *b);
extern void dump_block (const Block *b);
#endif

#endif  /* Not _block_h_ */
