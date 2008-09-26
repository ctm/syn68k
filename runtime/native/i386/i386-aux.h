#ifndef _I386_AUX_H_
#define _I386_AUX_H_

#include "syn68k_private.h"
#include "native.h"

extern int host_testb_reg           (COMMON_ARGS, int32 reg);
extern int host_testw_reg           (COMMON_ARGS, int32 reg);
extern int host_testl_reg           (COMMON_ARGS, int32 reg);

#define host_testb_swapped_reg host_testb_reg
extern int host_testw_swapped_reg   (COMMON_ARGS, int32 reg);
extern int host_testl_swapped_reg   (COMMON_ARGS, int32 reg);

extern int host_swap16_sext_test_reg (COMMON_ARGS, int32 reg);

extern int host_andb_imm_reg (COMMON_ARGS, int32 val, int32 reg);
extern int host_andw_imm_reg (COMMON_ARGS, int32 val, int32 reg);
extern int host_andl_imm_reg (COMMON_ARGS, int32 val, int32 reg);

extern int host_orb_imm_reg (COMMON_ARGS, int32 val, int32 reg);
extern int host_orw_imm_reg (COMMON_ARGS, int32 val, int32 reg);
extern int host_orl_imm_reg (COMMON_ARGS, int32 val, int32 reg);

extern int host_xorb_imm_reg (COMMON_ARGS, int32 val, int32 reg);
extern int host_xorw_imm_reg (COMMON_ARGS, int32 val, int32 reg);
extern int host_xorl_imm_reg (COMMON_ARGS, int32 val, int32 reg);

extern int host_andb_ind_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_andw_ind_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_andl_ind_reg (COMMON_ARGS, int32 addr_reg, int32 reg);

extern int host_orb_ind_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_orw_ind_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_orl_ind_reg (COMMON_ARGS, int32 addr_reg, int32 reg);

extern int host_cmpb_ind_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_cmpw_ind_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_cmpl_ind_reg (COMMON_ARGS, int32 addr_reg, int32 reg);

extern int host_andb_predec_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_andw_predec_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_andl_predec_reg (COMMON_ARGS, int32 addr_reg, int32 reg);

extern int host_orb_predec_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_orw_predec_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_orl_predec_reg (COMMON_ARGS, int32 addr_reg, int32 reg);

extern int host_cmpb_predec_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_cmpw_predec_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_cmpl_predec_reg (COMMON_ARGS, int32 addr_reg, int32 reg);

extern int host_andb_postinc_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_andw_postinc_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_andl_postinc_reg (COMMON_ARGS, int32 addr_reg, int32 reg);

extern int host_orb_postinc_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_orw_postinc_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_orl_postinc_reg (COMMON_ARGS, int32 addr_reg, int32 reg);

extern int host_cmpb_postinc_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_cmpw_postinc_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_cmpl_postinc_reg (COMMON_ARGS, int32 addr_reg, int32 reg);

extern int host_andb_indoff_reg (COMMON_ARGS, int32 offset, int32 addr_reg,
				 int32 reg);
extern int host_andw_indoff_reg (COMMON_ARGS, int32 offset, int32 addr_reg,
				 int32 reg);
extern int host_andl_indoff_reg (COMMON_ARGS, int32 offset, int32 addr_reg,
				 int32 reg);

extern int host_orb_indoff_reg (COMMON_ARGS, int32 offset, int32 addr_reg,
				int32 reg);
extern int host_orw_indoff_reg (COMMON_ARGS, int32 offset, int32 addr_reg,
				int32 reg);
extern int host_orl_indoff_reg (COMMON_ARGS, int32 offset, int32 addr_reg,
				int32 reg);

extern int host_cmpb_indoff_reg (COMMON_ARGS, int32 offset, int32 addr_reg,
				 int32 reg);
extern int host_cmpw_indoff_reg (COMMON_ARGS, int32 offset, int32 addr_reg,
				 int32 reg);
extern int host_cmpl_indoff_reg (COMMON_ARGS, int32 offset, int32 addr_reg,
				 int32 reg);

extern int host_addb_ind_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_subb_ind_reg (COMMON_ARGS, int32 addr_reg, int32 reg);

extern int host_addb_predec_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_subb_predec_reg (COMMON_ARGS, int32 addr_reg, int32 reg);

extern int host_addb_postinc_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_subb_postinc_reg (COMMON_ARGS, int32 addr_reg, int32 reg);

extern int host_addb_indoff_reg (COMMON_ARGS, int32 offset, int32 addr_reg,
				 int32 reg);
extern int host_subb_indoff_reg (COMMON_ARGS, int32 offset, int32 addr_reg,
				 int32 reg);


extern int host_andb_ind_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_andw_ind_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_andl_ind_reg (COMMON_ARGS, int32 addr_reg, int32 reg);

