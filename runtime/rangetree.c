/*
 * rangetree.c - Routines for manipulating a red-black tree data structure
 *               that maps 68k addresses to the Block that address is contained
 *               in.  It is generally slower than the hash table in hash.c, but
 *               more powerful because it can find the block corresponding
 *               to an address even when that address is not the address of
 *               the first byte in the block.  The tree is sorted by the
 *               low address of the range, and these routines work because
 *               any two overlapping blocks must have the same ending
 *               address; whatever m68k code caused one block to end will
 *               also cause the other to end.  There exist pathological
 *               exceptions where operands are used as opcodes.
 *
 *   Algorithms taken from Cormen/Leiserson/Rivest's _Introduction to
 *   Algorithms_.
 */


#include <stddef.h>
#include <assert.h>

#if 0
#include <sys/param.h>
#endif

#include <stdio.h>
#include "block.h"
#include "rangetree.h"

typedef Block * Tree;

static Tree root = NULL;
static Block null_tree_block;   /* NULL sentry. */

#define NULL_TREE (&null_tree_block)
#define BLACK 0
#define RED 1
#define BLOCK_TO_TREE(b) ((Tree) (b))
#define TREE_TO_BLOCK(t) ((Block *) (t))
#define LEFT(t) ((t)->range_tree_left)
#define RIGHT(t) ((t)->range_tree_right)
#define PARENT(t) ((t)->range_tree_parent)
#define GRANDPARENT(t) (PARENT (PARENT (t)))
#define IS_RED(t) ((t)->range_tree_color != BLACK)
#define IS_BLACK(t) ((t)->range_tree_color == BLACK)
#define COLOR(t) ((t)->range_tree_color)
#define SET_COLOR(t,c) ((t)->range_tree_color = (c))

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

/* Initializes the range tree.  Call this before calling any other range tree
 * functions, and call it exactly once.
 */
void
range_tree_init ()
{
  SET_COLOR (NULL_TREE, BLACK);
  NULL_TREE->m68k_start_address = 666999666;
  root = NULL_TREE;
}


/* Helper function for range_tree_destroy(). */
static void
range_tree_destroy_aux (Tree t)
{
  if (t == NULL_TREE)
    return;
  range_tree_destroy_aux (LEFT (t));
  range_tree_destroy_aux (RIGHT (t));
  LEFT (t) = NULL_TREE;
  RIGHT (t) = NULL_TREE;
}


/* Frees all memory associated with the range tree and NULLifies all
 * range tree pointers in the Blocks that were in the tree.
 */
void
range_tree_destroy ()
{
  range_tree_destroy_aux (root);
  root = NULL_TREE;
}


#if 0
/* No longer used. */

/* Given a 68k address, this attempts to locate some Block that contains
 * it.  No guarantess are made about which such Block will be returned if
 * there is more than one which intersects the specified address.  If such
 * a Block is found that Block is returned, else NULL.
 */
Block *
range_tree_lookup (syn68k_addr_t addr)
{
  Tree t = root;
  
  while (t != NULL_TREE)
    {
      if (addr >= (TREE_TO_BLOCK (t))->m68k_start_address)
	{
	  if (addr < (TREE_TO_BLOCK (t))->m68k_start_address
	      + (TREE_TO_BLOCK (t))->m68k_code_length)
	    break;
	  t = RIGHT (t);
	}
      else t = LEFT (t);
    }

  return (t == NULL_TREE) ? NULL : TREE_TO_BLOCK (t);
}
#endif


/* Given a 68k address, returns a pointer to the Block with the lowest
 * m68k_start_address such that the m68k_start_address is >= addr.
 * Returns NULL if no such Block exists.
 */
Block *
range_tree_find_first_at_or_after (syn68k_addr_t addr)
{
  Tree t = root, best = NULL;

  while (t != NULL_TREE)
    {
      if ((TREE_TO_BLOCK (t))->m68k_start_address >= addr)
	{
	  best = t;
	  t = LEFT (t);  /* Only lesser children can beat us. */
	}
      else
	t = RIGHT (t);   /* Our address isn't high enough. */
    }

  return TREE_TO_BLOCK (best);
}


/* Given a 68k address, returns a pointer to the Block with the lowest
 * m68k_start_address such that its m68k addresses intersect the range
 * [low, high].  Returns NULL if no such Block exists.  This assumes that
 * overlapping blocks will have the same ending address.
 */
