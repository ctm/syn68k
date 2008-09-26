#ifndef RUNTIME
#define RUNTIME  /* So stuff gets typedefed properly. */
#endif
#include "syn68k_private.h"

#ifdef GENERATE_NATIVE_CODE

#include "native.h"
#include "host-xlate.h"
#include "i386-isa.h"
#include "xlate-aux.h"
#include "i386-aux.h"


const guest_code_descriptor_t xlate_cmpmb_postinc_postinc_1_0 =
{
  {
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { TRUE, TRUE, 0, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ, REGSET_BYTE,
  {
    { host_cmpmb_postinc_postinc, {{ 1, 0, 0, 0 }} },
  },
  NULL
};


const guest_code_descriptor_t xlate_cmpmw_postinc_postinc_1_0 =
{
  {
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { TRUE, TRUE, 0, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ, REGSET_ALL,
  {
    { host_cmpmw_postinc_postinc, {{ 1, 0, 0, 0 }} },
  },
  NULL
};


const guest_code_descriptor_t xlate_cmpml_postinc_postinc_1_0 =
{
  {
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { TRUE, TRUE, 0, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ, REGSET_ALL,
  {
    { host_cmpml_postinc_postinc, {{ 1, 0, 0, 0 }} },
  },
  NULL
};


const guest_code_descriptor_t xlate_bclr_imm_reg_1_0 =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK | MAP_SWAP16_MASK | MAP_SWAP32_MASK,
	REQUEST_REG, ROS_UNTOUCHED_DIRTY, REGSET_BYTE },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_NONE, 0,
  {
    { host_bclr_imm_reg, {{ 1, 0, 0, 0 }} },
  },
  NULL
};


const guest_code_descriptor_t xlate_bset_imm_reg_1_0 =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK | MAP_SWAP16_MASK | MAP_SWAP32_MASK,
	REQUEST_REG, ROS_UNTOUCHED_DIRTY, REGSET_BYTE },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_NONE, 0,
  {
    { host_bset_imm_reg, {{ 1, 0, 0, 0 }} },
  },
  NULL
};


const guest_code_descriptor_t xlate_bchg_imm_reg_1_0 =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK | MAP_SWAP16_MASK | MAP_SWAP32_MASK,
	REQUEST_REG, ROS_UNTOUCHED_DIRTY, REGSET_BYTE },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_NONE, 0,
  {
    { host_bchg_imm_reg, {{ 1, 0, 0, 0 }} },
  },
  NULL
};


const guest_code_descriptor_t xlate_btst_imm_reg_1_0 =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK | MAP_SWAP16_MASK | MAP_SWAP32_MASK,
	REQUEST_REG, ROS_UNTOUCHED, REGSET_BYTE },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CCZ, 0,
  {
    { host_btst_imm_reg, {{ 1, 0, 0, 0 }} },
  },
  NULL
};


#define CONDL_BRANCH(name, cc)			\
const guest_code_descriptor_t xlate_ ## name =	\
{						\
  {						\
    { FALSE },					\
  },						\
  (cc), M68K_CC_NONE, 0,			\
  {						\
    { host_ ## name, {{ USE_BLOCK }} },		\
  },						\
  NULL						\
}


/* Some of these we can handle even without the CC bits being cached. */
CONDL_BRANCH (bcc, M68K_CC_NONE);
CONDL_BRANCH (bcs, M68K_CC_NONE);
CONDL_BRANCH (beq, M68K_CC_NONE);
CONDL_BRANCH (bge, M68K_CCN | M68K_CCV);
CONDL_BRANCH (bgt, M68K_CCN | M68K_CCV | M68K_CCZ);
CONDL_BRANCH (bhi, M68K_CC_NONE);
CONDL_BRANCH (ble, M68K_CCN | M68K_CCV | M68K_CCZ);
CONDL_BRANCH (bls, M68K_CC_NONE);
CONDL_BRANCH (blt, M68K_CCN | M68K_CCV);
CONDL_BRANCH (bmi, M68K_CC_NONE);
CONDL_BRANCH (bne, M68K_CC_NONE);
CONDL_BRANCH (bpl, M68K_CC_NONE);
CONDL_BRANCH (bvc, M68K_CC_NONE);
CONDL_BRANCH (bvs, M68K_CC_NONE);


#define SET_CC(name, i386_op, cc)				\
const guest_code_descriptor_t xlate_ ## name ## _reg_0 =	\
{								\
  {								\
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,		\
	ROS_NATIVE_DIRTY, REGSET_BYTE },			\
    { FALSE },							\
  },								\
  (cc), M68K_CC_NONE, 0,					\
  {								\
    { i386_op,       {{ 0 }} },					\
    { i386_negb_reg, {{ 0 }} },					\
  },								\
  NULL								\
}


SET_CC (scc, i386_setnc_reg,  M68K_CCC);
SET_CC (scs, i386_setc_reg,   M68K_CCC);
SET_CC (seq, i386_setz_reg,   M68K_CCZ);
SET_CC (sge, i386_setge_reg,  M68K_CCN | M68K_CCV);
SET_CC (sgt, i386_setnle_reg, M68K_CCN | M68K_CCV | M68K_CCZ);
SET_CC (shi, i386_setnbe_reg, M68K_CCC | M68K_CCZ);
SET_CC (sle, i386_setle_reg,  M68K_CCN | M68K_CCV | M68K_CCZ);
SET_CC (sls, i386_setbe_reg,  M68K_CCC | M68K_CCZ);
SET_CC (slt, i386_setl_reg,   M68K_CCN | M68K_CCV);
SET_CC (smi, i386_sets_reg,   M68K_CCN);
SET_CC (sne, i386_setnz_reg,  M68K_CCZ);
SET_CC (spl, i386_setns_reg,  M68K_CCN);
SET_CC (svc, i386_setno_reg,  M68K_CCV);
SET_CC (svs, i386_seto_reg,   M68K_CCV);


