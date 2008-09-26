#include "xlate.h"


const xlate_descriptor_t xlate_table[] =
{
  /* For moves, watch out!  word moves into address reg are sign extended.
   * Moves into address regs never touch the cc bits.
   */

  /* move imm,<ea> */
  { "move@_imm_reg_1_0",      OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_IMM, { 1, } }, { AMODE_REG,     { 0,   } } } },
  { "move@_imm_areg_1_0",     OP_MOVE, WL,  M68K_CC_NONE, "mov",
      { { AMODE_IMM, { 1, } }, { AMODE_AREG,    { 0,   } } } },
  { "move@_imm_abs_0_1",      OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_IMM, { 0, } }, { AMODE_ABS,     { 1,   } } } },
  { "move@_imm_ind_1_0",      OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_IMM, { 1, } }, { AMODE_IND,     { 0,   } } } },
  { "move@_imm_predec_1_0",   OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_IMM, { 1, } }, { AMODE_PREDEC,  { 0,   } } } },
  { "move@_imm_postinc_1_0",  OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_IMM, { 1, } }, { AMODE_POSTINC, { 0,   } } } },
  { "move@_imm_indoff_1_2_0", OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_IMM, { 1, } }, { AMODE_INDOFF,  { 2, 0 } } } },
  { "move@_imm_indix_1_0",    OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_IMM, { 1, } }, { AMODE_INDIX, { 0,   } } } },

  /* move imm,<ea>, with earlier imm */
  { "move@_imm_reg_0_1",      OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_IMM, { 0, } }, { AMODE_REG,     { 1,   } } } },
  { "move@_imm_areg_0_1",     OP_MOVE, WL,  M68K_CC_NONE, "mov",
      { { AMODE_IMM, { 0, } }, { AMODE_AREG,    { 1,   } } } },
  { "move@_imm_ind_0_1",      OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_IMM, { 0, } }, { AMODE_IND,     { 1,   } } } },
  { "move@_imm_predec_0_1",   OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_IMM, { 0, } }, { AMODE_PREDEC,  { 1,   } } } },
  { "move@_imm_postinc_0_1",  OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_IMM, { 0, } }, { AMODE_POSTINC, { 1,   } } } },
  { "move@_imm_indoff_0_2_1", OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_IMM, { 0, } }, { AMODE_INDOFF,  { 2, 1 } } } },
  { "move@_imm_indix_0_1",    OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_IMM, { 0, } }, { AMODE_INDIX, { 1,   } } } },

  /* move reg,<ea> */
  { "move@_reg_reg_1_0",       OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_REG, { 1, } }, { AMODE_REG,     { 0, } } } },
  { "move@_reg_areg_1_0",      OP_MOVE, WL,  M68K_CC_NONE, "mov",
      { { AMODE_REG, { 1, } }, { AMODE_AREG,    { 0, } } } },
  { "move@_reg_abs_0_1",       OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_REG, { 0, } }, { AMODE_ABS,     { 1, } } } },
  { "move@_reg_ind_1_0",       OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_REG, { 1, } }, { AMODE_IND,     { 0, } } } },
  { "move@_reg_predec_1_0",    OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_REG, { 1, } }, { AMODE_PREDEC,  { 0, } } } },
  { "move@_reg_postinc_1_0",   OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_REG, { 1, } }, { AMODE_POSTINC, { 0, } } } },
  { "move@_reg_indoff_1_2_0",  OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_REG, { 1, } }, { AMODE_INDOFF,  { 2, 0 } } } },
  { "move@_reg_indix_1_0",     OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_REG, { 1, } }, { AMODE_INDIX,   { 0, } } } },

  /* move areg,<ea> */
  { "move@_areg_reg_1_0",       OP_MOVE, WL, M68K_CC_CNVZ, "mov",
      { { AMODE_AREG, { 1, } }, { AMODE_REG,     { 0, } } } },
  { "move@_areg_areg_1_0",      OP_MOVE, WL, M68K_CC_NONE, "mov",
      { { AMODE_AREG, { 1, } }, { AMODE_AREG,    { 0, } } } },
  { "move@_areg_abs_0_1",       OP_MOVE, WL, M68K_CC_CNVZ, "mov",
      { { AMODE_AREG, { 0, } }, { AMODE_ABS,     { 1, } } } },
  { "move@_areg_ind_1_0",       OP_MOVE, WL, M68K_CC_CNVZ, "mov",
      { { AMODE_AREG, { 1, } }, { AMODE_IND,     { 0, } } } },
  { "move@_areg_predec_1_0",    OP_MOVE, WL, M68K_CC_CNVZ, "mov",
      { { AMODE_AREG, { 1, } }, { AMODE_PREDEC,  { 0, } } } },
  { "move@_areg_postinc_1_0",   OP_MOVE, WL, M68K_CC_CNVZ, "mov",
      { { AMODE_AREG, { 1, } }, { AMODE_POSTINC, { 0, } } } },
  { "move@_areg_indoff_1_2_0",  OP_MOVE, WL, M68K_CC_CNVZ, "mov",
      { { AMODE_AREG, { 1, } }, { AMODE_INDOFF,  { 2, 0 } } } },
  { "move@_areg_indix_1_0",     OP_MOVE, WL, M68K_CC_CNVZ, "mov",
      { { AMODE_AREG, { 1, } }, { AMODE_INDIX,   { 0, } } } },

  /* move ind,<ea> */
  { "move@_ind_reg_1_0",       OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_IND, { 1, } }, { AMODE_REG,     { 0, } } } },
  { "move@_ind_areg_1_0",      OP_MOVE, WL,  M68K_CC_NONE, "mov",
      { { AMODE_IND, { 1, } }, { AMODE_AREG,    { 0, } } } },
  { "move@_ind_abs_0_1",       OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_IND, { 0, } }, { AMODE_ABS,     { 1, } } } },
  { "move@_ind_ind_1_0",       OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_IND, { 1, } }, { AMODE_IND,     { 0, } } } },
  { "move@_ind_predec_1_0",    OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_IND, { 1, } }, { AMODE_PREDEC,  { 0, } } } },
  { "move@_ind_postinc_1_0",   OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_IND, { 1, } }, { AMODE_POSTINC, { 0, } } } },
  { "move@_ind_indoff_1_2_0",  OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_IND, { 1, } }, { AMODE_INDOFF,  { 2, 0 } } } },
  { "move@_ind_indix_1_0",     OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_IND, { 1, } }, { AMODE_INDIX,   { 0, } } } },

  /* move postinc,<ea> */
  { "move@_postinc_reg_1_0",       OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_POSTINC, { 1, } }, { AMODE_REG,     { 0, } } } },
  { "move@_postinc_areg_1_0",      OP_MOVE, WL,  M68K_CC_NONE, "mov",
      { { AMODE_POSTINC, { 1, } }, { AMODE_AREG,    { 0, } } } },
  { "move@_postinc_abs_0_1",       OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_POSTINC, { 0, } }, { AMODE_ABS,     { 1, } } } },
  { "move@_postinc_ind_1_0",       OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_POSTINC, { 1, } }, { AMODE_IND,     { 0, } } } },
  { "move@_postinc_predec_1_0",    OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_POSTINC, { 1, } }, { AMODE_PREDEC,  { 0, } } } },
  { "move@_postinc_postinc_1_0",   OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_POSTINC, { 1, } }, { AMODE_POSTINC, { 0, } } } },
  { "move@_postinc_indoff_1_2_0",  OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_POSTINC, { 1, } }, { AMODE_INDOFF,  { 2, 0 } } } },
  { "move@_postinc_indix_1_0",     OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_POSTINC, { 1, } }, { AMODE_INDIX,   { 0, } } } },

  /* move predec,<ea> */
  { "move@_predec_reg_1_0",       OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_PREDEC, { 1, } }, { AMODE_REG,     { 0, } } } },
  { "move@_predec_areg_1_0",      OP_MOVE, WL,  M68K_CC_NONE, "mov",
      { { AMODE_PREDEC, { 1, } }, { AMODE_AREG,    { 0, } } } },
  { "move@_predec_abs_0_1",       OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_PREDEC, { 0, } }, { AMODE_ABS,     { 1, } } } },
  { "move@_predec_ind_1_0",       OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_PREDEC, { 1, } }, { AMODE_IND,     { 0, } } } },
  { "move@_predec_predec_1_0",    OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_PREDEC, { 1, } }, { AMODE_PREDEC,  { 0, } } } },
  { "move@_predec_postinc_1_0",   OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_PREDEC, { 1, } }, { AMODE_POSTINC, { 0, } } } },
  { "move@_predec_indoff_1_2_0",  OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_PREDEC, { 1, } }, { AMODE_INDOFF,  { 2, 0 } } } },
  { "move@_predec_indix_1_0",     OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_PREDEC, { 1, } }, { AMODE_INDIX,   { 0, } } } },

  /* move abs,<ea> */
  { "move@_abs_reg_1_0",       OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_ABS, { 1, } }, { AMODE_REG,     { 0, } } } },
  { "move@_abs_areg_1_0",      OP_MOVE, WL,  M68K_CC_NONE, "mov",
      { { AMODE_ABS, { 1, } }, { AMODE_AREG,    { 0, } } } },
  { "move@_abs_abs_0_1",       OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_ABS, { 0, } }, { AMODE_ABS,     { 1, } } } },
  { "move@_abs_ind_1_0",       OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_ABS, { 1, } }, { AMODE_IND,     { 0, } } } },
  { "move@_abs_predec_1_0",    OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_ABS, { 1, } }, { AMODE_PREDEC,  { 0, } } } },
  { "move@_abs_postinc_1_0",   OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_ABS, { 1, } }, { AMODE_POSTINC, { 0, } } } },
  { "move@_abs_indoff_1_2_0",  OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_ABS, { 1, } }, { AMODE_INDOFF,  { 2, 0 } } } },

  /* move indoff,<ea> */
  { "move@_indoff_reg_2_1_0",       OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_INDOFF, { 2, 1 } }, { AMODE_REG,     { 0, } } } },
  { "move@_indoff_areg_2_1_0",      OP_MOVE, WL,  M68K_CC_NONE, "mov",
      { { AMODE_INDOFF, { 2, 1 } }, { AMODE_AREG,    { 0, } } } },
  { "move@_indoff_abs_1_0_2",       OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_INDOFF, { 1, 0 } }, { AMODE_ABS,     { 2, } } } },
  { "move@_indoff_ind_2_1_0",       OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_INDOFF, { 2, 1 } }, { AMODE_IND,     { 0, } } } },
  { "move@_indoff_predec_2_1_0",    OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_INDOFF, { 2, 1 } }, { AMODE_PREDEC,  { 0, } } } },
  { "move@_indoff_postinc_2_1_0",   OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_INDOFF, { 2, 1 } }, { AMODE_POSTINC, { 0, } } } },
  { "move@_indoff_indoff_2_1_3_0",  OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_INDOFF, { 2, 1 } }, { AMODE_INDOFF,  { 3, 0 } } } },
  { "move@_indoff_indix_2_1_0",     OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_INDOFF, { 2, 1 } }, { AMODE_INDIX,   { 0, } } } },

  /* move indix,<ea> */
  { "move@_indix_reg_1_0",       OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_INDIX, { 1, } }, { AMODE_REG,     { 0, } } } },
  { "move@_indix_areg_1_0",      OP_MOVE, WL,  M68K_CC_NONE, "mov",
      { { AMODE_INDIX, { 1, } }, { AMODE_AREG,    { 0, } } } },
  { "move@_indix_abs_0_1",       OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_INDIX, { 0, } }, { AMODE_ABS,     { 1, } } } },
  { "move@_indix_ind_1_0",       OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_INDIX, { 1, } }, { AMODE_IND,     { 0, } } } },
  { "move@_indix_predec_1_0",    OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_INDIX, { 1, } }, { AMODE_PREDEC,  { 0, } } } },
  { "move@_indix_postinc_1_0",   OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_INDIX, { 1, } }, { AMODE_POSTINC, { 0, } } } },
  { "move@_indix_indoff_1_2_0",  OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_INDIX, { 1, } }, { AMODE_INDOFF,  { 2, 0 } } } },
  { "move@_indix_indix_1_0",     OP_MOVE, BWL, M68K_CC_CNVZ, "mov",
      { { AMODE_INDIX, { 1, } }, { AMODE_INDIX,   { 0, } } } },

