#include <stdlib.h>
#include <stdio.h>
#include "readprofile.h"
#include "frequency.h"

int
main (int argc, char *argv[])
{
  /* Check arguments. */
  if (argc != 2)
    {
      fprintf (stderr, "Usage: %s <profile file>\n", argv[0]);
      exit (1);
    }

  /* Read in the profile file. */
  read_profile (argv[1]);

  /* Process it. */
  generate_frequency_report ();

  return EXIT_SUCCESS;
}
