#include "deathqueue.h"


Block *death_queue_head = NULL, *death_queue_tail = NULL;


/* Appends a block to the end of the doubly-linked death queue. */
void
death_queue_enqueue (Block *b)
{
  b->death_queue_prev = death_queue_tail;
  b->death_queue_next = NULL;

  if (death_queue_tail == NULL)
    death_queue_head = b;
  else
    death_queue_tail->death_queue_next = b;

  death_queue_tail = b;
}


/* Removes a block from the end of the doubly-linked death queue. */
void
death_queue_dequeue (Block *b)
{
  Block *p, *n;

  p = b->death_queue_prev;
  n = b->death_queue_next;

  if (p == NULL)
    {
      if (death_queue_head == b)  /* Verify this just to be safe. */
	death_queue_head = n;
    }
  else
    p->death_queue_next = n;

  if (n == NULL)
    {
      if (death_queue_tail == b)  /* Verify this just to be safe. */
	death_queue_tail = p;
    }
  else
    n->death_queue_prev = p;

  b->death_queue_prev = b->death_queue_next = NULL;
}