#define BITWISE(op)							\
  /* op imm,<ea> */							\
  { #op "@_imm_reg_1_0",      OP_BINARY, BWL, M68K_CC_CNVZ, #op,	\
      { { AMODE_IMM, { 1,  } }, { AMODE_REG, { 0, } } } },		\
  { #op "@_imm_abs_0_1",      OP_BINARY, BWL, M68K_CC_CNVZ, #op,	\
      { { AMODE_IMM, { 0, } }, { AMODE_ABS,     { 1,   } } } },		\
  { #op "@_imm_ind_1_0",      OP_BINARY, BWL, M68K_CC_CNVZ, #op,	\
      { { AMODE_IMM, { 1, } }, { AMODE_IND,     { 0,   } } } },		\
  { #op "@_imm_predec_1_0",   OP_BINARY, BWL, M68K_CC_CNVZ, #op,	\
      { { AMODE_IMM, { 1, } }, { AMODE_PREDEC,  { 0,   } } } },		\
  { #op "@_imm_postinc_1_0",  OP_BINARY, BWL, M68K_CC_CNVZ, #op,	\
      { { AMODE_IMM, { 1, } }, { AMODE_POSTINC, { 0,   } } } },		\
  { #op "@_imm_indoff_1_2_0", OP_BINARY, BWL, M68K_CC_CNVZ, #op,	\
      { { AMODE_IMM, { 1, } }, { AMODE_INDOFF,  { 2, 0 } } } },		\
									\
  /* op reg,<ea> */							\
  { #op "@_reg_abs_0_1",       OP_BINARY, BWL, M68K_CC_CNVZ, #op,	\
      { { AMODE_REG, { 0, } }, { AMODE_ABS,     { 1, } } } },		\
  { #op "@_reg_ind_0_1",       OP_BINARY, BWL, M68K_CC_CNVZ, #op,	\
      { { AMODE_REG, { 0, } }, { AMODE_IND,     { 1, } } } },		\
  { #op "@_reg_predec_0_1",    OP_BINARY, BWL, M68K_CC_CNVZ, #op,	\
      { { AMODE_REG, { 0, } }, { AMODE_PREDEC,  { 1, } } } },		\
  { #op "@_reg_postinc_0_1",   OP_BINARY, BWL, M68K_CC_CNVZ, #op,	\
      { { AMODE_REG, { 0, } }, { AMODE_POSTINC, { 1, } } } },		\
  { #op "@_reg_indoff_0_2_1",  OP_BINARY, BWL, M68K_CC_CNVZ, #op,	\
      { { AMODE_REG, { 0, } }, { AMODE_INDOFF,  { 2, 1 } } } }