extern int host_orb_ind_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_orw_ind_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_orl_ind_reg (COMMON_ARGS, int32 addr_reg, int32 reg);

extern int host_cmpb_ind_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_cmpw_ind_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_cmpl_ind_reg (COMMON_ARGS, int32 addr_reg, int32 reg);

extern int host_andb_predec_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_andw_predec_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_andl_predec_reg (COMMON_ARGS, int32 addr_reg, int32 reg);

extern int host_orb_predec_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_orw_predec_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_orl_predec_reg (COMMON_ARGS, int32 addr_reg, int32 reg);

extern int host_cmpb_predec_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_cmpw_predec_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_cmpl_predec_reg (COMMON_ARGS, int32 addr_reg, int32 reg);

extern int host_andb_postinc_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_andw_postinc_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_andl_postinc_reg (COMMON_ARGS, int32 addr_reg, int32 reg);

extern int host_orb_postinc_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_orw_postinc_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_orl_postinc_reg (COMMON_ARGS, int32 addr_reg, int32 reg);

extern int host_cmpb_postinc_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_cmpw_postinc_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_cmpl_postinc_reg (COMMON_ARGS, int32 addr_reg, int32 reg);

extern int host_andb_indoff_reg (COMMON_ARGS, int32 offset, int32 addr_reg,
				 int32 reg);
extern int host_andw_indoff_reg (COMMON_ARGS, int32 offset, int32 addr_reg,
				 int32 reg);
extern int host_andl_indoff_reg (COMMON_ARGS, int32 offset, int32 addr_reg,
				 int32 reg);

extern int host_orb_indoff_reg (COMMON_ARGS, int32 offset, int32 addr_reg,
				int32 reg);
extern int host_orw_indoff_reg (COMMON_ARGS, int32 offset, int32 addr_reg,
				int32 reg);
extern int host_orl_indoff_reg (COMMON_ARGS, int32 offset, int32 addr_reg,
				int32 reg);

extern int host_cmpb_indoff_reg (COMMON_ARGS, int32 offset, int32 addr_reg,
				 int32 reg);
extern int host_cmpw_indoff_reg (COMMON_ARGS, int32 offset, int32 addr_reg,
				 int32 reg);
extern int host_cmpl_indoff_reg (COMMON_ARGS, int32 offset, int32 addr_reg,
				 int32 reg);

extern int host_addb_ind_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_subb_ind_reg (COMMON_ARGS, int32 addr_reg, int32 reg);

extern int host_addb_predec_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_subb_predec_reg (COMMON_ARGS, int32 addr_reg, int32 reg);

extern int host_addb_postinc_reg (COMMON_ARGS, int32 addr_reg, int32 reg);
extern int host_subb_postinc_reg (COMMON_ARGS, int32 addr_reg, int32 reg);

extern int host_addb_indoff_reg (COMMON_ARGS, int32 offset, int32 addr_reg,
				 int32 reg);
extern int host_subb_indoff_reg (COMMON_ARGS, int32 offset, int32 addr_reg,
				 int32 reg);

extern int host_andb_reg_ind (COMMON_ARGS, int32 reg, int32 addr_reg);
extern int host_andw_reg_ind (COMMON_ARGS, int32 reg, int32 addr_reg);
extern int host_andl_reg_ind (COMMON_ARGS, int32 reg, int32 addr_reg);

extern int host_orb_reg_ind (COMMON_ARGS, int32 reg, int32 addr_reg);
extern int host_orw_reg_ind (COMMON_ARGS, int32 reg, int32 addr_reg);
extern int host_orl_reg_ind (COMMON_ARGS, int32 reg, int32 addr_reg);

extern int host_xorb_reg_ind (COMMON_ARGS, int32 reg, int32 addr_reg);
extern int host_xorw_reg_ind (COMMON_ARGS, int32 reg, int32 addr_reg);
extern int host_xorl_reg_ind (COMMON_ARGS, int32 reg, int32 addr_reg);

extern int host_cmpb_reg_ind (COMMON_ARGS, int32 reg, int32 addr_reg);
extern int host_cmpw_reg_ind (COMMON_ARGS, int32 reg, int32 addr_reg);
extern int host_cmpl_reg_ind (COMMON_ARGS, int32 reg, int32 addr_reg);

extern int host_andb_reg_predec (COMMON_ARGS, int32 reg, int32 addr_reg);
extern int host_andw_reg_predec (COMMON_ARGS, int32 reg, int32 addr_reg);
extern int host_andl_reg_predec (COMMON_ARGS, int32 reg, int32 addr_reg);

extern int host_orb_reg_predec (COMMON_ARGS, int32 reg, int32 addr_reg);
extern int host_orw_reg_predec (COMMON_ARGS, int32 reg, int32 addr_reg);
extern int host_orl_reg_predec (COMMON_ARGS, int32 reg, int32 addr_reg);

