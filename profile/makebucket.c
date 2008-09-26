#include "bucket.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include "amode.h"

#if 0
#define IS_MOVE(n, x)  (((n) & 0xC000) == 0 && (((n) & 0x3000) != 0) \
			&& !strncmp ((x), "move", 4))
#endif


int
main (int argc, char *argv[])
{
  int num_buckets;
  static unsigned short bucket_map[65536];
  static Bucket bucket[65536];
  static struct {
    unsigned char has_amode, has_reversed_amode;
    const char *name;
  } amode_info[65536];
  int num_amode_infos = 0;
  int pass;
  int i;

  if (argc != 2)
    {
      fprintf (stderr, "Usage: %s <file>", argv[0]);
      exit (-1);
    }

  num_buckets = 1;
  bucket[0].name = "unmapped_spew";

  for (pass = 2; pass >= 0; pass--)
    {
      FILE *fp;
      char cmd[1024];

      sprintf (cmd, "zcat %s", argv[1]);
      fp = popen (cmd, "r");
      assert (fp != NULL);

      while (!feof (fp))
	{
	  int opcode, has_amode, has_reversed_amode, litmask, litbits;
	  char name[1024]; /* Fixed length arrays usually bad, but not here. */
	  
	  if (fscanf (fp, " %d %d %d %d %d %s", &opcode, &has_amode,
		      &has_reversed_amode, &litmask, &litbits, name) != 6)
	    break;
	  
	  /* Make sure we have the right # of amodes for this pass. */
	  if (has_amode + has_reversed_amode != pass)
	    continue;

	  /* See if anything else with the same root name exists, and if it
	   * does, assume we have at least as many addressing modes as that
	   * guy.  Why?  Well, the syngen process loses info about the
	   * existence of amodes/reverse amodes in favor of rewriting the
	   * 68k.scm code you give it to special case amodes like registers.
	   * This hack will try to determine if there were originally amodes
	   * in this opcode.
	   */
	  for (i = num_amode_infos - 1; i >= 0; i--)
	    if (!strcmp (amode_info[i].name, name))
	      {
		has_amode |= amode_info[i].has_amode;
		has_reversed_amode |= amode_info[i].has_reversed_amode;
		break;
	      }

	  /* If we didn't find a match, add a new entry to the table and
	   * remember what amodes it has.
	   */
	  if (i < 0)
	    {
	      amode_info[num_amode_infos].has_amode = has_amode;
	      amode_info[num_amode_infos].has_reversed_amode
		= has_reversed_amode;
	      amode_info[num_amode_infos].name
		= strcpy (malloc (strlen (name) + 1), name);
	      num_amode_infos++;
	    }

	  /* Generate the canonical name. */
	  if (has_amode)
	    sprintf (name + strlen (name), " %s",
		     amode_to_string (opcode & 0x3F));
	  if (has_reversed_amode)
	    sprintf (name + strlen (name), ", %s",
		     amode_to_string (((opcode >> 9) & 7)
				      | ((opcode >> 3) & 0x38)));
	  
	  /* Look for a bucket with the same canonical name.
	   * Isn't O(n^2) great?
	   */
	  for (i = num_buckets - 1; i >= 0; i--)
	    if (!strcmp (bucket[i].name, name)
		&& bucket[i].litmask == litmask
		&& bucket[i].litbits == litbits)
	      {
		bucket_map[opcode] = i;
		break;
	      }
	  
	  /* Did we fail to find a matching bucket?  If so, make a new one. */
	  if (i < 0)
	    {
	      Bucket *b = &bucket[num_buckets++];
	      b->name    = strcpy (malloc (strlen (name) + 1), name);
	      b->litbits = litbits;
	      b->litmask = litmask;
	      b->count   = 0;
	      bucket_map[opcode] = b - bucket;
	    }
	}
      pclose (fp);
    }

  printf ("#include \"bucket.h\"\n"
	  "\n"
	  "Bucket bucket[%d] = {\n",
	  num_buckets);
  for (i = 0; i < num_buckets; i++)
    printf ("  { \"%s\", 0 },\n", bucket[i].name);
  puts ("};\n"
	"\n"
	"const unsigned short bucket_map[65536] = {");

  for (i = 0; i < 65536; i++)
    {
      if (i % 8 == 0)
	fputs ("\n ", stdout);
      printf (" 0x%04X,", (unsigned) bucket_map[i]);
    }

  printf ("\n"
	  "};\n"
	  "const int num_buckets = %d;\n",
	  num_buckets);

  return EXIT_SUCCESS;
}