#define BITWISE_DST_REG(op)						\
  /* op <ea>,reg */							\
  { #op "@_reg_reg_1_0",       OP_BINARY, BWL, M68K_CC_CNVZ, #op,	\
      { { AMODE_REG, { 1, } }, { AMODE_REG,     { 0, } } } },		\
  { #op "@_abs_reg_1_0",       OP_BINARY, BWL, M68K_CC_CNVZ, #op,	\
      { { AMODE_ABS, { 1, } }, { AMODE_REG,     { 0, } } } },		\
  { #op "@_ind_reg_1_0",       OP_BINARY, BWL, M68K_CC_CNVZ, #op,	\
      { { AMODE_IND, { 1, } }, { AMODE_REG,     { 0, } } } },		\
  { #op "@_predec_reg_1_0",    OP_BINARY, BWL, M68K_CC_CNVZ, #op,	\
      { { AMODE_PREDEC, { 1, } }, { AMODE_REG,  { 0, } } } },		\
  { #op "@_postinc_reg_1_0",   OP_BINARY, BWL, M68K_CC_CNVZ, #op,	\
      { { AMODE_POSTINC, { 1, } }, { AMODE_REG, { 0, } } } },		\
  { #op "@_indoff_reg_2_1_0",  OP_BINARY, BWL, M68K_CC_CNVZ, #op,	\
      { { AMODE_INDOFF, { 2, 1 } }, { AMODE_REG,  { 0 } } } }

  BITWISE (and),
  BITWISE_DST_REG (and),
  BITWISE (or),
  BITWISE_DST_REG (or),

  /* xor is different than AND and OR.  It can only do XOR dn,<ea>,
   * and it has the added ability for the <ea> to be a data register.
   */
  { "xor@_reg_reg_0_1",       OP_BINARY, BWL, M68K_CC_CNVZ, "xor",
      { { AMODE_REG, { 0, } }, { AMODE_REG,     { 1, } } } },
  BITWISE (xor),