const guest_code_descriptor_t xlate_st_reg_0 =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_NATIVE_DIRTY, REGSET_BYTE },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_NONE, 0,
  {
    { i386_movb_imm_reg, {{ USE_MINUS_ONE, 0 }} },
  },
  NULL
};


const guest_code_descriptor_t xlate_sf_reg_0 =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_NATIVE_DIRTY, REGSET_BYTE },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_NONE, 0,
  {
    { i386_movb_imm_reg, {{ USE_ZERO, 0 }} },
  },
  NULL
};


const guest_code_descriptor_t xlate_jmp =
{
  {
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_NONE, 0,
  {
    { host_jmp, {{ USE_BLOCK }} },
  },
  NULL
};


const guest_code_descriptor_t xlate_dbra =
{
  {
    /* Even though this takes a register, we pretend like it doesn't.
     * If the register is in memory, just leave it there...since this
     * is the last instruction in the block, there's no sense in
     * hauling it into memory only to spill it back again.
     */
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_NONE, 0,
  {
    { host_dbra, {{ 0, USE_BLOCK }} },
  },
  NULL
};


static const guest_code_descriptor_t xlate_swap_N1 =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG, ROS_NATIVE_DIRTY,
	REGSET_BYTE },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ, 0,
  {
    { host_swap, {{ 0 }} },
    { i386_testl_reg_reg, {{ 0, 0 }} },
  },
  NULL
};


const guest_code_descriptor_t xlate_swap =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG, ROS_NATIVE_DIRTY,
	REGSET_BYTE },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_NONE, 0,
  {
    { host_swap, {{ 0 }} },
  },
  &xlate_swap_N1
};


const guest_code_descriptor_t xlate_extbl =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG, ROS_NATIVE_DIRTY,
	REGSET_BYTE },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ, 0,
  {
    { host_extbl, {{ 0 }} },
  },
  NULL
};

const guest_code_descriptor_t xlate_extbw =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG, ROS_NATIVE_DIRTY,
	REGSET_BYTE },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ, 0,
  {
    { host_extbw, {{ 0 }} },
  },
  NULL
};

const guest_code_descriptor_t xlate_extwl =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG, ROS_NATIVE_DIRTY,
	REGSET_BYTE },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ, 0,
  {
    { host_extwl, {{ 0 }} },
  },
  NULL
};


const guest_code_descriptor_t xlate_unlk =
{
  {
    { TRUE, TRUE, 0, MAP_ALL_MASK, REQUEST_SPARE_REG, ROS_NATIVE_DIRTY,
	REGSET_ALL },  /* a7 */
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_SWAP32_DIRTY, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_NONE, 0,
  {
    { host_unlk, {{ 1, 0 }} },
  },
  NULL
};

const guest_code_descriptor_t xlate_link =
{
  {
    { TRUE, TRUE, 0, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_OFFSET_DIRTY, REGSET_ALL },  /* a7 */
    { TRUE, TRUE, 1, MAP_SWAP32_MASK, REQUEST_REG, ROS_NATIVE_DIRTY,
	REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_NONE, 0,
  {
    { host_link, {{ 2, 1, 0 }} },
  },
  NULL
};

const guest_code_descriptor_t xlate_moveml_reg_predec_0_1 =
{
  {
    { TRUE, TRUE, 0, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_NONE, REGSET_ALL,
  {
    { host_moveml_reg_predec, {{ 0, 1 }} }
  },
  NULL
};

const guest_code_descriptor_t xlate_moveml_postinc_reg_0_1 =
{
  {
    { TRUE, TRUE, 0, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_NONE, REGSET_ALL,
  {
    { host_moveml_postinc_reg, {{ 0, 1 }} }
  },
  NULL
};

const guest_code_descriptor_t xlate_pea_indoff =
{
  {
    { TRUE, TRUE, 0, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },  /* Always a7 */
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_NONE, REGSET_ALL,
  {
    { host_pea_indoff, {{ 2, 1, 0 }} }
  },
  NULL
};


const guest_code_descriptor_t xlate_leal_indoff_areg_2_1_0 =
{
  {
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { TRUE, TRUE, 0, MAP_ALL_MASK, REQUEST_SPARE_REG,
	ROS_NATIVE_DIRTY, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_NONE, 0,
  {
    { host_leal_indoff_areg, {{ 2, 1, 0 }} }
  },
  NULL
};


const guest_code_descriptor_t xlate_rts =
{
  {
    { TRUE, TRUE, 0, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_NONE, REGSET_ALL,
  {
    { host_rts, {{ USE_BLOCK, 0 }} },
  },
  NULL
};


static const guest_code_descriptor_t xlate_mulsw_imm_reg_1_0_N1 =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_NATIVE_DIRTY, REGSET_BYTE },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ, 0,
  {
    { i386_movswl_reg_reg, {{ 0, 0 }} },
    { i386_imull_imm_reg,  {{ 1, 0 }} },
    { i386_testl_reg_reg,  {{ 0, 0 }} },
  },
  NULL
};


const guest_code_descriptor_t xlate_mulsw_imm_reg_1_0 =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_NATIVE_DIRTY, REGSET_BYTE },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CCV, 0,
  {
    { i386_movswl_reg_reg, {{ 0, 0 }} },
    { i386_imull_imm_reg, {{ 1, 0 }} },
  },
  &xlate_mulsw_imm_reg_1_0_N1
};


static const guest_code_descriptor_t xlate_mulsw_reg_reg_1_0_N1 =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_NATIVE_DIRTY, REGSET_BYTE },
    { TRUE, FALSE, 1, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_BYTE },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ, REGSET_ALL,
  {
    { i386_movswl_reg_reg, {{ 0, 0 }} },
    { i386_movswl_reg_reg, {{ 1, USE_SCRATCH_REG }} },
    { i386_imull_reg_reg,  {{ USE_SCRATCH_REG, 0 }} },
    { i386_testl_reg_reg,  {{ 0, 0 }} },
  },
  NULL
};


const guest_code_descriptor_t xlate_mulsw_reg_reg_1_0 =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_NATIVE_DIRTY, REGSET_BYTE },
    { TRUE, FALSE, 1, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_BYTE },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CCV, REGSET_ALL,
  {
    { i386_movswl_reg_reg, {{ 0, 0 }} },
    { i386_movswl_reg_reg, {{ 1, USE_SCRATCH_REG }} },
    { i386_imull_reg_reg,  {{ USE_SCRATCH_REG, 0 }} },
  },
  &xlate_mulsw_reg_reg_1_0_N1
};


static const guest_code_descriptor_t xlate_mulsw_abs_reg_1_0_N1 =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_NATIVE_DIRTY, REGSET_BYTE },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ, REGSET_ALL,
  {
    { host_movew_abs_reg_swap, {{ 1, USE_SCRATCH_REG }} },
    { i386_movswl_reg_reg,     {{ 0, 0 }} },
    { i386_movswl_reg_reg,     {{ USE_SCRATCH_REG, USE_SCRATCH_REG }} },
    { i386_imull_reg_reg,      {{ USE_SCRATCH_REG, 0 }} },
    { i386_testl_reg_reg,      {{ 0, 0 }} },
  },
  NULL
};


const guest_code_descriptor_t xlate_mulsw_abs_reg_1_0 =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_NATIVE_DIRTY, REGSET_BYTE },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CCV, REGSET_ALL,
  {
    { host_movew_abs_reg_swap, {{ 1, USE_SCRATCH_REG }} },
    { i386_movswl_reg_reg,     {{ 0, 0 }} },
    { i386_movswl_reg_reg,     {{ USE_SCRATCH_REG, USE_SCRATCH_REG }} },
    { i386_imull_reg_reg,      {{ USE_SCRATCH_REG, 0 }} },
  },
  &xlate_mulsw_abs_reg_1_0_N1
};