Block *
range_tree_first_to_intersect (syn68k_addr_t low, syn68k_addr_t high)
{
  Tree t = root, best = NULL;

  while (t != NULL_TREE)
    {
      syn68k_addr_t l, h;

      l = (TREE_TO_BLOCK (t))->m68k_start_address;
      h = l + (TREE_TO_BLOCK (t))->m68k_code_length - 1;

      if (l > high)
	t = LEFT (t);
      else if (h < low)
	t = RIGHT (t);
      else  /* l <= high && h >= low */
	{
	  best = t;
	  t = LEFT (t);  /* Only lesser children can beat us. */
	}
    }

  return TREE_TO_BLOCK (best);
}


static void
left_rotate (Tree x)
{
  Tree y = RIGHT (x);

#ifdef DEBUG
  assert (y != NULL_TREE);
#endif

  /* Turn y's left subtree into x's right subtree. */
  RIGHT (x) = LEFT (y);
  if (LEFT (y) != NULL_TREE)
    PARENT (LEFT (y)) = x;

  /* Link x's parent to y. */
  PARENT (y) = PARENT (x);
  if (PARENT (x) == NULL_TREE)
    root = y;
  else if (x == LEFT (PARENT (x)))
    LEFT (PARENT (x)) = y;
  else RIGHT (PARENT (x)) = y;

  /* Put x on y's left. */
  LEFT (y) = x;
  PARENT (x) = y;
}


static void
right_rotate (Tree y)
{
  Tree x = LEFT (y);

#ifdef DEBUG
  assert (x != NULL_TREE);
#endif

  /* Turn x's right subtree into y's left subtree. */
  LEFT (y) = RIGHT (x);
  if (RIGHT (x) != NULL_TREE)
    PARENT (RIGHT (x)) = y;

  /* Link y's parent to x. */
  PARENT (x) = PARENT (y);
  if (PARENT (y) == NULL_TREE)
    root = x;
  else if (y == RIGHT (PARENT (y)))
    RIGHT (PARENT (y)) = x;
  else LEFT (PARENT (y)) = x;

  /* Put y on x's RIGHT. */
  RIGHT (x) = y;
  PARENT (y) = x;
}


/* Private helper function.  Inserts a given block into the range tree based
 * on the address range of the 68k code it occupies.  Does *NOT* attempt
 * to keep the tree balanced, and will happily violate the red-black
 * constraints.  Returns the one-node subtree it creates.
 */
static Tree
simple_tree_insert (Block *b)
{
  Tree t, *tp, parent = root;
  const syn68k_addr_t addr = b->m68k_start_address;
  
  /* First insert the block into the tree normally. */
  if (root == NULL_TREE)
    tp = &root;
  else
    {
      while (1)
	{
	  if (addr > (TREE_TO_BLOCK (parent))->m68k_start_address)
	    tp = &RIGHT (parent);
	  else tp = &LEFT (parent);
	  if (*tp == NULL_TREE)
	    {
	      *tp = BLOCK_TO_TREE (b);
	      break;
	    }
	  else parent = *tp;
	}
    }
  
  *tp = t = BLOCK_TO_TREE (b);
  PARENT (t) = parent;
  LEFT (t) = RIGHT (t) = NULL_TREE;

  return t;
}


/* Inserts a given block into the range tree based on the address range of
 * the 68k code it occupies.
 */
void
range_tree_insert (Block *b)
{
  Tree x = simple_tree_insert (b), y;

  SET_COLOR (x, RED);

  while (x != root && IS_RED (PARENT (x)))
    {
      if (PARENT (x) == LEFT (GRANDPARENT (x)))
	{
	  y = RIGHT (GRANDPARENT (x));
	  if (IS_RED (y))
	    {
	      SET_COLOR (PARENT (x), BLACK);
	      SET_COLOR (y, BLACK);
	      SET_COLOR (GRANDPARENT (x), RED);
	      x = GRANDPARENT (x);
	    }
	  else
	    {
	      if (x == RIGHT (PARENT (x)))
		{
		  x = PARENT (x);
		  left_rotate (x);
		}
	      SET_COLOR (PARENT (x), BLACK);
	      SET_COLOR (GRANDPARENT (x), RED);
	      right_rotate (GRANDPARENT (x));
	    }
	}
      else
	{
	  y = LEFT (GRANDPARENT (x));
	  if (IS_RED (y))
	    {
	      SET_COLOR (PARENT (x), BLACK);
	      SET_COLOR (y, BLACK);
	      SET_COLOR (GRANDPARENT (x), RED);
	      x = GRANDPARENT (x);
	    }
	  else
	    {
	      if (x == LEFT (PARENT (x)))
		{
		  x = PARENT (x);
		  right_rotate (x);
		}
	      SET_COLOR (PARENT (x), BLACK);
	      SET_COLOR (GRANDPARENT (x), RED);
	      left_rotate (GRANDPARENT (x));
	    }
	}
    }

  SET_COLOR (root, BLACK);
}