extern int host_xorb_reg_predec (COMMON_ARGS, int32 reg, int32 addr_reg);
extern int host_xorw_reg_predec (COMMON_ARGS, int32 reg, int32 addr_reg);
extern int host_xorl_reg_predec (COMMON_ARGS, int32 reg, int32 addr_reg);

extern int host_cmpb_reg_predec (COMMON_ARGS, int32 reg, int32 addr_reg);
extern int host_cmpw_reg_predec (COMMON_ARGS, int32 reg, int32 addr_reg);
extern int host_cmpl_reg_predec (COMMON_ARGS, int32 reg, int32 addr_reg);

extern int host_andb_reg_postinc (COMMON_ARGS, int32 reg, int32 addr_reg);
extern int host_andw_reg_postinc (COMMON_ARGS, int32 reg, int32 addr_reg);
extern int host_andl_reg_postinc (COMMON_ARGS, int32 reg, int32 addr_reg);

extern int host_orb_reg_postinc (COMMON_ARGS, int32 reg, int32 addr_reg);
extern int host_orw_reg_postinc (COMMON_ARGS, int32 reg, int32 addr_reg);
extern int host_orl_reg_postinc (COMMON_ARGS, int32 reg, int32 addr_reg);

extern int host_xorb_reg_postinc (COMMON_ARGS, int32 reg, int32 addr_reg);
extern int host_xorw_reg_postinc (COMMON_ARGS, int32 reg, int32 addr_reg);
extern int host_xorl_reg_postinc (COMMON_ARGS, int32 reg, int32 addr_reg);

extern int host_cmpb_reg_postinc (COMMON_ARGS, int32 reg, int32 addr_reg);
extern int host_cmpw_reg_postinc (COMMON_ARGS, int32 reg, int32 addr_reg);
extern int host_cmpl_reg_postinc (COMMON_ARGS, int32 reg, int32 addr_reg);

extern int host_andb_reg_indoff (COMMON_ARGS, int32 reg, int32 offset,
				 int32 addr_reg);
extern int host_andw_reg_indoff (COMMON_ARGS, int32 reg, int32 offset,
				 int32 addr_reg);
				 
extern int host_andl_reg_indoff (COMMON_ARGS, int32 reg, int32 offset,
				 int32 addr_reg);

extern int host_orb_reg_indoff (COMMON_ARGS, int32 reg, int32 offset,
				int32 addr_reg);
				
extern int host_orw_reg_indoff (COMMON_ARGS, int32 reg, int32 offset,
				int32 addr_reg);
				
extern int host_orl_reg_indoff (COMMON_ARGS, int32 reg, int32 offset,
				int32 addr_reg);

extern int host_xorb_reg_indoff (COMMON_ARGS, int32 reg, int32 offset,
				 int32 addr_reg);
				
extern int host_xorw_reg_indoff (COMMON_ARGS, int32 reg, int32 offset,
				 int32 addr_reg);
				
extern int host_xorl_reg_indoff (COMMON_ARGS, int32 reg, int32 offset,
				 int32 addr_reg);

extern int host_cmpb_reg_indoff (COMMON_ARGS, int32 reg, int32 offset,
				 int32 addr_reg);
				 
extern int host_cmpw_reg_indoff (COMMON_ARGS, int32 reg, int32 offset,
				 int32 addr_reg);
				 
extern int host_cmpl_reg_indoff (COMMON_ARGS, int32 reg, int32 offset,
				 int32 addr_reg);

extern int host_addb_reg_ind (COMMON_ARGS, int32 reg, int32 addr_reg);
extern int host_subb_reg_ind (COMMON_ARGS, int32 reg, int32 addr_reg);

extern int host_addb_reg_predec (COMMON_ARGS, int32 reg, int32 addr_reg);
extern int host_subb_reg_predec (COMMON_ARGS, int32 reg, int32 addr_reg);

extern int host_addb_reg_postinc (COMMON_ARGS, int32 reg, int32 addr_reg);
extern int host_subb_reg_postinc (COMMON_ARGS, int32 reg, int32 addr_reg);

extern int host_addb_reg_indoff (COMMON_ARGS, int32 reg, int32 offset,
				 int32 addr_reg);
				 
extern int host_subb_reg_indoff (COMMON_ARGS, int32 reg, int32 offset,
				 int32 addr_reg);

extern int host_andb_imm_abs (COMMON_ARGS, int32 val, int32 addr);
extern int host_andw_imm_abs (COMMON_ARGS, int32 val, int32 addr);
extern int host_andl_imm_abs (COMMON_ARGS, int32 val, int32 addr);

extern int host_orb_imm_abs (COMMON_ARGS, int32 val, int32 addr);
extern int host_orw_imm_abs (COMMON_ARGS, int32 val, int32 addr);
extern int host_orl_imm_abs (COMMON_ARGS, int32 val, int32 addr);