static const guest_code_descriptor_t xlate_mulsw_ind_reg_1_0_N1 =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_NATIVE_DIRTY, REGSET_BYTE },
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ, REGSET_ALL,
  {
    { host_movew_ind_reg_swap, {{ 1, USE_SCRATCH_REG }} },
    { i386_movswl_reg_reg,     {{ 0, 0 }} },
    { i386_movswl_reg_reg,     {{ USE_SCRATCH_REG, USE_SCRATCH_REG }} },
    { i386_imull_reg_reg,      {{ USE_SCRATCH_REG, 0 }} },
    { i386_testl_reg_reg,      {{ 0, 0 }} },
  },
  NULL
};


const guest_code_descriptor_t xlate_mulsw_ind_reg_1_0 =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_NATIVE_DIRTY, REGSET_BYTE },
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CCV, REGSET_ALL,
  {
    { host_movew_ind_reg_swap, {{ 1, USE_SCRATCH_REG }} },
    { i386_movswl_reg_reg,     {{ 0, 0 }} },
    { i386_movswl_reg_reg,     {{ USE_SCRATCH_REG, USE_SCRATCH_REG }} },
    { i386_imull_reg_reg,      {{ USE_SCRATCH_REG, 0 }} },
  },
  &xlate_mulsw_ind_reg_1_0_N1
};


static const guest_code_descriptor_t xlate_mulsw_indoff_reg_2_1_0_N1 =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_NATIVE_DIRTY, REGSET_BYTE },
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ, REGSET_ALL,
  {
    { host_movew_indoff_reg_swap, {{ 2, 1, USE_SCRATCH_REG }} },
    { i386_movswl_reg_reg,        {{ 0, 0 }} },
    { i386_movswl_reg_reg,        {{ USE_SCRATCH_REG, USE_SCRATCH_REG }} },
    { i386_imull_reg_reg,         {{ USE_SCRATCH_REG, 0 }} },
    { i386_testl_reg_reg,         {{ 0, 0 }} },
  },
  NULL
};


const guest_code_descriptor_t xlate_mulsw_indoff_reg_2_1_0 =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_NATIVE_DIRTY, REGSET_BYTE },
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CCV, REGSET_ALL,
  {
    { host_movew_indoff_reg_swap, {{ 2, 1, USE_SCRATCH_REG }} },
    { i386_movswl_reg_reg,        {{ 0, 0 }} },
    { i386_movswl_reg_reg,        {{ USE_SCRATCH_REG, USE_SCRATCH_REG }} },
    { i386_imull_reg_reg,         {{ USE_SCRATCH_REG, 0 }} },
  },
  &xlate_mulsw_indoff_reg_2_1_0_N1
};


static const guest_code_descriptor_t xlate_mulsw_predec_reg_1_0_N1 =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_NATIVE_DIRTY, REGSET_BYTE },
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ, REGSET_ALL,
  {
    { host_movew_predec_reg_swap, {{ 1, USE_SCRATCH_REG }} },
    { i386_movswl_reg_reg,        {{ 0, 0 }} },
    { i386_movswl_reg_reg,        {{ USE_SCRATCH_REG, USE_SCRATCH_REG }} },
    { i386_imull_reg_reg,         {{ USE_SCRATCH_REG, 0 }} },
    { i386_testl_reg_reg,         {{ 0, 0 }} },
  },
  NULL
};


