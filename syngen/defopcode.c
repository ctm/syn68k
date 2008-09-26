#include "common.h"
#include "defopcode.h"
#include "error.h"
#include "bitstring.h"
#include "generatecode.h"
#include "byteorder.h"
#include "reduce.h"
#include "syn68k_private.h"
#include "uniquestring.h"
#include "safe_alloca.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>

/* Global variables for this file. */
static OpcodeMappingInfo opcode_map_info[65536];
static const char *map_info_opcode_name[65536];
static uint16 map_index[65536];
static unsigned char synthetic_opcode_taken[65536];    /* Not booleans. */
static int num_map_infos;
static int parity;

/* Private helper functions. */
static void compute_optimal_shifts (const ParsedOpcodeInfo *info, int *shift,
				    List *isect_list, int literal_bits,
				    int literal_bits_mask);
static void compute_literal_bits (const char *pattern, int *lbp, int *lbmp);
static int compute_dashmask (const char *pattern);
static int compute_synthetic_opcode (int m68k_opcode,
				     const OpcodeMappingInfo *map);
static void reserve_synthetic_ops (OpcodeMappingInfo *maps, int num_variants,
				   int variant, int num_variant_mapping_sets,
				   List *isect_list, int literal_bits,
				   int literal_bits_mask);
static BOOL has_unexpanded_register_lvalue (List *code,
					    const char *opcode_bits,
					    const char *bits_to_expand,
					    int *index);
static void delete_field (List *code, int field_number, int val);
static void replace_dollar_number_with_list (List *code, int field,
					     List *list);

#define NO_MAP 0
#define OPCODE_TAKEN 0xFF
#define OPCODE_NOT_TAKEN 0xFE
#define SYNTHETIC_OPCODE_TAKEN(op, variant) \
  (synthetic_opcode_taken[(op) & 0xFFFF] != OPCODE_NOT_TAKEN \
   && synthetic_opcode_taken[(op) & 0xFFFF] != (variant))


/* Call this once before calling generate_opcode () for the first time.
 * Call done_generating_code () after you have called generate_opcode () for
 * the last time.
 */
void
begin_generating_code ()
{
  int i;

  /* Clear mapping table to NO_MAP. */
  for (i = 0; i < 65536; i++)
    map_index[i] = NO_MAP;

  for (i = 0; i < 65536; i++)
    synthetic_opcode_taken[i] = OPCODE_NOT_TAKEN;

  /* Zero the other arrays. */
  memset (opcode_map_info,        0, sizeof opcode_map_info);
  memset (map_info_opcode_name,   0, sizeof map_info_opcode_name);

  /* Set up default "no mapping" map; maps to opcode 0. */
  opcode_map_info[NO_MAP].cc_needed          = M68K_CC_ALL;
  opcode_map_info[NO_MAP].sequence_parity    = 0;
  opcode_map_info[NO_MAP].instruction_words  = 1;
  opcode_map_info[NO_MAP].ends_block         = TRUE;
  opcode_map_info[NO_MAP].next_block_dynamic = TRUE;
  map_info_opcode_name[0] = "(reserved)";

  /* Opcodes 0 through 0xB4 are reserved. */
  for (i = 0; i <= 0xB4; i++)
    synthetic_opcode_taken[i] = OPCODE_TAKEN;

  /* We've used one opcode map, and should now be on odd parity for the
   * next block of opcode maps.
   */
  num_map_infos = 1;
  parity = 1;

  if (!preprocess_only)
    {
      FILE *fp = fopen ("syn68k_header.c", "r");
      char buf[1024];
      size_t size;

      if (fp == NULL)
	fatal_error ("Unable to open syn68k_header.c for reading!\n");

      /* Copy the C preamble out to syn68k.c. */
      while ((size = fread (buf, 1, 1024, fp)) != 0)
	fwrite (buf, 1, size, syn68k_c_stream);

      fclose (fp);
    }
}


/* Call this after you have called generate_opcode () for the last time. */
void
done_generating_code ()
{
  if (!preprocess_only)
    {
      long i, max_opcode = -1;

      /* Close up the main interpreter function. */
      fputs ("\n"
	     "#ifndef USE_DIRECT_DISPATCH\n"
	     "       }\n"
	     "    }\n"
	     "}\n"
	     "#else\n"
	     "/* This function is the gateway to the threaded code.\n"
	     " * It allocates a bunch of space on the stack so that\n"
	     " * (hopefully) there will be room for the stack slots in\n"
	     " * the functions it jumps into.  This is scary stuff,\n"
	     " * but hopefully it will work.  We put this function at\n"
	     " * the end of the file so it won't be inlined.\n"
	     " * And use __attribute__((noinline)) where it's supported.\n"
	     " */\n"
	     "static void\n"
	     "threaded_gateway (void)\n"
	     "{\n"
	     "volatile char buf[1024];  /* Allocate some stack space. */\n"
	     "memset ((char *)buf, 0, 1);  /* Use the buffer in some way. */\n"
	     "NEXT_INSTRUCTION (ROUND_UP (PTR_WORDS));\n"
	     "}\n"
	     "\n"
	     "\n"
	     "/* This array is used only by the compilation system. */\n",
	     syn68k_c_stream);

      /* Output the decls for the dispatch table array. */
      for (i = 0; i < 65536; i++)
	{
	  if (synthetic_opcode_taken[i] == OPCODE_TAKEN)
	    {
	      fprintf (syn68k_c_stream,
		       "extern int handle_opc_0x%04lX "
		       "asm (\"_S68K_HANDLE_0x%04lX\");\n",
		       (unsigned long) i, (unsigned long) i);
	      max_opcode = i;
	    }
	}

      /* Output the beginning of the dispatch table array. */
      if (max_opcode >= 0)
	{
	  fprintf (syn68k_c_stream,
		   "\n"
		   "\n"
		   "const void *direct_dispatch_table[%ld] = {\n",
		   max_opcode + 1);

	  for (i = 0; i <= max_opcode; i++)
	    {
	      if (synthetic_opcode_taken[i] == OPCODE_TAKEN)
		fprintf (syn68k_c_stream,
			 "  (void *) &handle_opc_0x%04lX%s\n",
			 (unsigned long) i,
			 (i == max_opcode) ? "" : ",");
	      else
		fprintf (syn68k_c_stream, "  (void *) 0%s\n",
			 (i == max_opcode) ? "" : ",");
	    }
	  
	  fputs ("};\n", syn68k_c_stream);
	}

      fputs ("#endif  /* USE_DIRECT_DISPATCH */\n", syn68k_c_stream);

      /* Output opcode map. */
      if (verbose)
	printf ("Outputting opcode map index table..."), fflush (stdout);

      /* Output preamble for mapindex_c_stream. */
      fputs ("#include \"syn68k_private.h\"\n"
	     "\n"
	     "const uint16 opcode_map_index[65536] = {\n"
	     , mapindex_c_stream);

      /* Print out all of the values w/big endian indices. */
      for (i = 0; i < 65536; i++)
	{
	  unsigned ix = map_index[i];
	  if (ix == 0)      /* Iff no map computed, make it behave */
	    ix = map_index[0x4AFC];  /* like ILLEGAL. */
	  fprintf (mapindex_c_stream, "%s0x%04X,", ((i % 8) == 0) ? "  " : "",
		   ix);
	  if ((i % 8) == 7)
	    fprintf (mapindex_c_stream, "  /* 0x%04X */\n", (unsigned) i - 7);
	  else putc (' ', mapindex_c_stream);
	}

      /* Output postamble for map index. */
      fputs ("};\n", mapindex_c_stream);

      if (verbose)
	puts ("done.");

      /* Output map info. */
      if (verbose)
	printf ("Outputting opcode map information table..."), fflush (stdout);

      /* Add a dummy entry at the end with a different parity so the last
       * map info sequence will be terminated.
       */
      opcode_map_info[num_map_infos].sequence_parity = parity;
      map_info_opcode_name[num_map_infos] = "Internal use: array terminator";
      num_map_infos++;

      /* Output preamble for mapinfo_c_stream. */
      fprintf (mapinfo_c_stream,
	       "#include \"syn68k_private.h\"\n"
	       "#ifdef GENERATE_NATIVE_CODE\n"
	       "#include \"native.h\"\n"
	       "#include \"native/i386/host-xlate.h\"\n"
	       "#include \"native/i386/xlate-aux.h\"\n"
	       "#endif\n"
	       "\n"
	       "const OpcodeMappingInfo opcode_map_info[%d] = {\n",
	       num_map_infos);

      /* Print out all of the structs. */
      for (i = 0; i < num_map_infos; i++)
	{
	  const OpcodeMappingInfo *m = &opcode_map_info[i];
	  int j;

	  /* Put blank line between distinct opcode sequences. */
	  if (i > 0 && strcmp (map_info_opcode_name[i],
			       map_info_opcode_name[i - 1]))
	    putc ('\n', mapinfo_c_stream);

	  /* Output the struct. */
	  fprintf (mapinfo_c_stream,
		   "  /* 0x%04X: %s */\n"
		   "  { %d, 0x%02X, 0x%02X, 0x%02X, %d, %d, %d, %d, %d, "
		   "%d, %d, %2d, 0x%04X, 0x%04X,\n",
		   (unsigned) i, map_info_opcode_name[i],
		   m->sequence_parity,
		   (unsigned) m->cc_may_set,
		   (unsigned) m->cc_may_not_set,
		   (unsigned) m->cc_needed,
		   m->instruction_words, m->ends_block,
		   m->next_block_dynamic,
		   m->amode_size, m->reversed_amode_size, m->amode_expanded,
		   m->reversed_amode_expanded,
		   m->opcode_shift_count, (unsigned) m->opcode_and_bits,
		   (unsigned) m->opcode_add_bits);

	  /* Output the bitfields. */
	  fputs ("   { ", mapinfo_c_stream);
	  for (j = 0; j < MAX_BITFIELDS; j++)
	    {
	      if (j != 0 && (j % 2) == 0)
		fputs ("\n     ", mapinfo_c_stream);
	      fprintf (mapinfo_c_stream, "{ %d, %d, %d, %d, %d, %d }%s",
		       m->bitfield[j].rev_amode, m->bitfield[j].index,
		       m->bitfield[j].length, m->bitfield[j].sign_extend,
		       m->bitfield[j].make_native_endian,
		       m->bitfield[j].words,
		       (j == sizeof m->bitfield / sizeof m->bitfield[0] - 1)
		       ? "" : ", ");
	    }
#ifdef GENERATE_NATIVE_CODE
	  fprintf (mapinfo_c_stream,
		   " },\n"
		   "     %s%s },\n",
		   (m->guest_code_descriptor == NULL) ? "" : "&",
		   ((m->guest_code_descriptor == NULL)
		    ? "NULL" : m->guest_code_descriptor));
#else
	  fputs (" } },\n", mapinfo_c_stream);
#endif
	}
	  
      /* Output postamble for map info. */
      fputs ("};\n", mapinfo_c_stream);
      
      if (verbose)
	puts ("done.");
    }
}


