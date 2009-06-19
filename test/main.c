#include "syn68k_public.h"
#include "driver.h"
#include "callemulator.h"
#include "testtrap.h"
#include "crc.h"
#include "testrt.h"
#include "testqsort.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif


int
main (int argc, char *argv[])
{
  static uint32 trap_vectors[64];
  uint32 count = 10000;
  int i;
  int native_p;

  native_p = 1;

#ifdef NeXT
  malloc_debug (31);   /* Just to be safe. */
#endif

  ROMlib_offset = MEMORY_OFFSET;

  /* Set up default values for command line switches. */
  test_only_non_cc_variants = 0;
#ifdef mc68000
  generate_crc = 0;
#else
  generate_crc = 1;
#endif

  /* Grab command-line switches. */
  for (i = 1; i < argc; i++)
    {
#ifdef mc68000
      if (!strcmp (argv[i], "-crc"))
	generate_crc = 1;
      else
#endif
            if (!strcmp (argv[i], "-noncc"))
	test_only_non_cc_variants = 1;
      else if (strcmp (argv[i], "-notnative") == 0)
	native_p = 0;
      else if (atoi (argv[i]) != 0)
	count = atoi (argv[i]);
      else
	{
#ifdef mc68000
	  fprintf (stderr, "Usage: %s [test count] [-crc] [-noncc] [-notnative]\n",
		   argv[0]);
#else
	  fprintf (stderr, "Usage: %s [test count] [-noncc] [-notnative]\n",
		   argv[0]);
#endif
	  exit (-1);
	}
    }

  /* Initialize stuff. */
  initialize_68k_emulator (NULL, native_p, trap_vectors, 0);

#if 0
  test_rangetree ();
  exit (0);
#endif

#if 0
  test_qsort ();
  dump_profile ("../profile/instrfreq");
  exit (0);
#endif

#if 0
  test_traps ();
#else
  /* Run through test suite. */
  test_all_instructions (count);
#endif

  return EXIT_SUCCESS;
}

#if 0 && defined(GO32)
void __main(void)
{
}
#endif