const guest_code_descriptor_t xlate_mulsw_predec_reg_1_0 =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_NATIVE_DIRTY, REGSET_BYTE },
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CCV, REGSET_ALL,
  {
    { host_movew_predec_reg_swap, {{ 1, USE_SCRATCH_REG }} },
    { i386_movswl_reg_reg,        {{ 0, 0 }} },
    { i386_movswl_reg_reg,        {{ USE_SCRATCH_REG, USE_SCRATCH_REG }} },
    { i386_imull_reg_reg,         {{ USE_SCRATCH_REG, 0 }} },
  },
  &xlate_mulsw_predec_reg_1_0_N1
};


static const guest_code_descriptor_t xlate_mulsw_postinc_reg_1_0_N1 =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_NATIVE_DIRTY, REGSET_BYTE },
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ, REGSET_ALL,
  {
    { host_movew_postinc_reg_swap, {{ 1, USE_SCRATCH_REG }} },
    { i386_movswl_reg_reg,         {{ 0, 0 }} },
    { i386_movswl_reg_reg,         {{ USE_SCRATCH_REG, USE_SCRATCH_REG }} },
    { i386_imull_reg_reg,          {{ USE_SCRATCH_REG, 0 }} },
    { i386_testl_reg_reg,          {{ 0, 0 }} },
  },
  NULL
};


const guest_code_descriptor_t xlate_mulsw_postinc_reg_1_0 =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_NATIVE_DIRTY, REGSET_BYTE },
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CCV, REGSET_ALL,
  {
    { host_movew_postinc_reg_swap, {{ 1, USE_SCRATCH_REG }} },
    { i386_movswl_reg_reg,         {{ 0, 0 }} },
    { i386_movswl_reg_reg,         {{ USE_SCRATCH_REG, USE_SCRATCH_REG }} },
    { i386_imull_reg_reg,          {{ USE_SCRATCH_REG, 0 }} },
  },
  &xlate_mulsw_postinc_reg_1_0_N1
};

static const guest_code_descriptor_t xlate_muluw_imm_reg_1_0_N1 =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_NATIVE_DIRTY, REGSET_BYTE },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ, 0,
  {
    { i386_andl_imm_reg,   {{ USE_0xFFFF, 0 }} },
    { i386_imull_imm_reg,  {{ 1, 0 }} },
    { i386_testl_reg_reg,  {{ 0, 0 }} },
  },
  NULL
};


const guest_code_descriptor_t xlate_muluw_imm_reg_1_0 =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_NATIVE_DIRTY, REGSET_BYTE },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_NONE, 0,
  {
    { i386_andl_imm_reg,  {{ USE_0xFFFF, 0 }} },
    { i386_imull_imm_reg, {{ 1, 0 }} },
  },
  &xlate_muluw_imm_reg_1_0_N1
};


static const guest_code_descriptor_t xlate_muluw_reg_reg_1_0_N1 =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_NATIVE_DIRTY, REGSET_BYTE },
    { TRUE, FALSE, 1, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_BYTE },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ, REGSET_ALL,
  {
    { i386_andl_imm_reg,   {{ USE_0xFFFF, 0 }} },
    { i386_movzwl_reg_reg, {{ 1, USE_SCRATCH_REG }} },
    { i386_imull_reg_reg,  {{ USE_SCRATCH_REG, 0 }} },
    { i386_testl_reg_reg,  {{ 0, 0 }} },
  },
  NULL
};


const guest_code_descriptor_t xlate_muluw_reg_reg_1_0 =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_NATIVE_DIRTY, REGSET_BYTE },
    { TRUE, FALSE, 1, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_BYTE },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_NONE, REGSET_ALL,
  {
    { i386_andl_imm_reg,   {{ USE_0xFFFF, 0 }} },
    { i386_movzwl_reg_reg, {{ 1, USE_SCRATCH_REG }} },
    { i386_imull_reg_reg,  {{ USE_SCRATCH_REG, 0 }} },
  },
  &xlate_muluw_reg_reg_1_0_N1
};


static const guest_code_descriptor_t xlate_muluw_abs_reg_1_0_N1 =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_NATIVE_DIRTY, REGSET_BYTE },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ, REGSET_ALL,
  {
    { i386_xorl_reg_reg,       {{ USE_SCRATCH_REG, USE_SCRATCH_REG }} },
    { i386_andl_imm_reg,       {{ USE_0xFFFF, 0 }} },
    { host_movew_abs_reg_swap, {{ 1, USE_SCRATCH_REG }} },
    { i386_imull_reg_reg,      {{ USE_SCRATCH_REG, 0 }} },
    { i386_testl_reg_reg,      {{ 0, 0 }} },
  },
  NULL
};


const guest_code_descriptor_t xlate_muluw_abs_reg_1_0 =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_NATIVE_DIRTY, REGSET_BYTE },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_NONE, REGSET_ALL,
  {
    { i386_xorl_reg_reg,       {{ USE_SCRATCH_REG, USE_SCRATCH_REG }} },
    { i386_andl_imm_reg,       {{ USE_0xFFFF, 0 }} },
    { host_movew_abs_reg_swap, {{ 1, USE_SCRATCH_REG }} },
    { i386_imull_reg_reg,      {{ USE_SCRATCH_REG, 0 }} },
  },
  &xlate_muluw_abs_reg_1_0_N1
};


static const guest_code_descriptor_t xlate_muluw_ind_reg_1_0_N1 =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_NATIVE_DIRTY, REGSET_BYTE },
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ, REGSET_ALL,
  {
    { i386_xorl_reg_reg,       {{ USE_SCRATCH_REG, USE_SCRATCH_REG }} },
    { i386_andl_imm_reg,       {{ USE_0xFFFF, 0 }} },
    { host_movew_ind_reg_swap, {{ 1, USE_SCRATCH_REG }} },
    { i386_imull_reg_reg,      {{ USE_SCRATCH_REG, 0 }} },
    { i386_testl_reg_reg,      {{ 0, 0 }} },
  },
  NULL
};


