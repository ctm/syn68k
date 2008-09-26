#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "hash.h"

/* This file contains:
 *   make_symbol_table() - returns a new, empty symbol table.
 *   free_symbol_table() - deallocates all memory associated with an s.t.
 *   insert_symbol()     - inserts a key/value pair into a symbol table.
 *                         NOTE: key is NOT duplicated!
 *   lookup_symbol()     - returns the value associated with a key.
 *   dump_symbol_table() - prints out a symbol table in human-readable form.
 */


static int hash_code (const char *string);


/* Returns a new, empty symbol table needed for subsequent symbol table
 * manipulations.
 */

SymbolTable *
make_symbol_table (void)
{
  SymbolTable *s = malloc (sizeof (SymbolTable));
  Bucket **b;
  int i;

  for (i = 0, b = &s->bucket[0]; i < HASH_BUCKETS; i++, b++)
    {
      *b = (Bucket *) malloc (sizeof (Bucket));
      (*b)->num_symbols = 0;
      (*b)->max_symbols = INITIAL_SYMBOLS;
    }

  return s;
}


/* Deallocates all memory taken by a symbol table. */

void
free_symbol_table (SymbolTable *s)
{
  int i;

  for (i = 0; i < HASH_BUCKETS; i++)
    free (s->bucket[i]);
  free (s);
}


/* Inserts a symbol and an associated value into a symbol table. If successful,
 * returns HASH_NOERR.  If that symbol is already defined, it returns
 * HASH_DUPLICATE.  Note that the string NAME is not duplicated!  Only the
 * pointer is copied, so be sure not to deallocate NAME, or the symbol table
 * will become corrupt.  VAL is duplicated.
 */

HashErr
insert_symbol (SymbolTable *s, const char *name, SymbolInfo val)
{
  int i, bucket_num = hash_code (name);
  Symbol *sym;
  Bucket *b;
  char firstc = *name;

  b = s->bucket[bucket_num];
  
  /* First, check for a duplicate. */
  for (i = b->num_symbols, sym = &b->symbol[0]; i > 0; sym++, i--)
    if (firstc == sym->name[0] && !strcmp (name, sym->name))
      return HASH_DUPLICATE;

  /* If there isn't room, double the size of the bucket. */
  if (b->num_symbols >= b->max_symbols)
    {
      b->max_symbols *= 2;
      b = s->bucket[bucket_num] =
	(Bucket *) realloc (b, sizeof *b + ((b->max_symbols - INITIAL_SYMBOLS)
					    * sizeof (Symbol)));
    }

  sym = &b->symbol[b->num_symbols++];
  sym->name = name;
  sym->value = val;

  return HASH_NOERR;
}


/* Looks up a symbol in a symbol table.  If successful, it places the
 * associated value in *val, the original key in *original_name_ptr,
 * and returns HASH_NOERR.  If either val or original_name_ptr are NULL their
 * contents are not modified.  If not successful, *val, *original_name_ptr are
 * unchanged and HASH_NOTFOUND is returned.
 */

HashErr
lookup_symbol (const SymbolTable *s, const char *name, SymbolInfo *val,
	       const char **original_name_ptr)
{
  int i, bucket_num = hash_code (name);
  Bucket *b = s->bucket[bucket_num];
  Symbol *sym;
  char firstc = *name;

  for (i = b->num_symbols, sym = &b->symbol[0]; i > 0; sym++, i--)
    if (firstc == sym->name[0] && !strcmp (name, sym->name))
      {
	if (val != NULL)
	  *val = sym->value;
	if (original_name_ptr != NULL)
	  *original_name_ptr = sym->name;
	return HASH_NOERR;
      }

  return HASH_NOTFOUND;
}


/* Prints out a symbol table in human-readable format. */

void
dump_symbol_table (const SymbolTable *s)
{
  const Bucket *b;
  int i, j, collisions;

  for (i = collisions = 0; i < HASH_BUCKETS; i++)
    {
      b = s->bucket[i];
      if (b->num_symbols > 0)
	{
	  printf ("Bucket #%d:\n---------------\n", i);
	  for (j = 0; j < b->num_symbols; j++)
	    printf ("\t\"%s\"\t:\t%ld\n", b->symbol[j].name,
		    b->symbol[j].value.n);
	  collisions += b->num_symbols - 1;
	}
    }

  printf ("Total collisions: %d\n", collisions);
}


static int
hash_code (const char *string)
{
  unsigned long c, h = 0;
  
  while ((c = *string++))
    {
      h += c;
      h = (h << 3) + (h >> 2) + c;
    }

  return (h % HASH_BUCKETS);
}
