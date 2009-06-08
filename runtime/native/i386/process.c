#include "config.h"

#include "template.h"
#include "process.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

static int create_asmdata (const template_t *t, int num_operands);
static void dump_m68k_cc (char *s, const char *i386_cc);

static boolean_t
is_operand_holder (const char *p, int *operand_nump, boolean_t *pc_relative_pp)
{
  boolean_t retval;

  if (p[0] != '%')
    retval = FALSE;
  else
    {
      boolean_t unused;

      if (!pc_relative_pp)
	pc_relative_pp = &unused;

      *pc_relative_pp = p[1] == 'P';
      if (*pc_relative_pp)
	++p;
      retval = isdigit (p[1]);
      if (retval)
	if (operand_nump)
	  *operand_nump = atoi (&p[1]);
    }
  return retval;
}

static void
operand_replace (const char *old_code, char *new_code, int operand_num,
		 const char *new_operand, int end_num)
{
  const char *s;
  char *d;

  for (s = old_code, d = new_code; *s != '\0'; )
    {
      int num;
      boolean_t pc_p;

      if (!is_operand_holder (s, &num, &pc_p) || num != operand_num)
	*d++ = *s++;
      else
	{
	  if (pc_p)
	    {
	      sprintf (d, "code_end_%d+", end_num);
	      d+= strlen (d);
	      ++s; /* to skip over the P */
	    }
	  strcpy (d, new_operand);
	  d += strlen (d);
	  for (s++; isdigit (*s); s++)
	    ;
	}
    }

  *d = '\0';
}

int
process_template (FILE *fp, FILE *header_fp, const template_t *t,
		  const char *make, int swapop_p)
{
  int i, num_operands;
  char cc[128], cmd[1024], buf[2048];
  static char last_code_generated[32767];
  FILE *bits;

  /* Count the operands. */
  num_operands = count_operands (t->code);

  /* Generate the C code for all the operand combinations.  It's the
   * same for the case where our operands are swapped, so just reuse
   * the old information if swapop_p is true.
   */
  if (!swapop_p)
    {
      long size;

      if (create_asmdata (t, num_operands) == FAILURE)
	return 0;

      /* Build the program to analyze it.  If the make failed, we fail too. */
      system ("rm -f analyze analyze.o");
      sprintf (cmd, "%s -s analyze > /dev/null", make);
      if (system (cmd) != 0)
	return FAILURE;

      /* Get the output of the analyzing program. */
      sprintf (cmd, "./analyze");
      bits = popen (cmd, "r");
      if (bits == NULL)
	return FAILURE;

      /* Copy everything from our input pipe to our buffer. */
      size = fread (last_code_generated, 1, sizeof last_code_generated, bits);
      assert (size >= 0 && size < sizeof last_code_generated);
      last_code_generated[size] = '\0';
      if (pclose (bits) != 0)
	return FAILURE;
    }
  else
    abort ();

  /* Output the code. */
  sprintf (buf,
	   "/* Temporary machine-generated file.  Delete me. */\n"
	   "\n"
	   "#include \"syn68k_private.h\"\n"
	   "#include \"native.h\"\n"
	   "\n"
	   "\n"
	   "int\n"
	   "%s%s (COMMON_ARGS",
	   t->macro_name, swapop_p ? "_swapop" : "");
  fputs (buf, fp);

  /* Output the prototype for this function to the haeder. */
  sprintf (buf, "extern int %s%s (COMMON_ARGS",
	   t->macro_name, swapop_p ? "_swapop" : "");
  fputs (buf, header_fp);

  if (swapop_p)
    {
      for (i = num_operands - 1; i >= 0; i--)
	{
	  fprintf (fp,        ", int32 %s", t->operand_name[i]);
	  fprintf (header_fp, ", int32 %s", t->operand_name[i]);
	}
    }
  else  /* !swapop_p */
    {
      for (i = 0; i < num_operands; i++)
	{
	  assert (t->operand_name[i] != NULL);
	  fprintf (fp,        ", int32 %s", t->operand_name[i]);
	  fprintf (header_fp, ", int32 %s", t->operand_name[i]);
	}
    }
  fputs (");\n", header_fp);

  fputs (")\n"
	 "{\n"
	 "  host_code_t *code;\n",
	 fp);

  dump_m68k_cc (cc, t->i386_cc_out);
  if (cc[0] != '-')
    {
      if (!strcmp (cc, "CNVXZ"))
	fprintf (fp, "  SPILL_CC_BITS (c, codep, cc_spill_if_changed);\n");
      else
	{
	  int b;
	  fprintf (fp, "  SPILL_CC_BITS (c, codep, cc_spill_if_changed "
		   "& (");
	  for (b = 0; cc[b] != '\0'; b++)
	    fprintf (fp, "%sM68K_CC%c", (b > 0) ? "| " : "", cc[b]);
	  fputs ("));\n", fp);
	}

      /* Note which CC bits we just made valid. */
      if (strcmp (cc, "CNVXZ"))
	{
	  int b;
	  fprintf (fp,
		   "  {\n"
		   "     uint8 newly_valid_cc = (cc_to_compute & (");
	  for (b = 0; cc[b] != '\0'; b++)
	    fprintf (fp, "%sM68K_CC%c", (b > 0) ? "| " : "", cc[b]);
	  fputs ("));\n", fp);
	  fprintf (fp,
		   "\n"
		   "     c->cached_cc |= newly_valid_cc;\n"
		   "     c->dirty_cc  |= newly_valid_cc;\n"
		   "  }\n");
	}
      else
	{
	  fputs ("  c->cached_cc |= cc_to_compute;\n"
		 "  c->dirty_cc  |= cc_to_compute;\n",
		 fp);
	}
    }

  fputs ("  code = *codep;\n", fp);
  fputs (last_code_generated, fp);
  fputs ("  *codep = code;\n"
	 "  return 0;\n"
	 "}\n",
	 fp);

  /* Just so I can watch its progress more easily, flush stuff. */
  fflush (fp);
  fflush (header_fp);

  return SUCCESS;
}


