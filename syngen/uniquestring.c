#include "hash.h"
#include "uniquestring.h"
#include "common.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>


static SymbolTable *sym = NULL;

void
init_unique_string ()
{
  if (sym == NULL)
    sym = make_symbol_table ();
}


const char *
unique_string (const char *s)
{
  SymbolInfo si;
  const char *p;

  if (lookup_symbol (sym, s, &si, &p) != HASH_NOERR)
    {
      char *newp;

      newp = malloc (strlen (s) + 1);
      assert(newp);
      strcpy (newp, s);
      insert_symbol (sym, newp, si);
      p = newp;
    }

  return p;
}