/* Helper function.  Finds the Tree element with the smallest key
 * greater than x's, or NULL_TREE if no such element exists.
 */
static Tree
tree_successor (Tree x)
{
  Tree y;

  if (RIGHT (x) != NULL_TREE)
    {
      x = RIGHT (x);
      while (LEFT (x) != NULL_TREE)
	x = LEFT (x);
      return x;
    }
  
  y = PARENT (x);
  while (y != NULL_TREE && x == RIGHT (y))
    {
      x = y;
      y = PARENT (y);
    }

  return y;
}


/* Removes a given Block from the range tree. */
void
range_tree_remove (Block *b)
{
  Tree w, x, y, z = BLOCK_TO_TREE (b);
  int y_color;

  if (LEFT (z) == NULL_TREE || RIGHT (z) == NULL_TREE)
    y = z;
  else
    y = tree_successor (z);

  if (LEFT (y) != NULL_TREE)
    x = LEFT (y);
  else x = RIGHT (y);

  PARENT (x) = PARENT (y);
  if (PARENT (y) == NULL_TREE)
    root = x;
  else if (y == LEFT (PARENT (y)))
    LEFT (PARENT (y)) = x;
  else RIGHT (PARENT (y)) = x;
  
  y_color = COLOR (y);
  if (y != z)
    {
      if (PARENT (z) == NULL_TREE)
	root = y;
      else if (z == LEFT (PARENT (z)))
	LEFT (PARENT (z)) = y;
      else RIGHT (PARENT (z)) = y;

      LEFT (y) = LEFT (z);
      RIGHT (y) = RIGHT (z);
      PARENT (y) = PARENT (z);
      SET_COLOR (y, COLOR (z));

      if (LEFT (y) != NULL_TREE)
	PARENT (LEFT (y)) = y;
      if (RIGHT (y) != NULL_TREE)
	PARENT (RIGHT (y)) = y;
      if (PARENT (NULL_TREE) == z)
	PARENT (NULL_TREE) = y;
    }

  if (y_color == BLACK)
    {
      while (x != root && IS_BLACK (x))
	{
	  if (x == LEFT (PARENT (x)))
	    {
	      w = RIGHT (PARENT (x));

	      assert (w != NULL && w != NULL_TREE);

	      if (IS_RED (w))
		{
		  SET_COLOR (w, BLACK);
		  SET_COLOR (PARENT (x), RED);
		  left_rotate (PARENT (x));
		  w = RIGHT (PARENT (x));
		}
	      if (IS_BLACK (LEFT (w)) && IS_BLACK (RIGHT (w)))
		{
		  SET_COLOR (w, RED);
		  x = PARENT (x);
		}
	      else
		{
		  if (IS_BLACK (RIGHT (w)))
		    {
		      SET_COLOR (LEFT (w), BLACK);
		      SET_COLOR (w, RED);
		      right_rotate (w);
		      w = RIGHT (PARENT (x));
		    }

		  SET_COLOR (w, COLOR (PARENT (x)));
		  SET_COLOR (PARENT (x), BLACK);
		  SET_COLOR (RIGHT (w), BLACK);
		  left_rotate (PARENT (x));
		  x = root;
		}
	    }
	  else
	    {
	      w = LEFT (PARENT (x));

	      assert (w != NULL && w != NULL_TREE);

	      if (IS_RED (w))
		{
		  SET_COLOR (w, BLACK);
		  SET_COLOR (PARENT (x), RED);
		  right_rotate (PARENT (x));
		  w = LEFT (PARENT (x));
		}
	      if (IS_BLACK (RIGHT (w)) && IS_BLACK (LEFT (w)))
		{
		  SET_COLOR (w, RED);
		  x = PARENT (x);
		}
	      else
		{
		  if (IS_BLACK (LEFT (w)))
		    {
		      SET_COLOR (RIGHT (w), BLACK);
		      SET_COLOR (w, RED);
		      left_rotate (w);
		      w = LEFT (PARENT (x));
		    }

		  SET_COLOR (w, COLOR (PARENT (x)));
		  SET_COLOR (PARENT (x), BLACK);
		  SET_COLOR (LEFT (w), BLACK);
		  right_rotate (PARENT (x));
		  x = root;
		}
	    }
	}

      SET_COLOR (x, BLACK);
    }
}