#define ARITHMETIC(op, cc, areg_cc)				\
  { #op "@_imm_reg_1_0",      OP_BINARY, BWL, cc, #op,		\
      { { AMODE_IMM, { 1,  } }, { AMODE_REG, { 0, } } } },	\
  { #op "@_imm_areg_1_0",     OP_BINARY, L, areg_cc, #op,	\
      { { AMODE_IMM, { 1,  } }, { AMODE_AREG, { 0, } } } },	\
  { #op "@_imm_abs_0_1",      OP_BINARY, BWL, cc, #op,		\
      { { AMODE_IMM, { 0, } }, { AMODE_ABS,     { 1,   } } } },	\
  { #op "@_imm_ind_1_0",      OP_BINARY, BWL, cc, #op,		\
      { { AMODE_IMM, { 1, } }, { AMODE_IND,     { 0,   } } } },	\
  { #op "@_imm_predec_1_0",   OP_BINARY, BWL, cc, #op,		\
      { { AMODE_IMM, { 1, } }, { AMODE_PREDEC,  { 0,   } } } },	\
  { #op "@_imm_postinc_1_0",  OP_BINARY, BWL, cc, #op,		\
      { { AMODE_IMM, { 1, } }, { AMODE_POSTINC, { 0,   } } } },	\
  { #op "@_imm_indoff_1_2_0", OP_BINARY, BWL, cc, #op,		\
      { { AMODE_IMM, { 1, } }, { AMODE_INDOFF,  { 2, 0 } } } },	\
								\
  /* op <ea>,reg */						\
  { #op "@_reg_reg_1_0",       OP_BINARY, BWL, cc, #op,		\
      { { AMODE_REG, { 1, } }, { AMODE_REG,     { 0, } } } },	\
  { #op "@_areg_reg_1_0",      OP_BINARY, WL,  cc, #op,		\
      { { AMODE_AREG, { 1, } }, { AMODE_REG,    { 0, } } } },	\
  { #op "@_abs_reg_1_0",       OP_BINARY, BWL, cc, #op,		\
      { { AMODE_ABS, { 1, } }, { AMODE_REG,     { 0, } } } },	\
  { #op "@_ind_reg_1_0",       OP_BINARY, BWL, cc, #op,		\
      { { AMODE_IND, { 1, } }, { AMODE_REG,     { 0, } } } },	\
  { #op "@_predec_reg_1_0",    OP_BINARY, BWL, cc, #op,		\
      { { AMODE_PREDEC, { 1, } }, { AMODE_REG,  { 0, } } } },	\
  { #op "@_postinc_reg_1_0",   OP_BINARY, BWL, cc, #op,		\
      { { AMODE_POSTINC, { 1, } }, { AMODE_REG, { 0, } } } },	\
  { #op "@_indoff_reg_2_1_0",  OP_BINARY, BWL, cc, #op,		\
      { { AMODE_INDOFF, { 2, 1 } }, { AMODE_REG, { 0 } } } },	\
								\
  /* op <ea>,areg */						\
  { #op "@_reg_areg_1_0",       OP_BINARY, WL, areg_cc, #op,	\
      { { AMODE_REG, { 1, } }, { AMODE_AREG,     { 0, } } } },	\
  { #op "@_areg_areg_1_0",      OP_BINARY, WL, areg_cc, #op,	\
      { { AMODE_AREG, { 1, } }, { AMODE_AREG,    { 0, } } } },	\
  { #op "@_abs_areg_1_0",       OP_BINARY, WL, areg_cc, #op,	\
      { { AMODE_ABS, { 1, } }, { AMODE_AREG,     { 0, } } } },	\
  { #op "@_ind_areg_1_0",       OP_BINARY, WL, areg_cc, #op,	\
      { { AMODE_IND, { 1, } }, { AMODE_AREG,     { 0, } } } },	\
  { #op "@_predec_areg_1_0",    OP_BINARY, WL, areg_cc, #op,	\
      { { AMODE_PREDEC, { 1, } }, { AMODE_AREG,  { 0, } } } },	\
  { #op "@_postinc_areg_1_0",   OP_BINARY, WL, areg_cc, #op,	\
      { { AMODE_POSTINC, { 1, } }, { AMODE_AREG, { 0, } } } },	\
  { #op "@_indoff_areg_2_1_0",  OP_BINARY, WL, areg_cc, #op,	\
      { { AMODE_INDOFF, { 2, 1 } }, { AMODE_AREG, { 0 } } } }