static void
dump_m68k_cc (char *d, const char *i386_cc)
{
  cc_mask_t m68k_cc;
  const char *s;

  m68k_cc = M68K_CC_NONE;
  for (s = i386_cc; *s != '\0'; s++)
    switch (*s)
      {
      case 'c':
	m68k_cc |= (M68K_CCC | M68K_CCX);
	break;
      case 'o':
	m68k_cc |= M68K_CCV;
	break;
      case 's':
	m68k_cc |= M68K_CCN;
	break;
      case 'z':
	m68k_cc |= M68K_CCZ;
	break;
      case 'a':
      case 'p':
      case 'd':
      case 'i':
	break;
      default:
	fprintf (stderr, "Unknown i386 cc bit specifier \"%c\".\n", *s);
	break;
      }

  if (m68k_cc == M68K_CC_NONE)
    *d++ = '-';
  else
    {
      if (m68k_cc & M68K_CCC)
	*d++ = 'C';
      if (m68k_cc & M68K_CCN)
	*d++ = 'N';
      if (m68k_cc & M68K_CCV)
	*d++ = 'V';
      if (m68k_cc & M68K_CCX)
	*d++ = 'X';
      if (m68k_cc & M68K_CCZ)
	*d++ = 'Z';
    }

  *d = '\0';
}

int
count_operands (const char *s)
{
  int num_operands;

  for (num_operands = 0; ; num_operands++)
    {
      const char *p;
      for (p = s; *p != '\0'; p++)
	{
	  int num;

	  if (is_operand_holder (p, &num, NULL) && num == num_operands)
	    break;
	}
      /* If we failed to find the operand, we're done. */
      if (*p == '\0')
	break;
    }

  return num_operands;
}


#define MAX_VALUE_SET_ENTRIES 35