#ifdef DEBUG
static BOOL
range_tree_verify_aux (Tree t)
{
  BOOL ok = YES;

  if (t == NULL_TREE)
    return YES;
  if (IS_RED (t))
    {
      if (!IS_BLACK (LEFT (t)) || !IS_BLACK (RIGHT (t)))
	{
	  fprintf (stderr, "Internal inconsistency: red node does not have "
		   "two black children!\n");
	  ok = NO;
	}
    }

  if ((LEFT (t) != NULL_TREE && PARENT (LEFT (t)) != t)
      || (RIGHT (t) != NULL_TREE && PARENT (RIGHT (t)) != t))
    fprintf (stderr, "Internal inconsistency: child does not have the correct "
	     "parent.\n");

  if (LEFT (t) != NULL_TREE && ((TREE_TO_BLOCK (LEFT (t)))->m68k_start_address
				>= (TREE_TO_BLOCK (t))->m68k_start_address))
    fprintf (stderr, "Internal inconsistency: Left node has key >= to that "
	     "of its parent.\n");
      
  if (RIGHT (t) != NULL_TREE
      && (TREE_TO_BLOCK (RIGHT (t))->m68k_start_address
	  <= (TREE_TO_BLOCK (t))->m68k_start_address))
    fprintf (stderr, "Internal inconsistency: Right node has key <= to that "
	     "of its parent.\n");

  if (!range_tree_verify_aux (LEFT (t)))
    ok = 0;
  if (!range_tree_verify_aux (RIGHT (t)))
    ok = 0;

  return ok;
}


static void
path_length_extrema (Tree t, uint32 *longest, uint32 *shortest)
{
  uint32 l1, l2, s1, s2;

  if (t == NULL_TREE)
    {
      *longest = 0;
      *shortest = 0;
    }
  else
    {
      path_length_extrema (LEFT (t), &l1, &s1);
      path_length_extrema (RIGHT (t), &l2, &s2);

      *longest = MAX (l1, l2) + 1;
      *shortest = MIN (s1, s2) + 1;
    }
  
}


static uint32
black_length (Tree t)
{
  uint32 l, r;

  if (t == NULL_TREE)
    return 1;
  
  l = black_length (LEFT (t));
  r = black_length (RIGHT (t));

  if (l != r)
    {
      fprintf (stderr, "Internal inconsistency: black lengths don't match "
	       "(%lu/%lu).\n", l, r);
    }
  
  return l + (IS_BLACK (t) ? 1 : 0);
}


BOOL
range_tree_verify ()
{
  BOOL ok = range_tree_verify_aux (root);
  uint32 longest = 0, shortest = 0;

  path_length_extrema (root, &longest, &shortest);

  if (longest > 5 * shortest / 2)
    {
      fprintf (stderr, "Internal inconsistency: longest path in range tree "
	       "is more than 2.5 x as long as the shortest path (%lu/%lu)\n",
	       longest, shortest);
      ok = NO;
    }
#if 0
  printf ("Longest path = %lu, shortest path = %lu, ratio = %f\n",
	  longest, shortest, shortest == 0 ? 0 : (double) longest / shortest);
#endif

  black_length (root);

  if (!IS_BLACK (NULL_TREE))
    {
      fprintf (stderr, "Internal inconsistency: NULL_TREE color is "
	       "not black!\n");
      ok = NO;
    }

  /* FIXME - check for overlapping ranges here. */

  return ok;
}
#endif


#ifdef DEBUG
static void
dump_tree_aux (Tree t)
{
  if (t == NULL_TREE)
    return;
  printf ("Node %lu\t%c\t", (TREE_TO_BLOCK (t))->m68k_start_address,
	  IS_BLACK(t) ? 'B' : 'R');
  if (PARENT (t) == NULL_TREE)
    fputs ("p:<nil>\t", stdout);
  else printf ("p:%lu\t", (TREE_TO_BLOCK (PARENT (t)))->m68k_start_address);
  if (LEFT (t) == NULL_TREE)
    fputs ("l:<nil>\t", stdout);
  else printf ("l:%lu\t", (TREE_TO_BLOCK (LEFT (t)))->m68k_start_address);
  if (RIGHT (t) == NULL_TREE)
    fputs ("r:<nil>\n", stdout);
  else printf ("r:%lu\n", (TREE_TO_BLOCK (RIGHT (t)))->m68k_start_address);

  dump_tree_aux (LEFT (t));
  dump_tree_aux (RIGHT (t));
}


void
range_tree_dump ()
{
  dump_tree_aux (root);
}
#endif