#define ARITHMETIC_DST_EA(op, cc, areg_cc)			\
  /* op reg,<ea> */						\
  { #op "@_reg_reg_0_1",       OP_BINARY, BWL, cc, #op,		\
      { { AMODE_REG, { 0, } }, { AMODE_REG,     { 1, } } } },	\
  { #op "@_reg_areg_0_1",      OP_BINARY, WL,  areg_cc, #op,	\
      { { AMODE_REG, { 0, } }, { AMODE_AREG,    { 1, } } } },	\
  { #op "@_reg_abs_0_1",       OP_BINARY, BWL, cc, #op,		\
      { { AMODE_REG, { 0, } }, { AMODE_ABS,     { 1, } } } },	\
  { #op "@_reg_ind_0_1",       OP_BINARY, BWL, cc, #op,		\
      { { AMODE_REG, { 0, } }, { AMODE_IND,     { 1, } } } },	\
  { #op "@_reg_predec_0_1",    OP_BINARY, BWL, cc, #op,		\
      { { AMODE_REG, { 0, } }, { AMODE_PREDEC,  { 1, } } } },	\
  { #op "@_reg_postinc_0_1",   OP_BINARY, BWL, cc, #op,		\
      { { AMODE_REG, { 0, } }, { AMODE_POSTINC, { 1, } } } },	\
  { #op "@_reg_indoff_0_2_1",  OP_BINARY, BWL, cc, #op,		\
      { { AMODE_REG, { 0, } }, { AMODE_INDOFF,  { 2, 1 } } } },	\
								\
  /* op areg,<ea> */						\
  { #op "@_areg_reg_0_1",       OP_BINARY, WL, cc, #op,		\
      { { AMODE_AREG, { 0, } }, { AMODE_REG,     { 1, } } } },	\
  { #op "@_areg_areg_0_1",      OP_BINARY, WL, areg_cc, #op,	\
      { { AMODE_AREG, { 0, } }, { AMODE_AREG,    { 1, } } } },	\
  { #op "@_areg_abs_0_1",       OP_BINARY, WL, cc, #op,		\
      { { AMODE_AREG, { 0, } }, { AMODE_ABS,     { 1, } } } },	\
  { #op "@_areg_ind_0_1",       OP_BINARY, WL, cc, #op,		\
      { { AMODE_AREG, { 0, } }, { AMODE_IND,     { 1, } } } },	\
  { #op "@_areg_predec_0_1",    OP_BINARY, WL, cc, #op,		\
      { { AMODE_AREG, { 0, } }, { AMODE_PREDEC,  { 1, } } } },	\
  { #op "@_areg_postinc_0_1",   OP_BINARY, WL, cc, #op,		\
      { { AMODE_AREG, { 0, } }, { AMODE_POSTINC, { 1, } } } },	\
  { #op "@_areg_indoff_0_2_1",  OP_BINARY, WL, cc, #op,		\
      { { AMODE_AREG, { 0, } }, { AMODE_INDOFF,  { 2, 1 } } } }

