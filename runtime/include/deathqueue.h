#ifndef _deathqueue_h_
#define _deathqueue_h_

#include "block.h"

extern Block *death_queue_head, *death_queue_tail;
extern void death_queue_enqueue (Block *b);
extern void death_queue_dequeue (Block *b);

#endif  /* Not _deathqueue_h_ */