extern int host_xorb_imm_abs (COMMON_ARGS, int32 val, int32 addr);
extern int host_xorw_imm_abs (COMMON_ARGS, int32 val, int32 addr);
extern int host_xorl_imm_abs (COMMON_ARGS, int32 val, int32 addr);

extern int host_cmpb_imm_abs (COMMON_ARGS, int32 val, int32 addr);
extern int host_cmpw_imm_abs (COMMON_ARGS, int32 val, int32 addr);
extern int host_cmpl_imm_abs (COMMON_ARGS, int32 val, int32 addr);

extern int host_andb_imm_ind (COMMON_ARGS, int32 val, int32 addr);
extern int host_orb_imm_ind  (COMMON_ARGS, int32 val, int32 addr);
extern int host_xorb_imm_ind (COMMON_ARGS, int32 val, int32 addr);

extern int host_addb_imm_ind (COMMON_ARGS, int32 val, int32 addr);
extern int host_subb_imm_ind (COMMON_ARGS, int32 val, int32 addr);

extern int host_cmpb_imm_ind (COMMON_ARGS, int32 val, int32 addr);
extern int host_cmpw_imm_ind (COMMON_ARGS, int32 val, int32 addr);
extern int host_cmpl_imm_ind (COMMON_ARGS, int32 val, int32 addr);

extern int host_andw_imm_ind (COMMON_ARGS, int32 val, int32 addr);
extern int host_orw_imm_ind  (COMMON_ARGS, int32 val, int32 addr);
extern int host_xorw_imm_ind (COMMON_ARGS, int32 val, int32 addr);

extern int host_andl_imm_ind (COMMON_ARGS, int32 val, int32 addr);
extern int host_orl_imm_ind  (COMMON_ARGS, int32 val, int32 addr);
extern int host_xorl_imm_ind (COMMON_ARGS, int32 val, int32 addr);

extern int host_andb_imm_indoff (COMMON_ARGS, int32 val, int32 offset,
				 int32 addr_reg);
extern int host_orb_imm_indoff  (COMMON_ARGS, int32 val, int32 offset,
				 int32 addr_reg);
extern int host_xorb_imm_indoff (COMMON_ARGS, int32 val, int32 offset,
				 int32 addr_reg);

extern int host_addb_imm_indoff (COMMON_ARGS, int32 val, int32 offset,
				 int32 addr_reg);
extern int host_subb_imm_indoff (COMMON_ARGS, int32 val, int32 offset,
				 int32 addr_reg);

extern int host_cmpb_imm_indoff (COMMON_ARGS, int32 val, int32 offset,
				 int32 addr_reg);
extern int host_cmpw_imm_indoff (COMMON_ARGS, int32 val, int32 offset,
				 int32 addr_reg);
extern int host_cmpl_imm_indoff (COMMON_ARGS, int32 val, int32 offset,
				 int32 addr_reg);

extern int host_andw_imm_indoff (COMMON_ARGS, int32 val, int32 offset,
				 int32 addr_reg);
extern int host_orw_imm_indoff  (COMMON_ARGS, int32 val, int32 offset,
				 int32 addr_reg);
extern int host_xorw_imm_indoff (COMMON_ARGS, int32 val, int32 offset,
				 int32 addr_reg);

extern int host_andl_imm_indoff (COMMON_ARGS, int32 val, int32 offset,
				 int32 addr_reg);
extern int host_orl_imm_indoff  (COMMON_ARGS, int32 val, int32 offset,
				 int32 addr_reg);
extern int host_xorl_imm_indoff (COMMON_ARGS, int32 val, int32 offset,
				 int32 addr_reg);

extern int host_andb_imm_predec (COMMON_ARGS, int32 val, int32 addr);
extern int host_orb_imm_predec  (COMMON_ARGS, int32 val, int32 addr);
extern int host_xorb_imm_predec (COMMON_ARGS, int32 val, int32 addr);

extern int host_addb_imm_predec (COMMON_ARGS, int32 val, int32 addr);
extern int host_subb_imm_predec (COMMON_ARGS, int32 val, int32 addr);

extern int host_cmpb_imm_predec (COMMON_ARGS, int32 val, int32 addr);
extern int host_cmpw_imm_predec (COMMON_ARGS, int32 val, int32 addr);
extern int host_cmpl_imm_predec (COMMON_ARGS, int32 val, int32 addr);

extern int host_andw_imm_predec (COMMON_ARGS, int32 val, int32 addr);
extern int host_orw_imm_predec  (COMMON_ARGS, int32 val, int32 addr);
extern int host_xorw_imm_predec (COMMON_ARGS, int32 val, int32 addr);

extern int host_andl_imm_predec (COMMON_ARGS, int32 val, int32 addr);
extern int host_orl_imm_predec  (COMMON_ARGS, int32 val, int32 addr);
extern int host_xorl_imm_predec (COMMON_ARGS, int32 val, int32 addr);