void
generate_opcode (ParsedOpcodeInfo *info, SymbolTable *sym)
{
#if	!defined(__GNUC__)
  extern void *alloca(int);
#endif
  OperandInfo *operand_info;
  OpcodeMappingInfo *base;
  OpcodeMappingInfo *mapping;
  int num_variants, i, m68kop;
  CCVariant *var, *v;
  int num_variant_mapping_sets = 0;
  List expand;
  List opcode_pattern = { /* &expand */ 0, NULL,
			    { TOK_QUOTED_STRING,
			      { 0 } /* info->opcode_bits */,
				"(internal: generate_opcode)", 0 } };
  List isect = { /* &opcode_pattern */ 0, NULL,
		 { TOK_INTERSECT, { "intersect" },
		       "(internal: generate_opcode)", 0 } };
  List isect_list = { NULL, /* &isect */ 0,
		      { TOK_LIST, { "[LIST]" },
			  "(internal: generate_opcode)", 0 } };
  int *dashmask, *shift;
  int *amode_size, *reversed_amode_size;
  BOOL *amode_expanded, *reversed_amode_expanded;
  int literal_bits, literal_bits_mask;
  int *unexpanded_synthetic_opcode;
  List **add_to_code_token;
  const char **postcode;
  SAFE_DECL();

  opcode_pattern.cdr = &expand;
  opcode_pattern.token.u.string = info->opcode_bits;
  isect.cdr = &opcode_pattern;
  isect_list.car = &isect;
  
  /* Provide feedback to the user. */
  if (verbose)
    printf ("Processing \"%s\"...", info->name), fflush (stdout);

  /* Count the number of CC variants we have. */
  for (num_variants = 0, var = info->cc_variant; var != NULL; var = var->next)
    num_variants++;

#if 0
  if (verbose)
    putchar ('\n');
  printf ("Entering with bits = %s\n", info->opcode_bits);
#endif

  /* If no variants (weird), don't do anything. */
  if (num_variants == 0)
    {
      if (verbose)
	puts ("done.");
      return;
    }

  /* Add legal addressing modes to intersection. */
  expand = *info->amode;
  expand.cdr = NULL;

  /* Compute those opcode bits which must be either 0 or 1, so we only
   * call is_member_of_set() when there is a chance a bit pattern could
   * match.  Purely a speed heuristic.
   */
  compute_literal_bits (info->opcode_bits, &literal_bits, &literal_bits_mask);

  /* If NO m68kops are legal, return.  This is not just a heuristic; we
   * really don't want to process opcodes that are illegal for some reason.
   * It may still be the case that all of the legal m68kops for this set
   * have already been done.
   */
  if (empty_set (&isect_list, literal_bits_mask, literal_bits))
    {
      if (verbose)
	puts ("<subsumed!>");
      return;
    }

  /* Allocate space for unexpanded_synthetic_opcode.  This helps us
   * share code for unexpanded cc variants.
   */
  unexpanded_synthetic_opcode = (int *) malloc (65536 * num_variants
						* sizeof (int));

  /* Compute optimal shift counts for different CC variants. */
  shift = (int *) SAFE_alloca (num_variants * sizeof (int));
  compute_optimal_shifts (info, shift, &isect_list, literal_bits,
			  literal_bits_mask);

  /* Compute the dashmasks for each CC variant. */
  dashmask = (int *) SAFE_alloca (num_variants * sizeof (int));
  for (i = 0, var = info->cc_variant; var != NULL; i++, var = var->next)
    dashmask[i] = compute_dashmask (var->bits_to_expand);

  /* Detect the presence of amodes and reversed amodes. */
  amode_size          = (int *) SAFE_alloca (num_variants * sizeof (int));
  reversed_amode_size = (int *) SAFE_alloca (num_variants * sizeof (int));
  for (i = 0, var = info->cc_variant; var != NULL; i++, var = var->next)
    {
      Token *t = has_token_of_type (var->code, TOK_DOLLAR_AMODE);
      if (t == NULL)
	t = has_token_of_type (var->code, TOK_DOLLAR_AMODE_PTR);
      amode_size[i] = (t == NULL) ? 0 : t->u.dollarinfo.size;
      t = has_token_of_type (var->code, TOK_DOLLAR_REVERSED_AMODE);
      if (t == NULL)
	t = has_token_of_type (var->code, TOK_DOLLAR_REVERSED_AMODE_PTR);
      reversed_amode_size[i] = (t == NULL) ? 0 : t->u.dollarinfo.size;
    }

  /* Determine whether or not the amode/reversed amodes are expanded. */
  amode_expanded          = (BOOL *) SAFE_alloca (num_variants * sizeof (BOOL));
  reversed_amode_expanded = (BOOL *) SAFE_alloca (num_variants * sizeof (BOOL));
  for (i = 0, var = info->cc_variant; var != NULL; i++, var = var->next)
    {
      amode_expanded[i] = ((dashmask[i] & 0x3F) == 0x00);
      reversed_amode_expanded[i] = (((dashmask[i] >> 6) & 0x3F) == 0x00);
    }
  
  /* For unexpanded opcodes, this table tells to which synthetic opcode we
   * should map.  Taking the m68kop & dashmask will give you an index
   * into this table; if the number at that table is not -1, it represents
   * the synthetic opcode to which you should map.  If it is -1, then no
   * such synthetic opcode has yet been created and it should be created.
   */
  memset (unexpanded_synthetic_opcode, -1,
	  num_variants * 65536 * sizeof (int));

  postcode = (const char **) SAFE_alloca (num_variants * sizeof (char *));
  for (i = 0; i < num_variants; i++)
    postcode[i] = unique_string ("");

  /* Make sure that all register rvalues hidden in unexpanded
   * addressing modes get extracted and treated as unexpanded registers.
   * We will do this by creating an artificial, expanded 68k opcode entry,
   * processing it, and then continuing on to process this opcode normally.
   */
  for (i = 0, v = info->cc_variant; v != NULL; i++, v = v->next)
    {
      BOOL changed = FALSE;
      List **original_var_code = (List **) SAFE_alloca (num_variants
						   * sizeof (List *));
      char **original_bits_to_expand =
	(char **) SAFE_alloca (num_variants * sizeof (const char *));
      char original_opcode_bits[16 * MAX_OPCODE_WORDS];
      BOOL old_verbose = verbose;
      int j;

      /* Save original opcode bits. */
      strcpy (original_opcode_bits, info->opcode_bits);

      /* Save original code + bits_to_expand. */
      for (j = 0, var = info->cc_variant; var != NULL; j++, var = var->next)
	{
	  assert (j < num_variants);
	  original_bits_to_expand[j] = var->bits_to_expand;
	  original_var_code[j] = var->code;
	}

      /* Do we have an addressing mode?  If so, split up the cases where the
       * addressing mode refers to a register (modes 0 and 1) and the cases
       * where it refers to memory (modes 2 through 7).  If we create a
       * register lvalue, that will be caught on recursion below.
       *
       * NOTE: We used to only do this for unexpanded addressing modes, but
       * it turns out this doesn't work because we need to separate the reg
       * cases right away so we know whether or not to swap them.
       */
      if (amode_size[i] != 0 /* && !amode_expanded[i]  NO GOOD; see NOTE. */
	  && has_token_of_type (v->code, TOK_DOLLAR_AMODE))
	{
	  int mind = 10;
	  TokenType replace = TOK_DOLLAR_AMODE;

#ifndef M68K_REGS_IN_ARRAY
	  /* Data register version. */
	  strncpy (info->opcode_bits + mind, "000", 3);

	  for (j = 0, var = info->cc_variant; var != NULL; j++,var = var->next)
	    {
	      assert (j < num_variants);
	      var->code = copy_list (original_var_code[j]);
	      replace_tokens_of_type (var->code, replace,
				      TOK_DOLLAR_DATA_REGISTER);
	    }

	  /* Generate code for the data register version. */
	  verbose = FALSE;
#ifdef SPLIT_WARNING
	  if (v->native_code_info != NULL)
	    parse_error (v->code, "Splitting up insn with native code assist; "
			 "this will scramble around your operands and cause "
			 "other confusion.  To fix this, make your case with "
			 "the native code assist more specific.\n");
#endif
	  generate_opcode (info, sym);

	  /* Address register version. */
	  info->opcode_bits[mind + 2] = '1';
	  
	  for (j = 0, var = info->cc_variant; var != NULL; j++,var = var->next)
	    {
	      var->code = copy_list (original_var_code[j]);
	      replace_tokens_of_type (var->code, replace,
				      TOK_DOLLAR_ADDRESS_REGISTER);
	    }

	  /* Generate code for the address register version. */
#ifdef SPLIT_WARNING
	  if (v->native_code_info != NULL)
	    parse_error (v->code, "Splitting up insn with native code assist; "
			 "this will scramble around your operands and cause "
			 "other confusion.  To fix this, make your case with "
			 "the native code assist more specific.\n");
#endif
	  generate_opcode (info, sym);

#else
	  strncpy (info->opcode_bits + mind, "00", 2);
	  for (j = 0, var = info->cc_variant; var != NULL; j++,var = var->next)
	    {
	      assert (j < num_variants);
	      var->code = copy_list (original_var_code[j]);
	      replace_tokens_of_type (var->code, replace,
				      TOK_DOLLAR_GENERAL_REGISTER);
	    }

	  /* Generate code for the data/address register version. */
	  verbose = FALSE;
#ifdef SPLIT_WARNING
	  if (v->native_code_info != NULL)
	    parse_error (v->code, "Splitting up insn with native code assist; "
			 "this will scramble around your operands and cause "
			 "other confusion.  To fix this, make your case with "
			 "the native code assist more specific.\n");
#endif
	  generate_opcode (info, sym);
#endif

	  changed = TRUE;
	}

      /* If we changed anything, don't loop any more. */
      if (changed)
	{
	  /* Restore original opcode bits. */
	  strcpy (info->opcode_bits, original_opcode_bits);

	  /* Restore original code + bits_to_expand. */
	  for (j = 0, var = info->cc_variant; var != NULL; j++,var = var->next)
	    {
	      assert (j < num_variants);
	      var->bits_to_expand = original_bits_to_expand[j];
	      var->code = original_var_code[j];
	    }

	  verbose = old_verbose;

	  /* All done. */
	  break;
	}
      ASSERT_SAFE(original_var_code);
      ASSERT_SAFE(original_bits_to_expand);
    }


  /* Make sure that all register rvalues hidden in unexpanded
   * addressing modes get extracted and treated as unexpanded registers.
   * We will do this by creating an artificial, expanded 68k opcode entry,
   * processing it, and then continuing on to process this opcode normally.
   */
  for (i = 0, v = info->cc_variant; v != NULL; i++, v = v->next)
    {
      BOOL changed = FALSE;
      List **original_var_code = (List **) SAFE_alloca (num_variants
						   * sizeof (List *));
      char **original_bits_to_expand =
	(char **) SAFE_alloca (num_variants * sizeof (const char *));
      char original_opcode_bits[16 * MAX_OPCODE_WORDS];
      BOOL old_verbose = verbose;
      int j;

      /* Save original opcode bits. */
      strcpy (original_opcode_bits, info->opcode_bits);

      /* Save original code + bits_to_expand. */
      for (j = 0, var = info->cc_variant; var != NULL; j++, var = var->next)
	{
	  original_bits_to_expand[j] = var->bits_to_expand;
	  original_var_code[j] = var->code;
	}

      /* Do we have a reversed addressing mode?  If so, split up the
       * cases where the addressing mode refers to a register (modes 0 and 1)
       * and the cases where it refers to memory (modes 2 through 7).  If
       * we create a register lvalue, that will be caught on recursion below.
       *
       * NOTE: We used to only do this for unexpanded addressing modes, but
       * it turns out this doesn't work because we need to separate the reg
       * cases right away so we know whether or not to swap them.
       */
      if (reversed_amode_size[i] != 0 /* && !reversed_amode_expanded[i] */
	  && has_token_of_type (v->code, TOK_DOLLAR_REVERSED_AMODE))
	{
	  int mind = 7;
	  TokenType replace = TOK_DOLLAR_REVERSED_AMODE;

	  /* Data register version. */
	  strncpy (info->opcode_bits + mind, "000", 3);

	  for (j = 0, var = info->cc_variant; var != NULL; j++,var = var->next)
	    {
	      var->code = copy_list (original_var_code[j]);
	      replace_tokens_of_type (var->code, replace,
				      TOK_DOLLAR_DATA_REGISTER);
	    }

	  /* Generate code for the data register version. */
	  verbose = FALSE;
#ifdef SPLIT_WARNING
	  if (v->native_code_info != NULL)
	    parse_error (v->code, "Splitting up insn with native code assist; "
			 "this will scramble around your operands and cause "
			 "other confusion.  To fix this, make your case with "
			 "the native code assist more specific.\n");
#endif
	  generate_opcode (info, sym);

	  /* Address register version. */
	  info->opcode_bits[mind + 2] = '1';
	  
	  for (j = 0, var = info->cc_variant; var != NULL; j++,var = var->next)
	    {
	      var->code = copy_list (original_var_code[j]);
	      replace_tokens_of_type (var->code, replace,
				      TOK_DOLLAR_ADDRESS_REGISTER);
	    }

	  /* Generate code for the address register version. */
#ifdef SPLIT_WARNING
	  if (v->native_code_info != NULL)
	    parse_error (v->code, "Splitting up insn with native code assist; "
			 "this will scramble around your operands and cause "
			 "other confusion.  To fix this, make your case with "
			 "the native code assist more specific.\n");
#endif
	  generate_opcode (info, sym);
	  changed = TRUE;
	}

      /* If we changed anything, don't loop any more. */
      if (changed)
	{
	  /* Restore original opcode bits. */
	  strcpy (info->opcode_bits, original_opcode_bits);

	  /* Restore original code + bits_to_expand. */
	  for (j = 0, var = info->cc_variant; var != NULL; j++,var = var->next)
	    {
	      var->bits_to_expand = original_bits_to_expand[j];
	      var->code = original_var_code[j];
	    }

	  verbose = old_verbose;

	  /* All done. */
	  break;
	}
      ASSERT_SAFE(original_var_code);
      ASSERT_SAFE(original_bits_to_expand);
    }


  /* See if we need to split this opcode up and recurse because we
   * have an expanded addressing mode (or reversed addressing mode)
   * 111/000 [(xxx).W], 111/001 [(xxx).L], 101/xxx [(d16,An)]
   */
  for (i = 0, v = info->cc_variant; v != NULL; i++, v = v->next)
    {
      BOOL changed = FALSE;
      List **original_var_code = (List **) SAFE_alloca (num_variants
						   * sizeof (List *));
      char **original_bits_to_expand =
	(char **) SAFE_alloca (num_variants * sizeof (const char *));
      char original_opcode_bits[16 * MAX_OPCODE_WORDS + 1];
      BOOL old_verbose = verbose;
      int j;

      /* Save original opcode bits. */
      strcpy (original_opcode_bits, info->opcode_bits);

      /* Save original code + bits_to_expand. */
      for (j = 0, var = info->cc_variant; var != NULL; j++, var = var->next)
	{
	  original_bits_to_expand[j] = var->bits_to_expand;
	  original_var_code[j] = var->code;
	}

      if (amode_size[i] != 0 && amode_expanded[i])
	{
	  int mind, rind, shr;
	  TokenType replace;
	  Token t;
	  List ll, ld, ln;

	  replace = TOK_DOLLAR_AMODE;
	  mind = 10;
	  rind = 13;
	  shr = 0;

	  t = *has_token_of_type (v->code, replace);

	  /* Create top level list. */
	  ll.cdr = NULL;
	  ll.car = &ld;
	  ll.token.type = TOK_LIST;
	  ll.token.u.string = "[LIST]";
	  ll.token.filename = "internal:defopcode.c";
	  ll.token.lineno   = 0;

	  /* Create deref. */
	  ld.cdr = &ln;
	  ld.car = NULL;
	  ld.token.type = TOK_DEREF;
	  ld.token.u.derefinfo.size = t.u.dollarinfo.size;
	  ld.token.u.derefinfo.sgnd = t.u.dollarinfo.sgnd;

	  /* Create address to be deref'd. */
	  ln.cdr = NULL;
	  ln.car = NULL;
	  ln.token.type = TOK_DOLLAR_NUMBER;
	  ln.token.u.dollarinfo.sgnd  = TRUE;
	  ln.token.u.dollarinfo.size  = 4;
	  ln.token.u.dollarinfo.which = num_fields (original_opcode_bits) + 1;

	  propagate_fileinfo (&ll.token, &ll);

	  /* (xxx).W */
	  strncpy (info->opcode_bits + mind, "111", 3);
	  strncpy (info->opcode_bits + rind, "000", 3);
	  make_unique_field_of_width (info->opcode_bits,
				      info->opcode_bits
				      + strlen (info->opcode_bits),
				      16);

	  /* Loop over all variants and change amode ref to addr ref. */
	  for (j = 0, var = info->cc_variant; var != NULL; j++,var = var->next)
	    {
	      var->code = copy_list (original_var_code[j]);
	      replace_tokens_of_type_with_list (var->code, replace, &ll);
	      delete_field (var->code, field_with_index (original_opcode_bits,
							 MIN (rind, mind)),
			    (7 << (13 - mind)) >> shr);
	    }

	  /* Turn off verbose mode & generate code. */
	  verbose = FALSE;
#ifdef SPLIT_WARNING
	  if (v->native_code_info != NULL)
	    parse_error (v->code, "Splitting up insn with native code assist; "
			 "this will scramble around your operands and cause "
			 "other confusion.  To fix this, make your case with "
			 "the native code assist more specific.\n");
#endif
	  generate_opcode (info, sym);

	  /* (xxx).L */
	  strncpy (info->opcode_bits + mind, "111", 3);
	  strncpy (info->opcode_bits + rind, "001", 3);
	  info->opcode_bits[strlen (original_opcode_bits)] = '\0';
	  make_unique_field_of_width (info->opcode_bits,
				      info->opcode_bits
				      + strlen (info->opcode_bits),
				      32);

	  /* Loop over all variants and change amode ref to addr ref. */
	  for (j = 0, var = info->cc_variant; var != NULL; j++,var = var->next)
	    {
	      var->code = copy_list (original_var_code[j]);
	      replace_tokens_of_type_with_list (var->code, replace, &ll);
	      delete_field (var->code, field_with_index (original_opcode_bits,
							 MIN (rind, mind)),
			    ((1 << (13 - rind)) | (7 << (13 - mind))) >> shr);
	    }

	  /* Generate code. */
#ifdef SPLIT_WARNING
	  if (v->native_code_info != NULL)
	    parse_error (v->code, "Splitting up insn with native code assist; "
			 "this will scramble around your operands and cause "
			 "other confusion.  To fix this, make your case with "
			 "the native code assist more specific.\n");
#endif
	  generate_opcode (info, sym);

 	  /* (d16,An) */
	  {
	    char buf[256], buf2[256];
	    List *ls, *ls2;

	    /* Must replace all $n.mxx with (derefxx (+ $n.aul $x.sl)) and
	     * replace all $n.xx with
	     * ([((5 << (13 - mind - shr)) + ($n.ul << (13 - rind - shr)))])
	     */

	    strcpy (info->opcode_bits, original_opcode_bits);
	    strncpy (info->opcode_bits + mind, "101", 3);
	    make_unique_field_of_width (info->opcode_bits,
					info->opcode_bits
					+ strlen (info->opcode_bits),
					16);

	    sprintf (buf, "(deref%c%c (+ $%d.aul $%d.sl))",
		     t.u.dollarinfo.sgnd ? 's' : 'u',
		     " bw l"[t.u.dollarinfo.size],
		     t.u.dollarinfo.which, num_fields (info->opcode_bits));
	    ls = string_to_list (buf, NULL);

	    if (13 - rind - shr == 0)
	      sprintf (buf2, "(+ %d $%d.ul)", 5 << (13 - mind - shr),
		       t.u.dollarinfo.which);
	    else
	      sprintf (buf2, "(+ %d (<< $%d.ul %d))", 5 << (13 - mind - shr),
		       t.u.dollarinfo.which, 13 - rind - shr);
	    ls2 = string_to_list (buf2, NULL);

	    /* Loop over all variants and change amode ref to addr ref. */
	    for (j = 0, var = info->cc_variant; var != NULL;
		 j++, var = var->next)
	      {
		var->code = copy_list (original_var_code[j]);
		replace_dollar_number_with_list (var->code,
						 t.u.dollarinfo.which, ls2);
		replace_tokens_of_type_with_list (var->code, replace, ls);
	      }
	  }

	  /* Generate code. */
#ifdef SPLIT_WARNING
	  if (v->native_code_info != NULL)
	    parse_error (v->code, "Splitting up insn with native code assist; "
			 "this will scramble around your operands and cause "
			 "other confusion.  To fix this, make your case with "
			 "the native code assist more specific.\n");
#endif
	  generate_opcode (info, sym);

	  changed = TRUE;
	}

      /* If we changed anything, don't loop any more. */
      if (changed)
	{
	  /* Restore original opcode bits. */
	  strcpy (info->opcode_bits, original_opcode_bits);

	  /* Restore original code + bits_to_expand. */
	  for (j = 0, var = info->cc_variant; var != NULL; j++,var = var->next)
	    {
	      var->bits_to_expand = original_bits_to_expand[j];
	      var->code = original_var_code[j];
	    }

	  verbose = old_verbose;

	  /* All done. */
	  break;
	}
      ASSERT_SAFE(original_var_code);
      ASSERT_SAFE(original_bits_to_expand);
    }

  /* See if we need to split this opcode up and recurse because we
   * have an expanded addressing mode (or reversed addressing mode)
   * 111/000 [(xxx).W], 111/001 [(xxx).L], 101/xxx [(d16,An)]
   */
  for (i = 0, v = info->cc_variant; v != NULL; i++, v = v->next)
    {
      BOOL changed = FALSE;
      List **original_var_code = (List **) SAFE_alloca (num_variants
						   * sizeof (List *));
      char **original_bits_to_expand =
	(char **) SAFE_alloca (num_variants * sizeof (const char *));
      char original_opcode_bits[16 * MAX_OPCODE_WORDS + 1];
      BOOL old_verbose = verbose;
      int j;

      /* Save original opcode bits. */
      strcpy (original_opcode_bits, info->opcode_bits);

      /* Save original code + bits_to_expand. */
      for (j = 0, var = info->cc_variant; var != NULL; j++, var = var->next)
	{
	  original_bits_to_expand[j] = var->bits_to_expand;
	  original_var_code[j] = var->code;
	}

      if (reversed_amode_size[i] != 0 && reversed_amode_expanded[i])
	{
	  int mind, rind, shr;
	  TokenType replace;
	  Token t;
	  List ll, ld, ln;

	  replace = TOK_DOLLAR_REVERSED_AMODE;
	  mind = 7;
	  rind = 4;
	  shr = 6;

	  t = *has_token_of_type (v->code, replace);

	  /* Create top level list. */
	  ll.cdr = NULL;
	  ll.car = &ld;
	  ll.token.type = TOK_LIST;
	  ll.token.u.string = "[LIST]";
	  ll.token.filename = "internal:defopcode.c";
	  ll.token.lineno   = 0;

	  /* Create deref. */
	  ld.cdr = &ln;
	  ld.car = NULL;
	  ld.token.type = TOK_DEREF;
	  ld.token.u.derefinfo.size = t.u.dollarinfo.size;
	  ld.token.u.derefinfo.sgnd = t.u.dollarinfo.sgnd;

	  /* Create address to be deref'd. */
	  ln.cdr = NULL;
	  ln.car = NULL;
	  ln.token.type = TOK_DOLLAR_NUMBER;
	  ln.token.u.dollarinfo.sgnd  = TRUE;
	  ln.token.u.dollarinfo.size  = 4;
	  ln.token.u.dollarinfo.which = num_fields (original_opcode_bits) + 1;

	  propagate_fileinfo (&ll.token, &ll);

	  /* (xxx).W */
	  strncpy (info->opcode_bits + mind, "111", 3);
	  strncpy (info->opcode_bits + rind, "000", 3);
	  make_unique_field_of_width (info->opcode_bits,
				      info->opcode_bits
				      + strlen (info->opcode_bits),
				      16);

	  /* Loop over all variants and change amode ref to addr ref. */
	  for (j = 0, var = info->cc_variant; var != NULL; j++,var = var->next)
	    {
	      var->code = copy_list (original_var_code[j]);
	      replace_tokens_of_type_with_list (var->code, replace, &ll);
	      delete_field (var->code, field_with_index (original_opcode_bits,
							 MIN (rind, mind)),
			    (7 << (13 - mind)) >> shr);
	    }

	  /* Turn off verbose mode & generate code. */
	  verbose = FALSE;
#ifdef SPLIT_WARNING
	  if (v->native_code_info != NULL)
	    parse_error (v->code, "Splitting up insn with native code assist; "
			 "this will scramble around your operands and cause "
			 "other confusion.  To fix this, make your case with "
			 "the native code assist more specific.\n");
#endif
	  generate_opcode (info, sym);

	  /* (xxx).L */
	  strncpy (info->opcode_bits + mind, "111", 3);
	  strncpy (info->opcode_bits + rind, "001", 3);
	  info->opcode_bits[strlen (original_opcode_bits)] = '\0';
	  make_unique_field_of_width (info->opcode_bits,
				      info->opcode_bits
				      + strlen (info->opcode_bits),
				      32);

	  /* Loop over all variants and change amode ref to addr ref. */
	  for (j = 0, var = info->cc_variant; var != NULL; j++,var = var->next)
	    {
	      var->code = copy_list (original_var_code[j]);
	      replace_tokens_of_type_with_list (var->code, replace, &ll);
	      delete_field (var->code, field_with_index (original_opcode_bits,
							 MIN (rind, mind)),
			    ((1 << (13 - rind)) | (7 << (13 - mind))) >> shr);
	    }

	  /* Generate code. */
#ifdef SPLIT_WARNING
	  if (v->native_code_info != NULL)
	    parse_error (v->code, "Splitting up insn with native code assist; "
			 "this will scramble around your operands and cause "
			 "other confusion.  To fix this, make your case with "
			 "the native code assist more specific.\n");
#endif
	  generate_opcode (info, sym);

 	  /* (d16,An) */
	  {
	    char buf[256], buf2[256];
	    List *ls, *ls2;

	    /* Must replace all $n.mxx with (derefxx (+ $n.aul $x.sl)) and
	     * replace all $n.xx with
	     * ([((5 << (13 - mind - shr)) + ($n.ul << (13 - rind - shr)))])
	     */

	    strcpy (info->opcode_bits, original_opcode_bits);
	    strncpy (info->opcode_bits + mind, "101", 3);
	    make_unique_field_of_width (info->opcode_bits,
					info->opcode_bits
					+ strlen (info->opcode_bits),
					16);

	    sprintf (buf, "(deref%c%c (+ $%d.aul $%d.sl))",
		     t.u.dollarinfo.sgnd ? 's' : 'u',
		     " bw l"[t.u.dollarinfo.size],
		     t.u.dollarinfo.which, num_fields (info->opcode_bits));
	    ls = string_to_list (buf, NULL);

	    if (13 - rind - shr == 0)
	      sprintf (buf2, "(+ %d $%d.ul)", 5 << (13 - mind - shr),
		       t.u.dollarinfo.which);
	    else
	      sprintf (buf2, "(+ %d (<< $%d.ul %d))", 5 << (13 - mind - shr),
		       t.u.dollarinfo.which, 13 - rind - shr);
	    ls2 = string_to_list (buf2, NULL);

	    /* Loop over all variants and change amode ref to addr ref. */
	    for (j = 0, var = info->cc_variant; var != NULL;
		 j++, var = var->next)
	      {
		var->code = copy_list (original_var_code[j]);
		replace_dollar_number_with_list (var->code,
						 t.u.dollarinfo.which, ls2);
		replace_tokens_of_type_with_list (var->code, replace, ls);
	      }
	  }

	  /* Generate code. */
#ifdef SPLIT_WARNING
	  if (v->native_code_info != NULL)
	    parse_error (v->code, "Splitting up insn with native code assist; "
			 "this will scramble around your operands and cause "
			 "other confusion.  To fix this, make your case with "
			 "the native code assist more specific.\n");
#endif
	  generate_opcode (info, sym);

	  changed = TRUE;
	}

      /* If we changed anything, don't loop any more. */
      if (changed)
	{
	  /* Restore original opcode bits. */
	  strcpy (info->opcode_bits, original_opcode_bits);

	  /* Restore original code + bits_to_expand. */
	  for (j = 0, var = info->cc_variant; var != NULL; j++,var = var->next)
	    {
	      var->bits_to_expand = original_bits_to_expand[j];
	      var->code = original_var_code[j];
	    }

	  verbose = old_verbose;

	  /* All done. */
	  break;
	}
      ASSERT_SAFE(original_var_code);
      ASSERT_SAFE(original_bits_to_expand);
    }


  /* Expand out any #data addressing modes by creating a new opcode table
   * entry that explicitly mentions the operand and then recursing to
   * generate it.
   */
  for (i = 0, v = info->cc_variant; v != NULL; i++, v = v->next)
    {
      BOOL changed = FALSE;
      List **original_var_code = (List **) SAFE_alloca (num_variants
						   * sizeof (List *));
      char **original_bits_to_expand =
	(char **) SAFE_alloca (num_variants * sizeof (const char *));
      char original_opcode_bits[16 * MAX_OPCODE_WORDS + 1];
      BOOL old_verbose = verbose;
      int j;

      /* Save original opcode bits. */
      strcpy (original_opcode_bits, info->opcode_bits);

      /* Save original code + bits_to_expand. */
      for (j = 0, var = info->cc_variant; var != NULL; j++, var = var->next)
	{
	  original_bits_to_expand[j] = var->bits_to_expand;
	  original_var_code[j] = var->code;
	}

      if (amode_size[i] != 0 || reversed_amode_size[i] != 0)
	{
	  int mind, rind, shr;
	  TokenType replace;
	  Token t, *tp;
	  List ln;

	  if (amode_size[i] != 0)
	    {
	      replace = TOK_DOLLAR_AMODE;
	      mind = 10;
	      rind = 13;
	      shr = 0;
	    }
	  else
	    {
	      replace = TOK_DOLLAR_REVERSED_AMODE;
	      mind = 7;
	      rind = 4;
	      shr = 6;
	    }

	  tp = has_token_of_type (v->code, replace);

	  if (tp != NULL)
	    {
	      t = *tp;

	      /* #<data> */
	      /* Set up ln. */
	      ln.token.type               = TOK_DOLLAR_NUMBER;
	      ln.token.u.dollarinfo.size  = t.u.dollarinfo.size;
	      ln.token.u.dollarinfo.sgnd  = t.u.dollarinfo.sgnd;
	      ln.token.u.dollarinfo.which = num_fields (info->opcode_bits) + 1;
	      ln.token.filename           = "Internal/defopcode.c";
	      ln.token.lineno             = 0;
	      ln.car = ln.cdr = NULL;
	      
	      strncpy (info->opcode_bits + mind, "111", 3);
	      strncpy (info->opcode_bits + rind, "100", 3);
	      if (t.u.dollarinfo.size == 1)
		strcat (info->opcode_bits, "00000000");
	      make_unique_field_of_width (info->opcode_bits,
					  info->opcode_bits
					  + strlen (info->opcode_bits),
					  t.u.dollarinfo.size * 8);
	      
	      /* Loop over all variants and change amode ref to operand. */
	      for (j = 0, var = info->cc_variant; var != NULL;
		   j++, var = var->next)
		{
		  var->code = copy_list (original_var_code[j]);
		  replace_tokens_of_type_with_list (var->code, replace, &ln);
		  
		  delete_field (var->code,
				field_with_index (original_opcode_bits,
						  MIN (rind, mind)),
				((4 << (13 - rind))
				 | (7 << (13 - mind))) >> shr);
		}
	      
	      /* Generate code. */
	      verbose = FALSE;
#ifdef SPLIT_WARNING
	      if (v->native_code_info != NULL)
		parse_error (v->code, "Splitting up insn with native code assist; "
			     "this will scramble around your operands and cause "
			     "other confusion.  To fix this, make your case with "
			     "the native code assist more specific.\n");
#endif
	      generate_opcode (info, sym);
	      
	      changed = TRUE;
	    }
	}

      /* If we changed anything, don't loop any more. */
      if (changed)
	{
	  /* Restore original opcode bits. */
	  strcpy (info->opcode_bits, original_opcode_bits);

	  /* Restore original code + bits_to_expand. */
	  for (j = 0, var = info->cc_variant; var != NULL; j++,var = var->next)
	    {
	      var->bits_to_expand = original_bits_to_expand[j];
	      var->code = original_var_code[j];
	    }

	  verbose = old_verbose;

	  /* All done. */
	  break;
	}
      ASSERT_SAFE(original_var_code);
      ASSERT_SAFE(original_bits_to_expand);
    }


#ifndef M68K_REGS_IN_ARRAY
  /* See if we need to split this opcode up and recurse because we have
   * an unexpanded register as an lvalue.
   */
  for (i = 0, v = info->cc_variant; v != NULL; i++, v = v->next)
    {
      BOOL changed = FALSE;
      List **original_var_code = (List **) SAFE_alloca (num_variants
						   * sizeof (List *));
      char **original_bits_to_expand =
	(char **) SAFE_alloca (num_variants * sizeof (const char *));
      char original_opcode_bits[16 * MAX_OPCODE_WORDS];
      BOOL old_verbose = verbose;
      int index;
      int j;

      /* Save original opcode bits. */
      strcpy (original_opcode_bits, info->opcode_bits);

      /* Save original code + bits_to_expand. */
      for (j = 0, var = info->cc_variant; var != NULL; j++, var = var->next)
	{
	  original_bits_to_expand[j] = var->bits_to_expand;
	  original_var_code[j] = var->code;
	}

      /* Do we have an unexpanded register as an lvalue?  If so, we need
       * to expand this register so we can have a legal lvalue and recurse.
       */
      if (has_unexpanded_register_lvalue (v->code, info->opcode_bits,
					  v->bits_to_expand, &index))
	{
	  char *new_bits_to_expand = (char *) SAFE_alloca (17 * num_variants);

	  for (j = 0, var = info->cc_variant; var != NULL; j++,var = var->next)
	    {
	      strcpy (&new_bits_to_expand[j * 17], var->bits_to_expand);
	      new_bits_to_expand[j * 17 + index] 
		= new_bits_to_expand[j * 17 + index + 1] 
		  = new_bits_to_expand[j * 17 + index + 2] = 'x';
	      var->bits_to_expand = &new_bits_to_expand[j * 17];
	      var->code = copy_list (original_var_code[j]);
	    }

	  /* Turn off verbose mode; looks weird if you leave it on. */
	  verbose = FALSE;
#ifdef SPLIT_WARNING
	  if (v->native_code_info != NULL)
	    parse_error (v->code,
			 "Splitting up insn with native code assist; "
			 "this will scramble around your operands and cause "
			 "other confusion.  To fix this, make your case with "
			 "the native code assist more specific.\n");
#endif
	  generate_opcode (info, sym);

	  changed = TRUE;
	  ASSERT_SAFE(new_bits_to_expand);
	}

      /* If we changed anything, don't loop any more. */
      if (changed)
	{
	  /* Restore original opcode bits. */
	  strcpy (info->opcode_bits, original_opcode_bits);

	  /* Restore original code + bits_to_expand. */
	  for (j = 0, var = info->cc_variant; var != NULL; j++,var = var->next)
	    {
	      var->bits_to_expand = original_bits_to_expand[j];
	      var->code = original_var_code[j];
	    }

	  verbose = old_verbose;

	  /* All done. */
	  break;
	}
      ASSERT_SAFE(original_var_code);
      ASSERT_SAFE(original_bits_to_expand);
    }
#endif

  /* Allocate array for ptrs to scheme code value to add to code. */
  add_to_code_token = (List **) SAFE_alloca (num_variants * sizeof (List *));
  for (i = 0; i < num_variants; i++)
    add_to_code_token[i] = NULL;

  /* Insert postambles to handle unexpanded predec/postinc's. */
  for (i = 0, v = info->cc_variant; v != NULL; i++, v = v->next)
    {
      char buf[2048];
      Token *ta, *tr;
      List *ls;

      /* Handle predecrement for unexpanded addressing modes by explicitly
       * checking for a predecrement mode and decrementing the pointer
       * if we find it.  FIXME - this will perform the check even if
       * predec/postinc are not legal expansions!
       */
      if (!info->dont_postincdec_unexpanded
	  && !reversed_amode_expanded[i]
	  && (tr = has_token_of_type (v->code, TOK_DOLLAR_REVERSED_AMODE)))
	{
	  int s = tr->u.dollarinfo.size;
	  
	  /* Create a list describing the special stuff we want to do.
	   * Here we are relying on the fact that the bit pattern for
	   * the reversed addressing mode was already swapped to be
	   * a non-reversed addressing mode.  This should happen at
	   * translation time.
	   */
	  sprintf (buf,
		   "(list "
		   "\"\\n#ifdef M68K_REGS_IN_ARRAY\\n\" "
		   "(call \"CLEANUP_AMODE\" $%d.ul %d) "
		   "\"\\n#else\\n\" "
		   "(switch $%d.ul (0x18 (assign a0.ul (+ a0.ul %d)))"
		                  "(0x19 (assign a1.ul (+ a1.ul %d)))"
		                  "(0x1A (assign a2.ul (+ a2.ul %d)))"
	                          "(0x1B (assign a3.ul (+ a3.ul %d)))"
	                          "(0x1C (assign a4.ul (+ a4.ul %d)))"
	                          "(0x1D (assign a5.ul (+ a5.ul %d)))"
	                          "(0x1E (assign a6.ul (+ a6.ul %d)))"
	                          "(0x1F (assign a7.ul (+ a7.ul %d)))"
                                  "(0x20 (assign a0.ul (- a0.ul %d)))"
	                          "(0x21 (assign a1.ul (- a1.ul %d)))"
	                          "(0x22 (assign a2.ul (- a2.ul %d)))"
	                          "(0x23 (assign a3.ul (- a3.ul %d)))"
	                          "(0x24 (assign a4.ul (- a4.ul %d)))"
	                          "(0x25 (assign a5.ul (- a5.ul %d)))"
	                          "(0x26 (assign a6.ul (- a6.ul %d)))"
	                          "(0x27 (assign a7.ul (- a7.ul %d)))) "
		   "\"\\n#endif\\n\""
		   ")",
		   tr->u.dollarinfo.which, s,
		   tr->u.dollarinfo.which,
		   s, s, s, s, s, s, s, (s == 1) ? 2 : s,
		   s, s, s, s, s, s, s, (s == 1) ? 2 : s);
	  
	  /* Insert this code into original code. */
	  ls = string_to_list (buf, NULL);
	  v->code->cdr = CDAR (ls);
	  CDAR (ls) = v->code;
	  v->code = ls;
	}

      if (!info->dont_postincdec_unexpanded
	  && !amode_expanded[i]
	  && (ta = has_token_of_type (v->code, TOK_DOLLAR_AMODE)) != NULL)
	{
	  assert (ta->u.dollarinfo.size == 1
		  || ta->u.dollarinfo.size == 2
		  || ta->u.dollarinfo.size == 4);

	  sprintf (buf, "(list "
		   "\"\\n#ifdef M68K_REGS_IN_ARRAY\\n\" "
		   "      (call \"CLEANUP_AMODE\" $%d.ul %d) "
		   "\"\\n#else\\n\""
		   "      (assign \"amode\" $%d.ul) "
		   "      (assign code (+ code 666))"  /* 666 repl. later. */
		   "\"\\n#endif\\n\" "
		   ")",
		   ta->u.dollarinfo.which, ta->u.dollarinfo.size,
		   ta->u.dollarinfo.which);
	  ls = string_to_list (buf, NULL);
	  add_to_code_token[i] = CDDAR (CDDAR (CDR (CDDR (CDDAR (ls)))));

	  v->code->cdr = CDAR (ls);
	  CDAR (ls) = v->code;
	  v->code = ls;

	  sprintf (buf, "\n#ifndef M68K_REGS_IN_ARRAY\n"
		   "        CLEANUP_AMODE (amode, %d);\n"
		   "#endif\n",
		   ta->u.dollarinfo.size);
	  postcode[i] = unique_string (buf);
	}
    }

  /* Insert byte swaps, compute bitfields for OpcodeMappingInfo,
   * and generate code to grab operands.
   */
  operand_info = (OperandInfo *) SAFE_alloca (num_variants
					      * sizeof (OperandInfo));
  for (i = 0, v = info->cc_variant; v != NULL; i++, v = v->next)
    {
      int operands;
      operands = compute_operand_info (&operand_info[i], info, v);
      if (add_to_code_token[i] != NULL)
	add_to_code_token[i]->token.u.n = (!info->ends_block) ? operands : 0;
    }

  /* Grab a pointer to the first OpcodeMappingInfo struct in our set
   * of sequences.
   */
  base = &opcode_map_info[num_map_infos];

  /* Loop over all 64K possible opcodes and process those which match. */
  for (m68kop = 0; m68kop < 65536; m68kop++)
    {
      int set;

      /* If this 68k opcode has already been done, or we shouldn't be
       * doing it, move on and try the next one.
       */
      if ((m68kop & literal_bits_mask) != literal_bits
	  || map_index[m68kop] != NO_MAP
	  || !is_member_of_set (m68kop, &isect_list))
	continue;

      /* See if one of the sets of opcode maps we've already made works. */
      mapping = NULL;   /* Default: no mapping set found. */
      for (set = 0; set < num_variant_mapping_sets; set++)
	{
	  for (i = 0; i < num_variants; i++)
	    {
	      int new;
	      int maskedop = m68kop & ~(dashmask[i] & ~literal_bits_mask);

	      new = compute_synthetic_opcode (m68kop,
					      &base[set * num_variants + i]);

	      /* If the computed synthetic opcode is taken, punt.  Note that
	       * it's OK for it to be taken if we are not fully expanding
	       * and therefore we are sharing a synthetic opcode with
	       * some other 68k opcode.
	       */
	      if (unexpanded_synthetic_opcode[i * 65536 + maskedop] != -1)
		{
		  if (new != unexpanded_synthetic_opcode[i * 65536 + maskedop])
		    break;
		}
	      else if (SYNTHETIC_OPCODE_TAKEN (new, i))
		break;
	    }

	  /* Did we find a match for all of the variants? */
	  if (i == num_variants)
	    {
	      mapping = &base[set * num_variants];
	      break;
	    }
	}

      /* Did we fail to find a useful set of mappings? */
      if (mapping == NULL)
	{
	  /* Store the base of the new mapping set. */
	  mapping = &opcode_map_info[num_map_infos];

	  /* Create new mapping here. */
	  for (i = 0, var = info->cc_variant; var; i++, var = var->next)
	    {
	      OpcodeMappingInfo *m = &mapping[i];
	      int maskedop = m68kop & ~(dashmask[i] & ~literal_bits_mask);
	      int add, n;

	      /* Remember this opcode. */
	      map_info_opcode_name[num_map_infos + i] = info->name;

	      /* Configure this opcode mapping. */
	      m->sequence_parity         = parity;
	      m->cc_may_set              = var->cc_may_set;
	      m->cc_may_not_set          = var->cc_may_not_set;
	      m->cc_needed               = var->cc_needed;
	      m->instruction_words       = strlen (info->opcode_bits) / 16;
	      m->ends_block              = info->ends_block;
	      m->next_block_dynamic      = info->next_block_dynamic;
	      m->opcode_shift_count      = shift[i];
	      m->opcode_and_bits         = ~(dashmask[i] | literal_bits_mask);
	      m->opcode_add_bits         = 0;   /* Dummy; replaced below. */
	      m->amode_size              = ((amode_size[i] == 4)
					    ? 3 : amode_size[i]);
	      m->reversed_amode_size     = ((reversed_amode_size[i] == 4)
					    ? 3 : reversed_amode_size[i]);
	      m->amode_expanded          = amode_expanded[i];
	      m->reversed_amode_expanded = reversed_amode_expanded[i];

	      /* Copy precomputed operand bitfield information. */
	      
	      memcpy (m->bitfield, operand_info[i].bitfield,
		      sizeof m->bitfield);
	      if (operand_info[i].num_bitfields < MAX_BITFIELDS)
		{
		  m->bitfield[operand_info[i].num_bitfields].index
		    = MAGIC_END_INDEX;
		  m->bitfield[operand_info[i].num_bitfields].length
		    = MAGIC_END_LENGTH;
		}

#ifdef GENERATE_NATIVE_CODE
	      if (var->native_code_info == NULL)
		m->guest_code_descriptor = NULL;
	      else
		m->guest_code_descriptor = var->native_code_info;
#endif

	      /* Find the add_bits that will map us to the smallest untaken
	       * synthetic opcode, or to the smallest synthetic opcode
	       * we are allowed to share because we aren't fully expanding.
	       */
	      n = compute_synthetic_opcode (m68kop, m);

	      if (unexpanded_synthetic_opcode[i * 65536 + maskedop] != -1)
		add = unexpanded_synthetic_opcode[i * 65536 + maskedop] - n;
	      else
		{
		  add = -n;
		  while (SYNTHETIC_OPCODE_TAKEN (add + n, i))
		    add++;
		}

	      m->opcode_add_bits = (add & 0xFFFF);

	      /* Reserve all synthetic opcodes for this CC variant that
	       * we can now attain, so that other CC variants don't choose
	       * mappings that conflict.
	       */
	      reserve_synthetic_ops (base, num_variants, i,
				     num_variant_mapping_sets + 1,
				     &isect_list, literal_bits,
				     literal_bits_mask);
	    }

	  num_map_infos += num_variants;
	  num_variant_mapping_sets++;
	  parity = !parity;
	}

      /* Record the index to the correct mapping information. */
      map_index[m68kop] = mapping - opcode_map_info;

      /* Output profiling info about this opcode; this information
       * will can be used by the profiler to group related bit patterns
       * together and determine how frequently different 68k instructions
       * are used.
       */
      fprintf (profileinfo_stream, "%d %d %d %d %d %s\n",
	       m68kop, amode_size[0] != 0, reversed_amode_size[0] != 0,
	       literal_bits_mask, literal_bits, info->name);

      /* Loop over all of the CC variants, unreserve opcodes, and reserve
       * them again.  This minimizes the number of synthetic ops reserved.
       * Why?  Because two different OpcodeMappingInfo structs may map
       * a given CC variant to different synthetic opcodes for the same 68k
       * opcode.  Since we don't know which OpcodeMappingInfo will be
       * chosen for that 68k opcode until we actually process it, we end
       * up reserving more opcodes than we actually need to (since some
       * of the reservations are mutually exclusive).  Once we nail down
       * to where a 68k opcode maps, re-reserving everything will avoid
       * reserving any mutually exclusive reservations.
       */
#if 0   /* Doesn't help; increases OpcodeMappingInfo's more than
	 * it decreases the # of synthetic opcodes.
	 */
      if (optimization_level > 0)
	for (i = 0; i < num_variants; i++)
	  {
	    unsigned char *p;
	    int synop = compute_synthetic_opcode (m68kop, &mapping[i]);
	    int j;
	    
	    /* Unreserve synthetic opcodes for this variant. */
	    for (p = synthetic_opcode_taken, j = 65535; j >= 0; p++, j--)
	      if (*p == i)
		*p = OPCODE_NOT_TAKEN;
	    
	    /* Reserve the synthetic opcode we just made. */
	    if (synthetic_opcode_taken[synop] == OPCODE_NOT_TAKEN)
	      synthetic_opcode_taken[synop] = i;
	    
	    /* Reserve synthetic opcodes for this variant. */
	    reserve_synthetic_ops (base, num_variants, i,
				   num_variant_mapping_sets, &isect_list,
				   literal_bits, literal_bits_mask);
	  }
#endif

      /* Loop over all of the CC variants & generate code. */
      for (i = 0, var = info->cc_variant; var != NULL; i++, var = var->next)
	{
	  int synop = compute_synthetic_opcode (m68kop, &mapping[i]);
	  int maskedop = m68kop & ~(dashmask[i] & ~literal_bits_mask);

	  /* If code has already been generated for this opcode, we must
	   * be sharing code.  No need to generate new code, so try next
	   * variant.
	   */
	  if (synthetic_opcode_taken[synop] == OPCODE_TAKEN)
	    continue;

	  /* Sanity check - verify that this synop was properly reserved. */
	  if (synthetic_opcode_taken[synop] == OPCODE_NOT_TAKEN)
	    {
	      error ("Internal error: generating code for unreserved synop "
		     "0x%04X for m68kop\n                ", (unsigned) synop);
	      print_16_bits (stderr, m68kop);
	      error (" (dashmask = ");
	      print_16_bits (stderr, dashmask[i]);
	      error (", shift = %d,\n                add_bits = 0x%04X, "
		     "and_bits = ", mapping[i].opcode_shift_count,
		      mapping[i].opcode_add_bits);
	      print_16_bits (stderr, mapping[i].opcode_and_bits);
	      error (")");
	      error (" for variant %d.\n", i);
	    }
	  else if (synthetic_opcode_taken[synop] != i)
	    {
	      error ("Internal error generating code for synop 0x%04X for "
		     "m68kop ", (unsigned) synop);
	      print_16_bits (stderr, m68kop);
	      error (" for variant %d; already reserved for variant %d!\n",
		     i, synthetic_opcode_taken[synop]);
	    }

	  /* Remember the appropriate synthetic opcode for this guy
	   * in case he isn't fully expanded; this way, other m68k
	   * opcodes that should be mapped to the same handler will
	   * know which synthetic opcode to use.
	   */
	  unexpanded_synthetic_opcode[i * 65536 + maskedop]
	    = compute_synthetic_opcode (m68kop, &mapping[i]);
	  
	  /* Lock down this synthetic opcode forever. */
	  synthetic_opcode_taken[synop] = OPCODE_TAKEN;

	  /* Generate C code for this variant. */
	  generate_c_code (info, var, m68kop, synop, sym,
			   operand_info[i].operand_decls, postcode[i],
			   &mapping[i]);
	}
    }

  /* Unreserve all reserved (but not taken) opcodes, if any are left. */
  for (i = 0; i < 65536; i++)
    if (synthetic_opcode_taken[i] != OPCODE_TAKEN
	&& synthetic_opcode_taken[i] != OPCODE_NOT_TAKEN)
      {
	synthetic_opcode_taken[i] = OPCODE_NOT_TAKEN;
      }

  free (unexpanded_synthetic_opcode);

  ASSERT_SAFE(shift);
  ASSERT_SAFE(dashmask);
  ASSERT_SAFE(amode_size);
  ASSERT_SAFE(reversed_amode_size);
  ASSERT_SAFE(amode_expanded);
  ASSERT_SAFE(reversed_amode_expanded);
  ASSERT_SAFE(postcode);
  ASSERT_SAFE(add_to_code_token);
  ASSERT_SAFE(operand_info);

  if (verbose)
    puts ("done.");
}