typedef struct
{
  int num_values;
  long value[MAX_VALUE_SET_ENTRIES];
} value_set_t;

static int
create_asmdata (const template_t *t, int num_operands)
{
  static const value_set_t immediate_values[] =
    {  { 12, { 0, 1, 2, -1, -2, 127, -128, -127, 0x37, -100, 0x12, -97 } },
	 { 23, { 0, 1, 2, 0xFF, 0xFE, 127, -129, -128, -127, 128, 0x37,
		   -100, 0x12, -97, 32767, -32768, -32767, -1, -2, 0x871,
		   -1234, 0x1234, -561 } },
	 { 33, { 0, 1, 2, 0xFF, 0xFE, 128, -129, 127, -128, -127, 0x37, -100,
		   0x12, -97,
		   32767, -32768, -32767, -1, 0xFFFE, 0x871, 0xFA03,
		   0x1234, 0x8765, 0x7FFFFFFF, 0x80000000, 0x80000001,
		   0xFFFFFFFF, 0xFFFFFFFE, 0x871529, 0x392332, 0xFA034433,
                 0x12345678, 0x87654321 } },
     };
#if 0
  /* Explanation of this warning:
   * The i386 has slightly more compact opcode sequences in some situations
   * when %al/%ax/%eax is involved in an operation.  The register specifier
   * byte is omitted and the register is implicit in the special case opcode.
   * Unfortunately, this can cause:
   * addw $0x1234,%ax    ; Using %ax shaves off a byte
   * addw $1,%bx         ; small immediate shaves off one byte
   * to both require the same number of compiled bytes.  analyze.c isn't
   * yet smart enough to make fine distinctions between two different
   * bit patterns of the same size which don't accept the same operands into
   * the same bit offsets.
   */
#warning "Intentionally overlooking compact special cases for %al/%ax/%eax"
#endif

  static const value_set_t register_values[3] =
    {
      { 8, { 0, 1, 2, 3, 4, 5, 6, 7 } },
      { 6, { 0, 1, 2, 3, /* skip %bp,%sp   */ 6, 7 } },
      { 6, { 0, 1, 2, 3, /* skip %ebp,%esp */ 6, 7 } }
    };
  static const char *register_name[3][8] =
    {
      /* We don't consider %esp and %ebp here because they are "escape"
       * registers in some circumstances to indicate different addressing
       * modes, and we don't allocate anything into them anyway.
       * It's best to not confuse our software by allowing escape sequences.
       */
      { "%al",  "%cl",  "%dl",  "%bl", "%ah", "%ch", "%dh",  "%bh"  },
      { "%ax",  "%cx",  "%dx",  "%bx",  NULL, NULL,  "%si",  "%di"  },
      { "%eax", "%ecx", "%edx", "%ebx", NULL, NULL,  "%esi", "%edi" }
    };
  int current, n, op;
  int which[MAX_OPERANDS + 1];
  const value_set_t *value[MAX_OPERANDS];
  boolean_t done_p;
  long v;
  FILE *fp;

  memset (which, 0, sizeof which);
  memset (value, 0xff, sizeof value); /* help us detect uninitialized use */

  fp = fopen ("asmdata.h", "w");
  if (fp == NULL)
    {
      fprintf (stderr, "Unable to open asmdata.c for writing.\n");
      return FAILURE;
    }

  fputs ("/* This file is machine-generated and ephemeral.  DO NOT EDIT! */\n"
	 "\n"
	 "extern void asmdata (void);  /* Avoid compiler warnings. */\n"
	 "void asmdata ()\n"
	 "{\n",
	 fp);

  /* Determine which test value sets we will use for each operand. */
  for (n = 0; n < num_operands; n++)
    {
      if (t->operand[n].type == REGISTER)
	value[n] = &register_values[t->operand[n].size];
      else
	value[n] = &immediate_values[t->operand[n].size];
    }

  for (current = 0, done_p = FALSE; !done_p; current++)
    {
      char code[2][1024];
      int new_code = 0;
#if defined(HAVE_SYMBOL_UNDERSCORE)
      const char *symbol_prefix = "_";
#else
      const char *symbol_prefix = "";
#endif

      strcpy (&code[new_code][0], t->code);
      for (op = 0; op < num_operands; op++)
	{
	  char buf[100];

	  v = value[op]->value[which[op]];
	  if (t->operand[op].type == REGISTER)
	    strcpy (buf, register_name[t->operand[op].size][v]);
	  else
	    sprintf (buf, "%ld", v);
	  operand_replace (code[new_code], code[!new_code], op, buf, current);
	  new_code = !new_code;
	}

      /*
       * Use local labels so we get smaller branches even on Mac OS X.  The
       * following note comes from the Mac OS X Assembler reference:
       *
       * Note: The Mac OS X assembler for Intel i386 processors always
       * produces branch instructions that are long (32 bits) for non-local
       * labels. This allows the link editor to do procedure ordering (see
       * the description of the -sectorder option in the ld(1) man page).
       */

      fprintf (fp,
	       "  asm volatile (\"\\n\"\n"
	       "                \"%scode_start_%d:\\n\\t\"\n"
	       "                \"%s\\n\"\n"
	       "                \"Lcode_end_%d:\\n\"\n"
	       "                \"%scode_end_%d:\");\n",
               symbol_prefix, current, code[new_code], current,
               symbol_prefix, current);

      /* Try the next combination of operands. */
      for (op = num_operands - 1; op >= 0; op--)
	{
	  if (++which[op] >= value[op]->num_values)
	    {
	      which[op] = 0;
	      if (op == 0)
		done_p = TRUE;
	    }
	  else
	    break;
	}

      if (num_operands == 0)
	done_p = TRUE;
    }
  
  fputs ("}\n", fp);

  fprintf (fp,
	   "\n"
	   "#define NUM_SAMPLES %d\n"
	   "#define NUM_OPERANDS %d\n"
	   "#define TEMPLATE template[%d]\n"
	   "\n",
	   current, num_operands, t - &template[0]);

  fputs ("static const long value[NUM_SAMPLES][NUM_OPERANDS + 1] =\n"
	 "{\n",
	 fp);

  memset (which, 0, sizeof which);
  for (n = 0; n < current; n++)
    {
      fprintf (fp, "  {");
      for (op = 0; op < num_operands; op++)
	fprintf (fp, "%s 0x%lX", op == 0 ? "" : ",",
		 (unsigned long) value[op]->value[which[op]]);
      fputs (" },\n", fp);

      /* Try the next combination of operands. */
      for (op = num_operands - 1; op >= 0; op--)
	{
	  if (++which[op] >= value[op]->num_values)
	    which[op] = 0;
	  else
	    break;
	}
    }
  fputs ("};\n", fp);

  for (n = 0; n < current; n++)
    {
      fprintf (fp,
	       "extern uint8 code_start_%d;\n"
	       "extern uint8 code_end_%d;\n",
	       n, n);
    }

  fputs ("\n"
	 "typedef struct\n"
	 "{\n"
	 "  const uint8 *start, *end;\n"
	 "} sample_t;\n"
	 "\n",
	 fp);
  fputs ("static const sample_t sample[NUM_SAMPLES] =\n"
	 "{\n", fp);

  memset (which, 0, sizeof which);
  for (n = 0; n < current; n++)
    {
      fprintf (fp,
	       "  { &code_start_%d, &code_end_%d }",
	       n, n);
      /* Try the next combination of operands. */
      for (op = num_operands - 1; op >= 0; op--)
	{
	  if (++which[op] >= value[op]->num_values)
	    which[op] = 0;
	  else
	    break;
	}

      if (n < current - 1)
	fputs (",\n", fp);
    }
  fputs ("};\n", fp);
  
  fclose (fp);

  return SUCCESS;
}