extern int host_andb_imm_postinc (COMMON_ARGS, int32 val, int32 addr);
extern int host_orb_imm_postinc  (COMMON_ARGS, int32 val, int32 addr);
extern int host_xorb_imm_postinc (COMMON_ARGS, int32 val, int32 addr);

extern int host_addb_imm_postinc (COMMON_ARGS, int32 val, int32 addr);
extern int host_subb_imm_postinc (COMMON_ARGS, int32 val, int32 addr);

extern int host_cmpb_imm_postinc (COMMON_ARGS, int32 val, int32 addr);
extern int host_cmpw_imm_postinc (COMMON_ARGS, int32 val, int32 addr);
extern int host_cmpl_imm_postinc (COMMON_ARGS, int32 val, int32 addr);

extern int host_andw_imm_postinc (COMMON_ARGS, int32 val, int32 addr);
extern int host_orw_imm_postinc  (COMMON_ARGS, int32 val, int32 addr);
extern int host_xorw_imm_postinc (COMMON_ARGS, int32 val, int32 addr);

extern int host_andl_imm_postinc (COMMON_ARGS, int32 val, int32 addr);
extern int host_orl_imm_postinc  (COMMON_ARGS, int32 val, int32 addr);
extern int host_xorl_imm_postinc (COMMON_ARGS, int32 val, int32 addr);

extern int host_negb_ind     (COMMON_ARGS, int32 addr);
extern int host_negb_predec  (COMMON_ARGS, int32 addr);
extern int host_negb_postinc (COMMON_ARGS, int32 addr);
extern int host_negb_indoff  (COMMON_ARGS, int32 offset, int32 addr);

extern int host_movew_reg_abs_swap (COMMON_ARGS, int32 src_reg, int32 addr);
extern int host_movel_reg_abs_swap (COMMON_ARGS, int32 src_reg, int32 addr);

extern int host_movew_abs_reg_swap (COMMON_ARGS, int32 addr, int32 dst_reg);
extern int host_movel_abs_reg_swap (COMMON_ARGS, int32 addr, int32 dst_reg);

extern int host_moveb_imm_reg (COMMON_ARGS, int32 val, int32 reg);
extern int host_movew_imm_reg (COMMON_ARGS, int32 val, int32 reg);
extern int host_movel_imm_reg (COMMON_ARGS, int32 val, int32 reg);

extern int host_moveb_imm_abs (COMMON_ARGS, int32 val, int32 addr);
extern int host_movew_imm_abs (COMMON_ARGS, int32 val, int32 addr);
extern int host_movel_imm_abs (COMMON_ARGS, int32 val, int32 addr);

extern int host_moveb_imm_ind (COMMON_ARGS, int32 val, int32 addr);
extern int host_movew_imm_ind (COMMON_ARGS, int32 val, int32 addr);
extern int host_movel_imm_ind (COMMON_ARGS, int32 val, int32 addr);

extern int host_moveb_imm_indoff (COMMON_ARGS, int32 val, int32 offset,
				 int32 addr_reg);
extern int host_movew_imm_indoff (COMMON_ARGS, int32 val, int32 offset,
				 int32 addr_reg);
extern int host_movel_imm_indoff (COMMON_ARGS, int32 val, int32 offset,
				 int32 addr_reg);

extern int host_moveb_imm_predec (COMMON_ARGS, int32 val, int32 addr);
extern int host_movew_imm_predec (COMMON_ARGS, int32 val, int32 addr);
extern int host_movel_imm_predec (COMMON_ARGS, int32 val, int32 addr);

extern int host_moveb_imm_postinc (COMMON_ARGS, int32 val, int32 addr);
extern int host_movew_imm_postinc (COMMON_ARGS, int32 val, int32 addr);
extern int host_movel_imm_postinc (COMMON_ARGS, int32 val, int32 addr);

extern int host_moveb_reg_ind (COMMON_ARGS, int32 src_reg, int32 addr);
extern int host_movew_reg_ind (COMMON_ARGS, int32 src_reg, int32 addr);
extern int host_movel_reg_ind (COMMON_ARGS, int32 src_reg, int32 addr);

extern int host_moveb_reg_indoff (COMMON_ARGS, int32 src_reg, int32 offset,
				 int32 addr_reg);
extern int host_movew_reg_indoff (COMMON_ARGS, int32 src_reg, int32 offset,
				 int32 addr_reg);
extern int host_movel_reg_indoff (COMMON_ARGS, int32 src_reg, int32 offset,
				 int32 addr_reg);

extern int host_moveb_reg_predec (COMMON_ARGS, int32 src_reg, int32 addr);
extern int host_movew_reg_predec (COMMON_ARGS, int32 src_reg, int32 addr);
extern int host_movel_reg_predec (COMMON_ARGS, int32 src_reg, int32 addr);