const guest_code_descriptor_t xlate_muluw_ind_reg_1_0 =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_NATIVE_DIRTY, REGSET_BYTE },
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_NONE, REGSET_ALL,
  {
    { i386_xorl_reg_reg,       {{ USE_SCRATCH_REG, USE_SCRATCH_REG }} },
    { i386_andl_imm_reg,       {{ USE_0xFFFF, 0 }} },
    { host_movew_ind_reg_swap, {{ 1, USE_SCRATCH_REG }} },
    { i386_imull_reg_reg,      {{ USE_SCRATCH_REG, 0 }} },
  },
  &xlate_muluw_ind_reg_1_0_N1
};


static const guest_code_descriptor_t xlate_muluw_indoff_reg_2_1_0_N1 =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_NATIVE_DIRTY, REGSET_BYTE },
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ, REGSET_ALL,
  {
    { i386_xorl_reg_reg,          {{ USE_SCRATCH_REG, USE_SCRATCH_REG }} },
    { i386_andl_imm_reg,          {{ USE_0xFFFF, 0 }} },
    { host_movew_indoff_reg_swap, {{ 2, 1, USE_SCRATCH_REG }} },
    { i386_imull_reg_reg,         {{ USE_SCRATCH_REG, 0 }} },
    { i386_testl_reg_reg,         {{ 0, 0 }} },
  },
  NULL
};


const guest_code_descriptor_t xlate_muluw_indoff_reg_2_1_0 =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_NATIVE_DIRTY, REGSET_BYTE },
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_NONE, REGSET_ALL,
  {
    { i386_xorl_reg_reg,          {{ USE_SCRATCH_REG, USE_SCRATCH_REG }} },
    { i386_andl_imm_reg,          {{ USE_0xFFFF, 0 }} },
    { host_movew_indoff_reg_swap, {{ 2, 1, USE_SCRATCH_REG }} },
    { i386_imull_reg_reg,         {{ USE_SCRATCH_REG, 0 }} },
  },
  &xlate_muluw_indoff_reg_2_1_0_N1
};


static const guest_code_descriptor_t xlate_muluw_predec_reg_1_0_N1 =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_NATIVE_DIRTY, REGSET_BYTE },
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ, REGSET_ALL,
  {
    { i386_xorl_reg_reg,          {{ USE_SCRATCH_REG, USE_SCRATCH_REG }} },
    { i386_andl_imm_reg,          {{ USE_0xFFFF, 0 }} },
    { host_movew_predec_reg_swap, {{ 1, USE_SCRATCH_REG }} },
    { i386_imull_reg_reg,         {{ USE_SCRATCH_REG, 0 }} },
    { i386_testl_reg_reg,         {{ 0, 0 }} },
  },
  NULL
};


const guest_code_descriptor_t xlate_muluw_predec_reg_1_0 =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_NATIVE_DIRTY, REGSET_BYTE },
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_NONE, REGSET_ALL,
  {
    { i386_xorl_reg_reg,          {{ USE_SCRATCH_REG, USE_SCRATCH_REG }} },
    { i386_andl_imm_reg,          {{ USE_0xFFFF, 0 }} },
    { host_movew_predec_reg_swap, {{ 1, USE_SCRATCH_REG }} },
    { i386_imull_reg_reg,         {{ USE_SCRATCH_REG, 0 }} },
  },
  &xlate_muluw_predec_reg_1_0_N1
};


static const guest_code_descriptor_t xlate_muluw_postinc_reg_1_0_N1 =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_NATIVE_DIRTY, REGSET_BYTE },
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ, REGSET_ALL,
  {
    { i386_xorl_reg_reg,           {{ USE_SCRATCH_REG, USE_SCRATCH_REG }} },
    { i386_andl_imm_reg,           {{ USE_0xFFFF, 0 }} },
    { host_movew_postinc_reg_swap, {{ 1, USE_SCRATCH_REG }} },
    { i386_imull_reg_reg,          {{ USE_SCRATCH_REG, 0 }} },
    { i386_testl_reg_reg,          {{ 0, 0 }} },
  },
  NULL
};


const guest_code_descriptor_t xlate_muluw_postinc_reg_1_0 =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_NATIVE_DIRTY, REGSET_BYTE },
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_NONE, REGSET_ALL,
  {
    { i386_xorl_reg_reg,           {{ USE_SCRATCH_REG, USE_SCRATCH_REG }} },
    { i386_andl_imm_reg,           {{ USE_0xFFFF, 0 }} },
    { host_movew_postinc_reg_swap, {{ 1, USE_SCRATCH_REG }} },
    { i386_imull_reg_reg,          {{ USE_SCRATCH_REG, 0 }} },
  },
  &xlate_muluw_postinc_reg_1_0_N1
};


