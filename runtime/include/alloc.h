#ifndef _alloc_h_
#define _alloc_h_

#include <stddef.h>    /* typedef size_t */

extern void *xmalloc (size_t size);
extern void *xrealloc (void *old, size_t new_size);
extern void *xcalloc (size_t num_elems, size_t byte_size);

#endif  /* Not _alloc_h_ */
