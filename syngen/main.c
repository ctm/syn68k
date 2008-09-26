/*
 *     main.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "list.h"
#include "token.h"
#include "common.h"
#include "parse.h"
#include "error.h"
#include "reduce.h"
#include "defopcode.h"
#include "uniquestring.h"

static void usage (const char *progname);
static void unrecognized (const char *progname, const char *badopt);

/* Global variables to control compilation options. */
int optimization_level;
BOOL preprocess_only;
FILE *syn68k_c_stream, *mapinfo_c_stream, *mapindex_c_stream;
FILE *profileinfo_stream;
BOOL verbose;

int
main (int argc, char *argv[])
{
  SymbolTable *sym = make_symbol_table ();
  const char *include_dirs[32] = { ".", NULL };
  static const char *current_dir[] = { ".", NULL };
  int include_dir_index = 1;
  int i;

  init_tokenizer ();
  init_unique_string ();

  /* Set up global variables with default values. */
  optimization_level = 0;
  preprocess_only    = FALSE;
  verbose            = FALSE;
  syn68k_c_stream    = NULL;
  mapinfo_c_stream   = NULL;
  mapindex_c_stream  = NULL;
  profileinfo_stream = NULL;

  /* Parse command line arguments. */
  for (i = 1; i < argc; i++)
    {
      if (argv[i][0] != '-')
	{
	  open_file (argv[i], current_dir);

	  /* Open output code streams. */
	  if (!preprocess_only)
	    {
	      syn68k_c_stream    = fopen ("syn68k.c",    "w");
	      mapinfo_c_stream   = fopen ("mapinfo.c",   "w");
	      mapindex_c_stream  = fopen ("mapindex.c",  "w");
	      profileinfo_stream = fopen ("profileinfo", "w");
	    }

	  /* Initialize code generator and output header stuff. */
	  begin_generating_code ();

	  /* Munch through everything and generate appropriate stuff. */
	  parse_all_expressions (sym);

	  /* Do any necessary cleanup. */
	  done_generating_code ();

	  /* Close up all streams. */
	  if (!preprocess_only)
	    {
	      fclose (syn68k_c_stream);    syn68k_c_stream    = NULL;
	      fclose (mapinfo_c_stream);   mapinfo_c_stream   = NULL;
	      fclose (mapindex_c_stream);  mapindex_c_stream  = NULL;
	      fclose (profileinfo_stream); profileinfo_stream = NULL;
	    }
	  
	  continue;
	}

      switch (argv[i][1]) {

      case 's':
	if (!strcmp (argv[i], "-stdin"))
	  {
	    open_stream ("<standard input>", stdin);
	    parse_all_expressions (sym);
	  }
	else unrecognized (argv[0], argv[i]);
	break;

      case 'I':
	if (argv[i][2] == '\0')
	  fatal_error ("Missing include directory for -I option.\n");
	if (include_dir_index >= sizeof include_dirs / sizeof include_dirs[0])
	  fatal_error ("Too many -I directories specified.\n");

	include_dirs[include_dir_index++] = argv[i] + 2;
	include_dirs[include_dir_index]   = NULL;
	break;

      case 'E':
	if (argv[i][2] != '\0')
	  unrecognized (argv[0], argv[i]);
	preprocess_only = TRUE;
	break;

      case 'O':
	if (argv[i][2] == '\0')
	  optimization_level = 1;
	else optimization_level = atoi (argv[i] + 2);
	break;
	
      case 'v':
	if (argv[i][2] != '\0')
	  unrecognized (argv[0], argv[i]);
	verbose = TRUE;
	break;

      default:
	unrecognized (argv[0], argv[i]);
	break;
      }
    }

  return 0;
}


static void
unrecognized (const char *progname, const char *badopt)
{
  error ("Unrecognized option \"%s\".\n", badopt);
  usage (progname);
}


/* Prints out usage and exits. */
static void
usage (const char *progname)
{
  fatal_error ("Usage: %s <usage entered when stabilized>\n", progname);
}