extern int host_moveb_reg_postinc (COMMON_ARGS, int32 src_reg, int32 addr);
extern int host_movew_reg_postinc (COMMON_ARGS, int32 src_reg, int32 addr);
extern int host_movel_reg_postinc (COMMON_ARGS, int32 src_reg, int32 addr);

extern int host_movew_reg_ind_swap (COMMON_ARGS, int32 src_reg, int32 addr);
extern int host_movel_reg_ind_swap (COMMON_ARGS, int32 src_reg, int32 addr);

extern int host_movew_reg_indoff_swap (COMMON_ARGS, int32 src_reg,
				       int32 offset, int32 addr_reg);
extern int host_movel_reg_indoff_swap (COMMON_ARGS, int32 src_reg,
				       int32 offset, int32 addr_reg);

extern int host_movew_reg_predec_swap (COMMON_ARGS, int32 src_reg, int32 addr);
extern int host_movel_reg_predec_swap (COMMON_ARGS, int32 src_reg, int32 addr);

extern int host_movew_reg_postinc_swap (COMMON_ARGS, int32 src_reg,
					int32 addr);
extern int host_movel_reg_postinc_swap (COMMON_ARGS, int32 src_reg,
					int32 addr);

extern int host_moveb_ind_reg (COMMON_ARGS, int32 addr_reg, int32 dst_reg);
extern int host_movew_ind_reg (COMMON_ARGS, int32 addr_reg, int32 dst_reg);
extern int host_movel_ind_reg (COMMON_ARGS, int32 addr_reg, int32 dst_reg);

extern int host_moveb_indoff_reg (COMMON_ARGS, int32 offset, int32 addr_reg,
				 int32 dst_reg);
extern int host_movew_indoff_reg (COMMON_ARGS, int32 offset, int32 addr_reg,
				 int32 dst_reg);
extern int host_movel_indoff_reg (COMMON_ARGS, int32 offset, int32 addr_reg,
				 int32 dst_reg);

extern int host_moveb_predec_reg (COMMON_ARGS, int32 addr_reg, int32 dst_reg);
extern int host_movew_predec_reg (COMMON_ARGS, int32 addr_reg, int32 dst_reg);
extern int host_movel_predec_reg (COMMON_ARGS, int32 addr_reg, int32 dst_reg);

extern int host_moveb_postinc_reg (COMMON_ARGS, int32 addr_reg, int32 dst_reg);
extern int host_movew_postinc_reg (COMMON_ARGS, int32 addr_reg, int32 dst_reg);
extern int host_movel_postinc_reg (COMMON_ARGS, int32 addr_reg, int32 dst_reg);

extern int host_movew_ind_reg_swap (COMMON_ARGS, int32 addr_reg,
				    int32 dst_reg);
extern int host_movel_ind_reg_swap (COMMON_ARGS, int32 addr_reg,
				    int32 dst_reg);

extern int host_movew_indoff_reg_swap (COMMON_ARGS, int32 offset,
				       int32 addr_reg, int32 dst_reg);
extern int host_movel_indoff_reg_swap (COMMON_ARGS, int32 offset,
				       int32 addr_reg, int32 dst_reg);

extern int host_movew_predec_reg_swap (COMMON_ARGS, int32 addr_reg,
				       int32 dst_reg);
extern int host_movel_predec_reg_swap (COMMON_ARGS, int32 addr_reg,
				       int32 dst_reg);

extern int host_movew_postinc_reg_swap (COMMON_ARGS, int32 addr_reg,
					int32 dst_reg);
extern int host_movel_postinc_reg_swap (COMMON_ARGS, int32 addr_reg,
					int32 dst_reg);

extern int host_cmpmb_postinc_postinc (COMMON_ARGS, int32 reg1, int32 reg2);
extern int host_cmpmw_postinc_postinc (COMMON_ARGS, int32 reg1, int32 reg2);
extern int host_cmpml_postinc_postinc (COMMON_ARGS, int32 reg1, int32 reg2);

extern int host_addl_imm_reg (COMMON_ARGS, int32 val, int32 host_reg);
extern int host_subl_imm_reg (COMMON_ARGS, int32 val, int32 host_reg);
extern int host_cmpl_imm_reg (COMMON_ARGS, int32 val, int32 host_reg);

extern int host_bclr_imm_reg (COMMON_ARGS, int32 bitnum, int32 reg);
extern int host_bset_imm_reg (COMMON_ARGS, int32 bitnum, int32 reg);
extern int host_bchg_imm_reg (COMMON_ARGS, int32 bitnum, int32 reg);
extern int host_btst_imm_reg (COMMON_ARGS, int32 bitnum, int32 reg);

extern int host_addl_reg_reg (COMMON_ARGS, int32 reg1, int32 reg2);
extern int host_subl_reg_reg (COMMON_ARGS, int32 reg1, int32 reg2);
extern int host_cmpl_reg_reg (COMMON_ARGS, int32 reg1, int32 reg2);

