#include "xlate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


static void output_desc_list (FILE *table_fp, FILE *header_fp,
			      guest_code_descriptor_t *g);
static BOOL gcd_equal_p (const guest_code_descriptor_t *g1,
			 const guest_code_descriptor_t *g2);


int
main (void)
{
  const xlate_descriptor_t *x;
  FILE *table_fp, *header_fp;

  table_fp  = fopen ("host-xlate.c", "w");
  assert (table_fp);
  header_fp = fopen ("host-xlate.h", "w");
  assert (header_fp);

  fputs ("/* This file is machine-generated; DO NOT EDIT! */\n"
	 "\n"
	 "#ifndef _host_xlate_h_\n"
	 "#define _host_xlate_h_\n"
	 "\n",
	 header_fp);

  fputs ("/* This file is machine-generated; DO NOT EDIT! */\n"
	 "\n"
	 "#ifndef RUNTIME\n"
	 "#define RUNTIME  /* So stuff gets typedefed properly. */\n"
	 "#endif\n"
	 "#include \"syn68k_private.h\"\n"
	 "#include \"native.h\"\n"
	 "#include \"host-xlate.h\"\n"
	 "#include \"i386-isa.h\"\n"
	 "#include \"i386-aux.h\"\n"
	 "\n"
	 "\n",
	 table_fp);

  for (x = xlate_table; x->name != NULL; x++)
    output_desc_list (table_fp, header_fp, process_xlate_descriptor (x));

  fputs ("\n"
	 "#endif  /* _host_xlate_h */\n",
	 header_fp);
  
  fclose (table_fp);
  fclose (header_fp);

  return EXIT_SUCCESS;
}


static void
output_desc (FILE *table_fp, FILE *header_fp, guest_code_descriptor_t *g)
{
  int i;
  guest_code_descriptor_t *next;

  if (g->static_p)
    fputs ("static ", table_fp);
  else
    fprintf (header_fp, "extern const guest_code_descriptor_t %s;\n", g->name);
  fprintf (table_fp, "const guest_code_descriptor_t %s =\n"
	   "{\n", g->name);
  
  fputs ("  {\n", table_fp);
  for (i = 0; g->reg_op_info[i].legitimate_p; i++)
    {
      const reg_operand_info_t *r = &g->reg_op_info[i];
      fprintf (table_fp, "    { TRUE, %s, %d, 0x%X, %d, %d, 0x%X },\n",
	       r->add8_p ? "TRUE" : "FALSE", r->operand_num,
	       (unsigned) r->acceptable_mapping, r->request_type,
	       r->output_state, (unsigned) r->regset);
    }
  fputs ("    { FALSE },\n"
	 "  },\n",
	 table_fp);

  fprintf (table_fp, "  0x%X, 0x%X, 0x%X,\n"
	   "  {\n",
	   (unsigned) g->cc_in, (unsigned) g->cc_out,
	   (unsigned) g->scratch_reg);

  
  for (i = 0; i < MAX_COMPILE_FUNCS && g->compile_func[i].func; i++)
    {
      fprintf (table_fp, "    { %s, { { %d, %d, %d, %d } } },\n",
	       g->compile_func[i].func,
	       g->compile_func[i].order.operand_num[0],
	       g->compile_func[i].order.operand_num[1],
	       g->compile_func[i].order.operand_num[2],
	       g->compile_func[i].order.operand_num[3]);
    }

  next = g->next;
  if (next && !g->next->static_p)
    next = NULL;
  fprintf (table_fp,
	   "  },\n"
	   "  %s%s\n"
	   "};\n"
	   "\n", next ? "&" : "", next ? next->name : "NULL");
}


/* Prints out the list in reverse order. */
static void
output_desc_list (FILE *table_fp, FILE *header_fp,
		  guest_code_descriptor_t *g)
{
  if (g == NULL)
    return;

  /* Filter out two adjacent desc's that are equal except for the
   * cc bits they claim to compute.  If we hit this situation, we might
   * as well only bother with the guy who claims to compute the most
   * cc bits.
   */
  if (!g->static_p && g->next != NULL && g->next->static_p
      && (g->cc_out & g->next->cc_out) == g->cc_out)
    {
      int equal_p;
      unsigned save_cc_out;

      /* See if the two are essentially identical. */
      save_cc_out = g->cc_out;
      g->cc_out = g->next->cc_out;
      equal_p = gcd_equal_p (g, g->next);
      g->cc_out = save_cc_out;

      if (equal_p)
	{
	  g->next->name = g->name;
	  g->next->static_p = FALSE;
	  output_desc_list (table_fp, header_fp, g->next);
	  return;
	}
    }

  output_desc_list (table_fp, header_fp, g->next);
  output_desc (table_fp, header_fp, g);
}


static BOOL
gcd_equal_p (const guest_code_descriptor_t *g1,
	     const guest_code_descriptor_t *g2)
{
  int i;

  if (g1->cc_in != g2->cc_in
      || g1->cc_out != g2->cc_out
      || g1->scratch_reg != g2->scratch_reg)
    return FALSE;

  if (memcmp (g1->reg_op_info, g2->reg_op_info, sizeof g1->reg_op_info))
    return FALSE;

  for (i = 0; i < MAX_COMPILE_FUNCS; i++)
    {
      int g1n, g2n;

      g1n = (g1->compile_func[i].func == NULL);
      g2n = (g2->compile_func[i].func == NULL);
      if (g1n || g2n)
	{
	  if (g1n ^ g2n)
	    return FALSE;
	  break;
	}

      if (strcmp (g1->compile_func[i].func, g2->compile_func[i].func))
	return FALSE;
      if (memcmp (&g1->compile_func[i].order, &g2->compile_func[i].order,
		  sizeof g1->compile_func[i].order))
	return FALSE;
    }

  return TRUE;
}