int
synthetic_opcode_size (const OpcodeMappingInfo *map)
{
  int i, size = PTR_WORDS;  /* Account for the synthetic opcode. */

  for (i = 0; i < MAX_BITFIELDS; i++)
    {
      if (IS_TERMINATING_BITFIELD (&map->bitfield[i]))
	break;
      size += map->bitfield[i].words + 1;
    }

  return size;
}


/* For each cc variant, sees just how far it can shift the 68k pattern
 * to the right without losing any information it might need in the
 * translation to a synthetic opcode.  If it can shift out all of the bits,
 * returns a shift count of 0.
 */
static void
compute_optimal_shifts (const ParsedOpcodeInfo *info, int *shift,
			List *isect_list, int literal_bits,
			int literal_bits_mask)
{
  int m68kop;
  BOOL first = TRUE;
  int i;
  const CCVariant *var;
  int known_pattern, changing_bits = 0;

  /* Determine which bits in the 68k opcode can take on different values. */
  known_pattern = 0;
  for (m68kop = 0; m68kop < 65536; m68kop++)
    {
      if ((m68kop & literal_bits_mask) == literal_bits
	  && map_index[m68kop] == NO_MAP
	  && is_member_of_set (m68kop, isect_list))
	{
	  if (first)
	    {
	      known_pattern = m68kop;
	      first = FALSE;
	    }
	  else
	    changing_bits |= known_pattern ^ m68kop;
	}
    }
  
  /* Determine optimal shift count for each cc variant by locating the lowest
   * 68k opcode bit used in the 68k->synthetic translation which can take on
   * different values.
   */
  for (i = 0, var = info->cc_variant; var != NULL; i++, var = var->next)
    {
      int cbits = changing_bits;
      
      /* Recommend we shift out all '-'s in the bits_to_expand. */
      cbits &= ~compute_dashmask (var->bits_to_expand);
      
      /* Compute good shift count based on lowest changing bit. */
      if (cbits == 0)  /* If no bits change, don't bother shifting. */
	shift[i] = 0;
      else  /* Shift until lowest changing bit is in lsb position. */
	for (shift[i] = 0; !((cbits >> shift[i]) & 1); shift[i]++)
	  ;
    }
}