extern int host_bcc (COMMON_ARGS, Block *b);
extern int host_bcs (COMMON_ARGS, Block *b);
extern int host_beq (COMMON_ARGS, Block *b);
extern int host_bge (COMMON_ARGS, Block *b);
extern int host_bgt (COMMON_ARGS, Block *b);
extern int host_bhi (COMMON_ARGS, Block *b);
extern int host_ble (COMMON_ARGS, Block *b);
extern int host_bls (COMMON_ARGS, Block *b);
extern int host_blt (COMMON_ARGS, Block *b);
extern int host_bmi (COMMON_ARGS, Block *b);
extern int host_bne (COMMON_ARGS, Block *b);
extern int host_bpl (COMMON_ARGS, Block *b);
extern int host_bvc (COMMON_ARGS, Block *b);
extern int host_bvs (COMMON_ARGS, Block *b);

extern int host_jmp (COMMON_ARGS, Block *b);

extern int host_dbra (COMMON_ARGS, int32 guest_reg, Block *b);

extern int host_swap (COMMON_ARGS, int32 reg);

extern int host_extbw (COMMON_ARGS, int32 reg);
extern int host_extwl (COMMON_ARGS, int32 reg);
extern int host_extbl (COMMON_ARGS, int32 reg);

extern int host_unlk (COMMON_ARGS, int32 reg, int32 a7_reg);
extern int host_link (COMMON_ARGS, int32 offset, int32 reg, int32 a7_reg);

extern int host_moveml_reg_predec (COMMON_ARGS, int32 host_addr_reg,
				   int32 reg_mask);
extern int host_moveml_postinc_reg (COMMON_ARGS, int32 host_addr_reg,
				    int32 reg_mask);

extern int host_pea_indoff (COMMON_ARGS, int32 offset, int32 host_reg,
			    int32 a7_reg);
extern int host_leal_indoff_areg (COMMON_ARGS, int32 offset, int32 base_reg,
				  int32 dst_reg);

extern int host_rts (COMMON_ARGS, Block *b, int32 a7_reg);

extern int host_jsr_abs (COMMON_ARGS, int32 a7_reg, int32 target_addr,
			 Block *b, uint16 *m68k_code);
extern int host_jsr_pcd16 (COMMON_ARGS, int32 a7_reg, Block *b,
			   uint16 *m68k_code);
extern int host_jsr_d16 (COMMON_ARGS, int32 a7_reg, int32 base_addr_reg,
			 Block *b, uint16 *m68k_code);
extern int host_bsr (COMMON_ARGS, int32 a7_reg, Block *b, uint16 *m68k_code);

extern int host_moveb_imm_indix (COMMON_ARGS, int32 val, int32 base_addr_reg,
				 uint16 *m68k_addr);
extern int host_movew_imm_indix (COMMON_ARGS, int32 val, int32 base_addr_reg,
				 uint16 *m68k_addr);
extern int host_movel_imm_indix (COMMON_ARGS, int32 val, int32 base_addr_reg,
				 uint16 *m68k_addr);

extern int host_cmpb_imm_indix (COMMON_ARGS, int32 val, int32 base_addr_reg,
				uint16 *m68k_addr);
extern int host_cmpw_imm_indix (COMMON_ARGS, int32 val, int32 base_addr_reg,
				uint16 *m68k_addr);
extern int host_cmpl_imm_indix (COMMON_ARGS, int32 val, int32 base_addr_reg,
				uint16 *m68k_addr);

extern int host_addb_imm_indix (COMMON_ARGS, int32 val, int32 base_addr_reg,
				uint16 *m68k_addr);
extern int host_addw_imm_indix (COMMON_ARGS, int32 val, int32 base_addr_reg,
				uint16 *m68k_addr);
extern int host_addl_imm_indix (COMMON_ARGS, int32 val, int32 base_addr_reg,
				uint16 *m68k_addr);

extern int host_subb_imm_indix (COMMON_ARGS, int32 val, int32 base_addr_reg,
				uint16 *m68k_addr);
extern int host_subw_imm_indix (COMMON_ARGS, int32 val, int32 base_addr_reg,
				uint16 *m68k_addr);
extern int host_subl_imm_indix (COMMON_ARGS, int32 val, int32 base_addr_reg,
				uint16 *m68k_addr);

extern int host_moveb_reg_indix (COMMON_ARGS, int32 val, int32 base_addr_reg,
				 uint16 *m68k_addr);
extern int host_movew_reg_indix (COMMON_ARGS, int32 val, int32 base_addr_reg,
				 uint16 *m68k_addr);
extern int host_movel_reg_indix (COMMON_ARGS, int32 val, int32 base_addr_reg,
				 uint16 *m68k_addr);

extern int host_moveb_indix_reg (COMMON_ARGS, int32 base_addr_reg,
				 int32 dst_reg, uint16 *m68k_addr);
