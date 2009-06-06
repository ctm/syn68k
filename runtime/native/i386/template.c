#include "config.h"

#include "template.h"

/* $Id: template.c 61 2004-12-21 23:44:50Z ctm $ */

#if defined(HAVE_SYMBOL_UNDERSCORE)
#  define SYMBOL_PREFIX "_"
#else
#  define SYMBOL_PREFIX ""
#endif


const template_t template[] =
{
#define BINARY_OP(op, dstio, memout)					\
  { "i386_" op "b_reg_reg", "", "acopsz", "", "", "uv",			\
      op "b %0,%1",							\
      { "src", "dst" },							\
      { { SIZE_8,  REGISTER, IN }, { SIZE_8, REGISTER, dstio } } },	\
  { "i386_" op "w_reg_reg", "", "acopsz", "", "", "u",			\
      op "w %0,%1",							\
      { "src", "dst" },							\
      { { SIZE_16, REGISTER, IN }, { SIZE_16, REGISTER, dstio } } },	\
  { "i386_" op "l_reg_reg", "", "acopsz", "", "", "uv",			\
      op "l %0,%1",							\
      { "src", "dst" },							\
      { { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, dstio } } },	\
									\
  { "i386_" op "b_imm_reg", "", "acopsz", "", "", "uv",			\
      op "b $%0,%1",							\
      { "src", "dst" },							\
      { { SIZE_8,  CONSTANT, IN }, { SIZE_8, REGISTER, dstio } } },	\
  { "i386_" op "w_imm_reg", "", "acopsz", "", "", "u",			\
      op "w $%0,%1",							\
      { "src", "dst" },							\
      { { SIZE_16, CONSTANT, IN }, { SIZE_16, REGISTER, dstio } } },	\
  { "i386_" op "l_imm_reg", "", "acopsz", "", "", "uv",			\
      op "l $%0,%1",							\
      { "src", "dst" },							\
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, dstio } } },	\
									\
  { "i386_" op "b_imm_abs", "", "acopsz", "memory", memout, "uv",	\
      op "b $%0,%1",							\
      { "src", "dst" },							\
      { { SIZE_8,  CONSTANT, IN }, { SIZE_32, CONSTANT, IN } } },	\
  { "i386_" op "w_imm_abs", "", "acopsz", "memory", memout, "u",	\
      op "w $%0,%1",							\
      { "src", "dst" },							\
      { { SIZE_16, CONSTANT, IN }, { SIZE_32, CONSTANT, IN } } },	\
  { "i386_" op "l_imm_abs", "", "acopsz", "memory", memout, "uv",	\
      op "l $%0,%1",							\
      { "src", "dst" },							\
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, CONSTANT, IN } } },	\
									\
  { "i386_" op "b_reg_abs", "", "acopsz", "memory", memout, "uv",	\
      op "b %0,%1",							\
      { "src", "dst" },							\
      { { SIZE_8,  REGISTER, IN }, { SIZE_32, CONSTANT, IN } } },	\
  { "i386_" op "w_reg_abs", "", "acopsz", "memory", memout, "u",	\
      op "w %0,%1",							\
      { "src", "dst" },							\
      { { SIZE_16, REGISTER, IN }, { SIZE_32, CONSTANT, IN } } },	\
  { "i386_" op "l_reg_abs", "", "acopsz", "memory", memout, "uv",	\
      op "l %0,%1",							\
      { "src", "dst" },							\
      { { SIZE_32, REGISTER, IN }, { SIZE_32, CONSTANT, IN } } },	\
									\
  { "i386_" op "b_abs_reg", "", "acopsz", "memory", memout, "uv",	\
      op "b %0,%1",							\
      { "src", "dst" },							\
      { { SIZE_32, CONSTANT, IN }, { SIZE_8, REGISTER,  dstio } } },	\
  { "i386_" op "w_abs_reg", "", "acopsz", "memory", memout, "uv",	\
      op "w %0,%1",							\
      { "src", "dst" },							\
      { { SIZE_32, CONSTANT, IN }, { SIZE_16, REGISTER, dstio } } },	\
  { "i386_" op "l_abs_reg", "", "acopsz", "memory", memout, "uv",	\
      op "l %0,%1",							\
      { "src", "dst" },							\
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, dstio } } },	\
									\
  { "i386_" op "b_reg_ind", "", "acopsz", "memory", memout, "uv",	\
      op "b %0,(%1)",							\
      { "src", "dst" },							\
      { { SIZE_8,  REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
  { "i386_" op "w_reg_ind", "", "acopsz", "memory", memout, "u",	\
      op "w %0,(%1)",							\
      { "src", "dst" },							\
      { { SIZE_16, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
  { "i386_" op "l_reg_ind", "", "acopsz", "memory", memout, "uv",	\
      op "l %0,(%1)",							\
      { "src", "dst" },							\
      { { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
									\
  { "i386_" op "b_ind_reg", "", "acopsz", "memory", memout, "uv",	\
      op "b (%0),%1",							\
      { "src", "dst" },							\
      { { SIZE_32,  REGISTER, IN }, { SIZE_8, REGISTER, dstio } } },	\
  { "i386_" op "w_ind_reg", "", "acopsz", "memory", memout, "u",	\
      op "w (%0),%1",							\
      { "src", "dst" },							\
      { { SIZE_32, REGISTER, IN }, { SIZE_16, REGISTER, dstio } } },	\
  { "i386_" op "l_ind_reg", "", "acopsz", "memory", memout, "uv",	\
      op "l (%0),%1",							\
      { "src", "dst" },							\
      { { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, dstio } } },	\
									\
  { "i386_" op "b_reg_indoff", "", "acopsz", "memory", memout, "uv",	\
      op "b %0,%1(%2)",							\
      { "src", "offset", "dst" },					\
      { { SIZE_8,  REGISTER, IN }, { SIZE_32, CONSTANT, IN },		\
	{ SIZE_32, REGISTER, IN } } },					\
  { "i386_" op "w_reg_indoff", "", "acopsz", "memory", memout, "u",	\
      op "w %0,%1(%2)",							\
      { "src", "offset", "dst" },					\
      { { SIZE_16,  REGISTER, IN }, { SIZE_32, CONSTANT, IN },		\
	{ SIZE_32, REGISTER, IN } } },					\
  { "i386_" op "l_reg_indoff", "", "acopsz", "memory", memout, "uv",	\
      op "l %0,%1(%2)",							\
      { "src", "offset", "dst" },					\
      { { SIZE_32,  REGISTER, IN }, { SIZE_32, CONSTANT, IN },		\
	{ SIZE_32, REGISTER, IN } } },					\
									\
  { "i386_" op "b_indoff_reg", "", "acopsz", "memory", memout, "uv",	\
      op "b %0(%1),%2",							\
      { "offset", "src", "dst" },					\
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },		\
	{ SIZE_8, REGISTER, dstio } } },				\
  { "i386_" op "w_indoff_reg", "", "acopsz", "memory", memout, "u",	\
      op "w %0(%1),%2",							\
      { "offset", "src", "dst" },					\
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },		\
	{ SIZE_16, REGISTER, dstio } } },				\
  { "i386_" op "l_indoff_reg", "", "acopsz", "memory", memout, "uv",	\
      op "l %0(%1),%2",							\
      { "offset", "src", "dst" },					\
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },		\
	{ SIZE_32, REGISTER, dstio } } },				\
									\
  { "i386_" op "b_imm_ind", "", "acopsz", "memory", memout, "uv",	\
      op "b $%0,(%1)",							\
      { "src", "dst" },							\
      { { SIZE_8,  CONSTANT, IN }, { SIZE_32, REGISTER, IN } } },	\
  { "i386_" op "w_imm_ind", "", "acopsz", "memory", memout, "u",	\
      op "w $%0,(%1)",							\
      { "src", "dst" },							\
      { { SIZE_16, CONSTANT, IN }, { SIZE_32, REGISTER, IN } } },	\
  { "i386_" op "l_imm_ind", "", "acopsz", "memory", memout, "uv",	\
      op "l $%0,(%1)",							\
      { "src", "dst" },							\
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN } } },	\
									\
  { "i386_" op "b_imm_indoff", "", "acopsz", "memory", memout, "uv",	\
      op "b $%0,%1(%2)",						\
      { "src", "offset", "dst" },					\
      { { SIZE_8,  CONSTANT, IN }, { SIZE_32, CONSTANT, IN },		\
	{ SIZE_32, REGISTER, IN } } },					\
  { "i386_" op "w_imm_indoff", "", "acopsz", "memory", memout, "u",	\
      op "w $%0,%1(%2)",						\
      { "src", "offset", "dst" },					\
      { { SIZE_16, CONSTANT, IN }, { SIZE_32, CONSTANT, IN },		\
	{ SIZE_32, REGISTER, IN } } },					\
  { "i386_" op "l_imm_indoff", "", "acopsz", "memory", memout, "uv",	\
      op "l $%0,%1(%2)",						\
      { "src", "offset", "dst" },					\
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, CONSTANT, IN },		\
	{ SIZE_32, REGISTER, IN } } }

  BINARY_OP ("add",  INOUT, "memory"),
  BINARY_OP ("sub",  INOUT, "memory"),
  BINARY_OP ("and",  INOUT, "memory"),
  BINARY_OP ("or",   INOUT, "memory"),
  BINARY_OP ("xor",  INOUT, "memory"),
  BINARY_OP ("cmp",  IN,    ""),
  BINARY_OP ("test", IN,    ""),

  { "i386_adcb_reg_reg", "c", "acopsz", "", "", "u",
      "adcb %0,%1",
      { "src", "dst" },
      { { SIZE_8,  REGISTER, IN }, { SIZE_8, REGISTER, INOUT } } },
  { "i386_adcw_reg_reg", "c", "acopsz", "", "", "u",
      "adcw %0,%1",
      { "src", "dst" },
      { { SIZE_16, REGISTER, IN }, { SIZE_16, REGISTER, INOUT } } },
  { "i386_adcl_reg_reg", "c", "acopsz", "", "", "u",
      "adcl %0,%1",
      { "src", "dst" },
      { { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, INOUT } } },

