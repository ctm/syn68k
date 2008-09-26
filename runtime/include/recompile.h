#ifndef _recompile_h_
#define _recompile_h_

#ifdef GENERATE_NATIVE_CODE

#include "block.h"
#include "deathqueue.h"

extern void recompile_block_as_native (Block *b);
extern double native_fraction (void);

/* This is the number of times a nonnative block must be called before
 * we scrap it and recompile it as native.
 */
#define RECOMPILE_CUTOFF 50

/* This is how many times a descendent of a nonnative block about to be
 * recompiled must have been called before we decide to smash it as
 * well.  Smashing nearby blocks avoids extra recompiles.
 */
#define RECOMPILE_CHILD_CUTOFF 35


#endif  /* GENERATE_NATIVE_CODE */

#endif  /* !_recompile_h_ */
