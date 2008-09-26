#ifndef _profile_h_
#define _profile_h_

#ifdef PROFILE

#include "block.h"

extern void profile_block (Block *b);
extern void dump_profile (const char *file);

#endif

#endif  /* Not _profile_h_ */