/* Given a string like "00001--0-----000", returns a bit mask containing a 1 in
 * each position where the original string contained a '-', and a 0
 * everywhere else.
 */
static int
compute_dashmask (const char *pattern)
{
  int i, mask = 0, len = strlen (pattern);

  for (i = 0; i < len; i++)
    if (pattern[len - 1 - i] == '-')
      mask |= (1 << i);
  return mask;
}


/* Given a string like "00xx01x011dd1001", generates a mask for those bits
 * known to contain 0 or 1 values, and a list of those values.  For example,
 * in the example the mask would be 1100110111001111 and the values would
 * be 0000010011001001 (unused values are set to 0).
 */
static void
compute_literal_bits (const char *pattern, int *lbp, int *lbmp)
{
  int i;
  int literal_bits = 0, literal_bits_mask = 0;

  for (i = 0; i < 16; i++)
    {
      if (pattern[15 - i] == '0')
	literal_bits_mask |= 1 << i;
      else if (pattern[15 - i] == '1')
	{
	  literal_bits_mask |= 1 << i;
	  literal_bits |= 1 << i;
	}
    }

  *lbp = literal_bits;
  *lbmp = literal_bits_mask;
}


static int
compute_synthetic_opcode (int m68k_opcode, const OpcodeMappingInfo *map)
{
  return (((m68k_opcode & map->opcode_and_bits) >> map->opcode_shift_count)
	  + map->opcode_add_bits) & 0xFFFF;
}