#define ARITHMETIC_IMM_FIRST(op, cc, areg_cc)				\
  /* op imm,<ea> */							\
  { #op "@_imm_reg_0_1",      OP_BINARY, BWL, cc, #op,			\
      { { AMODE_IMM, { 0,  } }, { AMODE_REG, { 1, } } } },		\
  { #op "@_imm_areg_0_1",     OP_BINARY, BWL, areg_cc, #op,		\
      { { AMODE_IMM, { 0,  } }, { AMODE_AREG, { 1, } } } },		\
  { #op "@_imm_ind_0_1",      OP_BINARY, BWL, cc, #op,			\
      { { AMODE_IMM, { 0, } }, { AMODE_IND,     { 1,   } } } },		\
  { #op "@_imm_predec_0_1",   OP_BINARY, BWL, cc, #op,			\
      { { AMODE_IMM, { 0, } }, { AMODE_PREDEC,  { 1,   } } } },		\
  { #op "@_imm_postinc_0_1",  OP_BINARY, BWL, cc, #op,			\
      { { AMODE_IMM, { 0, } }, { AMODE_POSTINC, { 1,   } } } },		\
  { #op "@_imm_indoff_0_2_1", OP_BINARY, BWL, cc, #op,			\
      { { AMODE_IMM, { 0, } }, { AMODE_INDOFF,  { 2, 1 } } } }


  ARITHMETIC           (add, M68K_CC_CNVXZ, M68K_CC_NONE),
  ARITHMETIC_DST_EA    (add, M68K_CC_CNVXZ, M68K_CC_NONE),
  ARITHMETIC_IMM_FIRST (add, M68K_CC_CNVXZ, M68K_CC_NONE),
  ARITHMETIC           (sub, M68K_CC_CNVXZ, M68K_CC_NONE),
  ARITHMETIC_DST_EA    (sub, M68K_CC_CNVXZ, M68K_CC_NONE),
  ARITHMETIC_IMM_FIRST (sub, M68K_CC_CNVXZ, M68K_CC_NONE),
  ARITHMETIC           (cmp, M68K_CC_CNVZ,  M68K_CC_CNVZ),
  
  /* You must not call these with a shift count of 0!  Not normally
   * a problem since the m68k shift-by-constants allow constants
   * in the range of 1-8 only.
   */
  { "lsl@_imm_reg_0_1", OP_BINARY, BWL,
      M68K_CCC | M68K_CCN | M68K_CCX | M68K_CCZ, "shl",
      { { AMODE_IMM, { 0 } }, { AMODE_REG, { 1 } } } },
  { "lsr@_imm_reg_0_1", OP_BINARY, BWL,
      M68K_CCC | M68K_CCN | M68K_CCX | M68K_CCZ, "shr",
      { { AMODE_IMM, { 0 } }, { AMODE_REG, { 1 } } } },
  { "asr@_imm_reg_0_1", OP_BINARY, BWL,
      M68K_CCC | M68K_CCN | M68K_CCX | M68K_CCZ, "sar",
      { { AMODE_IMM, { 0 } }, { AMODE_REG, { 1 } } } },


