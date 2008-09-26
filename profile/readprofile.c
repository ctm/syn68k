#include "readprofile.h"
#include "syn68k_public.h"
#include <stdio.h>
#include <stdlib.h>

unsigned long instruction_count[65536];

void
read_profile (const char *filename)
{
  FILE *fp;

  fp = fopen (filename, "rb");
  if (fp == NULL)
    {
      perror ("Unable to open file");
      exit (-1);
    }

  /* Read in the data. */
  if (fread (instruction_count, sizeof (unsigned long), 65536, fp) != 65536)
    {
      fprintf (stderr, "Premature end of file!\n");
      exit (-3);
    }

  /* Byte swap if necessary. */
#ifdef LITTLEENDIAN
    {
      unsigned long *p = instruction_count;
      int i;

      for (i = 65535; i >= 0; p++, i--)
	*p = SWAPUL (*p);
    }
#endif

  fclose (fp);
}