#define BINARY_OP_INDIX(op, memout)				\
  { "i386_" op "b_imm_indix", "", "acopsz", "memory", memout, "uv",	\
      op "b $%0,%1(%2,%3)",						\
      { "src", "offset", "dst", "index" },				\
      { { SIZE_8,  CONSTANT, IN }, { SIZE_32, CONSTANT, IN },		\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
  { "i386_" op "w_imm_indix", "", "acopsz", "memory", memout, "uv",	\
      op "w $%0,%1(%2,%3)",						\
      { "src", "offset", "dst", "index" },				\
      { { SIZE_16, CONSTANT, IN }, { SIZE_32, CONSTANT, IN },		\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
  { "i386_" op "l_imm_indix", "", "acopsz", "memory", memout, "uv",	\
      op "l $%0,%1(%2,%3)",						\
      { "src", "offset", "dst", "index" },				\
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, CONSTANT, IN },		\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
									\
  { "i386_" op "b_imm_indix_no_offset", "", "acopsz", "memory", memout, "uv",\
      op "b $%0,(%1,%2)",						\
      { "src", "dst", "index" },					\
      { { SIZE_8,  CONSTANT, IN },					\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
  { "i386_" op "w_imm_indix_no_offset", "", "acopsz", "memory", memout, "uv",\
      op "w $%0,(%1,%2)",						\
      { "src", "dst", "index" },					\
      { { SIZE_16, CONSTANT, IN },					\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
  { "i386_" op "l_imm_indix_no_offset", "", "acopsz", "memory", memout, "uv",\
      op "l $%0,(%1,%2)",						\
      { "src", "dst", "index" },					\
      { { SIZE_32, CONSTANT, IN },					\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
									\
  { "i386_" op "b_imm_indix_scale2", "", "acopsz", "memory", memout, "uv",\
      op "b $%0,%1(%2,%3,2)",						\
      { "src", "offset", "dst", "index" },				\
      { { SIZE_8,  CONSTANT, IN }, { SIZE_32, CONSTANT, IN },		\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
  { "i386_" op "w_imm_indix_scale2", "", "acopsz", "memory", memout, "uv",\
      op "w $%0,%1(%2,%3,2)",						\
      { "src", "offset", "dst", "index" },				\
      { { SIZE_16, CONSTANT, IN }, { SIZE_32, CONSTANT, IN },		\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
  { "i386_" op "l_imm_indix_scale2", "", "acopsz", "memory", memout, "uv",\
      op "l $%0,%1(%2,%3,2)",						\
      { "src", "offset", "dst", "index" },				\
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, CONSTANT, IN },		\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
									\
  { "i386_" op "b_imm_indix_scale2_no_offset", "", "acopsz", "memory",  \
    memout, "uv",							\
      op "b $%0,(%1,%2,2)",						\
      { "src", "dst", "index" },					\
      { { SIZE_8,  CONSTANT, IN },					\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
  { "i386_" op "w_imm_indix_scale2_no_offset", "", "acopsz", "memory",  \
    memout, "uv",							\
      op "w $%0,(%1,%2,2)",						\
      { "src", "dst", "index" },					\
      { { SIZE_16, CONSTANT, IN },					\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
  { "i386_" op "l_imm_indix_scale2_no_offset", "", "acopsz", "memory",  \
    memout, "uv",							\
      op "l $%0,(%1,%2,2)",						\
      { "src", "dst", "index" },					\
      { { SIZE_32, CONSTANT, IN },					\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
									\
  { "i386_" op "b_imm_indix_scale4", "", "acopsz", "memory", memout, "uv",\
      op "b $%0,%1(%2,%3,4)",						\
      { "src", "offset", "dst", "index" },				\
      { { SIZE_8,  CONSTANT, IN }, { SIZE_32, CONSTANT, IN },		\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
  { "i386_" op "w_imm_indix_scale4", "", "acopsz", "memory", memout, "uv",\
      op "w $%0,%1(%2,%3,4)",						\
      { "src", "offset", "dst", "index" },				\
      { { SIZE_16, CONSTANT, IN }, { SIZE_32, CONSTANT, IN },		\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
  { "i386_" op "l_imm_indix_scale4", "", "acopsz", "memory", memout, "uv",\
      op "l $%0,%1(%2,%3,4)",						\
      { "src", "offset", "dst", "index" },				\
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, CONSTANT, IN },		\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
									\
  { "i386_" op "b_imm_indix_scale4_no_offset", "", "acopsz", "memory",  \
    memout, "uv",							\
      op "b $%0,(%1,%2,4)",						\
      { "src", "dst", "index" },					\
      { { SIZE_8,  CONSTANT, IN },					\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
  { "i386_" op "w_imm_indix_scale4_no_offset", "", "acopsz", "memory",  \
    memout, "uv",							\
      op "w $%0,(%1,%2,4)",						\
      { "src", "dst", "index" },					\
      { { SIZE_16, CONSTANT, IN },					\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
  { "i386_" op "l_imm_indix_scale4_no_offset", "", "acopsz", "memory",  \
    memout, "uv",							\
      op "l $%0,(%1,%2,4)",						\
      { "src", "dst", "index" },					\
      { { SIZE_32, CONSTANT, IN },					\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
									\
  { "i386_" op "b_imm_indix_scale8", "", "acopsz", "memory", memout, "uv",\
      op "b $%0,%1(%2,%3,8)",						\
      { "src", "offset", "dst", "index" },				\
      { { SIZE_8,  CONSTANT, IN }, { SIZE_32, CONSTANT, IN },		\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
  { "i386_" op "w_imm_indix_scale8", "", "acopsz", "memory", memout, "uv",\
      op "w $%0,%1(%2,%3,8)",						\
      { "src", "offset", "dst", "index" },				\
      { { SIZE_16, CONSTANT, IN }, { SIZE_32, CONSTANT, IN },		\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
  { "i386_" op "l_imm_indix_scale8", "", "acopsz", "memory", memout, "uv",\
      op "l $%0,%1(%2,%3,8)",						\
      { "src", "offset", "dst", "index" },				\
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, CONSTANT, IN },		\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
									\
  { "i386_" op "b_imm_indix_scale8_no_offset", "", "acopsz", "memory",  \
    memout, "uv",							\
      op "b $%0,(%1,%2,8)",						\
      { "src", "dst", "index" },					\
      { { SIZE_8,  CONSTANT, IN },					\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
  { "i386_" op "w_imm_indix_scale8_no_offset", "", "acopsz", "memory",  \
    memout, "uv",							\
      op "w $%0,(%1,%2,8)",						\
      { "src", "dst", "index" },					\
      { { SIZE_16, CONSTANT, IN },					\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
  { "i386_" op "l_imm_indix_scale8_no_offset", "", "acopsz", "memory",  \
    memout, "uv",							\
      op "l $%0,(%1,%2,8)",						\
      { "src", "dst", "index" },					\
      { { SIZE_32, CONSTANT, IN },					\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
									\
  { "i386_" op "b_indix_reg", "", "acopsz", "memory", memout, "uv",	\
      op "b %0(%1,%2),%3",						\
      { "offset", "base_addr_reg", "index", "dst_val" },		\
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },		\
	{ SIZE_32, REGISTER, IN }, { SIZE_8,  REGISTER, IN } } },	\
  { "i386_" op "w_indix_reg", "", "acopsz", "memory", memout, "uv",	\
      op "w %0(%1,%2),%3",						\
      { "offset", "base_addr_reg", "index", "dst_val" },		\
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },		\
	{ SIZE_32, REGISTER, IN }, { SIZE_16, REGISTER, IN } } },	\
  { "i386_" op "l_indix_reg", "", "acopsz", "memory", memout, "uv",	\
      op "l %0(%1,%2),%3",						\
      { "offset", "base_addr_reg", "index", "dst_val" },		\
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },		\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
									\
  { "i386_" op "b_indix_reg_no_offset", "", "acopsz", "memory", memout, "uv",\
      op "b (%0,%1),%2",						\
      { "base_addr_reg", "index", "dst_val" },				\
      { { SIZE_32, REGISTER, IN },					\
	{ SIZE_32, REGISTER, IN }, { SIZE_8, REGISTER, IN } } },	\
  { "i386_" op "w_indix_reg_no_offset", "", "acopsz", "memory", memout, "uv",\
      op "w (%0,%1),%2",						\
      { "base_addr_reg", "index", "dst_val" },				\
      { { SIZE_32, REGISTER, IN },					\
	{ SIZE_32, REGISTER, IN }, { SIZE_16, REGISTER, IN } } },	\
  { "i386_" op "l_indix_reg_no_offset", "", "acopsz", "memory", memout, "uv",\
      op "l (%0,%1),%2",						\
      { "base_addr_reg", "index", "dst_val" },				\
      { { SIZE_32, REGISTER, IN },					\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
									\
  { "i386_" op "b_indix_reg_scale2", "", "acopsz", "memory", memout, "uv",\
      op "b %0(%1,%2,2),%3",						\
      { "offset", "base_addr_reg", "index", "dst_val" },		\
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },		\
	{ SIZE_32, REGISTER, IN }, { SIZE_8,  REGISTER, IN } } },	\
  { "i386_" op "w_indix_reg_scale2", "", "acopsz", "memory", memout, "uv",\
      op "w %0(%1,%2,2),%3",						\
      { "offset", "base_addr_reg", "index", "dst_val" },		\
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },		\
	{ SIZE_32, REGISTER, IN }, { SIZE_16, REGISTER, IN } } },	\
  { "i386_" op "l_indix_reg_scale2", "", "acopsz", "memory", memout, "uv",\
      op "l %0(%1,%2,2),%3",						\
      { "offset", "base_addr_reg", "index", "dst_val" },		\
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },		\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
									\
  { "i386_" op "b_indix_reg_scale2_no_offset", "", "acopsz", "memory",  \
    memout, "uv",							\
      op "b (%0,%1,2),%2",						\
      { "base_addr_reg", "index", "dst_val" },				\
      { { SIZE_32,  REGISTER, IN },					\
	{ SIZE_32, REGISTER, IN }, { SIZE_8, REGISTER, IN } } },	\
  { "i386_" op "w_indix_reg_scale2_no_offset", "", "acopsz", "memory",  \
    memout, "uv",							\
      op "w (%0,%1,2),%2",						\
      { "base_addr_reg", "index", "dst_val" },				\
      { { SIZE_32, REGISTER, IN },					\
	{ SIZE_32, REGISTER, IN }, { SIZE_16, REGISTER, IN } } },	\
  { "i386_" op "l_indix_reg_scale2_no_offset", "", "acopsz", "memory",  \
    memout, "uv",							\
      op "l (%0,%1,2),%2",						\
      { "base_addr_reg", "index", "dst_val" },				\
      { { SIZE_32, REGISTER, IN },					\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
									\
  { "i386_" op "b_indix_reg_scale4", "", "acopsz", "memory", memout, "uv",\
      op "b %0(%1,%2,4),%3",						\
      { "offset", "base_addr_reg", "index", "dst_val" },		\
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },		\
	{ SIZE_32, REGISTER, IN }, { SIZE_8,  REGISTER, IN } } },	\
  { "i386_" op "w_indix_reg_scale4", "", "acopsz", "memory", memout, "uv",\
      op "w %0(%1,%2,4),%3",						\
      { "offset", "base_addr_reg", "index", "dst_val" },		\
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },		\
	{ SIZE_32, REGISTER, IN }, { SIZE_16, REGISTER, IN } } },	\
  { "i386_" op "l_indix_reg_scale4", "", "acopsz", "memory", memout, "uv",\
      op "l %0(%1,%2,4),%3",						\
      { "offset", "base_addr_reg", "index", "dst_val" },		\
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },		\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
									\
  { "i386_" op "b_indix_reg_scale4_no_offset", "", "acopsz", "memory",  \
    memout, "uv",							\
      op "b (%0,%1,4),%2",						\
      { "base_addr_reg", "index", "dst_val" },				\
      { { SIZE_32,  REGISTER, IN },					\
	{ SIZE_32, REGISTER, IN }, { SIZE_8, REGISTER, IN } } },	\
  { "i386_" op "w_indix_reg_scale4_no_offset", "", "acopsz", "memory",  \
    memout, "uv",							\
      op "w (%0,%1,4),%2",						\
      { "base_addr_reg", "index", "dst_val" },				\
      { { SIZE_32, REGISTER, IN },					\
	{ SIZE_32, REGISTER, IN }, { SIZE_16, REGISTER, IN } } },	\
  { "i386_" op "l_indix_reg_scale4_no_offset", "", "acopsz", "memory",  \
    memout, "uv",							\
      op "l (%0,%1,4),%2",						\
      { "base_addr_reg", "index", "dst_val" },				\
      { { SIZE_32, REGISTER, IN },					\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
									\
  { "i386_" op "b_indix_reg_scale8", "", "acopsz", "memory", memout, "uv",\
      op "b %0(%1,%2,8),%3",						\
      { "offset", "base_addr_reg", "index", "dst_val" },		\
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },		\
	{ SIZE_32, REGISTER, IN }, { SIZE_8,  REGISTER, IN } } },	\
  { "i386_" op "w_indix_reg_scale8", "", "acopsz", "memory", memout, "uv",\
      op "w %0(%1,%2,8),%3",						\
      { "offset", "base_addr_reg", "index", "dst_val" },		\
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },		\
	{ SIZE_32, REGISTER, IN }, { SIZE_16, REGISTER, IN } } },	\
  { "i386_" op "l_indix_reg_scale8", "", "acopsz", "memory", memout, "uv",\
      op "l %0(%1,%2,8),%3",						\
      { "offset", "base_addr_reg", "index", "dst_val" },		\
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },		\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
									\
  { "i386_" op "b_indix_reg_scale8_no_offset", "", "acopsz", "memory",  \
    memout, "uv",							\
      op "b (%0,%1,8),%2",						\
      { "base_addr_reg", "index", "dst_val" },				\
      { { SIZE_32,  REGISTER, IN },					\
	{ SIZE_32, REGISTER, IN }, { SIZE_8, REGISTER, IN } } },	\
  { "i386_" op "w_indix_reg_scale8_no_offset", "", "acopsz", "memory",  \
    memout, "uv",							\
      op "w (%0,%1,8),%2",						\
      { "base_addr_reg", "index", "dst_val" },				\
      { { SIZE_32, REGISTER, IN },					\
	{ SIZE_32, REGISTER, IN }, { SIZE_16, REGISTER, IN } } },	\
  { "i386_" op "l_indix_reg_scale8_no_offset", "", "acopsz", "memory",  \
    memout, "uv",							\
      op "l (%0,%1,8),%2",						\
      { "base_addr_reg", "index", "dst_val" },				\
      { { SIZE_32, REGISTER, IN },					\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } }


  BINARY_OP_INDIX ("cmp", ""),

#define BYTE_ARITH_OP_INDIX(op, memout)					\
  { "i386_" op "b_imm_indix", "", "acopsz", "memory", memout, "uv",	\
      op "b $%0,%1(%2,%3)",						\
      { "src", "offset", "dst", "index" },				\
      { { SIZE_8,  CONSTANT, IN }, { SIZE_32, CONSTANT, IN },		\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
									\
  { "i386_" op "b_imm_indix_no_offset", "", "acopsz", "memory", memout, "uv",\
      op "b $%0,(%1,%2)",						\
      { "src", "dst", "index" },					\
      { { SIZE_8,  CONSTANT, IN },					\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
									\
  { "i386_" op "b_imm_indix_scale2", "", "acopsz", "memory", memout, "uv",\
      op "b $%0,%1(%2,%3,2)",						\
      { "src", "offset", "dst", "index" },				\
      { { SIZE_8,  CONSTANT, IN }, { SIZE_32, CONSTANT, IN },		\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
									\
  { "i386_" op "b_imm_indix_scale2_no_offset", "", "acopsz", "memory",  \
    memout, "uv",							\
      op "b $%0,(%1,%2,2)",						\
      { "src", "dst", "index" },					\
      { { SIZE_8,  CONSTANT, IN },					\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
									\
  { "i386_" op "b_imm_indix_scale4", "", "acopsz", "memory", memout, "uv",\
      op "b $%0,%1(%2,%3,4)",						\
      { "src", "offset", "dst", "index" },				\
      { { SIZE_8,  CONSTANT, IN }, { SIZE_32, CONSTANT, IN },		\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
									\
  { "i386_" op "b_imm_indix_scale4_no_offset", "", "acopsz", "memory",  \
    memout, "uv",							\
      op "b $%0,(%1,%2,4)",						\
      { "src", "dst", "index" },					\
      { { SIZE_8,  CONSTANT, IN },					\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
									\
  { "i386_" op "b_imm_indix_scale8", "", "acopsz", "memory", memout, "uv",\
      op "b $%0,%1(%2,%3,8)",						\
      { "src", "offset", "dst", "index" },				\
      { { SIZE_8,  CONSTANT, IN }, { SIZE_32, CONSTANT, IN },		\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
									\
  { "i386_" op "b_imm_indix_scale8_no_offset", "", "acopsz", "memory",  \
    memout, "uv",							\
      op "b $%0,(%1,%2,8)",						\
      { "src", "dst", "index" },					\
      { { SIZE_8,  CONSTANT, IN },					\
	{ SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },	\
									\
  { "i386_" op "b_indix_reg", "", "acopsz", "memory", memout, "uv",	\
      op "b %0(%1,%2),%3",						\
      { "offset", "base_addr_reg", "index", "dst_val" },		\
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },		\
	{ SIZE_32, REGISTER, IN }, { SIZE_8,  REGISTER, IN } } },	\
									\
  { "i386_" op "b_indix_reg_no_offset", "", "acopsz", "memory", memout, "uv",\
      op "b (%0,%1),%2",						\
      { "base_addr_reg", "index", "dst_val" },				\
      { { SIZE_32, REGISTER, IN },					\
	{ SIZE_32, REGISTER, IN }, { SIZE_8, REGISTER, IN } } },	\
									\
  { "i386_" op "b_indix_reg_scale2", "", "acopsz", "memory", memout, "uv",\
      op "b %0(%1,%2,2),%3",						\
      { "offset", "base_addr_reg", "index", "dst_val" },		\
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },		\
	{ SIZE_32, REGISTER, IN }, { SIZE_8,  REGISTER, IN } } },	\
									\
  { "i386_" op "b_indix_reg_scale2_no_offset", "", "acopsz", "memory",  \
    memout, "uv",							\
      op "b (%0,%1,2),%2",						\
      { "base_addr_reg", "index", "dst_val" },				\
      { { SIZE_32,  REGISTER, IN },					\
	{ SIZE_32, REGISTER, IN }, { SIZE_8, REGISTER, IN } } },	\
									\
  { "i386_" op "b_indix_reg_scale4", "", "acopsz", "memory", memout, "uv",\
      op "b %0(%1,%2,4),%3",						\
      { "offset", "base_addr_reg", "index", "dst_val" },		\
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },		\
	{ SIZE_32, REGISTER, IN }, { SIZE_8,  REGISTER, IN } } },	\
									\
  { "i386_" op "b_indix_reg_scale4_no_offset", "", "acopsz", "memory",  \
    memout, "uv",							\
      op "b (%0,%1,4),%2",						\
      { "base_addr_reg", "index", "dst_val" },				\
      { { SIZE_32,  REGISTER, IN },					\
	{ SIZE_32, REGISTER, IN }, { SIZE_8, REGISTER, IN } } },	\
									\
  { "i386_" op "b_indix_reg_scale8", "", "acopsz", "memory", memout, "uv",\
      op "b %0(%1,%2,8),%3",						\
      { "offset", "base_addr_reg", "index", "dst_val" },		\
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },		\
	{ SIZE_32, REGISTER, IN }, { SIZE_8,  REGISTER, IN } } },	\
									\
  { "i386_" op "b_indix_reg_scale8_no_offset", "", "acopsz", "memory",  \
    memout, "uv",							\
      op "b (%0,%1,8),%2",						\
      { "base_addr_reg", "index", "dst_val" },				\
      { { SIZE_32, REGISTER, IN },					\
	{ SIZE_32, REGISTER, IN }, { SIZE_8, REGISTER, IN } } }

  BYTE_ARITH_OP_INDIX ("add", "memory"),
  BYTE_ARITH_OP_INDIX ("sub", "memory"),


#define UNARY_OP(op, flags, pipe)					\
  { "i386_" op "b_reg", "", flags, "", "", pipe,			\
      op "b %0",							\
      { "dst" },							\
      { { SIZE_8, REGISTER, INOUT } } },				\
  { "i386_" op "w_reg", "", flags, "", "", pipe,			\
      op "w %0",							\
      { "dst" },							\
      { { SIZE_16, REGISTER, INOUT } } },				\
  { "i386_" op "l_reg", "", flags, "", "", pipe,			\
      op "l %0",							\
      { "dst" },							\
      { { SIZE_32, REGISTER, INOUT } } },				\
  { "i386_" op "b_abs", "", flags, "", "" " memory", pipe,		\
      op "b %0",							\
      { "dst" },							\
      { { SIZE_32, CONSTANT, IN } } },					\
  { "i386_" op "w_abs", "", flags, "memory", "memory", pipe,		\
      op "w %0",							\
      { "dst" },							\
      { { SIZE_32, CONSTANT, IN } } },					\
  { "i386_" op "l_abs", "", flags, "memory", "memory", pipe,		\
      op "l %0",							\
      { "dst" },							\
      { { SIZE_32, CONSTANT, IN } } },					\
  { "i386_" op "b_ind", "", flags, "", "" " memory", pipe,		\
      op "b (%0)",							\
      { "dst_addr_reg" },						\
      { { SIZE_32, REGISTER, IN } } },					\
  { "i386_" op "w_ind", "", flags, "memory", "memory", pipe,		\
      op "w (%0)",							\
      { "dst_addr_reg" },						\
      { { SIZE_32, REGISTER, IN } } },					\
  { "i386_" op "l_ind", "", flags, "memory", "memory", pipe,		\
      op "l (%0)",							\
      { "dst_addr_reg" },						\
      { { SIZE_32, REGISTER, IN } } },					\
  { "i386_" op "b_indoff", "", flags, "", "" " memory", pipe,		\
      op "b %0(%1)",							\
      { "offset", "dst_addr_reg" },					\
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN } } },	\
  { "i386_" op "w_indoff", "", flags, "memory", "memory", pipe,		\
      op "w %0(%1)",							\
      { "offset", "dst_addr_reg" },					\
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN } } },	\
  { "i386_" op "l_indoff", "", flags, "memory", "memory", pipe,		\
      op "l %0(%1)",							\
      { "offset", "dst_addr_reg" },					\
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN } } }

  UNARY_OP ("neg", "acopsz", "-"),
  UNARY_OP ("not", "", "-"),
  UNARY_OP ("inc", "aopsz", "uv"),
  UNARY_OP ("dec", "aopsz", "uv"),

  { "i386_bswap", "", "", "", "", "-",
      "bswap %0",
      { "dst_reg" },
      { { SIZE_32, REGISTER, INOUT } } },

  { "i386_call_abs", "", "", "", "volatile", "v",
      "call " SYMBOL_PREFIX  "%P0",
      { "addr" },
      { { BROKEN_SIZE_32, CONSTANT, IN } } },

  { "i386_cbtw", "", "", "(reg8 AL)", "(reg16 AX)", "-",
      "cbtw",
      { },
      { } },

  { "i386_cwtl", "", "", "(reg16 AX)", "(reg32 EAX)", "-",
      "cwtl",
      { },
      { } },

  { "i386_cltd", "", "", "(reg32 EAX)", "(reg32 EDX)", "-",
      "cltd",  /* EAX sext -> EDX:EAX */
      { },
      { } },

  { "i386_clc", "", "c", "", "", "-",
      "clc",
      { },
      { } },

  { "i386_stc", "", "c", "", "", "-",
      "stc",
      { },
      { } },

  { "i386_cld", "", "d", "", "", "-",
      "cld",
      { },
      { } },

  { "i386_std", "", "d", "", "", "-",
      "std",
      { },
      { } },

  { "i386_cli", "", "i", "", "volatile", "-",
      "cli",
      { },
      { } },

  { "i386_sti", "", "i", "", "volatile", "-",
      "sti",
      { },
      { } },

#define DIV(op)  							      \
  { "i386_" op "b", "", "acopsz", "(reg16 AX)", "(reg16 AX) ", "-",	      \
      op "b %0",							      \
      { "divisor" },							      \
      { { SIZE_8, REGISTER, IN } } },					      \
  { "i386_" op "w", "", "acopsz", "(reg16 AX DX)", "(reg16 AX DX)", "-",      \
      op "w %0",							      \
      { "divisor" },							      \
      { { SIZE_16, REGISTER, IN } } },					      \
  { "i386_" op "l", "", "acopsz", "(reg32 EAX EDX)", "(reg32 EAX EDX)", "-",  \
      op "l %0",							      \
      { "divisor" },							      \
      { { SIZE_32, REGISTER, IN } } },					      \
									      \
  { "i386_" op "b_abs", "", "acopsz", "memory (reg16 AX)", "(reg16 AX) ", "-",\
      op "b %0",							      \
      { "divisor" },							      \
      { { SIZE_32, CONSTANT, IN } } },					      \
  { "i386_" op "w_abs", "", "acopsz", "memory (reg16 AX DX)",		      \
      "(reg16 AX DX)", "-", 						      \
      op "w %0",							      \
      { "divisor" },							      \
      { { SIZE_32, CONSTANT, IN } } },					      \
  { "i386_" op "l_abs", "", "acopsz", "memory (reg32 EAX EDX)",		      \
      "(reg32 EAX EDX)", "-", 						      \
      op "l %0",							      \
      { "divisor" },							      \
      { { SIZE_32, CONSTANT, IN } } }

  DIV ("div"),
  DIV ("idiv"),

#define CONDL_BRANCH(op, flags)			\
  { "i386_" op, flags, "", "", "volatile", "v",	\
      op " " SYMBOL_PREFIX "%P0",				\
      { "target" },				\
      { { BROKEN_SIZE_32, CONSTANT, IN } } }
  
  CONDL_BRANCH ("jc",   "c"),
  CONDL_BRANCH ("jbe",  "cz"),
  CONDL_BRANCH ("jz",   "z"),
  CONDL_BRANCH ("jl",   "os"),
  CONDL_BRANCH ("jle",  "osz"),
  CONDL_BRANCH ("jnc",  "c"),
  CONDL_BRANCH ("jnbe", "cz"),
  CONDL_BRANCH ("jnz",  "z"),
  CONDL_BRANCH ("jge",  "os"),
  CONDL_BRANCH ("jnle", "osz"),
  CONDL_BRANCH ("jno",  "o"),
  CONDL_BRANCH ("jo",   "o"),
  CONDL_BRANCH ("jns",  "s"),
  CONDL_BRANCH ("js",   "s"),

  { "i386_jmp", "", "", "", "volatile", "v",
      "jmp " SYMBOL_PREFIX "%P0",
      { "target" },
      { { BROKEN_SIZE_32, CONSTANT, IN } } },
  { "i386_jmp_reg", "", "", "", "volatile", "-",
      "jmp *%0",
      { "target" },
      { { SIZE_32, REGISTER, IN } } },

  { "i386_lahf", "acpsz", "", "", "(reg8 4)", "-",
      "lahf",
      { },
      { } },

  { "i386_leaw_indoff", "", "", "", "", "-",
      "leal %0(%1),%2",
      { "offset", "base", "dst" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, OUT } } },

  { "i386_leal_indoff", "", "", "", "", "-",
      "leal %0(%1),%2",
      { "offset", "base", "dst" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, OUT } } },

#if 0
  { "i386_leaw_indix_reg", "", "", "", "", "-",
      "leaw %0(%1,%2),%3",
      { "offset", "base", "index", "dst" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_16, REGISTER, OUT } } },
  { "i386_leaw_indix_reg_scale2", "", "", "", "", "-",
      "leaw %0(%1,%2,2),%3",
      { "offset", "base", "index", "dst" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_16, REGISTER, OUT } } },
  { "i386_leaw_indix_reg_scale4", "", "", "", "", "-",
      "leaw %0(%1,%2,4),%3",
      { "offset", "base", "index", "dst" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_16, REGISTER, OUT } } },
  { "i386_leaw_indix_reg_scale8", "", "", "", "", "-",
      "leaw %0(%1,%2,8),%3",
      { "offset", "base", "index", "dst" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_16, REGISTER, OUT } } },
#endif

  { "i386_leal_indix_reg", "", "", "", "", "-",
      "leal %0(%1,%2),%3",
      { "offset", "base", "index", "dst" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, OUT } } },
  { "i386_leal_indix_reg_scale2", "", "", "", "", "-",
      "leal %0(%1,%2,2),%3",
      { "offset", "base", "index", "dst" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, OUT } } },
  { "i386_leal_indix_reg_scale4", "", "", "", "", "-",
      "leal %0(%1,%2,4),%3",
      { "offset", "base", "index", "dst" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, OUT } } },
  { "i386_leal_indix_reg_scale8", "", "", "", "", "-",
      "leal %0(%1,%2,8),%3",
      { "offset", "base", "index", "dst" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, OUT } } },

#if 0
  { "i386_leaw_indix_reg_no_offset", "", "", "", "", "-",
      "leaw (%0,%1),%2",
      { "base", "index", "dst" },
      { { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_16, REGISTER, OUT } } },
  { "i386_leaw_indix_reg_scale2_no_offset", "", "", "", "", "-",
      "leaw (%0,%1,2),%2",
      { "base", "index", "dst" },
      { { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_16, REGISTER, OUT } } },
  { "i386_leaw_indix_reg_scale4_no_offset", "", "", "", "", "-",
      "leaw (%0,%1,4),%2",
      { "base", "index", "dst" },
      { { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_16, REGISTER, OUT } } },
  { "i386_leaw_indix_reg_scale8_no_offset", "", "", "", "", "-",
      "leaw (%0,%1,8),%2",
      { "base", "index", "dst" },
      { { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_16, REGISTER, OUT } } },
#endif

  { "i386_leal_indix_reg_no_offset", "", "", "", "", "-",
      "leal (%0,%1),%2",
      { "base", "index", "dst" },
      { { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, OUT } } },
  { "i386_leal_indix_reg_scale2_no_offset", "", "", "", "", "-",
      "leal (%0,%1,2),%2",
      { "base", "index", "dst" },
      { { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, OUT } } },
  { "i386_leal_indix_reg_scale4_no_offset", "", "", "", "", "-",
      "leal (%0,%1,4),%2",
      { "base", "index", "dst" },
      { { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, OUT } } },
  { "i386_leal_indix_reg_scale8_no_offset", "", "", "", "", "-",
      "leal (%0,%1,8),%2",
      { "base", "index", "dst" },
      { { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, OUT } } },

#define MOVE_EXT(op)							\
  { "i386_" op "wl_abs_reg", "", "", "memory", "", "-",			\
      op "wl %0,%1",							\
      { "src_addr", "dst_reg" },					\
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, OUT } } },	\
									\
  { "i386_" op "wl_indoff_reg", "", "", "memory", "", "-",		\
      op "wl %0(%1),%2",						\
      { "offset", "src_addr", "dst_reg" },				\
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },		\
	  { SIZE_32, REGISTER, OUT } } },				\
									\
  { "i386_" op "bl_reg_reg", "", "", "", "", "-",			\
      op "bl %0,%1",							\
      { "src_reg", "dst_reg" },						\
      { { SIZE_8, REGISTER, IN }, { SIZE_32, REGISTER, OUT } } },	\
									\
  { "i386_" op "wl_reg_reg", "", "", "", "", "-",			\
      op "wl %0,%1",							\
      { "src_reg", "dst_reg" },						\
      { { SIZE_16, REGISTER, IN }, { SIZE_32, REGISTER, OUT } } }

  MOVE_EXT ("movs"),
  MOVE_EXT ("movz"),

  { "i386_movb_reg_reg", "", "", "", "", "uv",
      "movb %0,%1",
      { "src", "dst" },
      { { SIZE_8, REGISTER, IN }, { SIZE_8, REGISTER, OUT } } },
  { "i386_movw_reg_reg", "", "", "", "", "uv",
      "movw %0,%1",
      { "src", "dst" },
      { { SIZE_16, REGISTER, IN }, { SIZE_16, REGISTER, OUT } } },
  { "i386_movl_reg_reg", "", "", "", "", "uv",
      "movl %0,%1",
      { "src", "dst" },
      { { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, OUT } } },

  { "i386_movb_reg_abs", "", "", "", "memory", "uv",
      "movb %0,%1",
      { "src", "dst_addr" },
      { { SIZE_8, REGISTER, IN }, { SIZE_32, CONSTANT, IN } } },
  { "i386_movw_reg_abs", "", "", "", "memory", "uv",
      "movw %0,%1",
      { "src", "dst_addr" },
      { { SIZE_16, REGISTER, IN }, { SIZE_32, CONSTANT, IN } } },
  { "i386_movl_reg_abs", "", "", "", "memory", "uv",
      "movl %0,%1",
      { "src", "dst_addr" },
      { { SIZE_32, REGISTER, IN }, { SIZE_32, CONSTANT, IN } } },

  { "i386_movb_abs_reg", "", "", "memory", "", "uv",
      "movb %0,%1",
      { "src_addr", "dst" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_8, REGISTER, OUT } } },
  { "i386_movw_abs_reg", "", "", "memory", "", "uv",
      "movw %0,%1",
      { "src_addr", "dst" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_16, REGISTER, OUT } } },
  { "i386_movl_abs_reg", "", "", "memory", "", "uv",
      "movl %0,%1",
      { "src_addr", "dst" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, OUT } } },

  { "i386_movb_imm_reg", "", "", "", "", "uv",
      "movb $%0,%1",
      { "src", "dst" },
      { { SIZE_8, CONSTANT, IN }, { SIZE_8, REGISTER, OUT } } },
  { "i386_movw_imm_reg", "", "", "", "", "uv",
      "movw $%0,%1",
      { "src", "dst" },
      { { SIZE_16, CONSTANT, IN }, { SIZE_16, REGISTER, OUT } } },
  { "i386_movl_imm_reg", "", "", "", "", "uv",
      "movl $%0,%1",
      { "src", "dst" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, OUT } } },

  { "i386_movb_imm_abs", "", "", "", "memory", "uv",
      "movb $%0,%1",
      { "src", "dst_addr" },
      { { SIZE_8, CONSTANT, IN }, { SIZE_32, CONSTANT, IN } } },
  { "i386_movw_imm_abs", "", "", "", "memory", "uv",
      "movw $%0,%1",
      { "src", "dst_addr" },
      { { SIZE_16, CONSTANT, IN }, { SIZE_32, CONSTANT, IN } } },
  { "i386_movl_imm_abs", "", "", "", "memory", "uv",
      "movl $%0,%1",
      { "src", "dst_addr" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, CONSTANT, IN } } },

  { "i386_movb_imm_indoff", "", "", "", "memory", "uv",
      "movb $%0,%1(%2)",
      { "src", "offset", "dst_addr" },
      { { SIZE_8, CONSTANT, IN }, { SIZE_32, CONSTANT, IN },
	  { SIZE_32, REGISTER, IN } } },
  { "i386_movw_imm_indoff", "", "", "", "memory", "uv",
      "movw $%0,%1(%2)",
      { "src", "offset", "dst_addr" },
      { { SIZE_16, CONSTANT, IN }, { SIZE_32, CONSTANT, IN },
	  { SIZE_32, REGISTER, IN } } },
  { "i386_movl_imm_indoff", "", "", "", "memory", "uv",
      "movl $%0,%1(%2)",
      { "src", "offset", "dst_addr" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, CONSTANT, IN },
	  { SIZE_32, REGISTER, IN } } },

  { "i386_movb_imm_ind", "", "", "", "memory", "uv",
      "movb $%0,(%1)",
      { "src", "dst_addr" },
      { { SIZE_8, CONSTANT, IN }, { SIZE_32, REGISTER, IN } } },
  { "i386_movw_imm_ind", "", "", "", "memory", "uv",
      "movw $%0,(%1)",
      { "src", "dst_addr" },
      { { SIZE_16, CONSTANT, IN }, { SIZE_32, REGISTER, IN } } },
  { "i386_movl_imm_ind", "", "", "", "memory", "uv",
      "movl $%0,(%1)",
      { "src", "dst_addr" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN } } },

  { "i386_movb_reg_indoff", "", "", "", "memory", "uv",
      "movb %0,%1(%2)",
      { "src", "offset", "dst_addr" },
      { { SIZE_8, REGISTER, IN }, { SIZE_32, CONSTANT, IN },
	  { SIZE_32, REGISTER, IN } } },
  { "i386_movw_reg_indoff", "", "", "", "memory", "uv",
      "movw %0,%1(%2)",
      { "src", "offset", "dst_addr" },
      { { SIZE_16, REGISTER, IN }, { SIZE_32, CONSTANT, IN },
	  { SIZE_32, REGISTER, IN } } },
  { "i386_movl_reg_indoff", "", "", "", "memory", "uv",
      "movl %0,%1(%2)",
      { "src", "offset", "dst_addr" },
      { { SIZE_32, REGISTER, IN }, { SIZE_32, CONSTANT, IN },
	  { SIZE_32, REGISTER, IN } } },

  { "i386_movb_reg_ind", "", "", "", "memory", "uv",
      "movb %0,(%1)",
      { "src", "dst_addr" },
      { { SIZE_8, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },
  { "i386_movw_reg_ind", "", "", "", "memory", "uv",
      "movw %0,(%1)",
      { "src", "dst_addr" },
      { { SIZE_16, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },
  { "i386_movl_reg_ind", "", "", "", "memory", "uv",
      "movl %0,(%1)",
      { "src", "dst_addr" },
      { { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },

  { "i386_movb_indoff_reg", "", "", "memory", "", "uv",
      "movb %0(%1),%2",
      { "offset", "src", "dst" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_8, REGISTER, OUT } } },
  { "i386_movw_indoff_reg", "", "", "memory", "", "uv",
      "movw %0(%1),%2",
      { "offset", "src", "dst" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_16, REGISTER, OUT } } },
  { "i386_movl_indoff_reg", "", "", "memory", "", "uv",
      "movl %0(%1),%2",
      { "offset", "src", "dst" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, OUT } } },
  
  { "i386_movb_ind_reg", "", "", "memory", "", "uv",
      "movb (%0),%1",
      { "src_addr", "dst" },
      { { SIZE_32, REGISTER, IN }, { SIZE_8, REGISTER, OUT } } },
  { "i386_movw_ind_reg", "", "", "memory", "", "uv",
      "movw (%0),%1",
      { "src_addr", "dst" },
      { { SIZE_32, REGISTER, IN }, { SIZE_16, REGISTER, OUT } } },
  { "i386_movl_ind_reg", "", "", "memory", "", "uv",
      "movl (%0),%1",
      { "src_addr", "dst" },
      { { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, OUT } } },

  { "i386_movb_imm_indix", "", "", "", "memory", "uv",
      "movb $%0,%1(%2,%3)",
      { "const_val", "offset", "base", "index" },
      { { SIZE_8, CONSTANT, IN }, { SIZE_32, CONSTANT, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },
  { "i386_movw_imm_indix", "", "", "", "memory", "uv",
      "movw $%0,%1(%2,%3)",
      { "const_val", "offset", "base", "index" },
      { { SIZE_16, CONSTANT, IN }, { SIZE_32, CONSTANT, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },
  { "i386_movl_imm_indix", "", "", "", "memory", "uv",
      "movl $%0,%1(%2,%3)",
      { "const_val", "offset", "base", "index" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, CONSTANT, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },

  { "i386_movb_imm_indix_scale2", "", "", "", "memory", "uv",
      "movb $%0,%1(%2,%3,2)",
      { "const_val", "offset", "base", "index" },
      { { SIZE_8, CONSTANT, IN }, { SIZE_32, CONSTANT, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },
  { "i386_movw_imm_indix_scale2", "", "", "", "memory", "uv",
      "movw $%0,%1(%2,%3,2)",
      { "const_val", "offset", "base", "index" },
      { { SIZE_16, CONSTANT, IN }, { SIZE_32, CONSTANT, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },
  { "i386_movl_imm_indix_scale2", "", "", "", "memory", "uv",
      "movl $%0,%1(%2,%3,2)",
      { "const_val", "offset", "base", "index" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, CONSTANT, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },

  { "i386_movb_imm_indix_scale4", "", "", "", "memory", "uv",
      "movb $%0,%1(%2,%3,4)",
      { "const_val", "offset", "base", "index" },
      { { SIZE_8, CONSTANT, IN }, { SIZE_32, CONSTANT, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },
  { "i386_movw_imm_indix_scale4", "", "", "", "memory", "uv",
      "movw $%0,%1(%2,%3,4)",
      { "const_val", "offset", "base", "index" },
      { { SIZE_16, CONSTANT, IN }, { SIZE_32, CONSTANT, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },
  { "i386_movl_imm_indix_scale4", "", "", "", "memory", "uv",
      "movl $%0,%1(%2,%3,4)",
      { "const_val", "offset", "base", "index" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, CONSTANT, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },

  { "i386_movb_imm_indix_scale8", "", "", "", "memory", "uv",
      "movb $%0,%1(%2,%3,8)",
      { "const_val", "offset", "base", "index" },
      { { SIZE_8, CONSTANT, IN }, { SIZE_32, CONSTANT, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },
  { "i386_movw_imm_indix_scale8", "", "", "", "memory", "uv",
      "movw $%0,%1(%2,%3,8)",
      { "const_val", "offset", "base", "index" },
      { { SIZE_16, CONSTANT, IN }, { SIZE_32, CONSTANT, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },
  { "i386_movl_imm_indix_scale8", "", "", "", "memory", "uv",
      "movl $%0,%1(%2,%3,8)",
      { "const_val", "offset", "base", "index" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, CONSTANT, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },

  { "i386_movb_imm_indix_no_offset", "", "", "", "memory", "uv",
      "movb $%0,(%1,%2)",
      { "const_val", "base", "index" },
      { { SIZE_8, CONSTANT, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN } } },
  { "i386_movw_imm_indix_no_offset", "", "", "", "memory", "uv",
      "movw $%0,(%1,%2)",
      { "const_val", "base", "index" },
      { { SIZE_16, CONSTANT, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN } } },
  { "i386_movl_imm_indix_no_offset", "", "", "", "memory", "uv",
      "movl $%0,(%1,%2)",
      { "const_val", "base", "index" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN } } },

  { "i386_movb_imm_indix_scale2_no_offset", "", "", "", "memory", "uv",
      "movb $%0,(%1,%2,2)",
      { "const_val", "base", "index" },
      { { SIZE_8, CONSTANT, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN } } },
  { "i386_movw_imm_indix_scale2_no_offset", "", "", "", "memory", "uv",
      "movw $%0,(%1,%2,2)",
      { "const_val", "base", "index" },
      { { SIZE_16, CONSTANT, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN } } },
  { "i386_movl_imm_indix_scale2_no_offset", "", "", "", "memory", "uv",
      "movl $%0,(%1,%2,2)",
      { "const_val", "base", "index" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN } } },

  { "i386_movb_imm_indix_scale4_no_offset", "", "", "", "memory", "uv",
      "movb $%0,(%1,%2,4)",
      { "const_val", "base", "index" },
      { { SIZE_8, CONSTANT, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN } } },
  { "i386_movw_imm_indix_scale4_no_offset", "", "", "", "memory", "uv",
      "movw $%0,(%1,%2,4)",
      { "const_val", "base", "index" },
      { { SIZE_16, CONSTANT, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN } } },
  { "i386_movl_imm_indix_scale4_no_offset", "", "", "", "memory", "uv",
      "movl $%0,(%1,%2,4)",
      { "const_val", "base", "index" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN } } },

  { "i386_movb_imm_indix_scale8_no_offset", "", "", "", "memory", "uv",
      "movb $%0,(%1,%2,8)",
      { "const_val", "base", "index" },
      { { SIZE_8, CONSTANT, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN } } },
  { "i386_movw_imm_indix_scale8_no_offset", "", "", "", "memory", "uv",
      "movw $%0,(%1,%2,8)",
      { "const_val", "base", "index" },
      { { SIZE_16, CONSTANT, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN } } },
  { "i386_movl_imm_indix_scale8_no_offset", "", "", "", "memory", "uv",
      "movl $%0,(%1,%2,8)",
      { "const_val", "base", "index" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN } } },

  { "i386_movb_reg_indix", "", "", "", "memory", "uv",
      "movb %0,%1(%2,%3)",
      { "src_reg", "offset", "base", "index" },
      { { SIZE_8, REGISTER, IN }, { SIZE_32, CONSTANT, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },
  { "i386_movw_reg_indix", "", "", "", "memory", "uv",
      "movw %0,%1(%2,%3)",
      { "src_reg", "offset", "base", "index" },
      { { SIZE_16, REGISTER, IN }, { SIZE_32, CONSTANT, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },
  { "i386_movl_reg_indix", "", "", "", "memory", "uv",
      "movl %0,%1(%2,%3)",
      { "src_reg", "offset", "base", "index" },
      { { SIZE_32, REGISTER, IN }, { SIZE_32, CONSTANT, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },

  { "i386_movb_reg_indix_scale2", "", "", "", "memory", "uv",
      "movb %0,%1(%2,%3,2)",
      { "src_reg", "offset", "base", "index" },
      { { SIZE_8, REGISTER, IN }, { SIZE_32, CONSTANT, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },
  { "i386_movw_reg_indix_scale2", "", "", "", "memory", "uv",
      "movw %0,%1(%2,%3,2)",
      { "src_reg", "offset", "base", "index" },
      { { SIZE_16, REGISTER, IN }, { SIZE_32, CONSTANT, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },
  { "i386_movl_reg_indix_scale2", "", "", "", "memory", "uv",
      "movl %0,%1(%2,%3,2)",
      { "src_reg", "offset", "base", "index" },
      { { SIZE_32, REGISTER, IN }, { SIZE_32, CONSTANT, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },

  { "i386_movb_reg_indix_scale4", "", "", "", "memory", "uv",
      "movb %0,%1(%2,%3,4)",
      { "src_reg", "offset", "base", "index" },
      { { SIZE_8, REGISTER, IN }, { SIZE_32, CONSTANT, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },
  { "i386_movw_reg_indix_scale4", "", "", "", "memory", "uv",
      "movw %0,%1(%2,%3,4)",
      { "src_reg", "offset", "base", "index" },
      { { SIZE_16, REGISTER, IN }, { SIZE_32, CONSTANT, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },
  { "i386_movl_reg_indix_scale4", "", "", "", "memory", "uv",
      "movl %0,%1(%2,%3,4)",
      { "src_reg", "offset", "base", "index" },
      { { SIZE_32, REGISTER, IN }, { SIZE_32, CONSTANT, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },

  { "i386_movb_reg_indix_scale8", "", "", "", "memory", "uv",
      "movb %0,%1(%2,%3,8)",
      { "src_reg", "offset", "base", "index" },
      { { SIZE_8, REGISTER, IN }, { SIZE_32, CONSTANT, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },
  { "i386_movw_reg_indix_scale8", "", "", "", "memory", "uv",
      "movw %0,%1(%2,%3,8)",
      { "src_reg", "offset", "base", "index" },
      { { SIZE_16, REGISTER, IN }, { SIZE_32, CONSTANT, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },
  { "i386_movl_reg_indix_scale8", "", "", "", "memory", "uv",
      "movl %0,%1(%2,%3,8)",
      { "src_reg", "offset", "base", "index" },
      { { SIZE_32, REGISTER, IN }, { SIZE_32, CONSTANT, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN } } },

  { "i386_movb_reg_indix_no_offset", "", "", "", "memory", "uv",
      "movb %0,(%1,%2)",
      { "src_reg", "base", "index" },
      { { SIZE_8, REGISTER, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN } } },
  { "i386_movw_reg_indix_no_offset", "", "", "", "memory", "uv",
      "movw %0,(%1,%2)",
      { "src_reg", "base", "index" },
      { { SIZE_16, REGISTER, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN } } },
  { "i386_movl_reg_indix_no_offset", "", "", "", "memory", "uv",
      "movl %0,(%1,%2)",
      { "src_reg", "base", "index" },
      { { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN } } },

  { "i386_movb_reg_indix_scale2_no_offset", "", "", "", "memory", "uv",
      "movb %0,(%1,%2,2)",
      { "src_reg", "base", "index" },
      { { SIZE_8, REGISTER, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN } } },
  { "i386_movw_reg_indix_scale2_no_offset", "", "", "", "memory", "uv",
      "movw %0,(%1,%2,2)",
      { "src_reg", "base", "index" },
      { { SIZE_16, REGISTER, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN } } },
  { "i386_movl_reg_indix_scale2_no_offset", "", "", "", "memory", "uv",
      "movl %0,(%1,%2,2)",
      { "src_reg", "base", "index" },
      { { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN } } },

  { "i386_movb_reg_indix_scale4_no_offset", "", "", "", "memory", "uv",
      "movb %0,(%1,%2,4)",
      { "src_reg", "base", "index" },
      { { SIZE_8, REGISTER, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN } } },
  { "i386_movw_reg_indix_scale4_no_offset", "", "", "", "memory", "uv",
      "movw %0,(%1,%2,4)",
      { "src_reg", "base", "index" },
      { { SIZE_16, REGISTER, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN } } },
  { "i386_movl_reg_indix_scale4_no_offset", "", "", "", "memory", "uv",
      "movl %0,(%1,%2,4)",
      { "src_reg", "base", "index" },
      { { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN } } },

  { "i386_movb_reg_indix_scale8_no_offset", "", "", "", "memory", "uv",
      "movb %0,(%1,%2,8)",
      { "src_reg", "base", "index" },
      { { SIZE_8, REGISTER, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN } } },
  { "i386_movw_reg_indix_scale8_no_offset", "", "", "", "memory", "uv",
      "movw %0,(%1,%2,8)",
      { "src_reg", "base", "index" },
      { { SIZE_16, REGISTER, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN } } },
  { "i386_movl_reg_indix_scale8_no_offset", "", "", "", "memory", "uv",
      "movl %0,(%1,%2,8)",
      { "src_reg", "base", "index" },
      { { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN } } },

  { "i386_movb_indix_reg", "", "", "memory", "", "uv",
      "movb %0(%1,%2),%3",
      { "offset", "base", "index", "dst" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_8, REGISTER, OUT } } },
  { "i386_movw_indix_reg", "", "", "memory", "", "uv",
      "movw %0(%1,%2),%3",
      { "offset", "base", "index", "dst" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_16, REGISTER, OUT } } },
  { "i386_movl_indix_reg", "", "", "memory", "", "uv",
      "movl %0(%1,%2),%3",
      { "offset", "base", "index", "dst" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, OUT } } },

  { "i386_movb_indix_reg_scale2", "", "", "memory", "", "uv",
      "movb %0(%1,%2,2),%3",
      { "offset", "base", "index", "dst" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_8, REGISTER, OUT } } },
  { "i386_movw_indix_reg_scale2", "", "", "memory", "", "uv",
      "movw %0(%1,%2,2),%3",
      { "offset", "base", "index", "dst" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_16, REGISTER, OUT } } },
  { "i386_movl_indix_reg_scale2", "", "", "memory", "", "uv",
      "movl %0(%1,%2,2),%3",
      { "offset", "base", "index", "dst" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, OUT } } },

  { "i386_movb_indix_reg_scale4", "", "", "memory", "", "uv",
      "movb %0(%1,%2,4),%3",
      { "offset", "base", "index", "dst" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_8, REGISTER, OUT } } },
  { "i386_movw_indix_reg_scale4", "", "", "memory", "", "uv",
      "movw %0(%1,%2,4),%3",
      { "offset", "base", "index", "dst" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_16, REGISTER, OUT } } },
  { "i386_movl_indix_reg_scale4", "", "", "memory", "", "uv",
      "movl %0(%1,%2,4),%3",
      { "offset", "base", "index", "dst" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, OUT } } },

  { "i386_movb_indix_reg_scale8", "", "", "memory", "", "uv",
      "movb %0(%1,%2,8),%3",
      { "offset", "base", "index", "dst" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_8, REGISTER, OUT } } },
  { "i386_movw_indix_reg_scale8", "", "", "memory", "", "uv",
      "movw %0(%1,%2,8),%3",
      { "offset", "base", "index", "dst" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_16, REGISTER, OUT } } },
  { "i386_movl_indix_reg_scale8", "", "", "memory", "", "uv",
      "movl %0(%1,%2,8),%3",
      { "offset", "base", "index", "dst" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, OUT } } },

  { "i386_movb_indix_reg_no_offset", "", "", "memory", "", "uv",
      "movb (%0,%1),%2",
      { "base", "index", "dst" },
      { { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_8, REGISTER, OUT } } },
  { "i386_movw_indix_reg_no_offset", "", "", "memory", "", "uv",
      "movw (%0,%1),%2",
      { "base", "index", "dst" },
      { { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_16, REGISTER, OUT } } },
  { "i386_movl_indix_reg_no_offset", "", "", "memory", "", "uv",
      "movl (%0,%1),%2",
      { "base", "index", "dst" },
      { { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, OUT } } },
  
  { "i386_movb_indix_reg_scale2_no_offset", "", "", "memory", "", "uv",
      "movb (%0,%1,2),%2",
      { "base", "index", "dst" },
      { { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_8, REGISTER, OUT } } },
  { "i386_movw_indix_reg_scale2_no_offset", "", "", "memory", "", "uv",
      "movw (%0,%1,2),%2",
      { "base", "index", "dst" },
      { { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_16, REGISTER, OUT } } },
  { "i386_movl_indix_reg_scale2_no_offset", "", "", "memory", "", "uv",
      "movl (%0,%1,2),%2",
      { "base", "index", "dst" },
      { { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, OUT } } },
  
  { "i386_movb_indix_reg_scale4_no_offset", "", "", "memory", "", "uv",
      "movb (%0,%1,4),%2",
      { "base", "index", "dst" },
      { { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_8, REGISTER, OUT } } },
  { "i386_movw_indix_reg_scale4_no_offset", "", "", "memory", "", "uv",
      "movw (%0,%1,4),%2",
      { "base", "index", "dst" },
      { { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_16, REGISTER, OUT } } },
  { "i386_movl_indix_reg_scale4_no_offset", "", "", "memory", "", "uv",
      "movl (%0,%1,4),%2",
      { "base", "index", "dst" },
      { { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, OUT } } },
  
  { "i386_movb_indix_reg_scale8_no_offset", "", "", "memory", "", "uv",
      "movb (%0,%1,8),%2",
      { "base", "index", "dst" },
      { { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_8, REGISTER, OUT } } },
  { "i386_movw_indix_reg_scale8_no_offset", "", "", "memory", "", "uv",
      "movw (%0,%1,8),%2",
      { "base", "index", "dst" },
      { { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_16, REGISTER, OUT } } },
  { "i386_movl_indix_reg_scale8_no_offset", "", "", "memory", "", "uv",
      "movl (%0,%1,8),%2",
      { "base", "index", "dst" },
      { { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, IN },
	  { SIZE_32, REGISTER, OUT } } },


  { "i386_imull_imm_reg", "", "acopsz", "", "", "-",
      "imull $%0,%1",
      { "src", "dst" },
      { { SIZE_32, CONSTANT, IN }, { SIZE_32, REGISTER, INOUT } } },
  { "i386_imull_reg_reg", "", "acopsz", "", "", "-",
      "imull %0,%1",
      { "src", "dst" },
      { { SIZE_32, REGISTER, IN }, { SIZE_32, REGISTER, INOUT } } },

  { "i386_popw", "", "", "(reg32 ESP) memory", "(reg32 ESP)", "u",
      "popw %0",
      { "dst_reg" },
      { { SIZE_16, REGISTER, OUT } } },
  { "i386_popl", "", "", "(reg32 ESP) memory", "(reg32 ESP)", "uv",
      "popl %0",
      { "dst_reg" },
      { { SIZE_32, REGISTER, OUT } } },

  { "i386_pushw", "", "", "(reg32 ESP) memory", "(reg32 ESP)", "u",
      "pushw %0",
      { "src_reg" },
      { { SIZE_16, REGISTER, IN } } },
  { "i386_pushl", "", "", "(reg32 ESP)", "(reg32 ESP) memory", "uv",
      "pushl %0",
      { "src_reg" },
      { { SIZE_32, REGISTER, IN } } },

  { "i386_pushw_imm", "", "", "(reg32 ESP)", "(reg32 ESP) memory", "u",
      "pushw $%0",
      { "const_val" },
      { { SIZE_16, CONSTANT, IN } } },
  { "i386_pushl_imm", "", "", "(reg32 ESP)", "(reg32 ESP) memory", "uv",
      "pushl $%0",
      { "const_val" },
      { { SIZE_32, CONSTANT, IN } } },

  { "i386_pushfl", "acdiopsz", "", "(reg32 ESP)", "(reg32 ESP) memory", "-",
      "pushfl",
      { },
      { } },

  /* We'll omit the push mem versions, because my docs say that doing a
   * load into a register followed by a push is faster.
   */

#define SHIFT(op, flags_in, flags_out)					  \
  { "i386_" op "b_reg", flags_in, flags_out, "(reg8 CL)", "", "-",	  \
      op "b %cl,%0",							  \
      { "dst_reg" },							  \
      { { SIZE_8, REGISTER, INOUT } } },				  \
  { "i386_" op "w_reg", flags_in, flags_out, "(reg8 CL)", "", "-",	  \
      op "w %cl,%0",							  \
      { "dst_reg" },							  \
      { { SIZE_16, REGISTER, INOUT } } },				  \
  { "i386_" op "l_reg", flags_in, flags_out, "(reg8 CL)", "", "-",	  \
      op "l %cl,%0",							  \
      { "dst_reg" },							  \
      { { SIZE_32, REGISTER, INOUT } } },				  \
  									  \
  { "i386_" op "b_abs", flags_in, flags_out, "(reg8 CL) memory",	  \
      "memory", "-",							  \
      op "b %cl,%0",							  \
      { "dst_addr" },							  \
      { { SIZE_32, CONSTANT, IN } } },					  \
  { "i386_" op "w_abs", flags_in, flags_out, "(reg8 CL) memory",	  \
      "memory", "-",							  \
      op "w %cl,%0",							  \
      { "dst_addr" },							  \
      { { SIZE_32, CONSTANT, IN } } },					  \
  { "i386_" op "l_abs", flags_in, flags_out, "(reg8 CL) memory",	  \
      "memory", "-",							  \
      op "l %cl,%0",							  \
      { "dst_addr" },							  \
      { { SIZE_32, CONSTANT, IN } } },					  \
  									  \
  { "i386_" op "b_imm_reg", flags_in, flags_out, "", "", "u",		  \
      op "b $%0,%1",							  \
      { "shift_const", "dst_reg" },					  \
      { { SIZE_8, CONSTANT, IN }, { SIZE_8, REGISTER, INOUT } } },	  \
  { "i386_" op "w_imm_reg", flags_in, flags_out, "", "", "u",		  \
      op "w $%0,%1",							  \
      { "shift_const", "dst_reg" },					  \
      { { SIZE_8, CONSTANT, IN }, { SIZE_16, REGISTER, INOUT } } },	  \
  { "i386_" op "l_imm_reg", flags_in, flags_out, "", "", "u",		  \
      op "l $%0,%1",							  \
      { "shift_const", "dst_reg" },					  \
      { { SIZE_8, CONSTANT, IN }, { SIZE_32, REGISTER, INOUT } } },	  \
									  \
  { "i386_" op "b_imm_abs", flags_in, flags_out, "memory", "memory", "u", \
      op "b $%0,%1",							  \
      { "shift_const", "dst_addr" },					  \
      { { SIZE_8, CONSTANT, IN }, { SIZE_32, CONSTANT, IN } } },	  \
  { "i386_" op "w_imm_abs", flags_in, flags_out, "memory", "memory", "u", \
      op "w $%0,%1",							  \
      { "shift_const", "dst_addr" },					  \
      { { SIZE_8, CONSTANT, IN }, { SIZE_32, CONSTANT, IN } } },	  \
  { "i386_" op "l_imm_abs", flags_in, flags_out, "memory", "memory", "u", \
      op "l $%0,%1",							  \
      { "shift_const", "dst_addr" },					  \
      { { SIZE_8, CONSTANT, IN }, { SIZE_32, CONSTANT, IN } } }

#if 0
  SHIFT ("rcl", "c", "co"),
  SHIFT ("rcr", "c", "co"),
#endif
  SHIFT ("rol", "", "co"),
  SHIFT ("ror", "", "co"),
  SHIFT ("shl", "", "acopsz"),
  SHIFT ("shr", "", "acopsz"),
  SHIFT ("sar", "", "acopsz"),

  { "i386_sahf", "", "acpsz", "(reg8 4)", "", "-",
      "sahf",
      { },
      { } },

#define SET(op, flags)					\
  { "i386_" op "_reg", flags, "", "", "", "-",		\
      op " %0",						\
      { "dst_reg" },					\
      { { SIZE_8, REGISTER, OUT } } },			\
  { "i386_" op "_ind", flags, "", "", "", "-",		\
      op " (%0)",					\
      { "addr" },					\
      { { SIZE_32, REGISTER, IN } } },			\
  { "i386_" op "_indoff", flags, "", "", "", "-",	\
      op " %0(%1)",					\
      { "offset", "addr" },				\
      { { SIZE_32, CONSTANT, IN },			\
	  { SIZE_32, REGISTER, IN } } },		\
  { "i386_" op "_abs", flags, "", "", "memory", "-",	\
      op " %0",						\
      { "dst_addr" },					\
      { { SIZE_32, CONSTANT, IN } } }
  
  SET ("setc",   "c"),
  SET ("setbe",  "cz"),
  SET ("setz",   "z"),
  SET ("setl",   "os"),
  SET ("setle",  "osz"),
  SET ("setnb",  "c"),
  SET ("setnc",  "c"),
  SET ("setnbe", "cz"),
  SET ("setnz",  "z"),
  SET ("setge",  "os"),
  SET ("setnle", "osz"),
  SET ("setno",  "o"),
  SET ("seto",   "o"),
  SET ("setns",  "s"),
  SET ("sets",   "s"),

  { NULL }
};
