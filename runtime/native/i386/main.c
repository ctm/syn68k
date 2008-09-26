#include "template.h"
#include "process.h"
#include <stdlib.h>


int
main (int argc, char *argv[])
{
#ifdef GENERATE_NATIVE_CODE
  FILE *fp, *header_fp;
  int i, success, swapop_p;

  success = SUCCESS;  /* default */

  if (argc != 2)
    {
      fprintf (stderr, "Usage: %s <make>\n", argv[0]);
      exit (EXIT_FAILURE);
    }

  header_fp = fopen ("i386-isa.h", "w");
  if (header_fp == NULL)
    {
      fprintf (stderr, "Unable to open \"i386-isa.h\" for writing.\n");
      exit (-1);
    }
  fputs ("/* This file is machine-generated; DO NOT EDIT! */\n"
	 "\n"
	 "#ifndef _I386_ISA_H_\n"
	 "#define _I386_ISA_H_\n"
	 "\n"
	 "#ifdef GENERATE_NATIVE_CODE\n"
	 "\n",
	 header_fp);

  swapop_p = FALSE;
  for (i = 0; success == SUCCESS && template[i].macro_name != NULL;
       i += swapop_p, swapop_p = !swapop_p)
    {
      char cmd[1024], c_file[1024], o_file[1024];
      
      /* Only bother swapping those that have exactly two operands. */
      /* Swapop hack is no longer useful. */
      if (swapop_p /* && count_operands (template[i].code) != 2 */)
	continue;

      if (!swapop_p)
	{
	  printf ("Processing %s...", template[i].macro_name);
	  fflush (stdout);
	}

      sprintf (c_file, "i386_stubs/_%s.c", template[i].macro_name);
      sprintf (o_file, "_i386_%d.o", i * 2 + swapop_p);/* Short for archive. */
      fp = fopen (c_file, "w");
      if (fp == NULL)
	{
	  fprintf (stderr, "Unable to open file \"%s\" for writing.\n",
		   c_file);
	  exit (EXIT_FAILURE);
	}
      if (process_template (fp, header_fp, &template[i], argv[1], swapop_p)
	  == FAILURE)
	success = FAILURE;
      if (!swapop_p)
	puts ("done.");
      if (template[i + 1].macro_name != NULL)
	putc ('\n', fp);  /* Add an extra blank line. */
      fclose (fp);

      sprintf (cmd, "%s -s nextobj NEW_C_FILE=%s NEW_O_FILE=%s > /dev/null",
	       argv[1], c_file, o_file);
      if (system (cmd))
	{
	  fprintf (stderr, "Unable to make \"%s\".\n", c_file);
	  success = FAILURE;
	}
    }

  fputs ("#endif   /* GENERATE_NATIVE_CODE */\n"
	 "\n"
	 "#endif  /* !_I386_ISA_H_ */\n", header_fp);
  fclose (header_fp);

  return (success == SUCCESS) ? EXIT_SUCCESS : EXIT_FAILURE;
#else  /* !GENERATE_NATIVE_CODE */
  return EXIT_SUCCESS;
#endif /* !GENERATE_NATIVE_CODE */
}