extern int host_movew_indix_reg (COMMON_ARGS, int32 base_addr_reg,
				 int32 dst_reg, uint16 *m68k_addr);
extern int host_movel_indix_reg (COMMON_ARGS, int32 base_addr_reg,
				 int32 dst_reg, uint16 *m68k_addr);

extern int host_cmpb_indix_reg (COMMON_ARGS, int32 base_addr_reg,
				int32 dst_reg, uint16 *m68k_addr);
extern int host_cmpw_indix_reg (COMMON_ARGS, int32 base_addr_reg,
				int32 dst_reg, uint16 *m68k_addr);
extern int host_cmpl_indix_reg (COMMON_ARGS, int32 base_addr_reg,
				int32 dst_reg, uint16 *m68k_addr);

extern int host_leal_indix_reg (COMMON_ARGS, int32 base_addr_reg,
				int32 dst_reg, uint16 *m68k_addr);

extern int host_divsw (COMMON_ARGS, uint16 *m68k_addr, uint16 *next_addr,
		       Block *b, int32 might_overflow_i386_p);
extern int host_divsw_imm_reg (COMMON_ARGS, int32 val);

extern int host_andb_reg_abs (COMMON_ARGS, int32 src, int32 dst_addr);
extern int host_andw_reg_abs (COMMON_ARGS, int32 src, int32 dst_addr);
extern int host_andl_reg_abs (COMMON_ARGS, int32 src, int32 dst_addr);

extern int host_orb_reg_abs (COMMON_ARGS, int32 src, int32 dst_addr);
extern int host_orw_reg_abs (COMMON_ARGS, int32 src, int32 dst_addr);
extern int host_orl_reg_abs (COMMON_ARGS, int32 src, int32 dst_addr);

extern int host_xorb_reg_abs (COMMON_ARGS, int32 src, int32 dst_addr);
extern int host_xorw_reg_abs (COMMON_ARGS, int32 src, int32 dst_addr);
extern int host_xorl_reg_abs (COMMON_ARGS, int32 src, int32 dst_addr);

extern int host_cmpb_reg_abs (COMMON_ARGS, int32 src, int32 dst_addr);
extern int host_cmpw_reg_abs (COMMON_ARGS, int32 src, int32 dst_addr);
extern int host_cmpl_reg_abs (COMMON_ARGS, int32 src, int32 dst_addr);

extern int host_moveb_reg_abs (COMMON_ARGS, int32 src, int32 dst_addr);
extern int host_movew_reg_abs (COMMON_ARGS, int32 src, int32 dst_addr);
extern int host_movel_reg_abs (COMMON_ARGS, int32 src, int32 dst_addr);

extern int host_addb_reg_abs (COMMON_ARGS, int32 src, int32 dst_addr);
extern int host_subb_reg_abs (COMMON_ARGS, int32 src, int32 dst_addr);

extern int host_andb_abs_reg (COMMON_ARGS, int32 src_addr, int32 dst);
extern int host_andw_abs_reg (COMMON_ARGS, int32 src_addr, int32 dst);
extern int host_andl_abs_reg (COMMON_ARGS, int32 src_addr, int32 dst);

extern int host_orb_abs_reg (COMMON_ARGS, int32 src_addr, int32 dst);
extern int host_orw_abs_reg (COMMON_ARGS, int32 src_addr, int32 dst);
extern int host_orl_abs_reg (COMMON_ARGS, int32 src_addr, int32 dst);

extern int host_xorb_abs_reg (COMMON_ARGS, int32 src_addr, int32 dst);
extern int host_xorw_abs_reg (COMMON_ARGS, int32 src_addr, int32 dst);
extern int host_xorl_abs_reg (COMMON_ARGS, int32 src_addr, int32 dst);

extern int host_cmpb_abs_reg (COMMON_ARGS, int32 src_addr, int32 dst);
extern int host_cmpw_abs_reg (COMMON_ARGS, int32 src_addr, int32 dst);
extern int host_cmpl_abs_reg (COMMON_ARGS, int32 src_addr, int32 dst);

extern int host_addb_abs_reg (COMMON_ARGS, int32 src_addr, int32 dst);
extern int host_subb_abs_reg (COMMON_ARGS, int32 src_addr, int32 dst);

extern int host_moveb_abs_reg (COMMON_ARGS, int32 src, int32 dst_addr);
extern int host_movew_abs_reg (COMMON_ARGS, int32 src, int32 dst_addr);
extern int host_movel_abs_reg (COMMON_ARGS, int32 src, int32 dst_addr);

extern int host_addb_imm_abs (COMMON_ARGS, int32 src, int32 dst_addr);
extern int host_subb_imm_abs (COMMON_ARGS, int32 src, int32 dst_addr);

extern int host_negb_abs (COMMON_ARGS, int32 dst_addr);

#endif  /* !_I386_AUX_H_*/
