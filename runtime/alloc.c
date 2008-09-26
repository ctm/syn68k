#include "alloc.h"
#include "destroyblock.h"
#include <stdio.h>
#include <stdlib.h>

void *
xmalloc (size_t size)
{
  void *p;

  while ((p = malloc (size)) == NULL)
    {
      if (!destroy_any_block ())
	{
	  fprintf (stderr, "Out of memory.  Tried to alloc 0x%lX bytes.\n",
		   (unsigned long) size);
	  exit (-1);
	}
    }

  return p;
}


void *
xrealloc (void *old, size_t new_size)
{
  void *p;

  while ((p = realloc (old, new_size)) == NULL && new_size)
    {
      if (!destroy_any_block ())
	{
	  fprintf (stderr, "Out of memory.  Tried to realloc 0x%lX bytes.\n",
		   (unsigned long) new_size);
	  exit (-1);
	}
    }

  return p;
}


void *
xcalloc (size_t num_elems, size_t byte_size)
{
  void *p;
  
  while ((p = calloc (num_elems, byte_size)) == NULL)
    {
      if (!destroy_any_block ())
	{
	  fprintf (stderr, "Out of memory.  Tried to calloc 0x%lX bytes.\n",
		   (unsigned long) (num_elems * byte_size));
	  exit (-1);
	}
    }

  return p;
}