const guest_code_descriptor_t xlate_jsr_abs_0 =
{
  {
    { TRUE, TRUE, 0, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },  /* Always a7 */
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_NONE, 0,
  {
    { host_jsr_abs, {{ 0, 1, USE_BLOCK, USE_M68K_PC }} },
  },
  NULL
};


const guest_code_descriptor_t xlate_jsr_pcd16_0 =
{
  {
    { TRUE, TRUE, 0, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },  /* Always a7 */
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_NONE, 0,
  {
    { host_jsr_pcd16, {{ 0, USE_BLOCK, USE_M68K_PC }} },
  },
  NULL
};


const guest_code_descriptor_t xlate_jsr_d16_0_1 =
{
  {
    { TRUE, TRUE, 0, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },  /* Always a7 */
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_NONE, REGSET_ALL,
  {
    { host_jsr_d16, {{ 0, 1, USE_BLOCK, USE_M68K_PC }} },
  },
  NULL
};


const guest_code_descriptor_t xlate_bsr =
{
  {
    { TRUE, TRUE, 0, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },  /* Always a7 */
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_NONE, 0,
  {
    { host_bsr, {{ 0, USE_BLOCK, USE_M68K_PC }} },
  },
  NULL
};

#if 0
const guest_code_descriptor_t xlate_moveb_imm_indix_1_0 =
{
  {
    { TRUE, TRUE, 0, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ, REGSET_BYTE,
  {
    { host_moveb_imm_indix, { 1, 0, USE_M68K_PC_PLUS_TWO } },
  },
  NULL
};

const guest_code_descriptor_t xlate_movew_imm_indix_1_0 =
{
  {
    { TRUE, TRUE, 0, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ, REGSET_ALL,
  {
    { host_movew_imm_indix, { 1, 0, USE_M68K_PC_PLUS_TWO } },
  },
  NULL
};

const guest_code_descriptor_t xlate_movel_imm_indix_1_0 =
{
  {
    { TRUE, TRUE, 0, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
  },
  M68K_CC_NONE, M68K_CC_CNVZ, REGSET_ALL,
  {
    { host_movel_imm_indix, { 1, 0, USE_M68K_PC_PLUS_FOUR } },
  },
  NULL
};


const guest_code_descriptor_t xlate_moveb_reg_indix_1_0 =
{
  {
    { TRUE, TRUE, 0, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { TRUE, FALSE, 1, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_BYTE },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ, REGSET_BYTE,
  {
    { host_moveb_reg_indix, { 1, 0, USE_M68K_PC } },
  },
  NULL
};

const guest_code_descriptor_t xlate_movew_reg_indix_1_0 =
{
  {
    { TRUE, TRUE, 0, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { TRUE, FALSE, 1, MAP_SWAP16_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_BYTE },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ, REGSET_BYTE,
  {
    { host_movew_reg_indix, { 1, 0, USE_M68K_PC } },
  },
  NULL
};

const guest_code_descriptor_t xlate_movel_reg_indix_1_0 =
{
  {
    { TRUE, TRUE, 0, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { TRUE, FALSE, 1, MAP_SWAP32_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_BYTE },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ, REGSET_BYTE,
  {
    { host_movel_reg_indix, { 1, 0, USE_M68K_PC } },
  },
  NULL
};


static const guest_code_descriptor_t xlate_moveb_indix_reg_1_0_N1 =
{
  {
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_NATIVE_DIRTY, REGSET_BYTE },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ, REGSET_BYTE,
  {
    { host_moveb_indix_reg, { 1, 0, USE_M68K_PC } },
    { i386_testb_reg_reg,   { 0, 0 } },
  },
  NULL
};

const guest_code_descriptor_t xlate_moveb_indix_reg_1_0 =
{
  {
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_NATIVE_DIRTY, REGSET_BYTE },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_NONE, REGSET_BYTE,
  {
    { host_moveb_indix_reg, { 1, 0, USE_M68K_PC } },
  },
  &xlate_moveb_indix_reg_1_0_N1
};

static const guest_code_descriptor_t xlate_movew_indix_reg_1_0_N1 =
{
  {
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { TRUE, FALSE, 0, MAP_NATIVE_MASK | MAP_SWAP16_MASK, REQUEST_REG,
	ROS_NATIVE_DIRTY, REGSET_BYTE },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ, REGSET_BYTE,
  {
    { host_movew_indix_reg,   { 1, 0, USE_M68K_PC } },
    { host_swap16,            { 0 } },
    { i386_testw_reg_reg,     { 0, 0 } },
  },
  NULL
};

const guest_code_descriptor_t xlate_movew_indix_reg_1_0 =
{
  {
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { TRUE, FALSE, 0, MAP_NATIVE_MASK | MAP_SWAP16_MASK, REQUEST_REG,
	ROS_SWAP16_DIRTY, REGSET_BYTE },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_NONE, REGSET_BYTE,
  {
    { host_movew_indix_reg, { 1, 0, USE_M68K_PC } },
  },
  &xlate_movew_indix_reg_1_0_N1
};

static const guest_code_descriptor_t xlate_movel_indix_reg_1_0_N1 =
{
  {
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { TRUE, FALSE, 0, MAP_ALL_MASK, REQUEST_SPARE_REG,
	ROS_NATIVE_DIRTY, REGSET_BYTE },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ, REGSET_BYTE,
  {
    { host_movel_indix_reg,   { 1, 0, USE_M68K_PC } },
    { host_swap32,            { 0 } },
    { i386_testl_reg_reg,     { 0, 0 } },
  },
  NULL
};

const guest_code_descriptor_t xlate_movel_indix_reg_1_0 =
{
  {
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { TRUE, FALSE, 0, MAP_ALL_MASK, REQUEST_SPARE_REG,
	ROS_SWAP32_DIRTY, REGSET_BYTE },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_NONE, REGSET_BYTE,
  {
    { host_movel_indix_reg, { 1, 0, USE_M68K_PC } },
  },
  &xlate_movel_indix_reg_1_0_N1
};

const guest_code_descriptor_t xlate_movel_indix_areg_1_0 =
{
  {
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { TRUE, TRUE, 0, MAP_ALL_MASK, REQUEST_SPARE_REG,
	ROS_SWAP32_DIRTY, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_NONE, REGSET_BYTE,
  {
    { host_movel_indix_reg, { 1, 0, USE_M68K_PC } },
  },
  NULL
};

#endif


const guest_code_descriptor_t xlate_moveb_zero_indix_0 =
{
  {
    { TRUE, TRUE, 0, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ, REGSET_BYTE,
  {
    { host_moveb_imm_indix, {{ USE_ZERO, 0, USE_M68K_PC }} },
  },
  NULL
};

const guest_code_descriptor_t xlate_movew_zero_indix_0 =
{
  {
    { TRUE, TRUE, 0, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ, REGSET_ALL,
  {
    { host_movew_imm_indix, {{ USE_ZERO, 0, USE_M68K_PC }} },
  },
  NULL
};

const guest_code_descriptor_t xlate_movel_zero_indix_0 =
{
  {
    { TRUE, TRUE, 0, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ, REGSET_ALL,
  {
    { host_movel_imm_indix, {{ USE_ZERO, 0, USE_M68K_PC }} },
  },
  NULL
};


const guest_code_descriptor_t xlate_cmpb_zero_indix_0 =
{
  {
    { TRUE, TRUE, 0, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ, REGSET_ALL,
  {
    { host_cmpb_imm_indix, {{ USE_ZERO, 0, USE_M68K_PC }} },
  },
  NULL
};

const guest_code_descriptor_t xlate_cmpw_zero_indix_0 =
{
  {
    { TRUE, TRUE, 0, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CCZ | M68K_CCC | M68K_CCV, REGSET_ALL,
  {
    { host_cmpw_imm_indix, {{ USE_ZERO, 0, USE_M68K_PC }} },
  },
  NULL
};

const guest_code_descriptor_t xlate_cmpl_zero_indix_0 =
{
  {
    { TRUE, TRUE, 0, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CCZ | M68K_CCC | M68K_CCV, REGSET_ALL,
  {
    { host_cmpl_imm_indix, {{ USE_ZERO, 0, USE_M68K_PC }} },
  },
  NULL
};


const guest_code_descriptor_t xlate_addb_imm_indix_0_1 =
{
  {
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVXZ, REGSET_ALL,
  {
    { host_addb_imm_indix, {{ 0, 1, USE_M68K_PC }} },
  },
  NULL
};


const guest_code_descriptor_t xlate_addw_imm_indix_0_1 =
{
  {
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVXZ, REGSET_ALL,
  {
    { host_addw_imm_indix, {{ 0, 1, USE_M68K_PC }} },
  },
  NULL
};


const guest_code_descriptor_t xlate_addl_imm_indix_0_1 =
{
  {
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVXZ, REGSET_ALL,
  {
    { host_addl_imm_indix, {{ 0, 1, USE_M68K_PC }} },
  },
  NULL
};

const guest_code_descriptor_t xlate_subb_imm_indix_0_1 =
{
  {
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVXZ, REGSET_ALL,
  {
    { host_subb_imm_indix, {{ 0, 1, USE_M68K_PC }} },
  },
  NULL
};


const guest_code_descriptor_t xlate_subw_imm_indix_0_1 =
{
  {
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVXZ, REGSET_ALL,
  {
    { host_subw_imm_indix, {{ 0, 1, USE_M68K_PC }} },
  },
  NULL
};


const guest_code_descriptor_t xlate_subl_imm_indix_0_1 =
{
  {
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVXZ, REGSET_ALL,
  {
    { host_subl_imm_indix, {{ 0, 1, USE_M68K_PC }} },
  },
  NULL
};


const guest_code_descriptor_t xlate_cmpb_indix_reg_1_0 =
{
  {
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_NATIVE_DIRTY, REGSET_BYTE },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ, REGSET_BYTE,
  {
    { host_cmpb_indix_reg, {{ 1, 0, USE_M68K_PC }} },
  },
  NULL
};

static const guest_code_descriptor_t xlate_cmpw_indix_reg_1_0_N1 =
{
  {
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_BYTE },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ, REGSET_ALL,
  {
    { host_movew_indix_reg, {{ 1, USE_SCRATCH_REG, USE_M68K_PC }} },
    { host_swap16,          {{ USE_SCRATCH_REG }} },
    { i386_cmpw_reg_reg,    {{ USE_SCRATCH_REG, 0 }} },
  },
  NULL
};

const guest_code_descriptor_t xlate_cmpw_indix_reg_1_0 =
{
  {
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { TRUE, FALSE, 0, MAP_SWAP16_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_BYTE },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CCZ, REGSET_ALL,
  {
    { host_cmpw_indix_reg, {{ 1, 0, USE_M68K_PC }} },
  },
  &xlate_cmpw_indix_reg_1_0_N1
};

static const guest_code_descriptor_t xlate_cmpl_indix_reg_1_0_N1 =
{
  {
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_BYTE },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ, REGSET_ALL,
  {
    { host_movel_indix_reg, {{ 1, USE_SCRATCH_REG, USE_M68K_PC }} },
    { host_swap32,          {{ USE_SCRATCH_REG }} },
    { i386_cmpl_reg_reg,    {{ USE_SCRATCH_REG, 0 }} },
  },
  NULL
};

const guest_code_descriptor_t xlate_cmpl_indix_reg_1_0 =
{
  {
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { TRUE, FALSE, 0, MAP_SWAP32_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_BYTE },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CCZ, REGSET_ALL,
  {
    { host_cmpl_indix_reg, {{ 1, 0, USE_M68K_PC }} },
  },
  &xlate_cmpl_indix_reg_1_0_N1
};


const guest_code_descriptor_t xlate_leal_indix_areg_1_0 =
{
  {
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_ALL },
    { TRUE, TRUE, 0, MAP_ALL_MASK, REQUEST_SPARE_REG,
	ROS_NATIVE_DIRTY, REGSET_ALL },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_NONE, REGSET_ALL,
  {
    { host_leal_indix_reg, {{ 1, 0, USE_M68K_PC }} }
  },
  NULL
};


const guest_code_descriptor_t xlate_divsw_imm_reg_1_0 =
{
  {
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG, ROS_NATIVE_DIRTY,
	1L << REG_EAX },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ,
  REGSET_ALL & ~((1L << REG_EAX) | (1L << REG_EDX)),
  {
    { host_divsw_imm_reg, {{ 1 }} },
  }
};


const guest_code_descriptor_t xlate_divsw_reg_reg_1_0 =
{
  {
    { TRUE, FALSE, 1, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_UNTOUCHED,
	REGSET_BYTE & ~((1L << REG_EDX) | (1L << REG_EAX)) },
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG, ROS_NATIVE_DIRTY,
	1L << REG_EAX },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ,
  REGSET_ALL & ~((1L << REG_EAX) | (1L << REG_EDX)),
  {
    { i386_movswl_reg_reg,     {{ 1, USE_SCRATCH_REG }} },
    { host_divsw,              {{ USE_M68K_PC, USE_M68K_PC_PLUS_TWO,
				  USE_BLOCK, USE_MINUS_ONE }} },
  }
};


const guest_code_descriptor_t xlate_divsw_ind_reg_1_0 =
{
  {
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED,
	REGSET_ALL & ~((1L << REG_EDX) | (1L << REG_EAX)) },
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG, ROS_NATIVE_DIRTY,
	1L << REG_EAX },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ,
  REGSET_ALL & ~((1L << REG_EAX) | (1L << REG_EDX)),
  {
    { host_movew_ind_reg_swap, {{ 1, USE_SCRATCH_REG }} },
    { i386_movswl_reg_reg,     {{ USE_SCRATCH_REG, USE_SCRATCH_REG }} },
    { host_divsw,              {{ USE_M68K_PC, USE_M68K_PC_PLUS_TWO,
				  USE_BLOCK, USE_MINUS_ONE }} },
  }
};


const guest_code_descriptor_t xlate_divsw_predec_reg_1_0 =
{
  {
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED,
	REGSET_ALL & ~((1L << REG_EDX) | (1L << REG_EAX)) },
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG, ROS_NATIVE_DIRTY,
	1L << REG_EAX },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ,
  REGSET_ALL & ~((1L << REG_EAX) | (1L << REG_EDX)),
  {
    { host_movew_predec_reg_swap, {{ 1, USE_SCRATCH_REG }} },
    { i386_movswl_reg_reg,        {{ USE_SCRATCH_REG, USE_SCRATCH_REG }} },
    { host_divsw,                 {{ USE_M68K_PC, USE_M68K_PC_PLUS_TWO,
				     USE_BLOCK, USE_MINUS_ONE }} },
  }
};


const guest_code_descriptor_t xlate_divsw_postinc_reg_1_0 =
{
  {
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED,
	REGSET_ALL & ~((1L << REG_EDX) | (1L << REG_EAX)) },
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG, ROS_NATIVE_DIRTY,
	1L << REG_EAX },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ,
  REGSET_ALL & ~((1L << REG_EAX) | (1L << REG_EDX)),
  {
    { host_movew_postinc_reg_swap, {{ 1, USE_SCRATCH_REG }} },
    { i386_movswl_reg_reg,         {{ USE_SCRATCH_REG, USE_SCRATCH_REG }} },
    { host_divsw,                  {{ USE_M68K_PC, USE_M68K_PC_PLUS_TWO,
				      USE_BLOCK, USE_MINUS_ONE }} },
  }
};


const guest_code_descriptor_t xlate_divsw_indoff_reg_2_1_0 =
{
  {
    { TRUE, TRUE, 1, MAP_NATIVE_MASK | MAP_OFFSET_MASK, REQUEST_REG,
	ROS_UNTOUCHED,
	REGSET_ALL & ~((1L << REG_EDX) | (1L << REG_EAX)) },
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG, ROS_NATIVE_DIRTY,
	1L << REG_EAX },
    { FALSE },
  },
  M68K_CC_NONE, M68K_CC_CNVZ,
  REGSET_ALL & ~((1L << REG_EAX) | (1L << REG_EDX)),
  {
    { host_movew_indoff_reg_swap,  {{ 2, 1, USE_SCRATCH_REG }} },
    { i386_movswl_reg_reg,         {{ USE_SCRATCH_REG, USE_SCRATCH_REG }} },
    { host_divsw,                  {{ USE_M68K_PC, USE_M68K_PC_PLUS_FOUR,
				      USE_BLOCK, USE_MINUS_ONE }} },
  }
};


const guest_code_descriptor_t xlate_addxb_reg_reg_1_0 =
{
  {
    { TRUE, FALSE, 1, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_BYTE },
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_NATIVE_DIRTY, REGSET_BYTE },
    { FALSE },
  },
  M68K_CCX, M68K_CCC | M68K_CCN | M68K_CCV | M68K_CCX, 0,
  {
    { i386_adcb_reg_reg, {{ 1, 0, 0, 0 }} },
  },
  NULL
};

const guest_code_descriptor_t xlate_addxw_reg_reg_1_0 =
{
  {
    { TRUE, FALSE, 1, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_BYTE },
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_NATIVE_DIRTY, REGSET_BYTE },
    { FALSE },
  },
  M68K_CCX, M68K_CCC | M68K_CCN | M68K_CCV | M68K_CCX, 0,
  {
    { i386_adcw_reg_reg, {{ 1, 0, 0, 0 }} },
  },
  NULL
};

const guest_code_descriptor_t xlate_addxl_reg_reg_1_0 =
{
  {
    { TRUE, FALSE, 1, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_UNTOUCHED, REGSET_BYTE },
    { TRUE, FALSE, 0, MAP_NATIVE_MASK, REQUEST_REG,
	ROS_NATIVE_DIRTY, REGSET_BYTE },
    { FALSE },
  },
  M68K_CCX, M68K_CCC | M68K_CCN | M68K_CCV | M68K_CCX, 0,
  {
    { i386_adcl_reg_reg, {{ 1, 0, 0, 0 }} },
  },
  NULL
};


#endif  /* GENERATE_NATIVE_CODE */
