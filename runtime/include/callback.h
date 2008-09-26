#ifndef _callback_h_
#define _callback_h_

#include "syn68k_private.h"
#include "block.h"

#define MAX_CALLBACKS 4352  /* Arbitrary...4096 plus some slop. */

#define CALLBACK_STUB_BASE   (MAGIC_ADDRESS_BASE + 128)
#define CALLBACK_STUB_LENGTH (MAX_CALLBACKS * sizeof (uint16))

#define IS_CALLBACK(n) (((syn68k_addr_t) (n)) - CALLBACK_STUB_BASE \
			< CALLBACK_STUB_LENGTH)

extern void callback_init (void);
extern syn68k_addr_t callback_install (callback_handler_t func,
				       void *arbitrary_argument);
extern void callback_remove (syn68k_addr_t m68k_address);
extern int callback_compile (Block *parent, uint32 m68k_address, Block **new);
extern void *callback_argument (syn68k_addr_t callback_address);
extern callback_handler_t callback_function (syn68k_addr_t callback_address);

#endif  /* Not callback_h_ */