#define UNARY(op, cc, i386_name)			\
  { #op "@_reg_0",      OP_UNARY, BWL, cc, i386_name,	\
      { { AMODE_REG,     { 0, } } } },			\
  { #op "@_abs_0",      OP_UNARY, BWL, cc, i386_name,	\
      { { AMODE_ABS,     { 1,   } } } },		\
  { #op "@_ind_0",      OP_UNARY, BWL, cc, i386_name,	\
      { { AMODE_IND,     { 0,   } } } },		\
  { #op "@_predec_0",   OP_UNARY, BWL, cc, i386_name,	\
      { { AMODE_PREDEC,  { 0,   } } } },		\
  { #op "@_postinc_0",  OP_UNARY, BWL, cc, i386_name,	\
      { { AMODE_POSTINC, { 0,   } } } },		\
  { #op "@_indoff_1_0", OP_UNARY, BWL, cc, i386_name,	\
      { { AMODE_INDOFF,  { 1, 0 } } } }

  UNARY (tst, M68K_CC_CNVZ,  "test"),
  UNARY (neg, M68K_CC_CNVXZ, "neg"),
  UNARY (not, M68K_CC_CNVZ,  "not"),

  { "tst@_areg_0",      OP_UNARY, WL, M68K_CC_CNVZ, "test",
      { { AMODE_AREG,     { 0, } } } },

  { NULL }
};