/* This function "reserves" all synthetic opcodes that might be computed
 * during the mapping from 68k->synthetic opcodes for a particular variant.
 * This helps minimize the number of OpcodeMappingInfo sequences we'll need
 * by minimizing conflicts between different CC variants of the same opcode.
 */
static void
reserve_synthetic_ops (OpcodeMappingInfo *maps, int num_variants, int variant,
		       int num_variant_mapping_sets, List *isect_list,
		       int literal_bits, int literal_bits_mask)
{
  int m68kop;
  int i;

  for (m68kop = 0; m68kop < 65536; m68kop++)
    {
      if ((m68kop & literal_bits_mask) != literal_bits
	  || map_index[m68kop] != NO_MAP
	  || !is_member_of_set (m68kop, isect_list))
	continue;

      for (i = 0; i < num_variant_mapping_sets; i++)
	{
	  int synop = compute_synthetic_opcode (m68kop, &maps[num_variants * i
							      + variant]);

	  if (!SYNTHETIC_OPCODE_TAKEN (synop, variant))
	    synthetic_opcode_taken[synop] = variant;
	}
    }
}


static BOOL
has_unexpanded_register_lvalue (List *code, const char *opcode_bits,
				const char *bits_to_expand, int *index)
{
  if (code == NULL)
    return FALSE;

  if (code->token.type == TOK_ASSIGN && code->cdr != NULL
      && (code->cdr->token.type == TOK_DOLLAR_DATA_REGISTER
	  || code->cdr->token.type == TOK_DOLLAR_ADDRESS_REGISTER))
    {
      int field = code->cdr->token.u.dollarinfo.which;
      if (!field_expanded (field, opcode_bits, bits_to_expand))
	{
	  PatternRange range;
	  pattern_range (opcode_bits, field, &range);
	  if (range.index < 16)
	    {
	      /* Only report regs that we can possibly expand. */
	      *index = range.index;
	      return TRUE;
	    }
	}
    }

  /* Recurse on the rest of the list. */
  return (has_unexpanded_register_lvalue (code->car, opcode_bits,
					  bits_to_expand, index)
	  || has_unexpanded_register_lvalue (code->cdr, opcode_bits,
					     bits_to_expand, index));
}


static void
delete_field (List *code, int field_number, int val)
{
  if (code == NULL)
    return;
  if (IS_DOLLAR_TOKEN (code->token.type))
    {
      if (code->token.u.dollarinfo.which == field_number)
	{
	  if (code->token.type != TOK_DOLLAR_NUMBER)
	    parse_error (code, "Deleting field that still exists!\n");
	  else
	    {
	      code->token.type = TOK_NUMBER;
	      code->token.u.n = val;
	    }
	}
      else if (code->token.u.dollarinfo.which > field_number)
	--code->token.u.dollarinfo.which;
    }

  delete_field (code->car, field_number, val);
  delete_field (code->cdr, field_number, val);
}


static void
replace_dollar_number_with_list (List *code, int field, List *list)
{
  if (code == NULL)
    return;

  if (code->token.type == TOK_DOLLAR_NUMBER
      && code->token.u.dollarinfo.which == field)
    {
      List *new = copy_list (list);
      List *old_cdr = code->cdr;

      *code = *new;
      code->cdr = old_cdr;
    }
  else
    replace_dollar_number_with_list (code->car, field, list);
  replace_dollar_number_with_list (code->cdr, field, list);
}
