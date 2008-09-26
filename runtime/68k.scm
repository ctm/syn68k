; This file defines the semantics and bit patterns of the 68000 instruction
; set.  Enjoy.

; NOTE!  This always runs in supervisor mode, although there is no fundamental
; problem with modifying it to support both supervisor and non-supervisor
; modes.

(include "68k.defines.scm")

; Modify this macro if you want to tweak behavior when modifying the SR
(define (LOAD_NEW_SR new_sr)
  (list
   "{ uint32 tmp_sr, old_sr"
   (assign "tmp_sr" new_sr)
   (assign "old_sr" "cpu_state.sr")
   (assign "cpu_state.sr" (& "tmp_sr" (~ 31))) ; Force low 5 bits to zero
   (assign ccc  (& "tmp_sr"     0x1))
   (assign ccv  (& "tmp_sr"     0x2))
   (assign ccnz (& (~ "tmp_sr") 0x4))
   (assign ccn  (& "tmp_sr"     0x8))
   (assign ccx  (& "tmp_sr"     0x10))

   (call "SWITCH_A7" "old_sr" "tmp_sr")

   ; Note that we should check for another interrupt eventually.  This
   ; is technically wrong, since we should check for an interrupt NOW
   ; since the next instruction might raise the mask level back up
   ; again before we get around to checking.  That's unlikely for the
   ; programs we are executing, though.  If it turns out to be a
   ; problem, we can extend our code so that we know the current m68k
   ; pc whenever the SR might be changed.  Technically we should only
   ; need to do this when the interrupt mask level decreases, but to
   ; be safe we'll always check whenever the SR is mucked with (the
   ; performance hit should be irrelevant, because very few people muck
   ; with the SR, and it's fairly expensive to do so anyway).
   (call "interrupt_note_if_present")
   "}"))

(define (TRAP trapno pc addr)
  (list
   "{ const uint16 *tmp_addr"
   (call "SAVE_CPU_STATE")
   (assign "tmp_addr" (call "code_lookup"
			    (call "trap_direct" trapno pc addr)))
   (call "LOAD_CPU_STATE")
   (assign code "tmp_addr")
   "}"))


; Functions to determine truth value (0 or 1) for various cc combinations
(define CC_CC (not ccc))
(define CC_CS (<> ccc 0))
(define CC_EQ (not ccnz))
(define CC_NE (<> ccnz 0))
(define CC_GE (^ (<> ccn 0) (not ccv)))
(define CC_GT (and ccnz (^ (<> ccn 0) (not ccv))))
(define CC_HI (and ccnz (not ccc)))
(define CC_LE (or (not ccnz) (^ (not ccn) (not ccv))))
(define CC_LS (or ccc (not ccnz)))
(define CC_LT (^ (not ccn) (not ccv)))
(define CC_MI (<> ccn 0))
(define CC_PL (not ccn))
(define CC_VC (not ccv))
(define CC_VS (<> ccv 0))


(defopcode a_line_trap
  (list 68000 amode_implicit (ends_block next_block_dynamic)
	(list "1010xxxxxxxxxxxx"))
  (list "%%%%%" "CNVXZ" dont_expand
	(TRAP 10 (deref "uint32 *" code 0) 0)))


(define (ABCD name amode bit_pattern src dst preamble postamble)
  (defopcode name
    (list 68000 amode () (list bit_pattern))
    (list "C??X<" "---X-" dont_expand
	  (list
	   preamble
	   (assign tmp.uw (+ (>> src 4) (>> dst 4)))
	   (assign tmp2.uw (+ (& src 0xF) (& dst 0xF) (<> ccx 0)))
	   (if (>= tmp2.uw 10)
	       (assign tmp2.uw (+ tmp2.uw (<< tmp.uw 4) 6))
	       (assign tmp2.uw (+ tmp2.uw (<< tmp.uw 4))))
	   (if (>= tmp2.uw 0xA0)
	       (list
		(assign tmp2.uw (- tmp2.uw 0xA0))
		(assign ccx (assign ccc 1)))
	       (assign ccx (assign ccc 0)))
	   (assign ccnz (| ccnz tmp2.ub))
	   (assign ccv 0)                ; For 68020; left alone on 68040
	   (assign ccn (& tmp2.ub 0x80)) ; For 68020; left alone on 68040
	   (assign dst tmp2.ub)
	   postamble))))

(ABCD abcd_dreg amode_implicit "1100xxx100000yyy" $2.dub $1.dub (list) (list))

; Handle the abcd an@-,an@- case (annoying)
(define (SAME_ABCD name reg bits size)
  (ABCD name amode_same_reg bits
	(derefub reg)
	(derefub (- reg size))
	(assign reg (- reg size))
	(assign reg (- reg size))))

(SAME_ABCD abcd_a7@-,a7@- a7.ul "1100111100001111" 2)
(SAME_ABCD abcd_an@-,an@- $1.aul "1100aaa100001aaa" 1)

(ABCD abcd_an@-,a7@- amode_implicit "1100111100001yyy"
      (derefub $1.aul)
      (derefub a7.ul)
      (list
       (assign a7.ul (- a7.ul 2))
       (assign $1.aul (- $1.aul 1)))
      (list))

(ABCD abcd_a7@-,an@- amode_implicit "1100xxx100001111"
      (derefub a7.ul)
      (derefub $1.aul)
      (list
       (assign $1.aul (- $1.aul 1))
       (assign a7.ul (- a7.ul 2)))
      (list))

(ABCD abcd_mem amode_implicit "1100xxx100001yyy"
      (derefub $2.aul)
      (derefub $1.aul)
      (list
       (assign $1.aul (- $1.aul 1))
       (assign $2.aul (- $2.aul 1)))
      (list))


(define (ADDB name bit_pattern expand amode src dst native pre post)
  (defopcode name 
    (list 68000 amode () bit_pattern)
    (list "-----" "-----" expand
	  (native_code native)
	  (list
	   pre
	   (assign dst (+ dst src))
	   post))
    (list "CNVXZ" "-----" dont_expand
	  (native_code native)
	  (list
	   "\n#ifdef FAST_CC_FUNCS\n"
	   pre
	   (call "INLINE_ADDB_NOSWAP" src dst)
	   post
	   "\n#else\n"
	   pre
	   (assign tmp2.ub dst)
	   (assign ccv (& (~ (^ tmp2.ub src)) 0x80))
	   (ASSIGN_NNZ_BYTE (assign dst (assign tmp.uw (+ dst src))))
	   post
	   (assign ccx (assign ccc (>> tmp.uw 8)))
	   (assign ccv (& ccv (^ tmp.ub tmp2.ub)))
	   "\n#endif\n"))))

(define (ADDB name bit_pattern expand amode src dst native)
  (ADDB  name bit_pattern expand amode src dst native (list) (list)))

; one byte adds are never swapped
(define (ADDB_NOSWAP name bit_pattern expand amode src dst native pre post)
  (ADDB name bit_pattern expand amode src dst native pre post))
(define (ADDB_NOSWAP name bit_pattern expand amode src dst native)
  (ADDB_NOSWAP name bit_pattern expand amode src dst native (list) (list)))


(define (SUBB name bit_pattern expand amode src dst native pre post)
  (defopcode name 
    (list 68000 amode () bit_pattern)
    (list "-----" "-----" expand
	  (native_code native)
	  (list
	   pre
	   (assign dst (- dst src))
	   post))
    (list "CNVXZ" "-----" dont_expand
	  (native_code native)
	  (list
	   "\n#ifdef FAST_CC_FUNCS\n"
	   pre
	   (call "INLINE_SUBB_NOSWAP" src dst)
	   post
	   "\n#else\n"
	   pre
	   (assign tmp2.ub dst)
	   (assign ccx (assign ccc (> (cast "uint8" src) tmp2.ub)))
	   (assign ccv (& (^ tmp2.ub src) 0x80))
	   (ASSIGN_NNZ_BYTE (assign dst (- dst src)))
	   (assign ccv (& ccv (^ dst tmp2.ub)))
	   post
	   "\n#endif\n"))))
(define (SUBB name bit_pattern expand amode src dst native)
  (SUBB name bit_pattern expand amode src dst native (list) (list)))


; one byte subs are never swapped
(define (SUBB_NOSWAP name bit_pattern expand amode src dst native pre post)
  (SUBB name bit_pattern expand amode src dst native pre post))
(define (SUBB_NOSWAP name bit_pattern expand amode src dst native)
  (SUBB_NOSWAP name bit_pattern expand amode src dst native (list) (list)))


(define (REAL_ADDW name bit_pattern expand amode src dst inline native
		   pre post)
  (defopcode name
    (list 68000 amode () bit_pattern)
    (list "-----" "-----" expand
	  (native_code native)
	  (list
	   pre
	   (assign dst (+ dst src))
	   post))
    (list "CNVXZ" "-----" dont_expand
	  (native_code native)
	  (list
	   "\n#ifdef FAST_CC_FUNCS\n"
	   pre
	   inline
	   post
	   "\n#else\n"
	   pre
	   (assign tmp2.uw dst)
	   (assign tmp3.uw (& (~ (^ tmp2.uw src)) 0x8000))
	   (ASSIGN_NNZ_WORD (assign dst (assign tmp.ul (+ tmp2.uw src))))
	   post
	   (assign ccx (assign ccc (>> tmp.ul 16)))
	   "\n#ifdef CCR_ELEMENT_8_BITS\n"
	   (assign ccv (>> (& tmp3.uw (^ tmp.uw tmp2.uw)) 15))
	   "\n#else\n"
	   (assign ccv (& tmp3.uw (^ tmp.uw tmp2.uw)))
	   "\n#endif\n"
	   "\n#endif\n"))))

(define (ADDW name bit_pattern expand amode src dst native pre post)
  (REAL_ADDW name bit_pattern expand amode src dst 
	   (assign dst (call "inline_addw" src dst)) native pre post))
(define (ADDW name bit_pattern expand amode src dst native)
  (REAL_ADDW name bit_pattern expand amode src dst 
	   (assign dst (call "inline_addw" src dst)) native (list) (list)))

(define (ADDW_NOSWAP name bit_pattern expand amode src dst native pre post)
  (REAL_ADDW name bit_pattern expand amode src dst
	   (call "INLINE_ADDW_NOSWAP" src dst) native pre post))
(define (ADDW_NOSWAP name bit_pattern expand amode src dst native)
  (REAL_ADDW name bit_pattern expand amode src dst
	   (call "INLINE_ADDW_NOSWAP" src dst) native (list) (list)))



(define (REAL_SUBW name bit_pattern expand amode src dst inline native
		   pre post)
  (defopcode name 
    (list 68000 amode () bit_pattern)
    (list "-----" "-----" expand
	  (native_code native)
	  (list
	   pre
	   (assign dst (- dst src))
	   post))
    (list "CNVXZ" "-----" dont_expand
	  (native_code native)
	  (list
	   "\n#ifdef FAST_CC_FUNCS\n"
	   pre
	   inline
	   post
	   "\n#else\n"
	   pre
	   (assign tmp2.uw dst)
	   (assign ccx (assign ccc (> (cast "uint16" src) tmp2.uw)))
	   (assign tmp3.uw (& (^ tmp2.uw src) 0x8000))
	   (ASSIGN_NNZ_WORD (assign dst (- dst src)))
	   "\n#ifdef CCR_ELEMENT_8_BITS\n"
	   (assign ccv (>> (& tmp3.uw (^ dst tmp2.uw)) 15))
	   "\n#else\n"
	   (assign ccv (& tmp3.uw (^ dst tmp2.uw)))
	   "\n#endif\n"
	   post
	   "\n#endif\n"))))

(define (SUBW name bit_pattern expand amode src dst native pre post)
  (REAL_SUBW name bit_pattern expand amode src dst 
	   (assign dst (call "inline_subw" src dst)) native pre post))
(define (SUBW name bit_pattern expand amode src dst native)
  (REAL_SUBW name bit_pattern expand amode src dst 
	   (assign dst (call "inline_subw" src dst)) native (list) (list)))

(define (SUBW_NOSWAP name bit_pattern expand amode src dst native pre post)
  (REAL_SUBW name bit_pattern expand amode src dst 
	   (call "INLINE_SUBW_NOSWAP" src dst) native pre post))
(define (SUBW_NOSWAP name bit_pattern expand amode src dst native)
  (REAL_SUBW name bit_pattern expand amode src dst 
	   (call "INLINE_SUBW_NOSWAP" src dst) native (list) (list)))



; This C code computes the C bit for an add, where s, d, r are the msb's
;   of src, dst, and result (respectively).
;  if (s ^ d)
;    return !r;
;  return d;

(define (REAL_ADDL name bit_pattern expand amode src dst inline native
		   pre post)
  (defopcode name
    (list 68000 amode () bit_pattern)
    (list "-----" "-----" expand
	  (native_code native)
	  (list
	   pre
	   (assign dst (+ dst src))
	   post))
    (list "CNVXZ" "-----" dont_expand
	  (native_code native)
	  (list
	   "\n#ifdef FAST_CC_FUNCS\n"
	   pre
	   inline
	   post
	   "\n#else\n"
	   pre
	   (assign tmp2.ul dst)
	   (assign ccv (>> (~ (^ tmp2.ul src)) 31))
	   (ASSIGN_NNZ_LONG (assign dst (assign tmp.ul (+ tmp2.ul src))))
	   post
	   (if ccv   ; Are the high bits of src and dst the same?
	       (assign ccx (assign ccc (SIGN_LONG tmp2.ul)))
	       (assign ccx (assign ccc (SIGN_LONG (~ tmp.ul)))))
	   (assign ccv (& ccv (>> (^ tmp.ul tmp2.ul) 31)))
	   "\n#endif\n"))))

(define (ADDL name bit_pattern expand amode src dst native pre post)
  (REAL_ADDL name bit_pattern expand amode src dst 
	   (assign dst (call "inline_addl" src dst)) native pre post))
(define (ADDL name bit_pattern expand amode src dst native)
  (REAL_ADDL name bit_pattern expand amode src dst 
	   (assign dst (call "inline_addl" src dst)) native (list) (list)))

(define (ADDL_NOSWAP name bit_pattern expand amode src dst native pre post)
  (REAL_ADDL name bit_pattern expand amode src dst 
	   (call "INLINE_ADDL_NOSWAP" src dst) native pre post))
(define (ADDL_NOSWAP name bit_pattern expand amode src dst native)
  (REAL_ADDL name bit_pattern expand amode src dst 
	   (call "INLINE_ADDL_NOSWAP" src dst) native (list) (list)))

(define (REAL_SUBL name bit_pattern expand amode src dst inline native
		   pre post)
  (defopcode name 
    (list 68000 amode () bit_pattern)
    (list "-----" "-----" expand
	  (native_code native)
	  (list
	   pre
	   (assign dst (- dst src))
	   post))
    (list "CNVXZ" "-----" dont_expand
	  (native_code native)
	  (list
	   "\n#ifdef FAST_CC_FUNCS\n"
	   pre
	   inline
	   post
	   "\n#else\n"
	   pre
	   (assign tmp2.ul dst)
	   (assign ccx (assign ccc (> (cast "uint32" src) tmp2.ul)))
	   (assign ccv (>> (^ tmp2.ul src) 31))
	   (ASSIGN_NNZ_LONG (assign dst (- dst src)))
	   (assign ccv (& ccv (>> (^ dst tmp2.ul) 31)))
	   post
	   "\n#endif\n"))))

(define (SUBL name bit_pattern expand amode src dst native pre post)
  (REAL_SUBL name bit_pattern expand amode src dst 
	     (assign dst (call "inline_subl" src dst)) native pre post))
(define (SUBL name bit_pattern expand amode src dst native)
  (REAL_SUBL name bit_pattern expand amode src dst 
	     (assign dst (call "inline_subl" src dst)) native (list) (list)))

(define (SUBL_NOSWAP name bit_pattern expand amode src dst native pre post)
  (REAL_SUBL name bit_pattern expand amode src dst 
	     (call "INLINE_SUBL_NOSWAP" src dst) native pre post))
(define (SUBL_NOSWAP name bit_pattern expand amode src dst native)
  (REAL_SUBL name bit_pattern expand amode src dst 
	     (call "INLINE_SUBL_NOSWAP" src dst) native (list) (list)))



(define (CMPB name bit_pattern expand amode src dst native pre post)
  (defopcode name 
    (list 68000 amode () bit_pattern)
    (list "CNV-Z" "-----" expand
	  (native_code native)
	  (list
	   "\n#ifdef FAST_CC_FUNCS\n"
	   pre
	   (call "inline_cmpb" src dst)
	   post
	   "\n#else\n"
	   pre
	   (assign tmp2.ub dst)
	   (assign tmp3.ub src)
	   (assign ccc (> tmp3.ub tmp2.ub))
	   (assign ccv (& (^ tmp2.ub tmp3.ub) 0x80))
	   (ASSIGN_NNZ_BYTE (assign tmp.ub (- tmp2.ub tmp3.ub)))
	   post
	   (assign ccv (& ccv (^ tmp.ub tmp2.ub)))
	   "\n#endif\n"))))

(define (CMPW name bit_pattern expand amode src dst native pre post)
  (defopcode name 
    (list 68000 amode () bit_pattern)
    (list "CNV-Z" "-----" expand
	  (native_code native)
	  (list
	   "\n#ifdef FAST_CC_FUNCS\n"
	   pre
	   (call "inline_cmpw" src dst)
	   post
	   "\n#else\n"
	   pre
	   (assign tmp2.ul dst)
	   (assign tmp3.ul src)
	   (assign ccc (> tmp3.ul tmp2.ul))
	   (assign tmp4.ul (& (^ tmp2.ul tmp3.ul) 0x8000))
	   (ASSIGN_NNZ_WORD (assign tmp.uw (- tmp2.ul tmp3.ul)))
	   post
	   "\n#ifdef CCR_ELEMENT_8_BITS\n"
	   (assign ccv (>> (& tmp4.ul (^ tmp.uw tmp2.ul)) 15))
	   "\n#else\n"
	   (assign ccv (& tmp4.ul (^ tmp.uw tmp2.ul)))
	   "\n#endif\n"
	   "\n#endif\n"))))


(define (CMPL name bit_pattern expand amode src dst native pre post)
  (defopcode name 
    (list 68000 amode () bit_pattern)
    (list "CNV-Z" "-----" expand
	  (native_code native)
	  (list
	   "\n#ifdef FAST_CC_FUNCS\n"
	   pre
	   (call "inline_cmpl" src dst)
	   post
	   "\n#else\n"
	   pre
	   (assign tmp2.ul dst)
	   (assign tmp3.ul src)
	   (assign ccc (> tmp3.ul tmp2.ul))
	   (assign tmp4.ul (& (^ tmp2.ul tmp3.ul) 0x80000000))
	   (ASSIGN_NNZ_LONG (assign tmp.ul (- tmp2.ul tmp3.ul)))
	   post
	   "\n#ifdef CCR_ELEMENT_8_BITS\n"
	   (assign ccv (>> (& tmp4.ul (^ tmp.ul tmp2.ul)) 31))
	   "\n#else\n"
	   (assign ccv (& tmp4.ul (^ tmp.ul tmp2.ul)))
	   "\n#endif\n"
	   "\n#endif\n"))))


; Handy macros if you don't care about pre and post.
(define (CMPB name bit_pattern expand amode src dst native)
  (CMPB name bit_pattern expand amode src dst native (list) (list)))
(define (CMPW name bit_pattern expand amode src dst native)
  (CMPW name bit_pattern expand amode src dst native (list) (list)))
(define (CMPL name bit_pattern expand amode src dst native)
  (CMPL name bit_pattern expand amode src dst native (list) (list)))


(define (ADD_SUB_CMP name real_macro base_bits size amode ea_dest_p native
		     areg_cmp_p)
  (real_macro name
	      (list base_bits "ddd"
		    (if (or ea_dest_p (and areg_cmp_p (= size LONG)))
			"1"
			"0")
		    (if areg_cmp_p
			"11"
			(if (= size BYTE)
			    "00"
			    (if (= size WORD)
				"01"
				"10")))
		    (switch amode
			    ((+ IMM     0)
			     (if (= size BYTE)
				 "11110000000000cccccccc"
				 (if (= size WORD)
				     "111100cccccccccccccccc"
				     "111100cccccccccccccccccccccccccccccccc")))
			    ((+ ABSW    0) "111000bbbbbbbbbbbbbbbb")
			    ((+ ABSL    0)
			     "111001bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb")
			    ((+ REG     0) "000ddd")
			    ((+ GREG    0) "00dddd")
			    ((+ AREG    0) "001ddd")
			    ((+ IND     0) "010ddd")
			    ((+ PREDEC  0) "100ddd")
			    ((+ POSTINC 0) "011ddd")
			    ((+ INDOFF  0) "101sssaaaaaaaaaaaaaaaa")))
	      dont_expand amode_implicit
	      (if ea_dest_p
		  (if (= size BYTE)
		      $1.dub
		      (if areg_cmp_p
			  $1.asl
			  (if (= size WORD)
			      $1.duw
			      $1.dul)))
		  (src_val amode (if areg_cmp_p AREG REG) size (not areg_cmp_p)))
	      (if ea_dest_p
		  (ea_val amode size (not areg_cmp_p))
		  (if (= size BYTE)
		      $1.dub
		      (if areg_cmp_p
			  $1.asl
			  (if (= size WORD)
			      $1.duw
			      $1.dul))))
	      native
	      (if (= amode PREDEC)
		  (if (and (= size BYTE) (= $2.ul 7))
		      (assign a7.ul (- a7.ul 2))
		      (assign $2.aul (- $2.aul size)))
		  (list))
	      (if (= amode POSTINC)
		  (if (and (= size BYTE) (= $2.ul 7))
		      (assign a7.ul (+ a7.ul 2))
		      (assign $2.aul (+ $2.aul size)))
		  (list))))


(ADD_SUB_CMP addb_imm_reg ADDB_NOSWAP "1101" BYTE IMM false
	     "xlate_addb_imm_reg_1_0" false)
(ADD_SUB_CMP addw_imm_reg ADDW_NOSWAP "1101" WORD IMM false
	     "xlate_addw_imm_reg_1_0" false)
(ADD_SUB_CMP addl_imm_reg ADDL_NOSWAP "1101" LONG IMM false
	     "xlate_addl_imm_reg_1_0" false)

(ADD_SUB_CMP addb_reg_reg ADDB_NOSWAP "1101" BYTE REG false
	     "xlate_addb_reg_reg_1_0" false)
(ADD_SUB_CMP addw_reg_reg ADDW_NOSWAP "1101" WORD REG false
	     "xlate_addw_reg_reg_1_0" false)
(ADD_SUB_CMP addl_reg_reg ADDL_NOSWAP "1101" LONG REG false
	     "xlate_addl_reg_reg_1_0" false)

(ADD_SUB_CMP addw_areg_reg ADDW_NOSWAP "1101" WORD AREG false
	     "xlate_addw_areg_reg_1_0" false)
(ADD_SUB_CMP addl_areg_reg ADDL_NOSWAP "1101" LONG AREG false
	     "xlate_addl_areg_reg_1_0" false)

(ADD_SUB_CMP addb_absw_reg ADDB "1101" BYTE ABSW false
	     "xlate_addb_abs_reg_1_0" false)
(ADD_SUB_CMP addw_absw_reg ADDW "1101" WORD ABSW false
	     "xlate_addw_abs_reg_1_0" false)
(ADD_SUB_CMP addl_absw_reg ADDL "1101" LONG ABSW false
	     "xlate_addl_abs_reg_1_0" false)

(ADD_SUB_CMP addb_absl_reg ADDB "1101" BYTE ABSL false
	     "xlate_addb_abs_reg_1_0" false)
(ADD_SUB_CMP addw_absl_reg ADDW "1101" WORD ABSL false
	     "xlate_addw_abs_reg_1_0" false)
(ADD_SUB_CMP addl_absl_reg ADDL "1101" LONG ABSL false
	     "xlate_addl_abs_reg_1_0" false)

(ADD_SUB_CMP addb_ind_reg ADDB "1101" BYTE IND false
	     "xlate_addb_ind_reg_1_0" false)
(ADD_SUB_CMP addw_ind_reg ADDW "1101" WORD IND false
	     "xlate_addw_ind_reg_1_0" false)
(ADD_SUB_CMP addl_ind_reg ADDL "1101" LONG IND false
	     "xlate_addl_ind_reg_1_0" false)

(ADD_SUB_CMP addb_predec_reg ADDB "1101" BYTE PREDEC false
	     "xlate_addb_predec_reg_1_0" false)
(ADD_SUB_CMP addw_predec_reg ADDW "1101" WORD PREDEC false
	     "xlate_addw_predec_reg_1_0" false)
(ADD_SUB_CMP addl_predec_reg ADDL "1101" LONG PREDEC false
	     "xlate_addl_predec_reg_1_0" false)

(ADD_SUB_CMP addb_postinc_reg ADDB "1101" BYTE POSTINC false
	     "xlate_addb_postinc_reg_1_0" false)
(ADD_SUB_CMP addw_postinc_reg ADDW "1101" WORD POSTINC false
	     "xlate_addw_postinc_reg_1_0" false)
(ADD_SUB_CMP addl_postinc_reg ADDL "1101" LONG POSTINC false
	     "xlate_addl_postinc_reg_1_0" false)

(ADD_SUB_CMP addb_indoff_reg ADDB "1101" BYTE INDOFF false
	     "xlate_addb_indoff_reg_2_1_0" false)
(ADD_SUB_CMP addw_indoff_reg ADDW "1101" WORD INDOFF false
	     "xlate_addw_indoff_reg_2_1_0" false)
(ADD_SUB_CMP addl_indoff_reg ADDL "1101" LONG INDOFF false
	     "xlate_addl_indoff_reg_2_1_0" false)

(ADD_SUB_CMP addb_reg_absw ADDB "1101" BYTE ABSW true
	     "xlate_addb_reg_abs_0_1" false)
(ADD_SUB_CMP addw_reg_absw ADDW "1101" WORD ABSW true
	     "xlate_addw_reg_abs_0_1" false)
(ADD_SUB_CMP addl_reg_absw ADDL "1101" LONG ABSW true
	     "xlate_addl_reg_abs_0_1" false)

(ADD_SUB_CMP addb_reg_absl ADDB "1101" BYTE ABSL true
	     "xlate_addb_reg_abs_0_1" false)
(ADD_SUB_CMP addw_reg_absl ADDW "1101" WORD ABSL true
	     "xlate_addw_reg_abs_0_1" false)
(ADD_SUB_CMP addl_reg_absl ADDL "1101" LONG ABSL true
	     "xlate_addl_reg_abs_0_1" false)

(ADD_SUB_CMP addb_reg_ind ADDB "1101" BYTE IND true
	     "xlate_addb_reg_ind_0_1" false)
(ADD_SUB_CMP addw_reg_ind ADDW "1101" WORD IND true
	     "xlate_addw_reg_ind_0_1" false)
(ADD_SUB_CMP addl_reg_ind ADDL "1101" LONG IND true
	     "xlate_addl_reg_ind_0_1" false)

(ADD_SUB_CMP addb_reg_predec ADDB "1101" BYTE PREDEC true
	     "xlate_addb_reg_predec_0_1" false)
(ADD_SUB_CMP addw_reg_predec ADDW "1101" WORD PREDEC true
	     "xlate_addw_reg_predec_0_1" false)
(ADD_SUB_CMP addl_reg_predec ADDL "1101" LONG PREDEC true
	     "xlate_addl_reg_predec_0_1" false)

(ADD_SUB_CMP addb_reg_postinc ADDB "1101" BYTE POSTINC true
	     "xlate_addb_reg_postinc_0_1" false)
(ADD_SUB_CMP addw_reg_postinc ADDW "1101" WORD POSTINC true
	     "xlate_addw_reg_postinc_0_1" false)
(ADD_SUB_CMP addl_reg_postinc ADDL "1101" LONG POSTINC true
	     "xlate_addl_reg_postinc_0_1" false)

(ADD_SUB_CMP addb_reg_indoff ADDB "1101" BYTE INDOFF true
	     "xlate_addb_reg_indoff_0_2_1" false)
(ADD_SUB_CMP addw_reg_indoff ADDW "1101" WORD INDOFF true
	     "xlate_addw_reg_indoff_0_2_1" false)
(ADD_SUB_CMP addl_reg_indoff ADDL "1101" LONG INDOFF true
	     "xlate_addl_reg_indoff_0_2_1" false)



(ADD_SUB_CMP subb_imm_reg SUBB_NOSWAP "1001" BYTE IMM false
	     "xlate_subb_imm_reg_1_0" false)
(ADD_SUB_CMP subw_imm_reg SUBW_NOSWAP "1001" WORD IMM false
	     "xlate_subw_imm_reg_1_0" false)
(ADD_SUB_CMP subl_imm_reg SUBL_NOSWAP "1001" LONG IMM false
	     "xlate_subl_imm_reg_1_0" false)

(ADD_SUB_CMP subb_reg_reg SUBB_NOSWAP "1001" BYTE REG false
	     "xlate_subb_reg_reg_1_0" false)
(ADD_SUB_CMP subw_reg_reg SUBW_NOSWAP "1001" WORD REG false
	     "xlate_subw_reg_reg_1_0" false)
(ADD_SUB_CMP subl_reg_reg SUBL_NOSWAP "1001" LONG REG false
	     "xlate_subl_reg_reg_1_0" false)

(ADD_SUB_CMP subw_areg_reg SUBW_NOSWAP "1001" WORD AREG false
	     "xlate_subw_areg_reg_1_0" false)
(ADD_SUB_CMP subl_areg_reg SUBL_NOSWAP "1001" LONG AREG false
	     "xlate_subl_areg_reg_1_0" false)

(ADD_SUB_CMP subb_absw_reg SUBB "1001" BYTE ABSW false
	     "xlate_subb_abs_reg_1_0" false)
(ADD_SUB_CMP subw_absw_reg SUBW "1001" WORD ABSW false
	     "xlate_subw_abs_reg_1_0" false)
(ADD_SUB_CMP subl_absw_reg SUBL "1001" LONG ABSW false
	     "xlate_subl_abs_reg_1_0" false)

(ADD_SUB_CMP subb_absl_reg SUBB "1001" BYTE ABSL false
	     "xlate_subb_abs_reg_1_0" false)
(ADD_SUB_CMP subw_absl_reg SUBW "1001" WORD ABSL false
	     "xlate_subw_abs_reg_1_0" false)
(ADD_SUB_CMP subl_absl_reg SUBL "1001" LONG ABSL false
	     "xlate_subl_abs_reg_1_0" false)

(ADD_SUB_CMP subb_ind_reg SUBB "1001" BYTE IND false
	     "xlate_subb_ind_reg_1_0" false)
(ADD_SUB_CMP subw_ind_reg SUBW "1001" WORD IND false
	     "xlate_subw_ind_reg_1_0" false)
(ADD_SUB_CMP subl_ind_reg SUBL "1001" LONG IND false
	     "xlate_subl_ind_reg_1_0" false)

(ADD_SUB_CMP subb_predec_reg SUBB "1001" BYTE PREDEC false
	     "xlate_subb_predec_reg_1_0" false)
(ADD_SUB_CMP subw_predec_reg SUBW "1001" WORD PREDEC false
	     "xlate_subw_predec_reg_1_0" false)
(ADD_SUB_CMP subl_predec_reg SUBL "1001" LONG PREDEC false
	     "xlate_subl_predec_reg_1_0" false)

(ADD_SUB_CMP subb_postinc_reg SUBB "1001" BYTE POSTINC false
	     "xlate_subb_postinc_reg_1_0" false)
(ADD_SUB_CMP subw_postinc_reg SUBW "1001" WORD POSTINC false
	     "xlate_subw_postinc_reg_1_0" false)
(ADD_SUB_CMP subl_postinc_reg SUBL "1001" LONG POSTINC false
	     "xlate_subl_postinc_reg_1_0" false)

(ADD_SUB_CMP subb_indoff_reg SUBB "1001" BYTE INDOFF false
	     "xlate_subb_indoff_reg_2_1_0" false)
(ADD_SUB_CMP subw_indoff_reg SUBW "1001" WORD INDOFF false
	     "xlate_subw_indoff_reg_2_1_0" false)
(ADD_SUB_CMP subl_indoff_reg SUBL "1001" LONG INDOFF false
	     "xlate_subl_indoff_reg_2_1_0" false)

(ADD_SUB_CMP subb_reg_absw SUBB "1001" BYTE ABSW true
	     "xlate_subb_reg_abs_0_1" false)
(ADD_SUB_CMP subw_reg_absw SUBW "1001" WORD ABSW true
	     "xlate_subw_reg_abs_0_1" false)
(ADD_SUB_CMP subl_reg_absw SUBL "1001" LONG ABSW true
	     "xlate_subl_reg_abs_0_1" false)

(ADD_SUB_CMP subb_reg_absl SUBB "1001" BYTE ABSL true
	     "xlate_subb_reg_abs_0_1" false)
(ADD_SUB_CMP subw_reg_absl SUBW "1001" WORD ABSL true
	     "xlate_subw_reg_abs_0_1" false)
(ADD_SUB_CMP subl_reg_absl SUBL "1001" LONG ABSL true
	     "xlate_subl_reg_abs_0_1" false)

(ADD_SUB_CMP subb_reg_ind SUBB "1001" BYTE IND true
	     "xlate_subb_reg_ind_0_1" false)
(ADD_SUB_CMP subw_reg_ind SUBW "1001" WORD IND true
	     "xlate_subw_reg_ind_0_1" false)
(ADD_SUB_CMP subl_reg_ind SUBL "1001" LONG IND true
	     "xlate_subl_reg_ind_0_1" false)

(ADD_SUB_CMP subb_reg_predec SUBB "1001" BYTE PREDEC true
	     "xlate_subb_reg_predec_0_1" false)
(ADD_SUB_CMP subw_reg_predec SUBW "1001" WORD PREDEC true
	     "xlate_subw_reg_predec_0_1" false)
(ADD_SUB_CMP subl_reg_predec SUBL "1001" LONG PREDEC true
	     "xlate_subl_reg_predec_0_1" false)

(ADD_SUB_CMP subb_reg_postinc SUBB "1001" BYTE POSTINC true
	     "xlate_subb_reg_postinc_0_1" false)
(ADD_SUB_CMP subw_reg_postinc SUBW "1001" WORD POSTINC true
	     "xlate_subw_reg_postinc_0_1" false)
(ADD_SUB_CMP subl_reg_postinc SUBL "1001" LONG POSTINC true
	     "xlate_subl_reg_postinc_0_1" false)

(ADD_SUB_CMP subb_reg_indoff SUBB "1001" BYTE INDOFF true
	     "xlate_subb_reg_indoff_0_2_1" false)
(ADD_SUB_CMP subw_reg_indoff SUBW "1001" WORD INDOFF true
	     "xlate_subw_reg_indoff_0_2_1" false)
(ADD_SUB_CMP subl_reg_indoff SUBL "1001" LONG INDOFF true
	     "xlate_subl_reg_indoff_0_2_1" false)



(ADD_SUB_CMP cmpb_imm_reg CMPB "1011" BYTE IMM false
	     "xlate_cmpb_imm_reg_1_0" false)
(ADD_SUB_CMP cmpw_imm_reg CMPW "1011" WORD IMM false
	     "xlate_cmpw_imm_reg_1_0" false)
(ADD_SUB_CMP cmpl_imm_reg CMPL "1011" LONG IMM false
	     "xlate_cmpl_imm_reg_1_0" false)

(ADD_SUB_CMP cmpb_reg_reg CMPB "1011" BYTE REG false
	     "xlate_cmpb_reg_reg_1_0" false)
(ADD_SUB_CMP cmpw_reg_reg CMPW "1011" WORD REG false
	     "xlate_cmpw_reg_reg_1_0" false)
(ADD_SUB_CMP cmpl_reg_reg CMPL "1011" LONG REG false
	     "xlate_cmpl_reg_reg_1_0" false)

(ADD_SUB_CMP cmpw_areg_reg CMPW "1011" WORD AREG false
	     "xlate_cmpw_areg_reg_1_0" false)
(ADD_SUB_CMP cmpl_areg_reg CMPL "1011" LONG AREG false
	     "xlate_cmpl_areg_reg_1_0" false)

(ADD_SUB_CMP cmpb_absw_reg CMPB "1011" BYTE ABSW false
	     "xlate_cmpb_abs_reg_1_0" false)
(ADD_SUB_CMP cmpw_absw_reg CMPW "1011" WORD ABSW false
	     "xlate_cmpw_abs_reg_1_0" false)
(ADD_SUB_CMP cmpl_absw_reg CMPL "1011" LONG ABSW false
	     "xlate_cmpl_abs_reg_1_0" false)

(ADD_SUB_CMP cmpb_absl_reg CMPB "1011" BYTE ABSL false
	     "xlate_cmpb_abs_reg_1_0" false)
(ADD_SUB_CMP cmpw_absl_reg CMPW "1011" WORD ABSL false
	     "xlate_cmpw_abs_reg_1_0" false)
(ADD_SUB_CMP cmpl_absl_reg CMPL "1011" LONG ABSL false
	     "xlate_cmpl_abs_reg_1_0" false)

(ADD_SUB_CMP cmpb_ind_reg CMPB "1011" BYTE IND false
	     "xlate_cmpb_ind_reg_1_0" false)
(ADD_SUB_CMP cmpw_ind_reg CMPW "1011" WORD IND false
	     "xlate_cmpw_ind_reg_1_0" false)
(ADD_SUB_CMP cmpl_ind_reg CMPL "1011" LONG IND false
	     "xlate_cmpl_ind_reg_1_0" false)

(ADD_SUB_CMP cmpb_predec_reg CMPB "1011" BYTE PREDEC false
	     "xlate_cmpb_predec_reg_1_0" false)
(ADD_SUB_CMP cmpw_predec_reg CMPW "1011" WORD PREDEC false
	     "xlate_cmpw_predec_reg_1_0" false)
(ADD_SUB_CMP cmpl_predec_reg CMPL "1011" LONG PREDEC false
	     "xlate_cmpl_predec_reg_1_0" false)

(ADD_SUB_CMP cmpb_postinc_reg CMPB "1011" BYTE POSTINC false
	     "xlate_cmpb_postinc_reg_1_0" false)
(ADD_SUB_CMP cmpw_postinc_reg CMPW "1011" WORD POSTINC false
	     "xlate_cmpw_postinc_reg_1_0" false)
(ADD_SUB_CMP cmpl_postinc_reg CMPL "1011" LONG POSTINC false
	     "xlate_cmpl_postinc_reg_1_0" false)

(ADD_SUB_CMP cmpb_indoff_reg CMPB "1011" BYTE INDOFF false
	     "xlate_cmpb_indoff_reg_2_1_0" false)
(ADD_SUB_CMP cmpw_indoff_reg CMPW "1011" WORD INDOFF false
	     "xlate_cmpw_indoff_reg_2_1_0" false)
(ADD_SUB_CMP cmpl_indoff_reg CMPL "1011" LONG INDOFF false
	     "xlate_cmpl_indoff_reg_2_1_0" false)


(ADD_SUB_CMP cmpw_imm_areg CMPL "1011" WORD IMM false
	     "xlate_cmpl_imm_areg_1_0" true)  ; use long version only
(ADD_SUB_CMP cmpl_imm_areg CMPL "1011" LONG IMM false
	     "xlate_cmpl_imm_areg_1_0" true)

(ADD_SUB_CMP cmpw_reg_areg CMPL "1011" WORD REG false
	     "xlate_cmpw_reg_areg_1_0" true)
(ADD_SUB_CMP cmpl_reg_areg CMPL "1011" LONG REG false
	     "xlate_cmpl_reg_areg_1_0" true)

(ADD_SUB_CMP cmpw_areg_areg CMPL "1011" WORD AREG false
	     "xlate_cmpw_areg_areg_1_0" true)
(ADD_SUB_CMP cmpl_areg_areg CMPL "1011" LONG AREG false
	     "xlate_cmpl_areg_areg_1_0" true)

(ADD_SUB_CMP cmpw_absw_areg CMPL "1011" WORD ABSW false
	     "xlate_cmpw_abs_areg_1_0" true)
(ADD_SUB_CMP cmpl_absw_areg CMPL "1011" LONG ABSW false
	     "xlate_cmpl_abs_areg_1_0" true)

(ADD_SUB_CMP cmpw_absl_areg CMPL "1011" WORD ABSL false
	     "xlate_cmpw_abs_areg_1_0" true)
(ADD_SUB_CMP cmpl_absl_areg CMPL "1011" LONG ABSL false
	     "xlate_cmpl_abs_areg_1_0" true)

(ADD_SUB_CMP cmpw_ind_areg CMPL "1011" WORD IND false
	     "xlate_cmpw_ind_areg_1_0" true)
(ADD_SUB_CMP cmpl_ind_areg CMPL "1011" LONG IND false
	     "xlate_cmpl_ind_areg_1_0" true)

(ADD_SUB_CMP cmpw_predec_areg CMPL "1011" WORD PREDEC false
	     "xlate_cmpw_predec_areg_1_0" true)
(ADD_SUB_CMP cmpl_predec_areg CMPL "1011" LONG PREDEC false
	     "xlate_cmpl_predec_areg_1_0" true)

(ADD_SUB_CMP cmpw_postinc_areg CMPL "1011" WORD POSTINC false
	     "xlate_cmpw_postinc_areg_1_0" true)
(ADD_SUB_CMP cmpl_postinc_areg CMPL "1011" LONG POSTINC false
	     "xlate_cmpl_postinc_areg_1_0" true)

(ADD_SUB_CMP cmpw_indoff_areg CMPL "1011" WORD INDOFF false
	     "xlate_cmpw_indoff_areg_2_1_0" true)
(ADD_SUB_CMP cmpl_indoff_areg CMPL "1011" LONG INDOFF false
	     "xlate_cmpl_indoff_areg_2_1_0" true)


; addb and subb
(ADDB addb_dest_reg (list "1101ddd000mmmmmm") dont_expand
      (intersect amode_all_combinations (not "xxxxxxxxxx001xxx"))  ; no an
      $2.mub $1.dub "none")
(ADDB addb_dest_ea  (list "1101ddd100mmmmmm") dont_expand
      amode_alterable_memory $1.dub $2.mub "none")

(SUBB subb_dest_reg (list "1001ddd000mmmmmm") dont_expand
      (intersect amode_all_combinations (not "xxxxxxxxxx001xxx"))  ; no an
      $2.mub $1.dub "none")
(SUBB subb_dest_ea  (list "1001ddd100mmmmmm") dont_expand
      amode_alterable_memory $1.dub $2.mub "none")

; addw and subw
(ADDW_NOSWAP addw_dest_reg (list "1101ddd001mmmmmm") dont_expand
	     amode_all_combinations $2.muw $1.duw "none")
(ADDW addw_dest_reg (list "1101ddd001mmmmmm") dont_expand
      amode_all_combinations $2.muw $1.duw "none")
(ADDW addw_dest_ea  (list "1101ddd101mmmmmm") dont_expand
      amode_alterable_memory $1.duw $2.muw "none")
(SUBW_NOSWAP subw_dest_reg (list "1001ddd001mmmmmm") dont_expand
	     amode_all_combinations $2.muw $1.duw
	     "none")
(SUBW subw_dest_reg (list "1001ddd001mmmmmm") dont_expand
      amode_all_combinations $2.muw $1.duw "none")
(SUBW subw_dest_ea  (list "1001ddd101mmmmmm") dont_expand
      amode_alterable_memory $1.duw $2.muw "none")



; addl and subl
(ADDL_NOSWAP addl_dest_reg (list "1101ddd010mmmmmm") dont_expand
	     amode_all_combinations $2.mul $1.dul
	     "none")
(ADDL addl_dest_reg (list "1101ddd010mmmmmm") dont_expand
      amode_all_combinations $2.mul $1.dul "none")
(ADDL addl_dest_ea  (list "1101ddd110mmmmmm") dont_expand
      amode_alterable_memory $1.dul $2.mul "none")
(SUBL_NOSWAP subl_reg_reg (list "1001ddd01000mmmm") dont_expand
	     amode_reg $2.gul $1.dul
	     "none")
(SUBL_NOSWAP subl_dest_reg (list "1001ddd010mmmmmm") dont_expand
	     amode_all_combinations $2.mul $1.dul
	     "none")
(SUBL subl_dest_reg (list "1001ddd010mmmmmm") dont_expand
      amode_all_combinations $2.mul $1.dul "none")
(SUBL subl_dest_ea  (list "1001ddd110mmmmmm") dont_expand
      amode_alterable_memory $1.dul $2.mul "none")



(define (ADDA_SUBA name op base_bits size amode native)
  (defopcode name
    (list 68000 amode_implicit ()
	  (list base_bits "ddd"
		(if (= size WORD)
		    "011"
		    "111")
		(switch amode
			((+ IMM     0)
			 (if (= size BYTE)
			     "11110000000000cccccccc"
			     (if (= size WORD)
				 "111100cccccccccccccccc"
				 "111100cccccccccccccccccccccccccccccccc")))
			((+ ABSW    0) "111000bbbbbbbbbbbbbbbb")
			((+ ABSL    0)
			 "111001bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb")
			((+ REG     0) "000ddd")
			((+ GREG    0) "00dddd")
			((+ AREG    0) "001ddd")
			((+ IND     0) "010ddd")
			((+ PREDEC  0) "100ddd")
			((+ POSTINC 0) "011ddd")
			((+ INDOFF  0) "101sssaaaaaaaaaaaaaaaa"))))
    (list "-----" "-----" dont_expand
	  (native_code native)
	  (list
	   (if (= amode PREDEC)
	       (if (and (= size BYTE) (= $2.ul 7))
		   (assign a7.ul (- a7.ul 2))
		   (assign $2.aul (- $2.aul size))))
	   (assign $1.asl (op $1.asl (src_val amode AREG size false)))
	   (if (= amode POSTINC)
	       (if (and (= size BYTE) (= $2.ul 7))
		   (assign a7.ul (+ a7.ul 2))
		   (assign $2.aul (+ $2.aul size))))))))

; addaw, addal, subaw, subal

(ADDA_SUBA addaw_reg_areg + "1101" WORD REG "xlate_addw_reg_areg_1_0")
(ADDA_SUBA addal_reg_areg + "1101" LONG REG "xlate_addl_reg_areg_1_0")

(ADDA_SUBA addaw_areg_areg + "1101" WORD AREG "xlate_addw_areg_areg_1_0")
(ADDA_SUBA addal_areg_areg + "1101" LONG AREG "xlate_addl_areg_areg_1_0")

(ADDA_SUBA addaw_ind_areg + "1101" WORD IND "xlate_addw_ind_areg_1_0")
(ADDA_SUBA addal_ind_areg + "1101" LONG IND "xlate_addl_ind_areg_1_0")

(ADDA_SUBA addaw_indoff_areg + "1101" WORD INDOFF
	   "xlate_addw_indoff_areg_2_1_0")
(ADDA_SUBA addal_indoff_areg + "1101" LONG INDOFF
	   "xlate_addl_indoff_areg_2_1_0")

(ADDA_SUBA addaw_postinc_areg + "1101" WORD POSTINC
	   "xlate_addw_postinc_areg_1_0")
(ADDA_SUBA addal_postinc_areg + "1101" LONG POSTINC
	   "xlate_addl_postinc_areg_1_0")

(ADDA_SUBA addaw_predec_areg + "1101" WORD PREDEC "xlate_addw_predec_areg_1_0")
(ADDA_SUBA addal_predec_areg + "1101" LONG PREDEC "xlate_addl_predec_areg_1_0")


(ADDA_SUBA subaw_reg_areg - "1001" WORD REG "xlate_subw_reg_areg_1_0")
(ADDA_SUBA subal_reg_areg - "1001" LONG REG "xlate_subl_reg_areg_1_0")

(ADDA_SUBA subaw_areg_areg - "1001" WORD AREG "xlate_subw_areg_areg_1_0")
(ADDA_SUBA subal_areg_areg - "1001" LONG AREG "xlate_subl_areg_areg_1_0")

(ADDA_SUBA subaw_ind_areg - "1001" WORD IND "xlate_subw_ind_areg_1_0")
(ADDA_SUBA subal_ind_areg - "1001" LONG IND "xlate_subl_ind_areg_1_0")

(ADDA_SUBA subaw_indoff_areg - "1001" WORD INDOFF
	   "xlate_subw_indoff_areg_2_1_0")
(ADDA_SUBA subal_indoff_areg - "1001" LONG INDOFF
	   "xlate_subl_indoff_areg_2_1_0")

(ADDA_SUBA subaw_postinc_areg - "1001" WORD POSTINC
	   "xlate_subw_postinc_areg_1_0")
(ADDA_SUBA subal_postinc_areg - "1001" LONG POSTINC
	   "xlate_subl_postinc_areg_1_0")

(ADDA_SUBA subaw_predec_areg - "1001" WORD PREDEC "xlate_subw_predec_areg_1_0")
(ADDA_SUBA subal_predec_areg - "1001" LONG PREDEC "xlate_subl_predec_areg_1_0")


(defopcode addaw
  (list 68000 amode_all_combinations () (list "1101aaa011mmmmmm"))
  (list "-----" "-----" dont_expand
	(assign $1.asl (+ $1.asl $2.msw))))

(defopcode addal
  (list 68000 amode_all_combinations () (list "1101aaa111mmmmmm"))
  (list "-----" "-----" dont_expand
	(assign $1.aul (+ $1.aul $2.mul))))

(defopcode subaw
  (list 68000 amode_all_combinations () (list "1001aaa011mmmmmm"))
  (list "-----" "-----" dont_expand
	(assign $1.asl (- $1.asl $2.msw))))

(defopcode subal
  (list 68000 amode_all_combinations () (list "1001aaa111mmmmmm"))
  (list "-----" "-----" dont_expand
	(assign $1.aul (- $1.aul $2.mul))))


(define (ADD_SUB_CMP_IMM_EA name main_op base_bits size amode native)
  (main_op name
	     (list base_bits
		   (if (= size BYTE)
		       "00"
		       (if (= size WORD)
			   "01"
			   "10"))
		   (switch amode
			   ((+ ABSW    0) "111000")
			   ((+ ABSL    0) "111001")
			   ((+ REG     0) "000ddd")  ; no areg
			   ((+ GREG    0) "00dddd")
			   ((+ AREG    0) "001ddd")
			   ((+ IND     0) "010ddd")
			   ((+ PREDEC  0) "100ddd")
			   ((+ POSTINC 0) "011ddd")
			   ((+ INDOFF  0) "101sss"))
		   (if (= size BYTE)
		       "00000000cccccccc"
		       (if (= size WORD)
			   "cccccccccccccccc"
			   "cccccccccccccccccccccccccccccccc"))
		   (switch amode
			   ((+ ABSW    0) "bbbbbbbbbbbbbbbb")
			   ((+ ABSL    0) "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb")
			   ((+ INDOFF  0) "aaaaaaaaaaaaaaaa")
			   (default "")))
	     dont_expand amode_implicit
	     (if (no_reg_op_p amode)
		 (if (= size BYTE)
		     $1.ub
		     (if (= size WORD)
			 $1.uw
			 $1.ul))
		 (if (= size BYTE)
		     $2.ub
		     (if (= size WORD)
			 $2.uw
			 $2.ul)))
	     (dst_val IMM amode size)
	     native
	     (if (= amode PREDEC)
		 (if (and (= size BYTE) (= $1.ul 7))
		     (assign a7.ul (- a7.ul 2))
		     (assign $1.aul (- $1.aul size)))
		 (list))
	     (if (= amode POSTINC)
		 (if (and (= size BYTE) (= $1.ul 7))
		     (assign a7.ul (+ a7.ul 2))
		     (assign $1.aul (+ $1.aul size)))
		 (list))))

(ADD_SUB_CMP_IMM_EA addib_imm_reg ADDB "00000110" BYTE REG
		    "xlate_addb_imm_reg_1_0")
(ADD_SUB_CMP_IMM_EA addiw_imm_reg ADDW "00000110" WORD REG
		    "xlate_addw_imm_reg_1_0")
(ADD_SUB_CMP_IMM_EA addil_imm_reg ADDL "00000110" LONG REG
		    "xlate_addl_imm_reg_1_0")

(ADD_SUB_CMP_IMM_EA addib_imm_absw ADDB "00000110" BYTE ABSW
		    "xlate_addb_imm_abs_0_1")
(ADD_SUB_CMP_IMM_EA addiw_imm_absw ADDW "00000110" WORD ABSW
		    "xlate_addw_imm_abs_0_1")
(ADD_SUB_CMP_IMM_EA addil_imm_absw ADDL "00000110" LONG ABSW
		    "xlate_addl_imm_abs_0_1")

(ADD_SUB_CMP_IMM_EA addib_imm_absl ADDB "00000110" BYTE ABSL
		    "xlate_addb_imm_abs_0_1")
(ADD_SUB_CMP_IMM_EA addiw_imm_absl ADDW "00000110" WORD ABSL
		    "xlate_addw_imm_abs_0_1")
(ADD_SUB_CMP_IMM_EA addil_imm_absl ADDL "00000110" LONG ABSL
		    "xlate_addl_imm_abs_0_1")

(ADD_SUB_CMP_IMM_EA addib_imm_ind ADDB "00000110" BYTE IND
		    "xlate_addb_imm_ind_1_0")
(ADD_SUB_CMP_IMM_EA addiw_imm_ind ADDW "00000110" WORD IND
		    "xlate_addw_imm_ind_1_0")
(ADD_SUB_CMP_IMM_EA addil_imm_ind ADDL "00000110" LONG IND
		    "xlate_addl_imm_ind_1_0")

(ADD_SUB_CMP_IMM_EA addib_imm_indoff ADDB "00000110" BYTE INDOFF
		    "xlate_addb_imm_indoff_1_2_0")
(ADD_SUB_CMP_IMM_EA addiw_imm_indoff ADDW "00000110" WORD INDOFF
		    "xlate_addw_imm_indoff_1_2_0")
(ADD_SUB_CMP_IMM_EA addil_imm_indoff ADDL "00000110" LONG INDOFF
		    "xlate_addl_imm_indoff_1_2_0")

(ADD_SUB_CMP_IMM_EA addib_imm_predec ADDB "00000110" BYTE PREDEC
		    "xlate_addb_imm_predec_1_0")
(ADD_SUB_CMP_IMM_EA addiw_imm_predec ADDW "00000110" WORD PREDEC
		    "xlate_addw_imm_predec_1_0")
(ADD_SUB_CMP_IMM_EA addil_imm_predec ADDL "00000110" LONG PREDEC
		    "xlate_addl_imm_predec_1_0")

(ADD_SUB_CMP_IMM_EA addib_imm_postinc ADDB "00000110" BYTE POSTINC
		    "xlate_addb_imm_postinc_1_0")
(ADD_SUB_CMP_IMM_EA addiw_imm_postinc ADDW "00000110" WORD POSTINC
		    "xlate_addw_imm_postinc_1_0")
(ADD_SUB_CMP_IMM_EA addil_imm_postinc ADDL "00000110" LONG POSTINC
		    "xlate_addl_imm_postinc_1_0")



(ADD_SUB_CMP_IMM_EA subib_imm_reg SUBB "00000100" BYTE REG
		    "xlate_subb_imm_reg_1_0")
(ADD_SUB_CMP_IMM_EA subiw_imm_reg SUBW "00000100" WORD REG
		    "xlate_subw_imm_reg_1_0")
(ADD_SUB_CMP_IMM_EA subil_imm_reg SUBL "00000100" LONG REG
		    "xlate_subl_imm_reg_1_0")

(ADD_SUB_CMP_IMM_EA subib_imm_absw SUBB "00000100" BYTE ABSW
		    "xlate_subb_imm_abs_0_1")
(ADD_SUB_CMP_IMM_EA subiw_imm_absw SUBW "00000100" WORD ABSW
		    "xlate_subw_imm_abs_0_1")
(ADD_SUB_CMP_IMM_EA subil_imm_absw SUBL "00000100" LONG ABSW
		    "xlate_subl_imm_abs_0_1")

(ADD_SUB_CMP_IMM_EA subib_imm_absl SUBB "00000100" BYTE ABSL
		    "xlate_subb_imm_abs_0_1")
(ADD_SUB_CMP_IMM_EA subiw_imm_absl SUBW "00000100" WORD ABSL
		    "xlate_subw_imm_abs_0_1")
(ADD_SUB_CMP_IMM_EA subil_imm_absl SUBL "00000100" LONG ABSL
		    "xlate_subl_imm_abs_0_1")

(ADD_SUB_CMP_IMM_EA subib_imm_ind SUBB "00000100" BYTE IND
		    "xlate_subb_imm_ind_1_0")
(ADD_SUB_CMP_IMM_EA subiw_imm_ind SUBW "00000100" WORD IND
		    "xlate_subw_imm_ind_1_0")
(ADD_SUB_CMP_IMM_EA subil_imm_ind SUBL "00000100" LONG IND
		    "xlate_subl_imm_ind_1_0")

(ADD_SUB_CMP_IMM_EA subib_imm_indoff SUBB "00000100" BYTE INDOFF
		    "xlate_subb_imm_indoff_1_2_0")
(ADD_SUB_CMP_IMM_EA subiw_imm_indoff SUBW "00000100" WORD INDOFF
		    "xlate_subw_imm_indoff_1_2_0")
(ADD_SUB_CMP_IMM_EA subil_imm_indoff SUBL "00000100" LONG INDOFF
		    "xlate_subl_imm_indoff_1_2_0")

(ADD_SUB_CMP_IMM_EA subib_imm_predec SUBB "00000100" BYTE PREDEC
		    "xlate_subb_imm_predec_1_0")
(ADD_SUB_CMP_IMM_EA subiw_imm_predec SUBW "00000100" WORD PREDEC
		    "xlate_subw_imm_predec_1_0")
(ADD_SUB_CMP_IMM_EA subil_imm_predec SUBL "00000100" LONG PREDEC
		    "xlate_subl_imm_predec_1_0")

(ADD_SUB_CMP_IMM_EA subib_imm_postinc SUBB "00000100" BYTE POSTINC
		    "xlate_subb_imm_postinc_1_0")
(ADD_SUB_CMP_IMM_EA subiw_imm_postinc SUBW "00000100" WORD POSTINC
		    "xlate_subw_imm_postinc_1_0")
(ADD_SUB_CMP_IMM_EA subil_imm_postinc SUBL "00000100" LONG POSTINC
		    "xlate_subl_imm_postinc_1_0")


(ADD_SUB_CMP_IMM_EA cmpib_imm_reg CMPB "00001100" BYTE REG
		    "xlate_cmpb_imm_reg_1_0")
(ADD_SUB_CMP_IMM_EA cmpiw_imm_reg CMPW "00001100" WORD REG
		    "xlate_cmpw_imm_reg_1_0")
(ADD_SUB_CMP_IMM_EA cmpil_imm_reg CMPL "00001100" LONG REG
		    "xlate_cmpl_imm_reg_1_0")

(ADD_SUB_CMP_IMM_EA cmpib_imm_absw CMPB "00001100" BYTE ABSW
		    "xlate_cmpb_imm_abs_0_1")
(ADD_SUB_CMP_IMM_EA cmpiw_imm_absw CMPW "00001100" WORD ABSW
		    "xlate_cmpw_imm_abs_0_1")
(ADD_SUB_CMP_IMM_EA cmpil_imm_absw CMPL "00001100" LONG ABSW
		    "xlate_cmpl_imm_abs_0_1")

(ADD_SUB_CMP_IMM_EA cmpib_imm_absl CMPB "00001100" BYTE ABSL
		    "xlate_cmpb_imm_abs_0_1")
(ADD_SUB_CMP_IMM_EA cmpiw_imm_absl CMPW "00001100" WORD ABSL
		    "xlate_cmpw_imm_abs_0_1")
(ADD_SUB_CMP_IMM_EA cmpil_imm_absl CMPL "00001100" LONG ABSL
		    "xlate_cmpl_imm_abs_0_1")

(ADD_SUB_CMP_IMM_EA cmpib_imm_ind CMPB "00001100" BYTE IND
		    "xlate_cmpb_imm_ind_1_0")
(ADD_SUB_CMP_IMM_EA cmpiw_imm_ind CMPW "00001100" WORD IND
		    "xlate_cmpw_imm_ind_1_0")
(ADD_SUB_CMP_IMM_EA cmpil_imm_ind CMPL "00001100" LONG IND
		    "xlate_cmpl_imm_ind_1_0")

(ADD_SUB_CMP_IMM_EA cmpib_imm_indoff CMPB "00001100" BYTE INDOFF
		    "xlate_cmpb_imm_indoff_1_2_0")
(ADD_SUB_CMP_IMM_EA cmpiw_imm_indoff CMPW "00001100" WORD INDOFF
		    "xlate_cmpw_imm_indoff_1_2_0")
(ADD_SUB_CMP_IMM_EA cmpil_imm_indoff CMPL "00001100" LONG INDOFF
		    "xlate_cmpl_imm_indoff_1_2_0")

(ADD_SUB_CMP_IMM_EA cmpib_imm_predec CMPB "00001100" BYTE PREDEC
		    "xlate_cmpb_imm_predec_1_0")
(ADD_SUB_CMP_IMM_EA cmpiw_imm_predec CMPW "00001100" WORD PREDEC
		    "xlate_cmpw_imm_predec_1_0")
(ADD_SUB_CMP_IMM_EA cmpil_imm_predec CMPL "00001100" LONG PREDEC
		    "xlate_cmpl_imm_predec_1_0")

(ADD_SUB_CMP_IMM_EA cmpib_imm_postinc CMPB "00001100" BYTE POSTINC
		    "xlate_cmpb_imm_postinc_1_0")
(ADD_SUB_CMP_IMM_EA cmpiw_imm_postinc CMPW "00001100" WORD POSTINC
		    "xlate_cmpw_imm_postinc_1_0")
(ADD_SUB_CMP_IMM_EA cmpil_imm_postinc CMPL "00001100" LONG POSTINC
		    "xlate_cmpl_imm_postinc_1_0")


; Special case indix,reg compares.
(CMPB cmpb_indix_reg (list "1011ddd000mmmmmm") dont_expand "xxxxxxxxxx110xxx"
      $2.mub $1.dub "xlate_cmpb_indix_reg_1_0")
(CMPW cmpw_indix_reg (list "1011ddd001mmmmmm") dont_expand "xxxxxxxxxx110xxx"
      $2.muw $1.duw "xlate_cmpw_indix_reg_1_0")
(CMPL cmpl_indix_reg (list "1011ddd010mmmmmm") dont_expand "xxxxxxxxxx110xxx"
      $2.mul $1.dul "xlate_cmpl_indix_reg_1_0")

(CMPL cmpl_indix_areg (list "101aaaa111mmmmmm") dont_expand "1011xxxxxx110xxx"
      $2.mul $1.gul "xlate_cmpl_indix_reg_1_0")


; addib and subib
(ADDB_NOSWAP addib_reg (list "0000011000000mmm" "00000000bbbbbbbb") dont_expand
	     amode_dreg $2.ub $1.dub
	     "none")
(ADDB_NOSWAP addib (list "0000011000mmmmmm" "00000000bbbbbbbb") dont_expand
	     amode_alterable_data $2.ub $1.mub "none")
(SUBB_NOSWAP subib_reg (list "0000010000000mmm" "00000000bbbbbbbb") dont_expand
	     amode_dreg $2.ub $1.dub
	     "none")
(SUBB_NOSWAP subib (list "0000010000mmmmmm" "00000000bbbbbbbb") dont_expand
	     amode_alterable_data $2.ub $1.mub "none")

; addiw and subiw
(ADDW_NOSWAP addiw_reg (list "0000011001000mmm" "wwwwwwwwwwwwwwww") dont_expand
	     amode_dreg $2.uw $1.duw
	     "none")
(ADDW addiw (list "0000011001mmmmmm" "wwwwwwwwwwwwwwww") dont_expand
      amode_alterable_memory $2.uw $1.muw "none")
(SUBW_NOSWAP subiw_reg (list "0000010001000mmm" "wwwwwwwwwwwwwwww") dont_expand
	     amode_dreg $2.uw $1.duw
	     "none")
(SUBW subiw (list "0000010001mmmmmm" "wwwwwwwwwwwwwwww") dont_expand
      amode_alterable_memory $2.uw $1.muw "none")

; addil and subil
(ADDL_NOSWAP addil_reg
	     (list "0000011010000mmm" "llllllllllllllll" "llllllllllllllll")
	     dont_expand amode_dreg $2.ul $1.dul
	     "none")
(ADDL addil (list "0000011010mmmmmm" "llllllllllllllll" "llllllllllllllll")
      dont_expand amode_alterable_memory $2.ul $1.mul "none")
(SUBL_NOSWAP subil
	     (list "0000010010000mmm" "llllllllllllllll" "llllllllllllllll")
	     dont_expand amode_dreg $2.ul $1.dul
	     "none")
(SUBL subil (list "0000010010mmmmmm" "llllllllllllllll" "llllllllllllllll")
      dont_expand amode_alterable_memory $2.ul $1.mul "none")



(define (ADDQ_SUBQ_IMM_EA name main_op base_bit size amode native eight_p)
  (main_op name
	   (list
	    (if eight_p "01nnnnn" "0101nnn")
	    base_bit
	    (if (= size BYTE)
		"00"
		(if (= size WORD)
		    "01"
		    "10"))
	    (switch amode
		    ((+ ABSW    0) "111000")
		    ((+ ABSL    0) "111001")
		    ((+ REG     0) "000ddd")  ; no areg
		    ((+ GREG    0) "00dddd")
		    ((+ AREG    0) "001ddd")
		    ((+ IND     0) "010ddd")
		    ((+ PREDEC  0) "100ddd")
		    ((+ POSTINC 0) "011ddd")
		    ((+ INDOFF  0) "101sss"))
	    (switch amode
		    ((+ ABSW    0) "bbbbbbbbbbbbbbbb")
		    ((+ ABSL    0) "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb")
		    ((+ INDOFF  0) "aaaaaaaaaaaaaaaa")
		    (default "")))
	   dont_expand
	   (if eight_p
	       "0101000xxxxxxxxx"
	       amode_implicit)
	   (if (= size BYTE)
	       $1.ub
	       (if (= size WORD)
		   $1.uw
		   $1.ul))
	   (src_val amode REG size true)  ; not really REG, but same effect
	   native
	   (if (= amode PREDEC)
	       (if (and (= size BYTE) (= $2.ul 7))
		   (assign a7.ul (- a7.ul 2))
		   (assign $2.aul (- $2.aul size)))
	       (list))
	   (if (= amode POSTINC)
	       (if (and (= size BYTE) (= $2.ul 7))
		   (assign a7.ul (+ a7.ul 2))
		   (assign $2.aul (+ $2.aul size)))
	       (list))))



(ADDQ_SUBQ_IMM_EA addqb8_imm_reg ADDB "0" BYTE REG
		  "xlate_addb_imm_reg_0_1" true)
(ADDQ_SUBQ_IMM_EA addqb_imm_reg ADDB "0" BYTE REG
		  "xlate_addb_imm_reg_0_1" false)
(ADDQ_SUBQ_IMM_EA addqw8_imm_reg ADDW "0" WORD REG
		  "xlate_addw_imm_reg_0_1" true)
(ADDQ_SUBQ_IMM_EA addqw_imm_reg ADDW "0" WORD REG
		  "xlate_addw_imm_reg_0_1" false)
(ADDQ_SUBQ_IMM_EA addql8_imm_reg ADDL "0" LONG REG
		  "xlate_addl_imm_reg_0_1" true)
(ADDQ_SUBQ_IMM_EA addql_imm_reg ADDL "0" LONG REG
		  "xlate_addl_imm_reg_0_1" false)

(ADDQ_SUBQ_IMM_EA addqb8_imm_absw ADDB "0" BYTE ABSW
		  "xlate_addb_imm_abs_0_1" true)
(ADDQ_SUBQ_IMM_EA addqb_imm_absw ADDB "0" BYTE ABSW
		  "xlate_addb_imm_abs_0_1" false)
(ADDQ_SUBQ_IMM_EA addqw8_imm_absw ADDW "0" WORD ABSW
		  "xlate_addw_imm_abs_0_1" true)
(ADDQ_SUBQ_IMM_EA addqw_imm_absw ADDW "0" WORD ABSW
		  "xlate_addw_imm_abs_0_1" false)
(ADDQ_SUBQ_IMM_EA addql8_imm_absw ADDL "0" LONG ABSW
		  "xlate_addl_imm_abs_0_1" true)
(ADDQ_SUBQ_IMM_EA addql_imm_absw ADDL "0" LONG ABSW
		  "xlate_addl_imm_abs_0_1" false)

(ADDQ_SUBQ_IMM_EA addqb8_imm_absl ADDB "0" BYTE ABSL
		  "xlate_addb_imm_abs_0_1" true)
(ADDQ_SUBQ_IMM_EA addqb_imm_absl ADDB "0" BYTE ABSL
		  "xlate_addb_imm_abs_0_1" false)
(ADDQ_SUBQ_IMM_EA addqw8_imm_absl ADDW "0" WORD ABSL
		  "xlate_addw_imm_abs_0_1" true)
(ADDQ_SUBQ_IMM_EA addqw_imm_absl ADDW "0" WORD ABSL
		  "xlate_addw_imm_abs_0_1" false)
(ADDQ_SUBQ_IMM_EA addql8_imm_absl ADDL "0" LONG ABSL
		  "xlate_addl_imm_abs_0_1" true)
(ADDQ_SUBQ_IMM_EA addql_imm_absl ADDL "0" LONG ABSL
		  "xlate_addl_imm_abs_0_1" false)

(ADDQ_SUBQ_IMM_EA addqb8_imm_ind ADDB "0" BYTE IND
		  "xlate_addb_imm_ind_0_1" true)
(ADDQ_SUBQ_IMM_EA addqb_imm_ind ADDB "0" BYTE IND
		  "xlate_addb_imm_ind_0_1" false)
(ADDQ_SUBQ_IMM_EA addqw8_imm_ind ADDW "0" WORD IND
		  "xlate_addw_imm_ind_0_1" true)
(ADDQ_SUBQ_IMM_EA addqw_imm_ind ADDW "0" WORD IND
		  "xlate_addw_imm_ind_0_1" false)
(ADDQ_SUBQ_IMM_EA addql8_imm_ind ADDL "0" LONG IND
		  "xlate_addl_imm_ind_0_1" true)
(ADDQ_SUBQ_IMM_EA addql_imm_ind ADDL "0" LONG IND
		  "xlate_addl_imm_ind_0_1" false)

(ADDQ_SUBQ_IMM_EA addqb8_imm_indoff ADDB "0" BYTE INDOFF
		  "xlate_addb_imm_indoff_0_2_1" true)
(ADDQ_SUBQ_IMM_EA addqb_imm_indoff ADDB "0" BYTE INDOFF
		  "xlate_addb_imm_indoff_0_2_1" false)
(ADDQ_SUBQ_IMM_EA addqw8_imm_indoff ADDW "0" WORD INDOFF
		  "xlate_addw_imm_indoff_0_2_1" true)
(ADDQ_SUBQ_IMM_EA addqw_imm_indoff ADDW "0" WORD INDOFF
		  "xlate_addw_imm_indoff_0_2_1" false)
(ADDQ_SUBQ_IMM_EA addql8_imm_indoff ADDL "0" LONG INDOFF
		  "xlate_addl_imm_indoff_0_2_1" true)
(ADDQ_SUBQ_IMM_EA addql_imm_indoff ADDL "0" LONG INDOFF
		  "xlate_addl_imm_indoff_0_2_1" false)

(ADDQ_SUBQ_IMM_EA addqb8_imm_predec ADDB "0" BYTE PREDEC
		  "xlate_addb_imm_predec_0_1" true)
(ADDQ_SUBQ_IMM_EA addqb_imm_predec ADDB "0" BYTE PREDEC
		  "xlate_addb_imm_predec_0_1" false)
(ADDQ_SUBQ_IMM_EA addqw8_imm_predec ADDW "0" WORD PREDEC
		  "xlate_addw_imm_predec_0_1" true)
(ADDQ_SUBQ_IMM_EA addqw_imm_predec ADDW "0" WORD PREDEC
		  "xlate_addw_imm_predec_0_1" false)
(ADDQ_SUBQ_IMM_EA addql8_imm_predec ADDL "0" LONG PREDEC
		  "xlate_addl_imm_predec_0_1" true)
(ADDQ_SUBQ_IMM_EA addql_imm_predec ADDL "0" LONG PREDEC
		  "xlate_addl_imm_predec_0_1" false)

(ADDQ_SUBQ_IMM_EA addqb8_imm_postinc ADDB "0" BYTE POSTINC
		  "xlate_addb_imm_postinc_0_1" true)
(ADDQ_SUBQ_IMM_EA addqb_imm_postinc ADDB "0" BYTE POSTINC
		  "xlate_addb_imm_postinc_0_1" false)
(ADDQ_SUBQ_IMM_EA addqw8_imm_postinc ADDW "0" WORD POSTINC
		  "xlate_addw_imm_postinc_0_1" true)
(ADDQ_SUBQ_IMM_EA addqw_imm_postinc ADDW "0" WORD POSTINC
		  "xlate_addw_imm_postinc_0_1" false)
(ADDQ_SUBQ_IMM_EA addql8_imm_postinc ADDL "0" LONG POSTINC
		  "xlate_addl_imm_postinc_0_1" true)
(ADDQ_SUBQ_IMM_EA addql_imm_postinc ADDL "0" LONG POSTINC
		  "xlate_addl_imm_postinc_0_1" false)


(ADDQ_SUBQ_IMM_EA subqb8_imm_reg SUBB "1" BYTE REG
		  "xlate_subb_imm_reg_0_1" true)
(ADDQ_SUBQ_IMM_EA subqb_imm_reg SUBB "1" BYTE REG
		  "xlate_subb_imm_reg_0_1" false)
(ADDQ_SUBQ_IMM_EA subqw8_imm_reg SUBW "1" WORD REG
		  "xlate_subw_imm_reg_0_1" true)
(ADDQ_SUBQ_IMM_EA subqw_imm_reg SUBW "1" WORD REG
		  "xlate_subw_imm_reg_0_1" false)
(ADDQ_SUBQ_IMM_EA subql8_imm_reg SUBL "1" LONG REG
		  "xlate_subl_imm_reg_0_1" true)
(ADDQ_SUBQ_IMM_EA subql_imm_reg SUBL "1" LONG REG
		  "xlate_subl_imm_reg_0_1" false)

(ADDQ_SUBQ_IMM_EA subqb8_imm_absw SUBB "1" BYTE ABSW
		  "xlate_subb_imm_abs_0_1" true)
(ADDQ_SUBQ_IMM_EA subqb_imm_absw SUBB "1" BYTE ABSW
		  "xlate_subb_imm_abs_0_1" false)
(ADDQ_SUBQ_IMM_EA subqw8_imm_absw SUBW "1" WORD ABSW
		  "xlate_subw_imm_abs_0_1" true)
(ADDQ_SUBQ_IMM_EA subqw_imm_absw SUBW "1" WORD ABSW
		  "xlate_subw_imm_abs_0_1" false)
(ADDQ_SUBQ_IMM_EA subql8_imm_absw SUBL "1" LONG ABSW
		  "xlate_subl_imm_abs_0_1" true)
(ADDQ_SUBQ_IMM_EA subql_imm_absw SUBL "1" LONG ABSW
		  "xlate_subl_imm_abs_0_1" false)

(ADDQ_SUBQ_IMM_EA subqb8_imm_absl SUBB "1" BYTE ABSL
		  "xlate_subb_imm_abs_0_1" true)
(ADDQ_SUBQ_IMM_EA subqb_imm_absl SUBB "1" BYTE ABSL
		  "xlate_subb_imm_abs_0_1" false)
(ADDQ_SUBQ_IMM_EA subqw8_imm_absl SUBW "1" WORD ABSL
		  "xlate_subw_imm_abs_0_1" true)
(ADDQ_SUBQ_IMM_EA subqw_imm_absl SUBW "1" WORD ABSL
		  "xlate_subw_imm_abs_0_1" false)
(ADDQ_SUBQ_IMM_EA subql8_imm_absl SUBL "1" LONG ABSL
		  "xlate_subl_imm_abs_0_1" true)
(ADDQ_SUBQ_IMM_EA subql_imm_absl SUBL "1" LONG ABSL
		  "xlate_subl_imm_abs_0_1" false)

(ADDQ_SUBQ_IMM_EA subqb8_imm_ind SUBB "1" BYTE IND
		  "xlate_subb_imm_ind_0_1" true)
(ADDQ_SUBQ_IMM_EA subqb_imm_ind SUBB "1" BYTE IND
		  "xlate_subb_imm_ind_0_1" false)
(ADDQ_SUBQ_IMM_EA subqw8_imm_ind SUBW "1" WORD IND
		  "xlate_subw_imm_ind_0_1" true)
(ADDQ_SUBQ_IMM_EA subqw_imm_ind SUBW "1" WORD IND
		  "xlate_subw_imm_ind_0_1" false)
(ADDQ_SUBQ_IMM_EA subql8_imm_ind SUBL "1" LONG IND
		  "xlate_subl_imm_ind_0_1" true)
(ADDQ_SUBQ_IMM_EA subql_imm_ind SUBL "1" LONG IND
		  "xlate_subl_imm_ind_0_1" false)

(ADDQ_SUBQ_IMM_EA subqb8_imm_indoff SUBB "1" BYTE INDOFF
		  "xlate_subb_imm_indoff_0_2_1" true)
(ADDQ_SUBQ_IMM_EA subqb_imm_indoff SUBB "1" BYTE INDOFF
		  "xlate_subb_imm_indoff_0_2_1" false)
(ADDQ_SUBQ_IMM_EA subqw8_imm_indoff SUBW "1" WORD INDOFF
		  "xlate_subw_imm_indoff_0_2_1" true)
(ADDQ_SUBQ_IMM_EA subqw_imm_indoff SUBW "1" WORD INDOFF
		  "xlate_subw_imm_indoff_0_2_1" false)
(ADDQ_SUBQ_IMM_EA subql8_imm_indoff SUBL "1" LONG INDOFF
		  "xlate_subl_imm_indoff_0_2_1" true)
(ADDQ_SUBQ_IMM_EA subql_imm_indoff SUBL "1" LONG INDOFF
		  "xlate_subl_imm_indoff_0_2_1" false)

(ADDQ_SUBQ_IMM_EA subqb8_imm_predec SUBB "1" BYTE PREDEC
		  "xlate_subb_imm_predec_0_1" true)
(ADDQ_SUBQ_IMM_EA subqb_imm_predec SUBB "1" BYTE PREDEC
		  "xlate_subb_imm_predec_0_1" false)
(ADDQ_SUBQ_IMM_EA subqw8_imm_predec SUBW "1" WORD PREDEC
		  "xlate_subw_imm_predec_0_1" true)
(ADDQ_SUBQ_IMM_EA subqw_imm_predec SUBW "1" WORD PREDEC
		  "xlate_subw_imm_predec_0_1" false)
(ADDQ_SUBQ_IMM_EA subql8_imm_predec SUBL "1" LONG PREDEC
		  "xlate_subl_imm_predec_0_1" true)
(ADDQ_SUBQ_IMM_EA subql_imm_predec SUBL "1" LONG PREDEC
		  "xlate_subl_imm_predec_0_1" false)

(ADDQ_SUBQ_IMM_EA subqb8_imm_postinc SUBB "1" BYTE POSTINC
		  "xlate_subb_imm_postinc_0_1" true)
(ADDQ_SUBQ_IMM_EA subqb_imm_postinc SUBB "1" BYTE POSTINC
		  "xlate_subb_imm_postinc_0_1" false)
(ADDQ_SUBQ_IMM_EA subqw8_imm_postinc SUBW "1" WORD POSTINC
		  "xlate_subw_imm_postinc_0_1" true)
(ADDQ_SUBQ_IMM_EA subqw_imm_postinc SUBW "1" WORD POSTINC
		  "xlate_subw_imm_postinc_0_1" false)
(ADDQ_SUBQ_IMM_EA subql8_imm_postinc SUBL "1" LONG POSTINC
		  "xlate_subl_imm_postinc_0_1" true)
(ADDQ_SUBQ_IMM_EA subql_imm_postinc SUBL "1" LONG POSTINC
		  "xlate_subl_imm_postinc_0_1" false)



(ADDB addqb8_imm_indix (list "010aaaa000mmmmmm")
      dont_expand "0101000000110xxx" $1.ul $2.mub
      "xlate_addb_imm_indix_0_1")
(ADDB addqb_imm_indix (list "0101aaa000mmmmmm")
      dont_expand "0101xxx000110xxx" $1.ul $2.mub
      "xlate_addb_imm_indix_0_1")

(ADDW addqw8_imm_indix (list "010aaaa001mmmmmm")
      dont_expand "0101000001110xxx" $1.ul $2.muw
      "xlate_addw_imm_indix_0_1")
(ADDW addqw_imm_indix (list "0101aaa001mmmmmm")
      dont_expand "0101xxx001110xxx" $1.ul $2.muw
      "xlate_addw_imm_indix_0_1")

(ADDL addql8_imm_indix (list "010aaaa010mmmmmm")
      dont_expand "0101000010110xxx" $1.ul $2.mul
      "xlate_addl_imm_indix_0_1")
(ADDL addql_imm_indix (list "0101aaa010mmmmmm")
      dont_expand "0101xxx010110xxx" $1.ul $2.mul
      "xlate_addl_imm_indix_0_1")


(SUBB subqb8_imm_indix (list "010aaaa000mmmmmm")
      dont_expand "0101000000110xxx" $1.ul $2.mub
      "xlate_subb_imm_indix_0_1")
(SUBB subqb_imm_indix (list "0101aaa000mmmmmm")
      dont_expand "0101xxx000110xxx" $1.ul $2.mub
      "xlate_subb_imm_indix_0_1")

(SUBW subqw8_imm_indix (list "010aaaa001mmmmmm")
      dont_expand "0101000001110xxx" $1.ul $2.muw
      "xlate_subw_imm_indix_0_1")
(SUBW subqw_imm_indix (list "0101aaa001mmmmmm")
      dont_expand "0101xxx001110xxx" $1.ul $2.muw
      "xlate_subw_imm_indix_0_1")

(SUBL subql8_imm_indix (list "010aaaa010mmmmmm")
      dont_expand "0101000010110xxx" $1.ul $2.mul
      "xlate_subl_imm_indix_0_1")
(SUBL subql_imm_indix (list "0101aaa010mmmmmm")
      dont_expand "0101xxx010110xxx" $1.ul $2.mul
      "xlate_subl_imm_indix_0_1")

  
; addqb
(ADDB_NOSWAP addqb8 (list "0101000000mmmmmm") (list "----------00xxxx")
	     amode_alterable_data 8 $1.mub "none")
(ADDB_NOSWAP addqb (list "0101nnn000mmmmmm") (list "----------00xxxx")
	     amode_alterable_data $1.ub $2.mub "none")

; subqb
(SUBB_NOSWAP subqb8 (list "0101000100mmmmmm") (list "----------00xxxx")
	     amode_alterable_data 8 $1.mub "none")
(SUBB_NOSWAP subqb (list "0101nnn100mmmmmm") (list "----------00xxxx")
	     amode_alterable_data $1.ub $2.mub "none")

; When dealing with address registers, we can only use word/long sizes
; and the entire address register is affected by the operation regardless
; of size.  We also don't affect cc bits.  This means we merge the word
; and long cases into one.
(defopcode addaq8
  (list 68000 (union "xx01000x01xxxxxx" "xx01000x10xxxxxx")  ; Word or long
	() (list "01nnnnn0ss001aaa"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_addl_imm_areg_0_1")
	(assign $3.aul (+ $3.aul $1.ul))))  ; known to be 8...
(defopcode addaq
  (list 68000 (union "xxxxxxxx01xxxxxx" "xxxxxxxx10xxxxxx")  ; Word or long
	() (list "0101nnn0ss001aaa"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_addl_imm_areg_0_1")
	(assign $3.aul (+ $3.aul $1.ul))))

(defopcode subaq8
  (list 68000 (union "xx01000x01xxxxxx" "xx01000x10xxxxxx")  ; Word or long
	() (list "01nnnnn1ss001aaa"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_subl_imm_areg_0_1")
	(assign $3.aul (- $3.aul $1.ul))))  ; known to be 8...
(defopcode subaq
  (list 68000 (union "xxxxxxxx01xxxxxx" "xxxxxxxx10xxxxxx")  ; Word or long
	() (list "0101nnn1ss001aaa"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_subl_imm_areg_0_1")
	(assign $3.aul (- $3.aul $1.ul))))


; addqw
(ADDW_NOSWAP addqw8  (list "010100000100mmmm") (list "----------00xxxx")
	     amode_reg 8 $1.guw "none")
(ADDW_NOSWAP addqw (list "0101nnn00100mmmm")
	     dont_expand amode_reg $1.ul $2.guw "none")
(ADDW addqw8  (list "0101000001mmmmmm") (list "----------00xxxx")
      amode_alterable 8 $1.muw "none")
(ADDW addqw (list "0101nnn001mmmmmm")
      dont_expand amode_alterable $1.ul $2.muw "none")

; subqw
(SUBW_NOSWAP subqw8  (list "010100010100mmmm") (list "----------00xxxx")
	     amode_reg 8 $1.guw "none")
(SUBW_NOSWAP subqw (list "0101nnn10100mmmm")
	     dont_expand amode_reg $1.ul $2.guw "none")
(SUBW subqw8  (list "0101000101mmmmmm") (list "----------00xxxx")
      amode_alterable 8 $1.muw "none")
(SUBW subqw (list "0101nnn101mmmmmm")
      dont_expand amode_alterable $1.ul $2.muw "none")

; addql
(ADDL_NOSWAP addql8 (list "0101000010mmmmmm") (list "----------00xxxx")
	     amode_reg 8 $1.mul "none")
(ADDL_NOSWAP addql (list "0101nnn010mmmmmm")
	     dont_expand amode_reg $1.ul $2.mul "none")
(ADDL addql8 (list "0101000010mmmmmm") (list "----------00xxxx")
      amode_alterable 8 $1.mul "none")
(ADDL addql (list "0101nnn010mmmmmm")
      dont_expand amode_alterable $1.ul $2.mul "none")

; subql
(SUBL_NOSWAP subql8 (list "0101000110mmmmmm") (list "----------00xxxx")
	     amode_reg 8 $1.mul "none")
(SUBL_NOSWAP subql (list "0101nnn110mmmmmm")
	     dont_expand amode_reg $1.ul $2.mul "none")

(SUBL subql8 (list "0101000110mmmmmm") (list "----------00xxxx")
      amode_alterable 8 $1.mul "none")
(SUBL subql (list "0101nnn110mmmmmm")
      dont_expand amode_alterable $1.ul $2.mul "none")


(CMPB cmpb (list "1011ddd000mmmmmm") dont_expand amode_all_combinations
      $2.mub $1.dub "none")
(CMPW cmpw (list "1011ddd001mmmmmm") dont_expand amode_all_combinations
      $2.muw $1.duw "none")
(CMPL cmpl (list "1011ddd010mmmmmm") dont_expand amode_all_combinations
      $2.mul $1.dul "none")

(CMPB cmpib (list "0000110000mmmmmm" "00000000bbbbbbbb") dont_expand
      (intersect amode_data (not "xxxxxxxxxx111100")) $2.ub $1.mub "none")
(CMPW cmpiw (list "0000110001mmmmmm" "wwwwwwwwwwwwwwww") dont_expand
      (intersect amode_data (not "xxxxxxxxxx111100")) $2.uw $1.muw "none")
(CMPL cmpil (list "0000110010mmmmmm" "llllllllllllllll" "llllllllllllllll")
      dont_expand
      (intersect amode_data (not "xxxxxxxxxx111100")) $2.ul $1.mul "none")

(CMPL cmpaw (list "1011aaa011mmmmmm") dont_expand
      amode_all_combinations $2.muw $1.asl "none")
(CMPL cmpal (list "1011aaa111mmmmmm") dont_expand
      amode_all_combinations $2.msl $1.asl "none")


(define (ADDXBW name preamble postamble src dst bits t1 t2 t3 shift
		sign_func native)
  (defopcode name
    (list 68000 amode_implicit () (list bits))
    (list "CNVX<" "---X-" dont_expand
	  (native_code native)
	  (list
	   preamble
	   (assign t3 dst)
	   (assign tmp3.ul (& (~ (^ t3 src)) (<< 1 (- shift 1))))
	   (assign ccn
		   (sign_func (assign dst
				      (assign t1
					      (+ t3 src (<> ccx 0))))))
	   (assign ccx (assign ccc (>> t1 shift)))
	   (assign ccv (<> (& tmp3.ul (^ t2 t3)) 0))
	   (assign ccnz (| ccnz (<> 0 t2)))
	   postamble))))

(ADDXBW addxb_reg (list) (list) $2.dub $1.dub "1101ddd100000ddd"
	tmp.uw tmp.ub tmp2.ub 8 SIGN_BYTE "xlate_addxb_reg_reg_1_0")
(ADDXBW addxb_a7@-,a7@-
	(assign a7.ul (- a7.ul 2))
	(assign a7.ul (- a7.ul 2))
	(derefub a7.ul) (derefub (- a7.ul 2))
	"1101111100001111"
	tmp.uw tmp.ub tmp2.ub 8 SIGN_BYTE "none")
(ADDXBW addxb_a7@-,an@-
	(assign a7.ul (- a7.ul 2))
	(assign $1.aul (- $1.aul 1))
	(derefub a7.ul) (derefub (- $1.aul 1))
	"1101aaa100001111"
	tmp.uw tmp.ub tmp2.ub 8 SIGN_BYTE "none")
(ADDXBW addxb_an@-,a7@-
	(assign $1.aul (- $1.aul 1))
	(assign a7.ul (- a7.ul 2))
	(derefub $1.aul) (derefub (- a7.ul 2))
	"1101111100001aaa"
	tmp.uw tmp.ub tmp2.ub 8 SIGN_BYTE "none")
(ADDXBW addxb_mem
	(assign $2.aul (- $2.aul 1))
	(assign $1.aul (- $1.aul 1))
	(derefub $2.aul) (derefub (- $1.aul 1))
	"1101aaa100001aaa"
	tmp.uw tmp.ub tmp2.ub 8 SIGN_BYTE "none")
(ADDXBW addxw_reg (list) (list) $2.duw $1.duw
	"1101ddd101000ddd" tmp.ul tmp.uw tmp2.uw 16 SIGN_WORD
	"xlate_addxw_reg_reg_1_0")
(ADDXBW addxw_mem
	(assign $2.aul (- $2.aul 2))
	(assign $1.aul (- $1.aul 2))
	(derefuw $2.aul) (derefuw (- $1.aul 2))
	"1101aaa101001aaa" tmp.ul tmp.uw tmp2.uw 16 SIGN_WORD "none")

(define (ADDXL name preamble postamble src dst bits native)
  (defopcode name
    (list 68000 amode_implicit () (list bits))
    (list "CNVX<" "---X-" dont_expand
	  (native_code native)
	  (list
	   preamble
	   (assign tmp2.ul dst)
	   (assign ccv (>> (~ (^ tmp2.ul src)) 31))
	   (assign ccn (SIGN_LONG (assign tmp.ul
					  (assign dst
						  (+ dst src (<> ccx 0))))))
	   (if ccv   ; Are the high bits of src and dst the same?
	       (assign ccx (assign ccc (SIGN_LONG tmp2.ul)))
	       (assign ccx (assign ccc (SIGN_LONG (~ tmp.ul)))))
	   (assign ccnz (| ccnz (<> 0 tmp.ul)))
	   (assign ccv (& ccv (>> (^ tmp.ul tmp2.ul) 31)))
	   postamble))))

(ADDXL addxl_reg (list) (list) $2.dul $1.dul "1101ddd110000ddd"
       "xlate_addxl_reg_reg_1_0")
(ADDXL addxl_mem
       (assign $2.aul (- $2.aul 4))
       (assign $1.aul (- $1.aul 4))
       (dereful $2.aul) (dereful (- $1.aul 4)) "1101aaa110001aaa" "none")


(define (SUBXBW name preamble postamble src dst bits t1 t2 t3 shift
		sign_func)
  (defopcode name
    (list 68000 amode_implicit () (list bits))
    (list "CNVX<" "---X-" dont_expand
	  (list
	   preamble
	   (assign t3 dst)
	   (assign ccx (<> ccx 0))
	   (assign ccc (> src (- t3 ccx)))
	   (assign tmp3.ul (& (^ t3 src) (<< 1 (- shift 1))))
	   (assign ccn (sign_func (assign dst (assign t1 (- t3 src ccx)))))
	   (assign ccx ccc)
	   (assign ccv (<> (& tmp3.ul (^ t2 t3)) 0))
	   (assign ccnz (| ccnz (<> 0 t2)))
	   postamble))))


(SUBXBW subxb_reg (list) (list) $2.dub $1.dub "1001ddd100000ddd"
	tmp.uw tmp.ub tmp2.ub 8 SIGN_BYTE)
(SUBXBW subxb_a7@-,a7@-
	(assign a7.ul (- a7.ul 4))
	(list)
	(derefub (+ a7.ul 2)) (derefub a7.ul) "1001111100001111"
	tmp.uw tmp.ub tmp2.ub 8 SIGN_BYTE)
(SUBXBW subxb_an@-,a7@-
	(list
	 (assign $1.aul (- $1.aul 1))
	 (assign a7.ul (- a7.ul 2)))
	(list)
	(derefub $1.aul) (derefub a7.ul) "1001111100001aaa"
	tmp.uw tmp.ub tmp2.ub 8 SIGN_BYTE)
(SUBXBW subxb_a7@-,an@-
	(list
	 (assign a7.ul (- a7.ul 2))
	 (assign $1.aul (- $1.aul 1)))
	(list)
	(derefub a7.ul) (derefub $1.aul) "1001aaa100001111"
	tmp.uw tmp.ub tmp2.ub 8 SIGN_BYTE)
(SUBXBW subxb_mem
	(assign $2.aul (- $2.aul 1))
	(assign $1.aul (- $1.aul 1))
	(derefub $2.aul) (derefub (- $1.aul 1)) "1001aaa100001aaa"
	tmp.uw tmp.ub tmp2.ub 8 SIGN_BYTE)
(SUBXBW subxw_reg (list) (list) $2.duw $1.duw "1001ddd101000ddd"
	tmp.ul tmp.uw tmp2.uw 16 SIGN_WORD)
(SUBXBW subxw_mem
	(assign $2.aul (- $2.aul 2))
	(assign $1.aul (- $1.aul 2))
	(derefuw $2.aul) (derefuw (- $1.aul 2)) "1001aaa101001aaa"
	tmp.ul tmp.uw tmp2.uw 16 SIGN_WORD)

(define (SUBXL name preamble postamble src dst bits)
  (defopcode name
    (list 68000 amode_implicit () (list bits))
    (list "CNVX<" "---X-" dont_expand
	  (list
	   preamble
	   (assign tmp3.ul src)
	   (assign tmp2.ul dst)
	   (assign ccc (> tmp3.ul tmp2.ul))
	   (assign ccv (>> (^ tmp2.ul tmp3.ul) 31))
	   (assign ccn (SIGN_LONG (assign tmp.ul
					  (assign dst
						  (- tmp2.ul tmp3.ul
						     (<> ccx 0))))))
	   (assign ccnz (| ccnz (<> 0 tmp.ul)))
	   (assign ccx (assign ccc (SIGN_LONG
				    (| (& tmp3.ul tmp.ul)
				       (& (~ tmp2.ul) (| tmp3.ul tmp.ul))))))
	   (assign ccv (& ccv (>> (^ tmp.ul tmp2.ul) 31)))
	   postamble))))

(SUBXL subxl_reg (list) (list) $2.dul $1.dul "1001ddd110000ddd")
(SUBXL subxl_mem
       (assign $2.aul (- $2.aul 4))
       (assign $1.aul (- $1.aul 4))
       (dereful $2.aul) (dereful (- $1.aul 4)) "1001aaa110001aaa")

; and, or, eor
(define (AND_OR_EOR name op bit_pattern amode expand src dst cc_func native
		    pre post)
  (defopcode name
    (list 68000 amode () bit_pattern)
    (list "-----" "-----" expand
	  (native_code native)
	  (list
	   pre
	   (assign dst (op dst src))
	   post))
    (list "CNV-Z" "-----" dont_expand
	  (native_code native)
	  (list
	   pre
	   (cc_func (assign dst (op dst src)))
	   post))))

(define (AND_OR_EOR name op bit_pattern amode expand src dst cc_func native)
  (AND_OR_EOR name op bit_pattern amode expand src dst cc_func native
	      (list) (list)))


(define (BITOP name main_op base_bits size amode ea_dest_p native)
  (AND_OR_EOR name main_op
	     (list base_bits "ddd" (if ea_dest_p "1" "0")
		   (if (= size BYTE)
		       "00"
		       (if (= size WORD)
			   "01"
			   "10"))
		   (switch amode
			   ((+ IMM     0)
			    (if (= size BYTE)
				"11110000000000cccccccc"
				(if (= size WORD)
				    "111100cccccccccccccccc"
				    "111100cccccccccccccccccccccccccccccccc")))
			   ((+ ABSW    0) "111000bbbbbbbbbbbbbbbb")
			   ((+ ABSL    0)
			    "111001bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb")
			   ((+ REG     0) "000ddd")  ; no areg
			   ((+ GREG    0) "00dddd")
			   ((+ AREG    0) "001ddd")
			   ((+ IND     0) "010ddd")
			   ((+ PREDEC  0) "100ddd")
			   ((+ POSTINC 0) "011ddd")
			   ((+ INDOFF  0) "101sssaaaaaaaaaaaaaaaa")))
	     amode_implicit dont_expand
	     (if ea_dest_p
		 (if (= size BYTE)
		     $1.dub
		     (if (= size WORD)
			 $1.duw
			 $1.dul))
		 (ea_val amode size false))
	     (if ea_dest_p
		 (ea_val amode size false)
		 (if (= size BYTE)
		     $1.dub
		     (if (= size WORD)
			 $1.duw
			 $1.dul)))
	     (if (= size BYTE)
		 ASSIGN_C_N_V_NZ_BYTE
		 (if (= size WORD)
		     ASSIGN_C_N_V_NZ_WORD
		     ASSIGN_C_N_V_NZ_LONG))
	     native
	     (if (= amode PREDEC)
		 (if (and (= size BYTE) (= $2.ul 7))
		     (assign a7.ul (- a7.ul 2))
		     (assign $2.aul (- $2.aul size)))
		 (list))
	     (if (= amode POSTINC)
		 (if (and (= size BYTE) (= $2.ul 7))
		     (assign a7.ul (+ a7.ul 2))
		     (assign $2.aul (+ $2.aul size)))
		 (list))))

(BITOP andb_imm_reg & "1100" BYTE IMM false "xlate_andb_imm_reg_1_0")
(BITOP andw_imm_reg & "1100" WORD IMM false "xlate_andw_imm_reg_1_0")
(BITOP andl_imm_reg & "1100" LONG IMM false "xlate_andl_imm_reg_1_0")

(BITOP andb_reg_reg & "1100" BYTE REG false "xlate_andb_reg_reg_1_0")
(BITOP andw_reg_reg & "1100" WORD REG false "xlate_andw_reg_reg_1_0")
(BITOP andl_reg_reg & "1100" LONG REG false "xlate_andl_reg_reg_1_0")

(BITOP andb_absw_reg & "1100" BYTE ABSW false "xlate_andb_abs_reg_1_0")
(BITOP andw_absw_reg & "1100" WORD ABSW false "xlate_andw_abs_reg_1_0")
(BITOP andl_absw_reg & "1100" LONG ABSW false "xlate_andl_abs_reg_1_0")

(BITOP andb_absl_reg & "1100" BYTE ABSL false "xlate_andb_abs_reg_1_0")
(BITOP andw_absl_reg & "1100" WORD ABSL false "xlate_andw_abs_reg_1_0")
(BITOP andl_absl_reg & "1100" LONG ABSL false "xlate_andl_abs_reg_1_0")

(BITOP andb_ind_reg & "1100" BYTE IND false "xlate_andb_ind_reg_1_0")
(BITOP andw_ind_reg & "1100" WORD IND false "xlate_andw_ind_reg_1_0")
(BITOP andl_ind_reg & "1100" LONG IND false "xlate_andl_ind_reg_1_0")

(BITOP andb_predec_reg & "1100" BYTE PREDEC false "xlate_andb_predec_reg_1_0")
(BITOP andw_predec_reg & "1100" WORD PREDEC false "xlate_andw_predec_reg_1_0")
(BITOP andl_predec_reg & "1100" LONG PREDEC false "xlate_andl_predec_reg_1_0")

(BITOP andb_postinc_reg & "1100" BYTE POSTINC false
       "xlate_andb_postinc_reg_1_0")
(BITOP andw_postinc_reg & "1100" WORD POSTINC false
       "xlate_andw_postinc_reg_1_0")
(BITOP andl_postinc_reg & "1100" LONG POSTINC false
       "xlate_andl_postinc_reg_1_0")

(BITOP andb_indoff_reg & "1100" BYTE INDOFF false
       "xlate_andb_indoff_reg_2_1_0")
(BITOP andw_indoff_reg & "1100" WORD INDOFF false
       "xlate_andw_indoff_reg_2_1_0")
(BITOP andl_indoff_reg & "1100" LONG INDOFF false
       "xlate_andl_indoff_reg_2_1_0")

(BITOP andb_reg_absw & "1100" BYTE ABSW true "xlate_andb_reg_abs_0_1")
(BITOP andw_reg_absw & "1100" WORD ABSW true "xlate_andw_reg_abs_0_1")
(BITOP andl_reg_absw & "1100" LONG ABSW true "xlate_andl_reg_abs_0_1")

(BITOP andb_reg_absl & "1100" BYTE ABSL true "xlate_andb_reg_abs_0_1")
(BITOP andw_reg_absl & "1100" WORD ABSL true "xlate_andw_reg_abs_0_1")
(BITOP andl_reg_absl & "1100" LONG ABSL true "xlate_andl_reg_abs_0_1")

(BITOP andb_reg_ind & "1100" BYTE IND true "xlate_andb_reg_ind_0_1")
(BITOP andw_reg_ind & "1100" WORD IND true "xlate_andw_reg_ind_0_1")
(BITOP andl_reg_ind & "1100" LONG IND true "xlate_andl_reg_ind_0_1")

(BITOP andb_reg_predec & "1100" BYTE PREDEC true "xlate_andb_reg_predec_0_1")
(BITOP andw_reg_predec & "1100" WORD PREDEC true "xlate_andw_reg_predec_0_1")
(BITOP andl_reg_predec & "1100" LONG PREDEC true "xlate_andl_reg_predec_0_1")

(BITOP andb_reg_postinc & "1100" BYTE POSTINC true
       "xlate_andb_reg_postinc_0_1")
(BITOP andw_reg_postinc & "1100" WORD POSTINC true
       "xlate_andw_reg_postinc_0_1")
(BITOP andl_reg_postinc & "1100" LONG POSTINC true
       "xlate_andl_reg_postinc_0_1")

(BITOP andb_reg_indoff & "1100" BYTE INDOFF true
       "xlate_andb_reg_indoff_0_2_1")
(BITOP andw_reg_indoff & "1100" WORD INDOFF true
       "xlate_andw_reg_indoff_0_2_1")
(BITOP andl_reg_indoff & "1100" LONG INDOFF true
       "xlate_andl_reg_indoff_0_2_1")


(BITOP orb_imm_reg | "1000" BYTE IMM false "xlate_orb_imm_reg_1_0")
(BITOP orw_imm_reg | "1000" WORD IMM false "xlate_orw_imm_reg_1_0")
(BITOP orl_imm_reg | "1000" LONG IMM false "xlate_orl_imm_reg_1_0")

(BITOP orb_reg_reg | "1000" BYTE REG false "xlate_orb_reg_reg_1_0")
(BITOP orw_reg_reg | "1000" WORD REG false "xlate_orw_reg_reg_1_0")
(BITOP orl_reg_reg | "1000" LONG REG false "xlate_orl_reg_reg_1_0")

(BITOP orb_absw_reg | "1000" BYTE ABSW false "xlate_orb_abs_reg_1_0")
(BITOP orw_absw_reg | "1000" WORD ABSW false "xlate_orw_abs_reg_1_0")
(BITOP orl_absw_reg | "1000" LONG ABSW false "xlate_orl_abs_reg_1_0")

(BITOP orb_absl_reg | "1000" BYTE ABSL false "xlate_orb_abs_reg_1_0")
(BITOP orw_absl_reg | "1000" WORD ABSL false "xlate_orw_abs_reg_1_0")
(BITOP orl_absl_reg | "1000" LONG ABSL false "xlate_orl_abs_reg_1_0")

(BITOP orb_ind_reg | "1000" BYTE IND false "xlate_orb_ind_reg_1_0")
(BITOP orw_ind_reg | "1000" WORD IND false "xlate_orw_ind_reg_1_0")
(BITOP orl_ind_reg | "1000" LONG IND false "xlate_orl_ind_reg_1_0")

(BITOP orb_predec_reg | "1000" BYTE PREDEC false "xlate_orb_predec_reg_1_0")
(BITOP orw_predec_reg | "1000" WORD PREDEC false "xlate_orw_predec_reg_1_0")
(BITOP orl_predec_reg | "1000" LONG PREDEC false "xlate_orl_predec_reg_1_0")

(BITOP orb_postinc_reg | "1000" BYTE POSTINC false
       "xlate_orb_postinc_reg_1_0")
(BITOP orw_postinc_reg | "1000" WORD POSTINC false
       "xlate_orw_postinc_reg_1_0")
(BITOP orl_postinc_reg | "1000" LONG POSTINC false
       "xlate_orl_postinc_reg_1_0")

(BITOP orb_indoff_reg | "1000" BYTE INDOFF false
       "xlate_orb_indoff_reg_2_1_0")
(BITOP orw_indoff_reg | "1000" WORD INDOFF false
       "xlate_orw_indoff_reg_2_1_0")
(BITOP orl_indoff_reg | "1000" LONG INDOFF false
       "xlate_orl_indoff_reg_2_1_0")

(BITOP orb_reg_absw | "1000" BYTE ABSW true "xlate_orb_reg_abs_0_1")
(BITOP orw_reg_absw | "1000" WORD ABSW true "xlate_orw_reg_abs_0_1")
(BITOP orl_reg_absw | "1000" LONG ABSW true "xlate_orl_reg_abs_0_1")

(BITOP orb_reg_absl | "1000" BYTE ABSL true "xlate_orb_reg_abs_0_1")
(BITOP orw_reg_absl | "1000" WORD ABSL true "xlate_orw_reg_abs_0_1")
(BITOP orl_reg_absl | "1000" LONG ABSL true "xlate_orl_reg_abs_0_1")

(BITOP orb_reg_ind | "1000" BYTE IND true "xlate_orb_reg_ind_0_1")
(BITOP orw_reg_ind | "1000" WORD IND true "xlate_orw_reg_ind_0_1")
(BITOP orl_reg_ind | "1000" LONG IND true "xlate_orl_reg_ind_0_1")

(BITOP orb_reg_predec | "1000" BYTE PREDEC true "xlate_orb_reg_predec_0_1")
(BITOP orw_reg_predec | "1000" WORD PREDEC true "xlate_orw_reg_predec_0_1")
(BITOP orl_reg_predec | "1000" LONG PREDEC true "xlate_orl_reg_predec_0_1")

(BITOP orb_reg_postinc | "1000" BYTE POSTINC true
       "xlate_orb_reg_postinc_0_1")
(BITOP orw_reg_postinc | "1000" WORD POSTINC true
       "xlate_orw_reg_postinc_0_1")
(BITOP orl_reg_postinc | "1000" LONG POSTINC true
       "xlate_orl_reg_postinc_0_1")

(BITOP orb_reg_indoff | "1000" BYTE INDOFF true
       "xlate_orb_reg_indoff_0_2_1")
(BITOP orw_reg_indoff | "1000" WORD INDOFF true
       "xlate_orw_reg_indoff_0_2_1")
(BITOP orl_reg_indoff | "1000" LONG INDOFF true
       "xlate_orl_reg_indoff_0_2_1")


(BITOP xorb_reg_reg ^ "1011" BYTE REG true "xlate_xorb_reg_reg_0_1")
(BITOP xorw_reg_reg ^ "1011" WORD REG true "xlate_xorw_reg_reg_0_1")
(BITOP xorl_reg_reg ^ "1011" LONG REG true "xlate_xorl_reg_reg_0_1")

(BITOP xorb_reg_absw ^ "1011" BYTE ABSW true "xlate_xorb_reg_abs_0_1")
(BITOP xorw_reg_absw ^ "1011" WORD ABSW true "xlate_xorw_reg_abs_0_1")
(BITOP xorl_reg_absw ^ "1011" LONG ABSW true "xlate_xorl_reg_abs_0_1")

(BITOP xorb_reg_absl ^ "1011" BYTE ABSL true "xlate_xorb_reg_abs_0_1")
(BITOP xorw_reg_absl ^ "1011" WORD ABSL true "xlate_xorw_reg_abs_0_1")
(BITOP xorl_reg_absl ^ "1011" LONG ABSL true "xlate_xorl_reg_abs_0_1")

(BITOP xorb_reg_ind ^ "1011" BYTE IND true "xlate_xorb_reg_ind_0_1")
(BITOP xorw_reg_ind ^ "1011" WORD IND true "xlate_xorw_reg_ind_0_1")
(BITOP xorl_reg_ind ^ "1011" LONG IND true "xlate_xorl_reg_ind_0_1")

(BITOP xorb_reg_predec ^ "1011" BYTE PREDEC true "xlate_xorb_reg_predec_0_1")
(BITOP xorw_reg_predec ^ "1011" WORD PREDEC true "xlate_xorw_reg_predec_0_1")
(BITOP xorl_reg_predec ^ "1011" LONG PREDEC true "xlate_xorl_reg_predec_0_1")

(BITOP xorb_reg_postinc ^ "1011" BYTE POSTINC true
       "xlate_xorb_reg_postinc_0_1")
(BITOP xorw_reg_postinc ^ "1011" WORD POSTINC true
       "xlate_xorw_reg_postinc_0_1")
(BITOP xorl_reg_postinc ^ "1011" LONG POSTINC true
       "xlate_xorl_reg_postinc_0_1")

(BITOP xorb_reg_indoff ^ "1011" BYTE INDOFF true
       "xlate_xorb_reg_indoff_0_2_1")
(BITOP xorw_reg_indoff ^ "1011" WORD INDOFF true
       "xlate_xorw_reg_indoff_0_2_1")
(BITOP xorl_reg_indoff ^ "1011" LONG INDOFF true
       "xlate_xorl_reg_indoff_0_2_1")




; And, or, eor, effective address destination.
(AND_OR_EOR eorb_dest_ea ^ (list "1011ddd100mmmmmm") amode_alterable_data
	    dont_expand $1.dub $2.mub ASSIGN_C_N_V_NZ_BYTE "none")
(AND_OR_EOR andb_dest_ea & (list "1100ddd100mmmmmm") amode_alterable_memory
	    dont_expand $1.dub $2.mub ASSIGN_C_N_V_NZ_BYTE "none")
(AND_OR_EOR orb_dest_ea  | (list "1000ddd100mmmmmm") amode_alterable_memory
	    dont_expand $1.dub $2.mub ASSIGN_C_N_V_NZ_BYTE "none")
(AND_OR_EOR eorw_dest_ea ^ (list "1011ddd101mmmmmm") amode_alterable_data
	    dont_expand $1.duw $2.muw ASSIGN_C_N_V_NZ_WORD "none")
(AND_OR_EOR andw_dest_ea & (list "1100ddd101mmmmmm") amode_alterable_memory
	    dont_expand $1.duw $2.muw ASSIGN_C_N_V_NZ_WORD "none")
(AND_OR_EOR orw_dest_ea  | (list "1000ddd101mmmmmm") amode_alterable_memory
	    dont_expand $1.duw $2.muw ASSIGN_C_N_V_NZ_WORD "none")
(AND_OR_EOR eorl_dest_ea ^ (list "1011ddd110mmmmmm") amode_alterable_data
	    dont_expand $1.dul $2.mul ASSIGN_C_N_V_NZ_LONG "none")
(AND_OR_EOR andl_dest_ea & (list "1100ddd110mmmmmm") amode_alterable_memory
	    dont_expand $1.dul $2.mul ASSIGN_C_N_V_NZ_LONG "none")
(AND_OR_EOR orl_dest_ea  | (list "1000ddd110mmmmmm") amode_alterable_memory
	    dont_expand $1.dul $2.mul ASSIGN_C_N_V_NZ_LONG "none")

; And, or, data register destination (no such thing for eor)
(AND_OR_EOR andb_dest_reg & (list "1100ddd000mmmmmm") amode_data
	    dont_expand $2.mub $1.dub ASSIGN_C_N_V_NZ_BYTE "none")
(AND_OR_EOR orb_dest_reg  | (list "1000ddd000mmmmmm") amode_data
	    dont_expand $2.mub $1.dub ASSIGN_C_N_V_NZ_BYTE "none")
(AND_OR_EOR andw_dest_reg & (list "1100ddd001mmmmmm") amode_data
	    dont_expand $2.muw $1.duw ASSIGN_C_N_V_NZ_WORD "none")
(AND_OR_EOR orw_dest_reg  | (list "1000ddd001mmmmmm") amode_data
	    dont_expand $2.muw $1.duw ASSIGN_C_N_V_NZ_WORD "none")
(AND_OR_EOR andl_dest_reg & (list "1100ddd010mmmmmm") amode_data
	    dont_expand $2.mul $1.dul ASSIGN_C_N_V_NZ_LONG "none")
(AND_OR_EOR orl_dest_reg  | (list "1000ddd010mmmmmm") amode_data
	    dont_expand $2.mul $1.dul ASSIGN_C_N_V_NZ_LONG "none")


(define (BITOP_IMM_EA name main_op base_bits size amode native)
  (AND_OR_EOR name main_op
	     (list base_bits
		   (if (= size BYTE)
		       "00"
		       (if (= size WORD)
			   "01"
			   "10"))
		   (switch amode
			   ((+ ABSW    0) "111000")
			   ((+ ABSL    0) "111001")
			   ((+ REG     0) "000ddd")  ; no areg
			   ((+ GREG    0) "00dddd")
			   ((+ AREG    0) "001ddd")
			   ((+ IND     0) "010ddd")
			   ((+ PREDEC  0) "100ddd")
			   ((+ POSTINC 0) "011ddd")
			   ((+ INDOFF  0) "101sss"))
		   (if (= size BYTE)
		       "00000000cccccccc"
		       (if (= size WORD)
			   "cccccccccccccccc"
			   "cccccccccccccccccccccccccccccccc"))
		   (switch amode
			   ((+ ABSW    0) "bbbbbbbbbbbbbbbb")
			   ((+ ABSL    0) "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb")
			   ((+ INDOFF  0) "aaaaaaaaaaaaaaaa")
			   (default "")))
	     amode_implicit dont_expand
	     (if (no_reg_op_p amode)
		 (if (= size BYTE)
		     $1.ub
		     (if (= size WORD)
			 $1.uw
			 $1.ul))
		 (if (= size BYTE)
		     $2.ub
		     (if (= size WORD)
			 $2.uw
			 $2.ul)))
	     (dst_val IMM amode size)
	     (if (= size BYTE)
		 ASSIGN_C_N_V_NZ_BYTE
		 (if (= size WORD)
		     ASSIGN_C_N_V_NZ_WORD
		     ASSIGN_C_N_V_NZ_LONG))
	     native
	     (if (= amode PREDEC)
		 (if (and (= size BYTE) (= $1.ul 7))
		     (assign a7.ul (- a7.ul 2))
		     (assign $1.aul (- $1.aul size)))
		 (list))
	     (if (= amode POSTINC)
		 (if (and (= size BYTE) (= $1.ul 7))
		     (assign a7.ul (+ a7.ul 2))
		     (assign $1.aul (+ $1.aul size)))
		 (list))))


(BITOP_IMM_EA andib_imm_reg & "00000010" BYTE REG "xlate_andb_imm_reg_1_0")
(BITOP_IMM_EA andiw_imm_reg & "00000010" WORD REG "xlate_andw_imm_reg_1_0")
(BITOP_IMM_EA andil_imm_reg & "00000010" LONG REG "xlate_andl_imm_reg_1_0")

(BITOP_IMM_EA andib_imm_ind & "00000010" BYTE IND "xlate_andb_imm_ind_1_0")
(BITOP_IMM_EA andiw_imm_ind & "00000010" WORD IND "xlate_andw_imm_ind_1_0")
(BITOP_IMM_EA andil_imm_ind & "00000010" LONG IND "xlate_andl_imm_ind_1_0")

(BITOP_IMM_EA andib_imm_indoff & "00000010" BYTE INDOFF
	      "xlate_andb_imm_indoff_1_2_0")
(BITOP_IMM_EA andiw_imm_indoff & "00000010" WORD INDOFF
	      "xlate_andw_imm_indoff_1_2_0")
(BITOP_IMM_EA andil_imm_indoff & "00000010" LONG INDOFF
	      "xlate_andl_imm_indoff_1_2_0")

(BITOP_IMM_EA andib_imm_predec & "00000010" BYTE PREDEC
	      "xlate_andb_imm_predec_1_0")
(BITOP_IMM_EA andiw_imm_predec & "00000010" WORD PREDEC
	      "xlate_andw_imm_predec_1_0")
(BITOP_IMM_EA andil_imm_predec & "00000010" LONG PREDEC
	      "xlate_andl_imm_predec_1_0")

(BITOP_IMM_EA andib_imm_postinc & "00000010" BYTE POSTINC
	      "xlate_andb_imm_postinc_1_0")
(BITOP_IMM_EA andiw_imm_postinc & "00000010" WORD POSTINC
	      "xlate_andw_imm_postinc_1_0")
(BITOP_IMM_EA andil_imm_postinc & "00000010" LONG POSTINC
	      "xlate_andl_imm_postinc_1_0")



(BITOP_IMM_EA orib_imm_reg | "00000000" BYTE REG "xlate_orb_imm_reg_1_0")
(BITOP_IMM_EA oriw_imm_reg | "00000000" WORD REG "xlate_orw_imm_reg_1_0")
(BITOP_IMM_EA oril_imm_reg | "00000000" LONG REG "xlate_orl_imm_reg_1_0")

(BITOP_IMM_EA orib_imm_ind | "00000000" BYTE IND "xlate_orb_imm_ind_1_0")
(BITOP_IMM_EA oriw_imm_ind | "00000000" WORD IND "xlate_orw_imm_ind_1_0")
(BITOP_IMM_EA oril_imm_ind | "00000000" LONG IND "xlate_orl_imm_ind_1_0")

(BITOP_IMM_EA orib_imm_indoff | "00000000" BYTE INDOFF
	      "xlate_orb_imm_indoff_1_2_0")
(BITOP_IMM_EA oriw_imm_indoff | "00000000" WORD INDOFF
	      "xlate_orw_imm_indoff_1_2_0")
(BITOP_IMM_EA oril_imm_indoff | "00000000" LONG INDOFF
	      "xlate_orl_imm_indoff_1_2_0")

(BITOP_IMM_EA orib_imm_predec | "00000000" BYTE PREDEC
	      "xlate_orb_imm_predec_1_0")
(BITOP_IMM_EA oriw_imm_predec | "00000000" WORD PREDEC
	      "xlate_orw_imm_predec_1_0")
(BITOP_IMM_EA oril_imm_predec | "00000000" LONG PREDEC
	      "xlate_orl_imm_predec_1_0")

(BITOP_IMM_EA orib_imm_postinc | "00000000" BYTE POSTINC
	      "xlate_orb_imm_postinc_1_0")
(BITOP_IMM_EA oriw_imm_postinc | "00000000" WORD POSTINC
	      "xlate_orw_imm_postinc_1_0")
(BITOP_IMM_EA oril_imm_postinc | "00000000" LONG POSTINC
	      "xlate_orl_imm_postinc_1_0")



(BITOP_IMM_EA xorib_imm_reg ^ "00001010" BYTE REG "xlate_xorb_imm_reg_1_0")
(BITOP_IMM_EA xoriw_imm_reg ^ "00001010" WORD REG "xlate_xorw_imm_reg_1_0")
(BITOP_IMM_EA xoril_imm_reg ^ "00001010" LONG REG "xlate_xorl_imm_reg_1_0")

(BITOP_IMM_EA xorib_imm_ind ^ "00001010" BYTE IND "xlate_xorb_imm_ind_1_0")
(BITOP_IMM_EA xoriw_imm_ind ^ "00001010" WORD IND "xlate_xorw_imm_ind_1_0")
(BITOP_IMM_EA xoril_imm_ind ^ "00001010" LONG IND "xlate_xorl_imm_ind_1_0")

(BITOP_IMM_EA xorib_imm_indoff ^ "00001010" BYTE INDOFF
	      "xlate_xorb_imm_indoff_1_2_0")
(BITOP_IMM_EA xoriw_imm_indoff ^ "00001010" WORD INDOFF
	      "xlate_xorw_imm_indoff_1_2_0")
(BITOP_IMM_EA xoril_imm_indoff ^ "00001010" LONG INDOFF
	      "xlate_xorl_imm_indoff_1_2_0")

(BITOP_IMM_EA xorib_imm_predec ^ "00001010" BYTE PREDEC
	      "xlate_xorb_imm_predec_1_0")
(BITOP_IMM_EA xoriw_imm_predec ^ "00001010" WORD PREDEC
	      "xlate_xorw_imm_predec_1_0")
(BITOP_IMM_EA xoril_imm_predec ^ "00001010" LONG PREDEC
	      "xlate_xorl_imm_predec_1_0")

(BITOP_IMM_EA xorib_imm_postinc ^ "00001010" BYTE POSTINC
	      "xlate_xorb_imm_postinc_1_0")
(BITOP_IMM_EA xoriw_imm_postinc ^ "00001010" WORD POSTINC
	      "xlate_xorw_imm_postinc_1_0")
(BITOP_IMM_EA xoril_imm_postinc ^ "00001010" LONG POSTINC
	      "xlate_xorl_imm_postinc_1_0")

; andi/ori/eori
(AND_OR_EOR eorib ^ (list "0000101000mmmmmm" "00000000bbbbbbbb")
	    amode_alterable_data dont_expand $2.ub $1.mub ASSIGN_C_N_V_NZ_BYTE
	    "none")
(AND_OR_EOR andib & (list "0000001000mmmmmm" "00000000bbbbbbbb")
	    amode_alterable_data dont_expand $2.ub $1.mub ASSIGN_C_N_V_NZ_BYTE
	    "none")
(AND_OR_EOR orib  | (list "0000000000mmmmmm" "00000000bbbbbbbb")
	    amode_alterable_data dont_expand $2.ub $1.mub ASSIGN_C_N_V_NZ_BYTE
	    "none")
(AND_OR_EOR eoriw ^ (list "0000101001mmmmmm" "wwwwwwwwwwwwwwww")
	    amode_alterable_data dont_expand $2.uw $1.muw ASSIGN_C_N_V_NZ_WORD
	    "none")
(AND_OR_EOR andiw & (list "0000001001mmmmmm" "wwwwwwwwwwwwwwww")
	    amode_alterable_data dont_expand $2.uw $1.muw ASSIGN_C_N_V_NZ_WORD
	    "none")
(AND_OR_EOR oriw  | (list "0000000001mmmmmm" "wwwwwwwwwwwwwwww")
	    amode_alterable_data dont_expand $2.uw $1.muw ASSIGN_C_N_V_NZ_WORD
	    "none")
(AND_OR_EOR eoril ^
	    (list "0000101010mmmmmm" "llllllllllllllll" "llllllllllllllll")
	    amode_alterable_data dont_expand $2.ul $1.mul
	    ASSIGN_C_N_V_NZ_LONG "none")
(AND_OR_EOR andil &
	    (list "0000001010mmmmmm" "llllllllllllllll" "llllllllllllllll")
	    amode_alterable_data dont_expand $2.ul $1.mul
	    ASSIGN_C_N_V_NZ_LONG "none")
(AND_OR_EOR oril  |
	    (list "0000000010mmmmmm" "llllllllllllllll" "llllllllllllllll")
	    amode_alterable_data dont_expand $2.ul $1.mul
	    ASSIGN_C_N_V_NZ_LONG "none")






(defopcode andi_to_ccr
  (list 68000 amode_implicit ()
	(list "0000001000111100" "00000000000bbbbb"))
  (list "<<<<<" "-----" dont_expand
	(list
	 (if (not (& $1.ub 0x1)) (assign ccc 0))
	 (if (not (& $1.ub 0x2)) (assign ccv 0))
	 (if (not (& $1.ub 0x8)) (assign ccn 0))
	 (if (not (& $1.ub 0x10)) (assign ccx 0))
	 (assign ccnz (| ccnz (& (~ $1.ub) 0x4))))))

(defopcode eori_to_ccr
  (list 68000 amode_implicit ()
	(list "0000101000111100" "00000000000bbbbb"))
  (list ">>>>>" "-----" dont_expand
	(list
	 (if (& $1.ub 0x1)  (assign ccc  (not ccc)))
	 (if (& $1.ub 0x2)  (assign ccv  (not ccv)))
	 (if (& $1.ub 0x4)  (assign ccnz (not ccnz)))
	 (if (& $1.ub 0x8)  (assign ccn  (not ccn)))
	 (if (& $1.ub 0x10) (assign ccx  (not ccx))))))

(defopcode ori_to_ccr
  (list 68000 amode_implicit ()
	(list "0000000000111100" "00000000000bbbbb"))
  (list "%%%%%" "-----" dont_expand
	(list
	 (assign ccc (| ccc (& $1.ub 0x1)))
	 (assign ccv (| ccv (& $1.ub 0x2)))
	 (assign ccn (| ccn (& $1.ub 0x8)))
	 (assign ccx (| ccx (& $1.ub 0x10)))
	 (if (& $1.ub 0x4)
	     (assign ccnz 0)))))


(defopcode move_from_ccr
  (list 68010 amode_alterable_data () (list "0100001011mmmmmm"))
  (list "-----" "CNVXZ" dont_expand
	(list
	 (assign tmp.uw            ; Temp needed to compensate for byteorder.c
		 (| (<> ccc 0)     ; confusion when assigning an expression
		    (<< (<> ccv 0) 1)   ; of unknown size to memory.  This
		    (<< (not ccnz) 2)   ; is the easiest workaround, as it
		    (<< (<> ccn 0) 3)   ; makes the expression sizes explicit.
		    (<< (<> ccx 0) 4)))
	 (assign $1.muw tmp.uw))))

(defopcode move_to_ccr
  (list 68000 amode_data () (list "0100010011mmmmmm"))
  (list "CNVXZ" "-----" dont_expand
	(list
	 (assign tmp.ul $1.muw)
	 (assign ccc (& tmp.ul 0x1))
	 (assign ccv (& tmp.ul 0x2))
	 (assign ccn (& tmp.ul 0x8))
	 (assign ccx (& tmp.ul 0x10))
	 (assign ccnz (& (~ tmp.ul) 0x4)))))


(defopcode move_from_sr
  (list 68000 amode_alterable_data () (list "0100000011mmmmmm"))
  (list "-----" "CNVXZ" dont_expand
	(list
	 (assign tmp.uw (| "cpu_state.sr"    ; See note for move_from_ccr
			   (<> ccc 0)
			   (<< (<> ccv 0) 1)
			   (<< (not ccnz) 2)
			   (<< (<> ccn 0) 3)
			   (<< (<> ccx 0) 4)))
	 (assign $1.muw tmp.uw))))

(defopcode move_to_sr
  (list 68000 amode_data () (list "0100011011mmmmmm"))
  (list "CNVXZ" "-----" dont_expand
	(LOAD_NEW_SR $1.muw)))


(defopcode andi_to_sr
  (list 68000 amode_data () (list "0000001001111100" "wwwwwwwwwwwwwwww"))
  (list "<<<<<" "-----" dont_expand
	(list
	 (assign tmp.uw (| "cpu_state.sr"
			   (<> ccc 0)
			   (<< (<> ccv 0) 1)
			   (<< (not ccnz) 2)
			   (<< (<> ccn 0) 3)
			   (<< (<> ccx 0) 4)))
	 (LOAD_NEW_SR (& tmp.uw $1.uw)))))

(defopcode ori_to_sr
  (list 68000 amode_data () (list "0000000001111100" "wwwwwwwwwwwwwwww"))
  (list ">>>>>" "-----" dont_expand
	(list
	 (assign tmp.uw (| "cpu_state.sr"
			   (<> ccc 0)
			   (<< (<> ccv 0) 1)
			   (<< (not ccnz) 2)
			   (<< (<> ccn 0) 3)
			   (<< (<> ccx 0) 4)))
	 (LOAD_NEW_SR (| tmp.uw $1.uw)))))

(defopcode eori_to_sr
  (list 68000 amode_data () (list "0000101001111100" "wwwwwwwwwwwwwwww"))
  (list "%%%%%" "-----" dont_expand
	(list
	 (assign tmp.uw (| "cpu_state.sr"
			   (<> ccc 0)
			   (<< (<> ccv 0) 1)
			   (<< (not ccnz) 2)
			   (<< (<> ccn 0) 3)
			   (<< (<> ccx 0) 4)))
	 (LOAD_NEW_SR (^ tmp.uw $1.uw)))))

(defopcode movec_to_reg
  (list 68010 amode_implicit () (list "0100111001111010" "ggggcccccccccccc"))
  (list "-----" "-----" dont_expand
	(list
	 (assign tmp.ul 0)  ; Default value
	 (switch $2.uw
		 (0x800 (assign tmp.ul (call "FETCH_USP")))
		 (0x002 (assign tmp.ul (& "cpu_state.cacr" 0x7)))
		 (0x801 (assign tmp.ul "cpu_state.vbr"))
		 (0x802 (assign tmp.ul (& "cpu_state.caar" 0xFC)))
		 (0x803 (assign tmp.ul (call "FETCH_MSP")))
		 (0x804 (assign tmp.ul (call "FETCH_ISP")))
		 )
	 (assign $1.gul tmp.ul))))


(defopcode movec_from_reg
  (list 68010 amode_implicit () (list "0100111001111010" "ggggcccccccccccc"))
  (list "-----" "-----" dont_expand
	(list
	 (assign tmp.ul $1.gul)
	 (switch $2.uw
		 (0x800 (call "SET_USP" tmp.ul))
		 (0x002 (assign "cpu_state.cacr" tmp.ul))  ; CACR, FIXME
		 (0x801 (assign "cpu_state.vbr" tmp.ul))
		 (0x802 (assign "cpu_state.caar" tmp.ul))  ; CAAR
		 (0x803 (call "SET_MSP" tmp.ul))
		 (0x804 (call "SET_ISP" tmp.ul))
		 ))))


(defopcode illegal
  (list 68000 amode_implicit (ends_block next_block_dynamic)
	(list "0100101011111100"))
  (list "-----" "-----" dont_expand
	(TRAP 4 (deref "uint32 *" code 0) 0)))

(defopcode aslw_ea
  (list 68000 amode_alterable_memory () (list "1110000111mmmmmm"))
  (list "-----" "-----" dont_expand      ; Why bother expanding this?
	(assign $1.muw (<< $1.muw 1)))
  (list "CNVXZ" "-----" dont_expand
	(list
	 (assign tmp.uw $1.muw)
	 (assign ccx (assign ccc (SIGN_WORD tmp.uw)))
	 (ASSIGN_NNZ_WORD (assign $1.muw (<< tmp.uw 1)))
	 (assign ccv (>> (^ $1.muw tmp.uw) 15)))))

(defopcode aslb_reg
  (list 68000 amode_implicit () (list "1110ddd100100ddd"))
  (list "-----" "-----" dont_expand
	(list
	 (assign tmp.ul (& $1.dul 63))
	 (if (>= tmp.ul 8)       ; Can only assume shift counts 0-31 portable!
	     (assign $2.dub 0)
	     (assign $2.dub (<< $2.dub tmp.ul)))))
  (list "CNV%Z" "-----" dont_expand
	(list
	 (assign tmp.ul (& $1.dul 63))
	 (if (>= tmp.ul 8)       ; Can only assume shift counts 0-31 portable!
	     (list
	      (if (= tmp.ul 8)
		  (assign ccc (& $2.dub 1)) ; lsb last shifted out
		  (assign ccc 0))
	      (assign ccx ccc)
	      (assign ccv $2.dub)
	      (SET_N_NZ 0 0)
	      (assign $2.dub 0))
	     (list
	      (assign ccc (& $2.dub (>> 0x100 tmp.ul)))
	      (if (<> tmp.ul 0)
		  (assign ccx ccc))
	      (assign ccv (& (^ $2.dsb (>> $2.dsb tmp.ul)) (>> -128 tmp.ul)))
	      (ASSIGN_NNZ_BYTE (assign $2.dub (<< $2.dub tmp.ul))))))))

(defopcode aslb_reg_by_8
  (list 68000 amode_implicit () (list "1110000100000ddd"))
  (list "-----" "-----" dont_expand
	(assign $1.dub 0))
  (list "C0VX1" "-----" dont_expand
	(list
	 (assign ccx (assign ccc (& $1.dub 1)))
	 (assign ccv $1.dub)
	 (assign $1.dub 0))))

(defopcode aslb_reg_by_const
  (list 68000 amode_implicit () (list "1110nnn100000ddd"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_lslb_imm_reg_0_1")
	(assign $2.dub (<< $2.dub $1.ul)))
  (list "CNVXZ" "-----" dont_expand
	(native_code "xlate_lslb_imm_reg_0_1")
	(list
	 (assign ccx (assign ccc (& $2.dub (>> 0x100 $1.ul))))
	 (assign ccv (& (^ $2.dsb (>> $2.dsb $1.ul))
			(>> -128 $1.ul)))
	 (ASSIGN_NNZ_BYTE (assign $2.dub (<< $2.dub $1.ul))))))

(defopcode aslw_reg
  (list 68000 amode_implicit () (list "1110ddd101100ddd"))
  (list "-----" "-----" dont_expand
	(list
	 (assign tmp.ul (& $1.dul 63))
	 (if (>= tmp.ul 16)
	     (assign $2.duw 0)
	     (assign $2.duw (<< $2.duw tmp.ul)))))
  (list "CNV%Z" "-----" dont_expand
	(list
	 (assign tmp.ul (& $1.dul 63))
	 (if (>= tmp.ul 16)
	     (list
	      (if (= tmp.ul 16)
		  (assign ccc (& $2.duw 1)) ; lsb last shifted out
		  (assign ccc 0))
	      (assign ccx ccc)
	      (SET_N_NZ 0 0)
	      (assign ccv (<> $2.duw 0))
	      (assign $2.duw 0))
	     (list
	      "\n#ifdef CCR_ELEMENT_8_BITS\n"
	      (assign ccc (<> (& $2.duw (>> 0x10000 tmp.ul)) 0))
	      "\n#else\n"
	      (assign ccc (& $2.duw (>> 0x10000 tmp.ul)))
	      "\n#endif\n"
	      (if (<> tmp.ul 0)
		  (assign ccx ccc))
	      (assign ccv (<> 0 (& (^ $2.dsw (>> $2.dsw tmp.ul))
				   (>> -32768 tmp.ul))))
	      (ASSIGN_NNZ_WORD (assign $2.duw (<< $2.duw tmp.ul))))))))

(defopcode aslw_reg_by_8
  (list 68000 "1110000101000ddd" () (list "11nnnn0101000ddd"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_lslw_imm_reg_0_1")
	(list
	 (assign $2.duw (<< $2.duw 8))
	 $1.ul))
  (list "CNVXZ" "-----" dont_expand
	(native_code "xlate_lslw_imm_reg_0_1")
	(list
	 "\n#ifdef CCR_ELEMENT_8_BITS\n"
	 (assign ccx (assign ccc (& (>> $2.duw 8) 1)))
	 "\n#else\n"
	 (assign ccx (assign ccc (& $2.duw 0x100)))
	 "\n#endif\n"
	 (assign ccv (<> 0 (& (^ $2.dsw (>> $2.dsw 8)) (>> -32768 8))))
	 (ASSIGN_NNZ_WORD (assign $2.duw (<< $2.duw 8)))
	 $1.ul)))

(defopcode aslw_reg_by_const
  (list 68000 amode_implicit () (list "1110nnn101000ddd"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_lslw_imm_reg_0_1")
 	(assign $2.duw (<< $2.duw $1.ul)))
  (list "CNVXZ" "-----" dont_expand
	(native_code "xlate_lslw_imm_reg_0_1")
	(list
	 "\n#ifdef CCR_ELEMENT_8_BITS\n"
	 (assign ccx (assign ccc (<> (& $2.duw (>> 0x10000 $1.ul)) 0)))
	 "\n#else\n"
	 (assign ccx (assign ccc (& $2.duw (>> 0x10000 $1.ul))))
	 "\n#endif\n"
	 (assign ccv (<> 0 (& (^ $2.dsw (>> $2.dsw $1.ul))
			      (>> -32768 $1.ul))))
	 (ASSIGN_NNZ_WORD (assign $2.duw (<< $2.duw $1.ul))))))

(defopcode asll_reg
  (list 68000 amode_implicit () (list "1110ddd110100ddd"))
  (list "-----" "-----" dont_expand
	(list
	 (assign tmp.ul (& $1.dul 63))
	 (if (>= tmp.ul 32)     ; Only assume shift counts <= 31 are portable!
	     (assign $2.dul 0)
	     (assign $2.dul (<< $2.dul tmp.ul)))))
  (list "CNV%Z" "-----" dont_expand
	(list
	 (assign tmp.ul (& $1.dul 63))
	 (if (>= tmp.ul 32)
	     (list
	      (if (= tmp.ul 32)
		  (assign ccc (& $2.dul 1)) ; lsb last shifted out
		  (assign ccc 0))
	      (assign ccx ccc)
	      (SET_N_NZ 0 0)
	      (assign ccv (<> 0 $2.dul))
	      (assign $2.dul 0))
	     (list
	      (if (= tmp.ul 0)
		  (list
		   (assign ccc 0)
		   (assign ccv 0)
		   (ASSIGN_NNZ_LONG $2.dul))
		  (list
		   "\n#ifdef CCR_ELEMENT_8_BITS\n"
		   (assign ccx (assign ccc (<> (& $2.dul (<< 1 (- 32 tmp.ul)))
					       0)))
		   (assign ccv (<> 0 (& (^ $2.dsl (>> $2.dsl tmp.ul))
					(>> 0x80000000 tmp.ul))))
		   "\n#else\n"
		   (assign ccx (assign ccc (& $2.dul (<< 1 (- 32 tmp.ul)))))
		   (assign ccv (& (^ $2.dsl (>> $2.dsl tmp.ul))
				  (>> 0x80000000 tmp.ul)))
		   "\n#endif\n"
		   (ASSIGN_NNZ_LONG (assign $2.dul (<< $2.dul tmp.ul))))))))))

(defopcode asll_reg_by_8
  (list 68000 "1110000110000xxx" () (list "11nnnn0110000ddd"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_lsll_imm_reg_0_1")
	(list
	 (assign $2.dul (<< $2.dul 8))
	 $1.ul))
  (list "CNVXZ" "-----" dont_expand
	(native_code "xlate_lsll_imm_reg_0_1")
	(list
	 (assign ccx (assign ccc (& (>> $2.dul 24) 1)))
	 "\n#ifdef CCR_ELEMENT_8_BITS\n"
	 (assign ccv (<> (& (^ $2.dsl (>> $2.dsl 8)) 0xFF800000) 0))
	 "\n#else\n"
	 (assign ccv (& (^ $2.dsl (>> $2.dsl 8)) 0xFF800000))
	 "\n#endif\n"
	 (ASSIGN_NNZ_LONG (assign $2.dul (<< $2.dul 8)))
	 $1.ul)))

(defopcode asll_reg_by_const
  (list 68000 amode_implicit () (list "1110nnn110000ddd"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_lsll_imm_reg_0_1")
	(assign $2.dul (<< $2.dul $1.ul)))
  (list "CNVXZ" "-----" dont_expand
	(native_code "xlate_lsll_imm_reg_0_1")
	(list
	 (assign ccx (assign ccc (& (>> $2.dul (- 32 $1.ul)) 1)))
	 "\n#ifdef CCR_ELEMENT_8_BITS\n"
	 (assign ccv (<> 0 (& (^ $2.dsl (>> $2.dsl $1.ul))
			      (>> 0x80000000 $1.ul))))
	 "\n#else\n"
	 (assign ccv (& (^ $2.dsl (>> $2.dsl $1.ul)) (>> 0x80000000 $1.ul)))
	 "\n#endif\n"
	 (ASSIGN_NNZ_LONG (assign $2.dul (<< $2.dul $1.ul))))))


(defopcode lslw_ea
  (list 68000 amode_alterable_memory () (list "1110001111mmmmmm"))
  (list "-----" "-----" dont_expand      ; Why bother expanding this?
	(assign $1.muw (<< $1.muw 1)))
  (list "CN0XZ" "-----" dont_expand
	(list
	 (assign tmp.uw $1.muw)
	 (assign ccx (assign ccc (SIGN_WORD tmp.uw)))
	 (ASSIGN_NNZ_WORD (assign $1.muw (<< tmp.uw 1))))))

(defopcode lslb_reg
  (list 68000 amode_implicit () (list "1110ddd100101ddd"))
  (list "-----" "-----" dont_expand
	(list
	 (assign tmp.ul (& $1.dul 63))
	 (if (>= tmp.ul 8)       ; Can only assume shift counts 0-31 portable!
	     (assign $2.dub 0)
	     (assign $2.dub (<< $2.dub tmp.ul)))))
  (list "CN0%Z" "-----" dont_expand
	(list
	 (assign tmp.ul (& $1.dul 63))
	 (if (>= tmp.ul 8)       ; Can only assume shift counts 0-31 portable!
	     (list
	      (if (= tmp.ul 8)
		  (assign ccc (& $2.dub 1)) ; lsb last shifted out
		  (assign ccc 0))
	      (assign ccx ccc)
	      (SET_N_NZ 0 0)
	      (assign $2.dub 0))
	     (list
	      (assign ccc (& $2.dub (>> 0x100 tmp.ul)))
	      (if (<> tmp.ul 0)
		  (assign ccx ccc))
	      (ASSIGN_NNZ_BYTE (assign $2.dub (<< $2.dub tmp.ul))))))))

(defopcode lslb_reg_by_8
  (list 68000 amode_implicit () (list "1110000100001ddd"))
  (list "-----" "-----" dont_expand
	(assign $1.dub 0))
  (list "C00X1" "-----" dont_expand
	(list
	 (assign ccx (assign ccc (& $1.dub 1)))
	 (assign $1.dub 0))))

(defopcode lslb_reg_by_const
  (list 68000 amode_implicit () (list "1110nnn100001ddd"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_lslb_imm_reg_0_1")
	(assign $2.dub (<< $2.dub $1.ul)))
  (list "CN0XZ" "-----" dont_expand
	(native_code "xlate_lslb_imm_reg_0_1")
	(list
	 (assign ccx (assign ccc (& $2.dub (>> 0x100 $1.ul))))
	 (ASSIGN_NNZ_BYTE (assign $2.dub (<< $2.dub $1.ul))))))

(defopcode lslw_reg
  (list 68000 amode_implicit () (list "1110ddd101101ddd"))
  (list "-----" "-----" dont_expand
	(list
	 (assign tmp.ul (& $1.dul 63))
	 (if (>= tmp.ul 16)
	     (assign $2.duw 0)
	     (assign $2.duw (<< $2.duw tmp.ul)))))
  (list "CN0%Z" "-----" dont_expand
	(list
	 (assign tmp.ul (& $1.dul 63))
	 (if (>= tmp.ul 16)
	     (list
	      (if (= tmp.ul 16)
		  (assign ccc (& $2.duw 1)) ; lsb last shifted out
		  (assign ccc 0))
	      (assign ccx ccc)
	      (SET_N_NZ 0 0)
	      (assign $2.duw 0))
	     (list
	      "\n#ifdef CCR_ELEMENT_8_BITS\n"
	      (assign ccc (<> (& $2.duw (>> 0x10000 tmp.ul)) 0))
	      "\n#else\n"
	      (assign ccc (& $2.duw (>> 0x10000 tmp.ul)))
	      "\n#endif\n"
	      (if (<> tmp.ul 0)
		  (assign ccx ccc))
	      (ASSIGN_NNZ_WORD (assign $2.duw (<< $2.duw tmp.ul))))))))

(defopcode lslw_reg_by_8
  (list 68000 "1110000101001xxx" () (list "11nnnn0101001ddd"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_lslw_imm_reg_0_1")
	(list
	 (assign $2.duw (<< $2.duw 8))
	 $1.ul))
  (list "CN0XZ" "-----" dont_expand
	(native_code "xlate_lslw_imm_reg_0_1")
	(list
	 (assign ccx (assign ccc (& (>> $2.duw 8) 1)))
	 (ASSIGN_NNZ_WORD (assign $2.duw (<< $2.duw 8)))
	 $1.ul)))

(defopcode lslw_reg_by_const
  (list 68000 amode_implicit () (list "1110nnn101001ddd"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_lslw_imm_reg_0_1")
	(assign $2.duw (<< $2.duw $1.ul)))
  (list "CN0XZ" "-----" dont_expand
	(native_code "xlate_lslw_imm_reg_0_1")
	(list
	 "\n#ifdef CCR_ELEMENT_8_BITS\n"
	 (assign ccx (assign ccc (<> 0 (& $2.duw (>> 0x10000 $1.ul)))))
	 "\n#else\n"
	 (assign ccx (assign ccc (& $2.duw (>> 0x10000 $1.ul))))
	 "\n#endif\n"
	 (ASSIGN_NNZ_WORD (assign $2.duw (<< $2.duw $1.ul))))))

(defopcode lsll_reg
  (list 68000 amode_implicit () (list "1110ddd110101ddd"))
  (list "-----" "-----" dont_expand
	(list
	 (assign tmp.ul (& $1.dul 63))
	 (if (>= tmp.ul 32)     ; Only assume shift counts <= 31 are portable!
	     (assign $2.dul 0)
	     (assign $2.dul (<< $2.dul tmp.ul)))))
  (list "CN0%Z" "-----" dont_expand
	(list
	 (assign tmp.ul (& $1.dul 63))
	 (if (>= tmp.ul 32)
	     (list
	      (if (= tmp.ul 32)
		  (assign ccc (& $2.dul 1)) ; lsb last shifted out
		  (assign ccc 0))
	      (assign ccx ccc)
	      (SET_N_NZ 0 0)
	      (assign $2.dul 0))
	     (list
	      (if (= tmp.ul 0)
		  (list
		   (assign ccc 0)
		   (ASSIGN_NNZ_LONG $2.dul))
		  (list
		   (assign ccx (assign ccc (& (>> $2.dul (- 32 tmp.ul)) 1)))
		   (ASSIGN_NNZ_LONG (assign $2.dul (<< $2.dul tmp.ul))))))))))

(defopcode lsll_reg_by_8
  (list 68000 "1110000110001xxx" () (list "11nnnn0110001ddd")) ; always 8
  (list "-----" "-----" dont_expand
	(native_code "xlate_lsll_imm_reg_0_1")
	(list
	 (assign $2.dul (<< $2.dul 8))
	 $1.ul))  ; hack to give native code an "8" operand.
  (list "CN0XZ" "-----" dont_expand
	(native_code "xlate_lsll_imm_reg_0_1")
	(list
	 (assign ccx (assign ccc (& (>> $2.dul 24) 1)))
	 (ASSIGN_NNZ_LONG (assign $2.dul (<< $2.dul 8)))
	 $1.ul)))  ; hack to give native code an "8" operand.

(defopcode lsll_reg_by_const
  (list 68000 amode_implicit () (list "1110nnn110001ddd"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_lsll_imm_reg_0_1")
	(assign $2.dul (<< $2.dul $1.ul)))
  (list "CN0XZ" "-----" dont_expand
	(native_code "xlate_lsll_imm_reg_0_1")
	(list
	 (assign ccx (assign ccc (& (>> $2.dul (- 32 $1.ul)) 1)))
	 (ASSIGN_NNZ_LONG (assign $2.dul (<< $2.dul $1.ul))))))


; asr
(defopcode asrw_ea
  (list 68000 amode_alterable_memory () (list "1110000011mmmmmm"))
  (list "-----" "-----" dont_expand      ; Why bother expanding this?
	(assign $1.msw (>> $1.msw 1)))
  (list "CN0XZ" "-----" dont_expand
	(list
	 (assign ccx (assign ccc (& $1.msw 1)))
	 (ASSIGN_NNZ_WORD (assign $1.msw (>> $1.msw 1))))))

(defopcode asrb_reg
  (list 68000 amode_implicit () (list "1110ddd000100ddd"))
  (list "-----" "-----" dont_expand
	(list
	 (assign tmp.ul (& $1.dul 63))
	 (if (>= tmp.ul 8)       ; Can only assume shift counts 0-31 portable!
	     (assign $2.dsb (>> $2.dsb 7))
	     (assign $2.dsb (>> $2.dsb tmp.ul)))))
  (list "CN0%Z" "-----" dont_expand
	(list
	 (assign tmp.ul (& $1.dul 63))
	 (if (>= tmp.ul 8)
	     (assign ccnz
		     (assign ccn
			     (assign ccx
				     (assign ccc
					     (assign $2.dsb (>> $2.dsb 7))))))
	     (list
	      (if (= tmp.ul 0)
		  (assign ccc 0)
		  (assign ccx (assign ccc (& $2.dub (<< 1 (- tmp.ul 1))))))
	      (ASSIGN_NNZ_BYTE (assign $2.dsb (>> $2.dsb tmp.ul))))))))

(defopcode asrb_reg_by_8
  (list 68000 "1110000000000xxx" () (list "11nnnn0000000ddd"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_asrb_imm_reg_0_1")
	(list
	 (assign $2.dsb (>> $2.dsb 7))
	 $1.ul))
  (list "CN0XZ" "-----" dont_expand
	(native_code "xlate_asrb_imm_reg_0_1")
	(list
	 (assign ccnz
		 (assign ccn
			 (assign ccx
				 (assign ccc
					 (assign $2.dsb (>> $2.dsb 7))))))
	 $1.ul)))

(defopcode asrb_reg_by_const
  (list 68000 amode_implicit () (list "1110nnn000000ddd"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_asrb_imm_reg_0_1")
	(assign $2.dsb (>> $2.dsb $1.ul)))
  (list "CN0XZ" "-----" dont_expand
	(native_code "xlate_asrb_imm_reg_0_1")
	(list
	 (assign ccx (assign ccc (& $2.dub (<< 1 (- $1.ul 1)))))
	 (ASSIGN_NNZ_BYTE (assign $2.dsb (>> $2.dsb $1.ul))))))

(defopcode asrw_reg
  (list 68000 amode_implicit () (list "1110ddd001100ddd"))
  (list "-----" "-----" dont_expand
	(list
	 (assign tmp.ul (& $1.dul 63))
	 (if (>= tmp.ul 16)
	     (assign $2.dsw (>> $2.dsw 15))
	     (assign $2.dsw (>> $2.dsw tmp.ul)))))
  (list "CN0%Z" "-----" dont_expand
	(list
	 (assign tmp.ul (& $1.dul 63))
	 (if (>= tmp.ul 16)
	     (assign ccnz
		     (assign ccn
			     (assign ccx
				     (assign ccc
					     (<> 0
						 (assign $2.dsw (>> $2.dsw 15)))))))
	     (list
	      (if (= tmp.ul 0)
		  (assign ccc 0)
		  (assign ccx (assign ccc
				      (& (>> $2.duw (- tmp.ul 1)) 1))))
	      (ASSIGN_NNZ_WORD (assign $2.dsw (>> $2.dsw tmp.ul))))))))


(defopcode asrw_reg_by_8
  (list 68000 "1110000001000ddd" () (list "11nnnn0001000ddd"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_asrw_imm_reg_0_1")
	(list
	 (assign $2.dsw (>> $2.dsw 8))
	 $1.ul))
  (list "CN0XZ" "-----" dont_expand
	(native_code "xlate_asrw_imm_reg_0_1")
	(list
	 (assign ccx (assign ccc (& $2.dub 0x80)))
	 (ASSIGN_NNZ_WORD (assign $2.dsw (>> $2.dsw 8)))
	 $1.ul)))

(defopcode asrw_reg_by_const
  (list 68000 amode_implicit () (list "1110nnn001000ddd"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_asrw_imm_reg_0_1")
	(assign $2.dsw (>> $2.dsw $1.ul)))
  (list "CN0XZ" "-----" dont_expand
	(native_code "xlate_asrw_imm_reg_0_1")
	(list
	 (assign ccx (assign ccc (& (>> $2.duw (- $1.ul 1)) 1)))
	 (ASSIGN_NNZ_WORD (assign $2.dsw (>> $2.dsw $1.ul))))))

(defopcode asrl_reg
  (list 68000 amode_implicit () (list "1110ddd010100ddd"))
  (list "-----" "-----" dont_expand
	(list
	 (assign tmp.ul (& $1.dul 63))
	 (if (>= tmp.ul 32)     ; Only assume shift counts <= 31 are portable!
	     (assign $2.dsl (>> $2.dsl 31))
	     (assign $2.dsl (>> $2.dsl tmp.ul)))))
  (list "CN0%Z" "-----" dont_expand
	(list
	 (assign tmp.ul (& $1.dul 63))
	 (if (>= tmp.ul 32)
	     (assign ccnz
		     (assign ccn
			     (assign ccx
				     (assign ccc
					     (<> 0
						 (assign $2.dsl (>> $2.dsl 31)))))))
	     (list
	      (if (= tmp.ul 0)
		  (assign ccc 0)
		  (assign ccx (assign ccc (& (>> $2.dul (- tmp.ul 1)) 1))))
	      (ASSIGN_NNZ_LONG (assign $2.dsl (>> $2.dsl tmp.ul))))))))

(defopcode asrl_reg_by_8
  (list 68000 "1110000010000xxx" () (list "11nnnn0010000ddd"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_asrl_imm_reg_0_1")
	(list
	 (assign $2.dsl (>> $2.dsl 8))
	 $1.ul))
  (list "CN0XZ" "-----" dont_expand
	(native_code "xlate_asrl_imm_reg_0_1")
	(list
	 (assign ccx (assign ccc (& $2.dul 0x80)))
	 (ASSIGN_NNZ_LONG (assign $2.dsl (>> $2.dsl 8)))
	 $1.ul)))

(defopcode asrl_reg_by_const
  (list 68000 amode_implicit () (list "1110nnn010000ddd"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_asrl_imm_reg_0_1")
	(assign $2.dsl (>> $2.dsl $1.ul)))
  (list "CN0XZ" "-----" dont_expand
	(native_code "xlate_asrl_imm_reg_0_1")
	(list
	 (assign ccx (assign ccc (& (>> $2.dul (- $1.ul 1)) 1)))
	 (ASSIGN_NNZ_LONG (assign $2.dsl (>> $2.dsl $1.ul))))))


; lsr
(defopcode lsrw_ea
  (list 68000 amode_alterable_memory () (list "1110001011mmmmmm"))
  (list "-----" "-----" dont_expand      ; Why bother expanding this?
	(assign $1.muw (>> $1.muw 1)))
  (list "CN0XZ" "-----" dont_expand
	(list
	 (assign ccx (assign ccc (& $1.muw 1)))
	 (ASSIGN_NNZ_WORD (assign $1.muw (>> $1.muw 1))))))

(defopcode lsrb_reg
  (list 68000 amode_implicit () (list "1110ddd000101ddd"))
  (list "-----" "-----" dont_expand
	(list
	 (assign tmp.ul (& $1.dul 63))
	 (if (>= tmp.ul 8)       ; Can only assume shift counts 0-31 portable!
	     (assign $2.dub 0)
	     (assign $2.dub (>> $2.dub tmp.ul)))))
  (list "CN0%Z" "-----" dont_expand
	(list
	 (assign tmp.ul (& $1.dul 63))
	 (if (> tmp.ul 8)
	     (list
	      (SET_C_N_V_NZ 0 0 0 0)
	      (assign ccx 0)
	      (assign $2.dub 0))
	     (list
	      (if (= tmp.ul 0)
		  (assign ccc 0)
		  (assign ccx (assign ccc (& $2.dub (<< 1 (- tmp.ul 1))))))
	      (ASSIGN_NNZ_BYTE (assign $2.dub (>> $2.dub tmp.ul))))))))

(defopcode lsrb_reg_by_8
  (list 68000 amode_implicit () (list "1110000000001ddd"))
  (list "-----" "-----" dont_expand
	(assign $1.dub 0))
  (list "CN0XZ" "-----" dont_expand
	(list
	 (assign ccx (assign ccc (& $1.dub 0x80)))
	 (SET_N_NZ 0 0)
	 (assign $1.dub 0))))

(defopcode lsrb_reg_by_const
  (list 68000 amode_implicit () (list "1110nnn000001ddd"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_lsrb_imm_reg_0_1")
	(assign $2.dub (>> $2.dub $1.ul)))
  (list "CN0XZ" "-----" dont_expand
	(native_code "xlate_lsrb_imm_reg_0_1")
	(list
	 (assign ccx (assign ccc (& $2.dub (<< 1 (- $1.ul 1)))))
	 (ASSIGN_NNZ_BYTE (assign $2.dub (>> $2.dub $1.ul))))))

(defopcode lsrw_reg
  (list 68000 amode_implicit () (list "1110ddd001101ddd"))
  (list "-----" "-----" dont_expand
	(list
	 (assign tmp.ul (& $1.dul 63))
	 (if (>= tmp.ul 16)
	     (assign $2.duw 0)
	     (assign $2.duw (>> $2.duw tmp.ul)))))
  (list "CN0%Z" "-----" dont_expand
	(list
	 (assign tmp.ul (& $1.dul 63))
	 (if (> tmp.ul 16)
	     (list
	      (SET_C_N_V_NZ 0 0 0 0)
	      (assign ccx 0)
	      (assign $2.duw 0))
	     (list
	      (if (= tmp.ul 0)
		  (assign ccc 0)
		  (assign ccx (assign ccc (& (>> $2.duw (- tmp.ul 1)) 1))))
	      (ASSIGN_NNZ_WORD (assign $2.duw (>> $2.duw tmp.ul))))))))


(defopcode lsrw_reg_by_8
  (list 68000 "1110000001001xxx" () (list "11nnnn0001001ddd"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_lsrw_imm_reg_0_1")
	(list
	 (assign $2.duw (>> $2.duw 8))
	 $1.ul))
  (list "CN0XZ" "-----" dont_expand
	(native_code "xlate_lsrw_imm_reg_0_1")
	(list
	 (assign ccx (assign ccc (& $2.dul 0x80)))
	 (ASSIGN_NNZ_WORD (assign $2.duw (>> $2.duw 8)))
	 $1.ul)))

(defopcode lsrw_reg_by_const
  (list 68000 amode_implicit () (list "1110nnn001001ddd"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_lsrw_imm_reg_0_1")
	(assign $2.duw (>> $2.duw $1.ul)))
  (list "CN0XZ" "-----" dont_expand
	(native_code "xlate_lsrw_imm_reg_0_1")
	(list
	 (assign ccx (assign ccc (& (>> $2.duw (- $1.ul 1)) 1)))
	 (ASSIGN_NNZ_WORD (assign $2.duw (>> $2.duw $1.ul))))))

(defopcode lsrl_reg
  (list 68000 amode_implicit () (list "1110ddd010101ddd"))
  (list "-----" "-----" dont_expand
	(list
	 (assign tmp.ul (& $1.dul 63))
	 (if (>= tmp.ul 32)     ; Only assume shift counts <= 31 are portable!
	     (assign $2.dul 0)
	     (assign $2.dul (>> $2.dul tmp.ul)))))
  (list "CN0%Z" "-----" dont_expand
	(list
	 (assign tmp.ul (& $1.dul 63))
	 (if (>= tmp.ul 32)
	     (list
	      (if (= tmp.ul 32)
		  (assign ccc (>> $2.dul 31))
		  (assign ccc 0))
	      (assign ccx ccc)
	      (SET_N_NZ 0 0)
	      (assign $2.dul 0))
	     (list
	      (if (= tmp.ul 0)
		  (assign ccc 0)
		  (assign ccx (assign ccc (& (>> $2.dul (- tmp.ul 1)) 1))))
	      (ASSIGN_NNZ_LONG (assign $2.dul (>> $2.dul tmp.ul))))))))

(defopcode lsrl_reg_by_8
  (list 68000 "1110000010001xxx" () (list "11nnnn0010001ddd"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_lsrl_imm_reg_0_1")
	(list
	 (assign $2.dul (>> $2.dul 8))
	 $1.ul))
  (list "CN0XZ" "-----" dont_expand
	(native_code "xlate_lsrl_imm_reg_0_1")
	(list
	 (assign ccx (assign ccc (& $2.dul 0x80)))
	 (ASSIGN_NNZ_LONG (assign $2.dul (>> $2.dul 8)))
	 $1.ul)))

(defopcode lsrl_reg_by_const
  (list 68000 amode_implicit () (list "1110nnn010001ddd"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_lsrl_imm_reg_0_1")
	(assign $2.dul (>> $2.dul $1.ul)))
  (list "CN0XZ" "-----" dont_expand
	(native_code "xlate_lsrl_imm_reg_0_1")
	(list
	 (assign ccx (assign ccc (& (>> $2.dul (- $1.ul 1)) 1)))
	 (ASSIGN_NNZ_LONG (assign $2.dul (>> $2.dul $1.ul))))))


(define (CONDL_BRANCH name namestr bit_pattern cc_needed branch_expr)
  (defopcode name
    (list 68000 amode_implicit (ends_block) (list bit_pattern))
    (list "-----" cc_needed dont_expand
	  (native_code "xlate_" namestr)
	  (if (not branch_expr) ; cause gcc to put branch taken case first (?)
	      (assign code (deref "uint16 **" code 0))
	      (assign code (deref "uint16 **" code 1))))))

; Conditional branches
(CONDL_BRANCH bhi "bhi" "01100010xxxxxxxx" "C---Z" CC_HI)
(CONDL_BRANCH bls "bls" "01100011xxxxxxxx" "C---Z" CC_LS)
(CONDL_BRANCH bcc "bcc" "01100100xxxxxxxx" "C----" CC_CC)
(CONDL_BRANCH bcs "bcs" "01100101xxxxxxxx" "C----" CC_CS)
(CONDL_BRANCH bne "bne" "01100110xxxxxxxx" "----Z" CC_NE)
(CONDL_BRANCH beq "beq" "01100111xxxxxxxx" "----Z" CC_EQ)
(CONDL_BRANCH bvc "bvc" "01101000xxxxxxxx" "--V--" CC_VC)
(CONDL_BRANCH bvs "bvs" "01101001xxxxxxxx" "--V--" CC_VS)
(CONDL_BRANCH bpl "bpl" "01101010xxxxxxxx" "-N---" CC_PL)
(CONDL_BRANCH bmi "bmi" "01101011xxxxxxxx" "-N---" CC_MI)
(CONDL_BRANCH bge "bge" "01101100xxxxxxxx" "-NV--" CC_GE)
(CONDL_BRANCH blt "blt" "01101101xxxxxxxx" "-NV--" CC_LT)
(CONDL_BRANCH bgt "bgt" "01101110xxxxxxxx" "-NV-Z" CC_GT)
(CONDL_BRANCH ble "ble" "01101111xxxxxxxx" "-NV-Z" CC_LE)

(defopcode bra
  (list 68000 amode_implicit (ends_block)
	(list "01100000xxxxxxxx"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_jmp")
	(assign code (deref "uint16 **" code 0))))

; bchg
(defopcode bchg_dynamic_reg
  (list 68000 amode_implicit () (list "0000ddd101000ddd"))
  (list "-----" "-----" dont_expand
	(assign $2.dul (^ $2.dul (<< 1 (& $1.dul 31)))))
  (list "----Z" "-----" dont_expand
	(list
	 (assign tmp.ul (<< 1 (& $1.dul 31)))
	 "\n#ifdef CCR_ELEMENT_8_BITS\n"
	 (assign ccnz (<> 0 (& $2.dul tmp.ul)))
	 "\n#else\n"
	 (assign ccnz (& $2.dul tmp.ul))
	 "\n#endif\n"
	 (assign $2.dul (^ $2.dul tmp.ul)))))

(defopcode bchg_dynamic_ea
  (list 68000 amode_alterable_memory () (list "0000ddd101mmmmmm"))
  (list "-----" "-----" dont_expand
	(assign $2.mub (^ $2.mub (<< 1 (& $1.dul 7)))))
  (list "----Z" "-----" dont_expand
	(list
	 (assign tmp.ub (<< 1 (& $1.dul 7)))
	 (assign ccnz (& $2.mub tmp.ub))
	 (assign $2.mub (^ $2.mub tmp.ub)))))

(defopcode bchg_static_reg
  (list 68000 amode_implicit ()
	(list "0000100001000ddd" "00000000000bbbbb"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_bchg_imm_reg_1_0")
	(assign $1.dul (^ $1.dul (<< 1 $2.uw))))
  (list "----Z" "-----" dont_expand
	(list
	 (assign tmp.ul (<< 1 $2.uw))
	 "\n#ifdef CCR_ELEMENT_8_BITS\n"
	 (assign ccnz (<> 0 (& $1.dul tmp.ul)))
	 "\n#else\n"
	 (assign ccnz (& $1.dul tmp.ul))
	 "\n#endif\n"
	 (assign $1.dul (^ $1.dul tmp.ul)))))

(defopcode bchg_static_ea
  (list 68000 amode_alterable_memory ()
	(list "0000100001mmmmmm" "0000000000000bbb"))
  (list "-----" "-----" dont_expand
	(assign $1.mub (^ $1.mub (<< 1 $2.uw))))
  (list "----Z" "-----" dont_expand
	(list
	 (assign tmp.ub (<< 1 $2.uw))
	 (assign ccnz (& $1.mub tmp.ub))
	 (assign $1.mub (^ $1.mub tmp.ub)))))


; bset
(defopcode bset_dynamic_reg
  (list 68000 amode_implicit () (list "0000ddd111000ddd"))
  (list "-----" "-----" dont_expand
	(assign $2.dul (| $2.dul (<< 1 (& $1.dul 31)))))
  (list "----Z" "-----" dont_expand
	(list
	 (assign tmp.ul (<< 1 (& $1.dul 31)))
	 "\n#ifdef CCR_ELEMENT_8_BITS\n"
	 (assign ccnz (<> 0 (& $2.dul tmp.ul)))
	 "\n#else\n"
	 (assign ccnz (& $2.dul tmp.ul))
	 "\n#endif\n"
	 (assign $2.dul (| $2.dul tmp.ul)))))

(defopcode bset_dynamic_ea
  (list 68000 amode_alterable_memory () (list "0000ddd111mmmmmm"))
  (list "-----" "-----" dont_expand
	(assign $2.mub (| $2.mub (<< 1 (& $1.dul 7)))))
  (list "----Z" "-----" dont_expand
	(list
	 (assign tmp.ub (<< 1 (& $1.dul 7)))
	 (assign ccnz (& $2.mub tmp.ub))
	 (assign $2.mub (| $2.mub tmp.ub)))))

(defopcode bset_static_reg
  (list 68000 amode_implicit ()
	(list "0000100011000ddd" "00000000000bbbbb"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_bset_imm_reg_1_0")
	(assign $1.dul (| $1.dul (<< 1 $2.ul))))
  (list "----Z" "-----" dont_expand
	(list
	 (assign tmp.ul (<< 1 $2.ul))
	 "\n#ifdef CCR_ELEMENT_8_BITS\n"
	 (assign ccnz (<> 0 (& $1.dul tmp.ul)))
	 "\n#else\n"
	 (assign ccnz (& $1.dul tmp.ul))
	 "\n#endif\n"
	 (assign $1.dul (| $1.dul tmp.ul)))))

(defopcode bset_static_ea
  (list 68000 amode_alterable_memory ()
	(list "0000100011mmmmmm" "0000000000000bbb"))
  (list "-----" "-----" dont_expand
	(assign $1.mub (| $1.mub (<< 1 $2.ul))))
  (list "----Z" "-----" dont_expand
	(list
	 (assign tmp.ub (<< 1 $2.ul))
	 (assign ccnz (& $1.mub tmp.ub))
	 (assign $1.mub (| $1.mub tmp.ub)))))


; bclr
(defopcode bclr_dynamic_reg
  (list 68000 amode_implicit () (list "0000ddd110000ddd"))
  (list "-----" "-----" dont_expand
	(assign $2.dul (& $2.dul (~ (<< 1 (& $1.dul 31))))))
  (list "----Z" "-----" dont_expand
	(list
	 (assign tmp.ul (<< 1 (& $1.dul 31)))
	 "\n#ifdef CCR_ELEMENT_8_BITS\n"
	 (assign ccnz (<> 0 (& $2.dul tmp.ul)))
	 "\n#else\n"
	 (assign ccnz (& $2.dul tmp.ul))
	 "\n#endif\n"
	 (assign $2.dul (& $2.dul (~ tmp.ul))))))

(defopcode bclr_dynamic_ea
  (list 68000 amode_alterable_memory () (list "0000ddd110mmmmmm"))
  (list "-----" "-----" dont_expand
	(assign $2.mub (& $2.mub (~ (<< 1 (& $1.dul 7))))))
  (list "----Z" "-----" dont_expand
	(list
	 (assign tmp.ub (<< 1 (& $1.dul 7)))
	 (assign ccnz (& $2.mub tmp.ub))
	 (assign $2.mub (& $2.mub (~ tmp.ub))))))

(defopcode bclr_static_reg
  (list 68000 amode_implicit ()
	(list "0000100010000ddd" "00000000000bbbbb"))
  (list "-----" "-----" dont_expand;
	(native_code "xlate_bclr_imm_reg_1_0")
	(assign $1.dul (& $1.dul (~ (<< 1 $2.ul)))))
  (list "----Z" "-----" dont_expand
	(list
	 (assign tmp.ul (<< 1 $2.ul))
	 "\n#ifdef CCR_ELEMENT_8_BITS\n"
	 (assign ccnz (<> 0 (& $1.dul tmp.ul)))
	 "\n#else\n"
	 (assign ccnz (& $1.dul tmp.ul))
	 "\n#endif\n"
	 (assign $1.dul (& $1.dul (~ tmp.ul))))))

(defopcode bclr_static_ea
  (list 68000 amode_alterable_memory ()
	(list "0000100010mmmmmm" "0000000000000bbb"))
  (list "-----" "-----" dont_expand
	(assign $1.mub (& $1.mub (~ (<< 1 $2.uw)))))
  (list "----Z" "-----" dont_expand
	(list
	 (assign tmp.ub (<< 1 $2.uw))
	 (assign ccnz (& $1.mub tmp.ub))
	 (assign $1.mub (& $1.mub (~ tmp.ub))))))


; btst
(defopcode btst_dynamic_reg
  (list 68000 amode_implicit () (list "0000ddd100000ddd"))
  (list "----Z" "-----" dont_expand
	(assign ccnz (& (>> $2.dul (& $1.dul 31)) 1))))

(defopcode btst_dynamic_ea
  (list 68000 (intersect amode_data (not amode_immediate))
	() (list "0000ddd100mmmmmm"))
  (list "----Z" "-----" dont_expand
	(assign ccnz (& $2.mub (<< 1 (& $1.dul 7))))))

(defopcode btst_static_reg
  (list 68000 amode_implicit ()
	(list "0000100000000ddd" "00000000000bbbbb"))
  (list "----Z" "-----" dont_expand
	(native_code "xlate_btst_imm_reg_1_0")
	(assign ccnz (& (>> $1.dul $2.ul) 1))))

(defopcode btst_static_ea
  (list 68000 (intersect amode_data (not amode_immediate))
	() (list "0000100000mmmmmm" "0000000000000bbb"))
  (list "----Z" "-----" dont_expand
	(assign ccnz (& $1.mub (<< 1 $2.ul)))))


(define (compute_bf_width_and_offset off wid offdst widdst mask_offset)
  
  ; Place width in widdst
  (if (& wid 0x20)
      (assign widdst (call "DATA_REGISTER" (& wid 7) ".ul.n"))
      (assign widdst wid))
  (assign widdst (+ (& (- widdst 1) 0x1F) 1)) ; Take low 5 bits & map 0 to 32
  
  ; Place offset in offdst
  (if (& off 0x20)
      (list
       (assign offdst (call "DATA_REGISTER" (& off 7) ".sl.n"))
       (if mask_offset
	   (assign offdst (& offdst 0x1F))))
      (assign offdst off offmsk)))

(define (compute_bf_mask_for_reg offset width dst)
  (if (= width 32)
      (assign dst (~ 0))  ; Avoid shifting by 32 because it's not portable
      (list
       (if (> (+ offset width) 32)
	   (assign dst (~ (<< (- (<< 1 (- 32 width)) 1) (- 32 offset))))
	   (assign dst (<< (- (<< 1 width) 1) (- 32 offset width)))))))


(defopcode bfffo_reg
  (list 68020 amode_implicit () (list "1110110111000ddd" "0dddzzzzzzwwwwww"))
  (list "0N0-Z" "-----" dont_expand
	(list
	 "{ uint32 width; int32 offset"
	 (compute_bf_width_and_offset $3.uw $4.uw "offset" "width" 0)
	 (assign tmp3.sl "offset")
	 (assign "offset" (& "offset" 0x1F))
	 (assign tmp.ul $1.dul)
	 (if (> (+ "offset" "width") 32)
	     (assign tmp.ul (| (<< tmp.ul (+ "offset" "width" -32))
			       (>> tmp.ul (- 64 "offset" "width"))))
	     (assign tmp.ul (>> tmp.ul (- 32 "offset" "width"))))
	 (if (<> "width" 32)
	     (assign tmp.ul (& tmp.ul (- (<< 1 "width") 1))))
	 (assign tmp2.ul (<< 1 (- "width" 1)))
	 (assign ccnz (<> 0 tmp.ul))
	 (assign ccn (<> 0 (& tmp.ul tmp2.ul)))
	 "for (; tmpul2 != 0; tmpul2 >>= 1) {"
	 (if (& tmp2.ul tmp.ul)
	     "break"
	     (assign tmp3.sl (+ tmp3.sl 1)))
	 "}"
	 (assign $2.dsl tmp3.sl)
	 "}")))

(defopcode bfffo_mem
  (list 68020 amode_control () (list "1110110111mmmmmm" "0dddzzzzzzwwwwww"))
  (list "0N0-Z" "-----" dont_expand
	(list
	 "{ uint32 width, ptr; int32 offset"
	 (compute_bf_width_and_offset $3.uw $4.uw "offset" "width" 0)
	 (assign tmp3.sl "offset")
	 (assign "ptr" $1.pub)
	 
	 ; Adjust pointer/offset so we have 0 <= offset <= 7
	 (assign "ptr" (+ "ptr" (>> "offset" 3)))
	 (assign "offset" (& "offset" 7))
	 
	 ; Align pointer on a long
	 (assign "offset" (+ "offset" (<< (& "ptr" 3) 3)))
	 (assign "ptr" (& "ptr" (~ 3)))
	 
	 ; Grab bits into tmp.ul
	 (assign tmp.ul (dereful "ptr"))
	 (if (> (+ "offset" "width") 32)
	     (assign tmp.ul (| (<< tmp.ul (+ "offset" "width" -32))
			       (>> (dereful (+ "ptr" 4))
				   (- 64 "offset" "width"))))
	     (assign tmp.ul (>> tmp.ul (- 32 "offset" "width"))))
	 (if (<> "width" 32)
	     (assign tmp.ul (& tmp.ul (- (<< 1 "width") 1))))
	 
	 ; Compute cc bits
	 (assign tmp2.ul (<< 1 (- "width" 1)))
	 (assign ccnz (<> 0 tmp.ul))
	 (assign ccn (<> 0 (& tmp.ul tmp2.ul)))
	 
	 ; Find first bit set
	 "for (; tmpul2 != 0; tmpul2 >>= 1) {"
	 (if (& tmp2.ul tmp.ul)
	     "break"
	     (assign tmp3.sl (+ tmp3.sl 1)))
	 "}"
	 
	 ; Write the result out to the dest register
	 (assign $2.dsl tmp3.sl)
	 "}")))

(define (BF_REG name bit_pattern tmpbits final)
  (defopcode name
    (list 68020 amode_implicit () (list bit_pattern "0000oooooowwwwww"))
    (list "0N0-Z" "-----" dont_expand
	  (list
	   "{ uint32 width; int32 offset"
	   (compute_bf_width_and_offset $2.uw $3.uw "offset" "width" 1)
	   "\n#ifdef CCR_ELEMENT_8_BITS\n"
	   (assign ccn (<> 0 (& $1.dul (>> (cast "uint32" 0x80000000)
					   "offset"))))
	   "\n#else\n"
	   (assign ccn (& $1.dul (>> (cast "uint32" 0x80000000) "offset")))
	   "\n#endif\n"
	   (compute_bf_mask_for_reg "offset" "width" tmpbits)
	   "\n#ifdef CCR_ELEMENT_8_BITS\n"
	   (assign ccnz (<> 0 (& tmpbits $1.dul)))
	   "\n#else\n"
	   (assign ccnz (& tmpbits $1.dul))
	   "\n#endif\n"
	   final
	   "}"))))

(BF_REG bftst_reg "1110100011000ddd" tmp3.ul (list))
(BF_REG bfchg_reg "1110101011000ddd" tmp3.ul
	(assign $1.dul (^ $1.dul tmp3.ul)))
(BF_REG bfset_reg "1110111011000ddd" tmp3.ul
	(assign $1.dul (| $1.dul tmp3.ul)))
(BF_REG bfclr_reg "1110110011000ddd" tmp3.ul
	(assign $1.dul (& $1.dul (~ tmp3.ul))))

(define (BF_MEM name bit_pattern final1 final2)
  (defopcode name
    (list 68020 amode_control () (list bit_pattern "0000oooooowwwwww"))
    (list "0N0-Z" "-----" dont_expand
	  (list
	   "{ uint32 width, ptr; int32 offset"
	   (compute_bf_width_and_offset $2.uw $3.uw "offset" "width" 0)
	   (assign "ptr" $1.pub)
	   
	   ; Adjust pointer/offset so we have 0 <= offset <= 7
	   (assign "ptr" (+ "ptr" (>> "offset" 3)))
	   (assign "offset" (& "offset" 7))
	   
	   ; Align pointer on a long
	   (assign "offset" (+ "offset" (<< (& "ptr" 3) 3)))
	   (assign "ptr" (& "ptr" (~ 3)))

	   (if (> (+ "offset" "width") 32)
	       (list
		"{ uint32 mask1, mask2"
		(assign "mask1" (- (<< 1 (- 32 "offset")) 1))
		(assign "mask2" (>> (~ 0x7FFFFFFF) (+ "offset" "width" -33)))
		(assign tmp.ul (dereful "ptr"))
		(assign tmp2.ul (dereful (+ "ptr" 4)))
		(assign ccnz (<> 0 (| (& tmp.ul "mask1") (& tmp2.ul "mask2"))))
		(assign ccn (<> 0 (& (>> (cast "uint32" 0x80000000) "offset")
				     tmp.ul)))
		final1
		"}")
	       (list
		"{ uint32 mask"
		(if (= "width" 32)
		    (assign "mask" (~ 0))
		    (assign "mask" (<< (- (<< 1 "width") 1)
				       (- 32 "offset" "width"))))
		(assign tmp.ul (dereful "ptr"))
		(assign ccnz (<> 0 (& "mask" tmp.ul)))
		(assign ccn (<> 0
				(& tmp.ul
				   (>> (cast "uint32" 0x80000000) "offset"))))
		final2
		"}"))
	   "}"))))


(BF_MEM bftst_mem "1110100011mmmmmm" (list) (list))
(BF_MEM bfchg_mem "1110101011mmmmmm"
	(list
	 (assign (dereful "ptr") (^ tmp.ul "mask1"))
	 (assign (dereful (+ "ptr" 4)) (^ tmp2.ul "mask2")))
	(assign (dereful "ptr") (^ "mask" tmp.ul)))
(BF_MEM bfset_mem "1110111011mmmmmm"
	(list
	 (assign (dereful "ptr") (| tmp.ul "mask1"))
	 (assign (dereful (+ "ptr" 4)) (| tmp2.ul "mask2")))
	(assign (dereful "ptr") (| "mask" tmp.ul)))
(BF_MEM bfclr_mem "1110110011mmmmmm"
	(list
	 (assign (dereful "ptr") (& tmp.ul (~ "mask1")))
	 (assign (dereful (+ "ptr" 4)) (& tmp2.ul (~ "mask2"))))
	(assign (dereful "ptr") (& tmp.ul (~ "mask"))))

(define (BFEXT_REG name bit_pattern compute cc)
  (defopcode name
    (list 68020 amode_implicit () (list bit_pattern "0dddzzzzzzwwwwww"))
    (list "0N0-Z" "-----" dont_expand
	  (list
	   "{ uint32 width; int32 offset"
	   (compute_bf_width_and_offset $3.uw $4.uw "offset" "width" 1)
	   (assign tmp.ul $1.dul)
	   compute
	   cc
	   (assign $2.dsl tmp.sl)
	   "}"))))

(BFEXT_REG bfexts_reg "1110101111000ddd"
	   (list
	    (if (> (+ "offset" "width") 32)
		(assign tmp.ul (| (<< tmp.ul "offset")
				  (>> tmp.ul (- 32 "offset"))))
		(assign tmp.ul (<< tmp.ul "offset")))
	    (assign tmp.sl (>> tmp.sl (- 32 "width"))))
	   (ASSIGN_NNZ_LONG tmp.ul))


(BFEXT_REG bfextu_reg "1110100111000ddd"
	   (list
	    (if (> (+ "offset" "width") 32)
		(assign tmp.ul (| (<< tmp.ul (+ "offset" "width" -32))
				  (>> tmp.ul (- 64 "offset" "width"))))
		(assign tmp.ul (>> tmp.ul (- 32 "offset" "width"))))
	    (if (<> "width" 32)
		(assign tmp.ul (& tmp.ul (- (<< 1 "width") 1)))))
	   (list
	    "\n#ifdef CCR_ELEMENT_8_BITS\n"
	    (assign ccnz (<> 0 tmp.ul))
	    (assign ccn (& (>> tmp.ul (- "width" 1)) 1))
	    "\n#else\n"
	    (assign ccn (& (assign ccnz tmp.ul) (<< 1 (- "width" 1))))
	    "\n#endif\n"))


(define (BFEXT_MEM name bit_pattern compute cc)
  (defopcode name
    (list 68020 amode_control () (list bit_pattern "0dddzzzzzzwwwwww"))
    (list "0N0-Z" "-----" dont_expand
	  (list
	   "{ uint32 width, ptr; int32 offset"
	   (compute_bf_width_and_offset $3.uw $4.uw "offset" "width" 0)
	   (assign "ptr" $1.pub)
	   
	   ; Adjust pointer/offset so we have 0 <= offset <= 7
	   (assign "ptr" (+ "ptr" (>> "offset" 3)))
	   (assign "offset" (& "offset" 7))
	   
	   ; Align pointer on a long
	   (assign "offset" (+ "offset" (<< (& "ptr" 3) 3)))
	   (assign "ptr" (& "ptr" (~ 3)))

	   compute
	   cc

	   (assign $2.dsl tmp.sl)
	   "}"))))

(BFEXT_MEM bfexts_mem "1110101111mmmmmm"
	   (list
	    (if (> (+ "offset" "width") 32)
		(assign tmp.ul (| (<< (dereful "ptr") "offset")
				  (>> (dereful (+ "ptr" 4)) (- 32 "offset"))))
		(assign tmp.ul (<< (dereful "ptr") "offset")))
	    (assign tmp.sl (>> tmp.sl (- 32 "width"))))
	   (ASSIGN_NNZ_LONG tmp.ul))

(BFEXT_MEM bfextu_mem "1110100111mmmmmm"
	   (list
	    (if (> (+ "offset" "width") 32)
		(assign tmp.ul (| (<< (dereful "ptr") (+ "offset" "width" -32))
				  (>> (dereful (+ "ptr" 4))
				      (- 64 "offset" "width"))))
		(assign tmp.ul (>> (dereful "ptr") (- 32 "offset" "width"))))
	    (if (<> "width" 32)
		(assign tmp.ul (& tmp.ul (- (<< 1 "width") 1)))))
	   (list
	    "\n#ifdef CCR_ELEMENT_8_BITS\n"
	    (assign ccnz (<> 0 tmp.ul))
	    (assign ccn (>> tmp.ul (- "width" 1)))
	    "\n#else\n"
	    (assign ccn (>> (assign ccnz tmp.ul) (- "width" 1)))
	    "\n#endif\n"))


(defopcode bfins_reg
  (list 68020 amode_implicit () (list "1110111111000ddd" "0dddzzzzzzwwwwww"))
  (list "0N0-Z" "-----" dont_expand
	(list
	 "{ uint32 width; int32 offset"
	 (compute_bf_width_and_offset $3.uw $4.uw "offset" "width" 1)
	 (assign tmp.ul $2.dul)
	 (if (> (+ "offset" "width") 32)
	     (assign tmp.ul (| (>> tmp.ul (+ "offset" "width" -32))
			       (<< tmp.ul (- 64 "offset" "width"))))
	     (assign tmp.ul (<< tmp.ul (- 32 "offset" "width"))))
	 (compute_bf_mask_for_reg "offset" "width" tmp2.ul)
	 (assign tmp.ul (& tmp.ul tmp2.ul))
	 "\n#ifdef CCR_ELEMENT_8_BITS\n"
	 (assign ccnz (<> 0 tmp.ul))
	 (assign ccn (<> 0
			 (& tmp.ul (>> (cast "uint32" 0x80000000) "offset"))))
	 "\n#else\n"
	 (assign ccn (& (assign ccnz tmp.ul)
			(>> (cast "uint32" 0x80000000) "offset")))
	 "\n#endif\n"
	 (assign $1.dul (| (& $1.dul (~ tmp2.ul)) tmp.ul))
	 "}")))


(defopcode bfins_mem
  (list 68020 amode_control () (list "1110111111mmmmmm" "0dddzzzzzzwwwwww"))
  (list "0N0-Z" "-----" dont_expand
	(list
	 "{ uint32 width, ptr; int32 offset"
	 (compute_bf_width_and_offset $3.uw $4.uw "offset" "width" 0)
	 (assign "ptr" $1.pub)
	 
	 ; Adjust pointer/offset so we have 0 <= offset <= 7
	 (assign "ptr" (+ "ptr" (>> "offset" 3)))
	 (assign "offset" (& "offset" 7))
	 
	 ; Align pointer on a long
	 (assign "offset" (+ "offset" (<< (& "ptr" 3) 3)))
	 (assign "ptr" (& "ptr" (~ 3)))
	 
	 ; Fetch the value to insert and mask out other bits
	 (assign tmp.ul $2.dul)
	 (if (<> "width" 32)
	     (assign tmp.ul (& tmp.ul (- (<< 1 "width") 1))))
	 
	 ; Set cc bits
	 (assign ccnz (<> 0 tmp.ul))
	 (assign ccn (& (>> tmp.ul (- "width" 1)) 1))
	 
	 (assign tmp2.ul (dereful "ptr"))
	 (if (>= (+ "offset" "width") 32)
	     (list
	      (assign tmp2.ul (& tmp2.ul
				 (~ (>> (cast "uint32" (~ 0)) "offset"))))
	      (assign tmp2.ul (| tmp2.ul (>> tmp.ul (+ "offset" "width" -32))))
	      (assign (dereful "ptr") tmp2.ul)
	      (assign tmp2.ul (dereful (+ "ptr" 4)))
	      (assign tmp2.ul (& tmp2.ul (>> (cast "uint32" (~ 0))
					     (+ "offset" "width" -32))))
	      (if (<> 32 (+ "offset" "width"))
		  (assign tmp2.ul (| tmp2.ul
				     (<< tmp.ul (- 64 "offset" "width")))))
	      (assign (dereful (+ "ptr" 4)) tmp2.ul))
	     (list
	      (assign tmp2.ul (& tmp2.ul (~ (<< (- (<< 1 "width") 1)
						(- 32 "offset" "width")))))
	      (assign tmp2.ul (| tmp2.ul (<< tmp.ul (- 32 "offset" "width"))))
	      (assign (dereful "ptr") tmp2.ul)))
	 "}")))

(defopcode bkpt
  (list 68000 amode_implicit (ends_block next_block_dynamic)
	(list "0100100001001vvv"))
  (list "-----" "-----" dont_expand
	(TRAP 4 (deref "uint32 *" code 0) 0)))

(define (CAS2 name bit_pattern deref_type mem1 mem2 tmp elt sgnmask c1 c2)
  (defopcode name
    (list 68020 amode_implicit () bit_pattern)
    (list "CNV-Z" "-----" dont_expand
	  (list
	   "{ uint32 ureg1, ureg2, mreg1, mreg2"
	   
	   ; Extract out the register numbers from the operands
	   (assign "ureg1" (call "DATA_REGISTER" (& $1.ul 7) elt))
	   (assign "mreg1" (call "GENERAL_REGISTER" (>> $1.ul 6) ".ul.n"))
	   (assign "ureg2" (call "DATA_REGISTER" (& $3.ul 7) elt))
	   (assign "mreg2" (call "GENERAL_REGISTER" (>> $3.ul 6) ".ul.n"))
	   
	   ; Fetch memory operands
	   (assign mem1 (deref_type "mreg1"))
	   (assign mem2 (deref_type "mreg2"))
	   
	   (assign tmp c1)
	   (if (= mem1 tmp)
	       (list
		(assign tmp c2)
		(if (= mem2 tmp)
		    
		    ; Both comparisons equal
		    (list
		     
		     ; Set cc bits accordingly
		     (assign ccnz (assign ccn (assign ccv (assign ccc 0))))
		     
		     ; Write update operands to memory operands
		     (assign (deref_type "mreg1") "ureg1")
		     (assign (deref_type "mreg2") "ureg2"))
		    
		    ; Second comparison failed
		    (list
		     ; Set cc bits accordingly
		     (assign ccnz 1)
		     (assign ccc (> tmp mem2))
		     (assign tmp4.ul (& (^ mem2 tmp) sgnmask))
		     (assign tmp (- mem2 tmp))
		     (assign ccn (<> 0 (& sgnmask tmp)))
		     (assign ccv (<> 0 (& tmp4.ul (^ tmp mem2))))
		     
		     ; Update compare operands with memory operands
		     (assign c1 mem1)
		     (assign c2 mem2))))
	       
	       ; First comparison failed
	       (list
		; Set cc bits accordingly
		(assign ccnz 1)
		(assign ccc (> tmp mem1))
		(assign tmp4.ul (& (^ mem1 tmp) sgnmask))
		(assign tmp (- mem1 tmp))
		(assign ccn (<> 0 (& sgnmask tmp)))
		(assign ccv (<> 0 (& tmp4.ul (^ tmp mem1))))
		
		; Update compare operands with memory operands
		(assign c1 mem1)
		(assign c2 mem2)))
	   
	   "}"
	   ))))

(CAS2 cas2w (list "0000110011111100" "rrrrrrrrrr000ccc" "rrrrrrrrrr000ccc")
      derefuw tmp.uw tmp2.uw tmp3.uw ".uw.n" 0x8000 $2.duw $4.duw)
(CAS2 cas2l (list "0000111011111100" "rrrrrrrrrr000ccc" "rrrrrrrrrr000ccc")
      dereful tmp.ul tmp2.ul tmp3.ul ".ul.n" 0x80000000 $2.dul $4.dul)

(define (CAS name bit_pattern cmp update ea tmp tmp2 mask)
  (defopcode name
    (list 68020 amode_alterable_memory () bit_pattern)
    (list "CNV-Z" "-----" dont_expand
	  (list
	   (assign tmp ea)
	   (assign ccc (> cmp tmp))
	   (assign tmp4.ul (& (^ tmp cmp) mask))
	   (assign ccnz (<> 0 (assign tmp2 (- tmp cmp))))
	   (assign ccn (<> 0 (& tmp2 mask)))
	   (assign ccv (<> 0 (& tmp4.ul (^ tmp tmp2))))
	   (if ccnz
	       (assign cmp ea)
	       (assign ea update))))))

(CAS casb (list "0000101011mmmmmm" "0000000uuu000ccc") $3.dub $2.dub $1.mub
     tmp.ub tmp2.ub 0x80)
(CAS casw (list "0000110011mmmmmm" "0000000uuu000ccc") $3.duw $2.duw $1.muw
     tmp.uw tmp2.uw 0x8000)
(CAS casl (list "0000111011mmmmmm" "0000000uuu000ccc") $3.dul $2.dul $1.mul
     tmp.ul tmp2.ul 0x80000000)



(define (CHK name bit_pattern dreg mem)
  (defopcode name
    (list 68000 amode_data (ends_block next_block_dynamic
				       skip_four_operand_words)
	  (list bit_pattern))
    (list "?N?-?" "-----" dont_expand
	  (list
	   (assign ccn (< dreg 0))
	   (if (or ccn (> dreg mem))
	       (TRAP 6 (deref "uint32 *" code 0) (deref "uint32 *" code 1))
	       (assign code (call "code_lookup "
				  (deref "uint32 *" code 0))))))))

(CHK chkw "0100ddd110mmmmmm" $1.dsw $2.msw)
(CHK chkl "0100ddd100mmmmmm" $1.dsl $2.msl)



(defopcode cmp2b/chk2b
  (list 68020 amode_control
	(ends_block next_block_dynamic skip_four_operand_words)
	(list "0000000011mmmmmm" "arrrt00000000000"))
  (list "C??-Z" "-----" dont_expand
	(list
	 (if $2.uw   ; Are we dealing with an address register?
	     (list
	      (assign tmp.sl (derefsb $1.psb))
	      (assign tmp2.sl (derefsb (+ $1.psb 1)))
	      (assign tmp3.ul $3.aul)
	      (assign ccnz (and (<> tmp3.ul tmp.ul) (<> tmp3.ul tmp2.ul)))
	      (assign ccc (> (- tmp3.ul tmp.ul) (- tmp2.ul tmp.ul))))
	     (list    ; Nope, its a data register.
	      (assign tmp.ub (derefub $1.pub))
	      (assign tmp2.ub (derefub (+ $1.pub 1)))
	      (assign tmp3.ub $3.dub)
	      (assign ccnz (and (<> tmp3.ub tmp.ub) (<> tmp3.ub tmp2.ub)))
	      (assign ccc (> (cast "uint8" (- tmp3.ub tmp.ub))
			     (cast "uint8" (- tmp2.ub tmp.ub))))))
	 (if (and $4.uw ccc)  ; Trap if carry set?
	     (TRAP 6 (deref "uint32 *" code 0) (deref "uint32 *" code 1))
	     (assign code (call "code_lookup "
				(deref "uint32 *" code 0)))))))

(defopcode cmp2w/chk2w
  (list 68020 amode_control
	(ends_block next_block_dynamic skip_four_operand_words)
	(list "0000001011mmmmmm" "arrrt00000000000"))
  (list "C??-Z" "-----" dont_expand
	(list
	 (if $2.uw   ; Are we dealing with an address register?
	     (list
	      (assign tmp.sl (derefsw $1.psw))
	      (assign tmp2.sl (derefsw (+ $1.psw 2)))
	      (assign tmp3.ul $3.aul)
	      (assign ccnz (and (<> tmp3.ul tmp.ul) (<> tmp3.ul tmp2.ul)))
	      (assign ccc (> (- tmp3.ul tmp.ul) (- tmp2.ul tmp.ul))))
	     (list    ; Nope, its a data register.
	      (assign tmp.uw (derefuw $1.puw))
	      (assign tmp2.uw (derefuw (+ $1.puw 2)))
	      (assign tmp3.uw $3.duw)
	      (assign ccnz (and (<> tmp3.uw tmp.uw) (<> tmp3.uw tmp2.uw)))
	      (assign ccc (> (cast "uint16" (- tmp3.uw tmp.uw))
			     (cast "uint16" (- tmp2.uw tmp.uw))))))
	 (if (and $4.uw ccc)  ; Trap if carry set?
	     (TRAP 6 (deref "uint32 *" code 0) (deref "uint32 *" code 1))
	     (assign code (call "code_lookup "
				(deref "uint32 *" code 0)))))))

(defopcode cmp2l/chk2l
  (list 68020 amode_control
	(ends_block next_block_dynamic skip_four_operand_words)
	(list "0000010011mmmmmm" "rrrrt00000000000"))
  (list "C??-Z" "-----" dont_expand
	(list
	 (assign tmp.ul (dereful $1.pul))
	 (assign tmp2.ul (dereful (+ $1.pul 4)))
	 (assign tmp3.ul $2.gul)
	 (assign ccnz (and (<> tmp3.ul tmp.ul) (<> tmp3.ul tmp2.ul)))
	 (assign ccc (> (- tmp3.ul tmp.ul) (- tmp2.ul tmp.ul)))
	 (if (and $3.uw ccc)  ; Trap if carry set?
	     (TRAP 6 (deref "uint32 *" code 0) (deref "uint32 *" code 1))
	     (assign code (call "code_lookup "
				(deref "uint32 *" code 0)))))))



(define (CLR name native size_bits amode_bits dst pre post)
  (defopcode name
    (list 68000 "0xxxxxxxxxxxxxxx" () (list "z1000010" size_bits amode_bits))
    (list "CNV-Z" "-----" dont_expand
	  (native_code native)
	  (list
	   pre
	   (assign dst 0)
	   post
	   (SET_C_N_V_NZ 0 0 0 0)
	   $1.ul))))  ; for native code

(define (CLR name native size_bits amode_bits dst)
  (CLR name native size_bits amode_bits dst (list) (list)))

(CLR clrb_reg "xlate_moveb_imm_reg_0_1" "00" "000ddd" $2.dub)
(CLR clrw_reg "xlate_movew_imm_reg_0_1" "01" "000ddd" $2.duw)
(CLR clrl_reg "xlate_movel_imm_reg_0_1" "10" "000ddd" $2.dul)

(CLR clrb_absw "xlate_moveb_imm_abs_0_1" "00" "111000aaaaaaaaaaaaaaaa"
     (derefub $2.sw))
(CLR clrw_absw "xlate_movew_imm_abs_0_1" "01" "111000aaaaaaaaaaaaaaaa"
     (derefuw $2.sw))
(CLR clrl_absw "xlate_movel_imm_abs_0_1" "10" "111000aaaaaaaaaaaaaaaa"
     (dereful $2.sw))

(CLR clrb_absl "xlate_moveb_imm_abs_0_1" "00"
     "111000aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" (derefub $2.ul))
(CLR clrw_absl "xlate_movew_imm_abs_0_1" "01"
     "111000aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" (derefuw $2.ul))
(CLR clrl_absl "xlate_movel_imm_abs_0_1" "10"
     "111000aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" (dereful $2.ul))

(CLR clrb_predec "xlate_moveb_imm_predec_0_1" "00" "100aaa"
     (derefub $2.aul)
     (if (= $2.ul 7)
	 (assign $2.aul (- $2.aul 2))
	 (assign $2.aul (- $2.aul 1)))
     (list))
(CLR clrw_predec "xlate_movew_imm_predec_0_1" "01" "100aaa"
     (derefuw $2.aul) (assign $2.aul (- $2.aul 2)) (list))
(CLR clrl_predec "xlate_movel_imm_predec_0_1" "10" "100aaa"
     (dereful $2.aul) (assign $2.aul (- $2.aul 4)) (list))

(CLR clrb_postinc "xlate_moveb_imm_postinc_0_1" "00" "011aaa"
     (derefub $2.aul)
     (list)
     (if (= $2.ul 7)
	 (assign $2.aul (+ $2.aul 2))
	 (assign $2.aul (+ $2.aul 1))))
(CLR clrw_postinc "xlate_movew_imm_postinc_0_1" "01" "011aaa"
     (derefuw $2.aul) (list) (assign $2.aul (+ $2.aul 2)))
(CLR clrl_postinc "xlate_movel_imm_postinc_0_1" "10" "011aaa"
     (dereful $2.aul) (list) (assign $2.aul (+ $2.aul 4)))

(CLR clrb_ind "xlate_moveb_imm_ind_0_1" "00" "010aaa" (derefub $2.aul))
(CLR clrw_ind "xlate_movew_imm_ind_0_1" "01" "010aaa" (derefuw $2.aul))
(CLR clrl_ind "xlate_movel_imm_ind_0_1" "10" "010aaa" (dereful $2.aul))

(CLR clrb_indoff "xlate_moveb_imm_indoff_0_2_1" "00"
     "101aaabbbbbbbbbbbbbbbb" (derefub (+ $2.asl $3.sw)))
(CLR clrw_indoff "xlate_movew_imm_indoff_0_2_1" "01"
     "101aaabbbbbbbbbbbbbbbb" (derefuw (+ $2.asl $3.sw)))
(CLR clrl_indoff "xlate_movel_imm_indoff_0_2_1" "10"
     "101aaabbbbbbbbbbbbbbbb" (dereful (+ $2.asl $3.sw)))

(defopcode clrb_indix
  (list 68000 "xxxxxxxxxx110xxx" () (list "0100001000aaaaaa"))
  (list "CNV-Z" "-----" dont_expand
	(native_code "xlate_moveb_zero_indix_0")
	(list
	 (assign $1.mub 0)
	 (SET_C_N_V_NZ 0 0 0 0))))
(defopcode clrw_indix
  (list 68000 "xxxxxxxxxx110xxx" () (list "0100001001aaaaaa"))
  (list "CNV-Z" "-----" dont_expand
	(native_code "xlate_movew_zero_indix_0")
	(list
	 (assign $1.muw 0)
	 (SET_C_N_V_NZ 0 0 0 0))))
(defopcode clrl_indix
  (list 68000 "xxxxxxxxxx110xxx" () (list "0100001010aaaaaa"))
  (list "CNV-Z" "-----" dont_expand
	(native_code "xlate_movel_zero_indix_0")
	(list
	 (assign $1.mul 0)
	 (SET_C_N_V_NZ 0 0 0 0))))


(defopcode clrb
  (list 68000 amode_alterable_data () (list "0100001000mmmmmm"))
  (list "-----" "-----" dont_expand
	(assign $1.mub 0))
  (list "000-1" "-----" dont_expand
	(assign $1.mub 0)))

(defopcode clrw
  (list 68000 amode_alterable_data () (list "0100001001mmmmmm"))
  (list "-----" "-----" dont_expand
	(assign $1.muw 0))
  (list "000-1" "-----" dont_expand
	(assign $1.muw 0)))

(defopcode clrl
  (list 68000 amode_alterable_data () (list "0100001010mmmmmm"))
  (list "-----" "-----" dont_expand
	(assign $1.mul 0))
  (list "000-1" "-----" dont_expand
	(assign $1.mul 0)))

(CMPB cmpmb_a7@+,a7@+ (list "1011111100001111") dont_expand
      amode_same_reg (derefub a7.ul) (derefub (+ a7.ul 2)) 
      "none"
      (list)
      (assign a7.ul (+ a7.ul 4)))
(CMPB cmpmb_an@+,a7@+ (list "1011111100001aaa") dont_expand
      amode_implicit (derefub $1.aul) (derefub a7.ul)
      "none"
      (list)
      (list
       (assign a7.ul (+ a7.ul 2))
       (assign $1.aul (+ $1.aul 1))))
(CMPB cmpmb_a7@+,an@+ (list "1011aaa100001111") dont_expand
      amode_implicit (derefub a7.ul) (derefub $1.aul)
      "none"
      (list)
      (list
       (assign a7.ul (+ a7.ul 2))
       (assign $1.aul (+ $1.aul 1))))

(CMPB cmpmb_an@+,an@+ (list "1011xxx100001yyy") dont_expand
      amode_same_reg (derefub $2.aul) (derefub (+ $1.aul 1))
      "xlate_cmpmb_postinc_postinc_1_0"
      (list)
      (assign $1.aul (+ $1.aul 2)))
(CMPW cmpmw_an@+,an@+ (list "1011xxx101001yyy") dont_expand
      amode_same_reg (derefuw $2.aul) (derefuw (+ $1.aul 2))
      "xlate_cmpmw_postinc_postinc_1_0"
      (list)
      (assign $1.aul (+ $1.aul 4)))
(CMPL cmpml_an@+,an@+ (list "1011xxx110001yyy") dont_expand
      amode_same_reg (dereful $2.aul) (dereful (+ $1.aul 4))
      "xlate_cmpml_postinc_postinc_1_0"
      (list)
      (assign $1.aul (+ $1.aul 8)))

(CMPB cmpmb (list "1011xxx100001yyy") dont_expand
      amode_implicit
      (derefub $2.aul) (derefub $1.aul)
      "xlate_cmpmb_postinc_postinc_1_0"
      (list)
      (list
       (assign $1.aul (+ $1.aul 1))
       (assign $2.aul (+ $2.aul 1))))

(CMPW cmpmw (list "1011xxx101001yyy") dont_expand
      amode_implicit
      (derefuw $2.aul) (derefuw $1.aul)
      "xlate_cmpmw_postinc_postinc_1_0"
      (list)
      (list
       (assign $1.aul (+ $1.aul 2))
       (assign $2.aul (+ $2.aul 2))))

(CMPL cmpml (list "1011xxx110001yyy") dont_expand
      amode_implicit
      (dereful $2.aul) (dereful $1.aul)
      "xlate_cmpml_postinc_postinc_1_0"
      (list)
      (list
       (assign $1.aul (+ $1.aul 4))
       (assign $2.aul (+ $2.aul 4))))


(define (DBCC name cc bit_pattern condn)
  (defopcode name
    (list 68000 amode_implicit (ends_block skip_two_pointers)
	  (list bit_pattern "wwwwwwwwwwwwwwww"))
    (list "-----" cc dont_expand
	  (if (or condn (= (assign $1.duw (- $1.duw 1)) 0xFFFF))
	      (assign code (deref "uint16 **" code 0))
	      (assign code (deref "uint16 **" code 1))))))

(DBCC dbhi "C---Z" "0101001011001ddd" CC_HI)
(DBCC dbls "C---Z" "0101001111001ddd" CC_LS)
(DBCC dbcc "C----" "0101010011001ddd" CC_CC)
(DBCC dbcs "C----" "0101010111001ddd" CC_CS)
(DBCC dbne "----Z" "0101011011001ddd" CC_NE)
(DBCC dbeq "----Z" "0101011111001ddd" CC_EQ)
(DBCC dbvc "--V--" "0101100011001ddd" CC_VC)
(DBCC dbvs "--V--" "0101100111001ddd" CC_VS)
(DBCC dbpl "-N---" "0101101011001ddd" CC_PL)
(DBCC dbmi "-N---" "0101101111001ddd" CC_MI)
(DBCC dbge "-NV--" "0101110011001ddd" CC_GE)
(DBCC dblt "-NV--" "0101110111001ddd" CC_LT)
(DBCC dbgt "-NV-Z" "0101111011001ddd" CC_GT)
(DBCC dble "-NV-Z" "0101111111001ddd" CC_LE)

(defopcode dbra
  (list 68000 amode_implicit (ends_block skip_two_pointers)
	(list "0101000111001ddd" "wwwwwwwwwwwwwwww"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_dbra")
	(if (>= (assign $1.dsw (- $1.dsw 1)) 0)
	    (assign code (deref "uint16 **" code 1))
	    (if (= $1.duw 0xFFFF)
		(assign code (deref "uint16 **" code 0))
		(assign code (deref "uint16 **" code 1))))))

(defopcode dbnever
  (list 68000 amode_implicit (ends_block skip_one_pointer)
	(list "0101000011001ddd" "wwwwwwwwwwwwwwww"))
  (list "-----" "-----" dont_expand
	(assign code (deref "uint16 **" code 0))))


; FIXME - won't postincrement/predecrement if it traps!
(defopcode divl
  (list 68020 amode_data (ends_block next_block_dynamic
				     skip_four_operand_words)
	(list "0100110001mmmmmm" "0qqqss0000000rrr"))
  (list "0NV-Z" "-----"  dont_expand
	(list
	 
	 ; Fetch divisor
	 (assign tmp.sl $1.msl)
	 
	 ; Check for division by zero
	 (if (= tmp.sl 0)
	     (TRAP 5 (deref "uint32 *" code 0) (deref "uint32 *" code 1))
	     (list

	      ; Fetch dividend
	      (assign tmp2.sl $2.dsl)
	      
	      (switch $3.uw
		      (0 (list    ; 32 bit unsigned divide
			  (if (<> $2.uw $4.uw)
			      (assign $4.dul (% tmp2.ul tmp.ul)))
			  (assign $2.dul (assign tmp2.ul (/ tmp2.ul tmp.ul)))
			  (ASSIGN_NNZ_LONG tmp2.ul)
			  (assign ccv 0)))
		      
		      (2 (list      ; 32 bit signed divide
			  (if (and (= tmp.sl -1) (= tmp2.sl 0x80000000))
			      (assign ccv 1)
			      (list
			       (assign ccv 0)
			       "\n#ifdef M68K_DIVISION_BEHAVIOR\n"
			       (if (<> $2.uw $4.uw)
				   (assign $4.dsl (% tmp2.sl tmp.sl)))
			       (ASSIGN_NNZ_LONG
				(assign tmp2.sl (/ tmp2.sl tmp.sl)))
			       (assign $2.dsl tmp2.sl)
			       "\n#else\n"
			       "{ int32 sgn, regneg"
			       (if (< tmp2.sl 0)
				   (list
				    (assign tmp2.sl (- 0 tmp2.sl))
				    (assign "sgn" -1)
				    (assign "regneg" 1))
				   (list
				    (assign "sgn" 1)
				    (assign "regneg" 0)))
			       (if (< tmp.sl 0)
				   (list
				    (assign tmp.sl (- 0 tmp.sl))
				    (assign "sgn" (- 0 "sgn"))))
			       (if (<> $2.uw $4.uw)
				   (list
				    (assign $4.dsl (% tmp2.ul tmp.ul))
				    (if "regneg"
					(assign $4.dsl (- $4.dsl tmp.ul)))))
			       
			       (assign tmp2.sl (/ tmp2.ul tmp.ul))
			       (if (< "sgn" 0)
				   (assign tmp2.sl (- 0 tmp2.sl)))
			       (assign $2.dsl tmp2.sl)
			       (ASSIGN_NNZ_LONG tmp2.sl)
			       "}"
			       "\n#endif\n"))))
		      
		      (1 (list    ; 64 bit unsigned divide
			  "{ uint64 dividend"
			  (assign "dividend" (| (<< (cast "uint64" $4.dul) 32)
						(cast "uint64" tmp2.ul)))
			  (if (<> $2.uw $4.uw)
			      (assign tmp3.ul (% "dividend" tmp.ul)))
			  (assign "dividend" (/ "dividend" tmp.ul))
			  (if (> "dividend" (cast "uint32" 0xFFFFFFFF))
			      (assign ccv 1)
			      (list
			       (assign $4.dul tmp3.ul)
			       (assign $2.dul (assign tmp.ul "dividend"))
			       (ASSIGN_NNZ_LONG tmp.ul)
			       (assign ccv 0)))
			  "}"))
		      
		      (default
			(list      ; 64 bit signed divide
			 "{ int64 dividend"
			 "\n#ifdef M68K_DIVISION_BEHAVIOR\nint32 remainder=0"
			 "\n#endif\n"
			 (assign "dividend" (| (<< (cast "int64" $4.dul) 32)
					       (cast "int64" tmp2.ul)))
			 "\n#ifdef M68K_DIVISION_BEHAVIOR\n"
			 (if (<> $2.uw $4.uw) ; Only compute rem if we need to
			     (assign "remainder" (% "dividend" tmp.sl)))
			 (assign "dividend" (/ "dividend" tmp.sl))
			 (if (> (cast "uint64" (+ "dividend"
						  (cast "uint32" 0x80000000)))
				(cast "uint32" 0xFFFFFFFF))
			     (assign ccv 1)
			     (list
			      (assign $4.dsl "remainder")
			      (assign $2.dsl "dividend")
			      (ASSIGN_NNZ_LONG "dividend")
			      (assign ccv 0)))
			 "\n#else\n"
			 "{ int32 sgn, regneg"
			 (if (< "dividend" 0)
			     (list
			      (assign "dividend" (- 0 "dividend"))
			      (assign "sgn" -1)
			      (assign "regneg" 1))
			     (list
			      (assign "sgn" 1)
			      (assign "regneg" 0)))
			 (if (< tmp.sl 0)
			     (list
			      (assign tmp.sl (- 0 tmp.sl))
			      (assign "sgn" (- 0 "sgn"))))
			 (if (<> $2.uw $4.uw) ; Only compute rem if we need to
			     (list
			      (assign tmp3.sl
				      (% (cast "uint64" "dividend") tmp.ul))
			      (if "regneg"  ; Make rem same sgn as dividend
				  (assign tmp3.sl (- 0 tmp3.sl)))))
			 
			 (assign "dividend" (/ (cast "uint64" "dividend")
					       tmp.ul))
			 (if (< "sgn" 0)
			     (assign "dividend" (- 0 "dividend")))
			 (if (> (cast "uint64" (+ "dividend"
						  (cast "uint32" 0x80000000)))
				(cast "uint32" 0xFFFFFFFF))
			     (assign ccv 1)
			     (list
			      (assign $4.dsl tmp3.sl)
			      (assign $2.dsl "dividend")
			      (ASSIGN_NNZ_LONG "dividend")
			      (assign ccv 0)))
			 "}"
			 "\n#endif\n"
			 "}")))
	      (assign code (call "code_lookup"
				 (deref "uint32 *" code 0))))))))



(define (DIVS name amode bits divisor native post)
  (defopcode divs	
    (list 68000 amode (skip_four_operand_words)
	  bits)
    (list "0NV-Z" "-----" dont_expand
	  (native_code native)
	  (list
	   "{ int32 divisor, dividend, sgn"
	   (assign "dividend" $1.dsl)
	   (if (< "dividend" 0)
	       (list
		(assign "dividend" (- 0 "dividend"))
		(assign "sgn" -1))
	       (assign "sgn" 1))
	   (assign "divisor" divisor)
	   (if (< "divisor" 0)
	       (list
		(assign "divisor" (- 0 "divisor"))
		(assign "sgn" (- 0 "sgn"))))
	   post
	   ; Check for division by zero
	   (if (= "divisor" 0)
	       (TRAP 5 (deref "uint32 *" code 0) (deref "uint32 *" code 1))
	       (list
		(assign tmp2.sl (/ (cast "uint32" "dividend")
				   (cast "uint32" "divisor")))
		(if (< "sgn" 0)
		    (assign tmp2.sl (- 0 tmp2.sl)))
		(ASSIGN_NNZ_LONG tmp2.sl)
		(if (>= (+ tmp2.ul 32768) 65536)  ; check for overflow
		    (assign ccv 1)
		    (list
		     (assign tmp.sl (% (cast "uint32" "dividend")
				       (cast "uint32" "divisor"))); compute rem
		     (if (and (<> tmp.sl 0)  ; make sure rem has same sign 
			      (< $1.dsl 0))  ; as dividend
			 (assign tmp.sl (- 0 tmp.sl)))
		     (assign $1.dsl (| tmp2.uw (<< tmp.sl 16)))
		     (assign ccv 0)))))
	   "}"))))


(DIVS divsw_imm_reg amode_implicit (list "1000ddd111111100wwwwwwwwwwwwwwww")
      $2.sl "xlate_divsw_imm_reg_1_0" (list))
(DIVS divsw_reg_reg amode_implicit (list "1000ddd111000ddd")
      $2.dsw "xlate_divsw_reg_reg_1_0" (list))
(DIVS divsw_ind_reg amode_implicit (list "1000ddd111010aaa")
      (derefsw $2.aul) "xlate_divsw_ind_reg_1_0" (list))
(DIVS divsw_predec_reg amode_implicit (list "1000ddd111100aaa")
      (derefsw (assign $2.aul (- $2.aul 2))) "xlate_divsw_predec_reg_1_0"
      (list))
(DIVS divsw_postinc_reg amode_implicit (list "1000ddd111011aaa")
      (derefsw $2.aul) "xlate_divsw_postinc_reg_1_0"
      (assign $2.aul (+ $2.aul 2)))
(DIVS divsw_indoff_reg amode_implicit (list "1000ddd111101aaawwwwwwwwwwwwwwww")
      (derefsw (+ $2.asl $3.sl)) "xlate_divsw_indoff_reg_2_1_0" (list))

(DIVS divs amode_data (list "1000ddd111mmmmmm") $2.msw "none" (list))

(defopcode divu
  (list 68000 amode_data (ends_block next_block_dynamic
				     skip_four_operand_words)
	(list "1000ddd011mmmmmm"))
  (list "0NV-Z" "-----" dont_expand
	(list
	 (assign tmp.uw $2.muw)
	 ; Check for division by zero
	 (if (= tmp.uw 0)
	     (TRAP 5 (deref "uint32 *" code 0) (deref "uint32 *" code 1))
	     (list
	      (assign tmp2.ul (/ $1.dul tmp.uw))
	      (assign ccnz (<> 0 tmp2.ul))
	      (assign ccn (& (>> tmp2.ul 15) 1))
	      (if (>= tmp2.ul 65536)
		  (assign ccv 1)
		  (list
		   (assign $1.dul (| tmp2.ul (<< (% $1.dul tmp.uw) 16)))
		   (assign ccv 0)))
	      (assign code (call "code_lookup"
				 (deref "uint32 *" code 0))))))))

(define (EXG name bit_pattern r1 r2)
  (defopcode name
    (list 68000 amode_implicit () bit_pattern)
    (list "-----" "-----" dont_expand
	  (list
	   (assign tmp.ul r1)
	   (assign r1 r2)
	   (assign r2 tmp.ul)
	   ;	   (assign r1 (^ r1 r2))      ; use xor hack if you prefer it
	   ;	   (assign r2 (^ r2 r1))
	   ;	   (assign r1 (^ r1 r2))
	   ))))

(EXG exgdd (list "1100ddd101000ddd") $1.dul $2.dul)
(EXG exgaa (list "1100aaa101001aaa") $1.aul $2.aul)
(EXG exgda (list "1100ddd110001aaa") $1.dul $2.aul)


(define (EXT name cpu bit_pattern from to cc_func native)
  (defopcode name
    (list cpu amode_implicit () (list bit_pattern))
    (list "-----" "-----" dont_expand
	  (native_code native)
	  (assign to from))
    (list "0N0-Z" "-----" dont_expand
	  (native_code native)
	  (cc_func (assign to from)))))

; You have some latitude choosing cc_func; I chose the ones I thought
; would be most efficient.
(EXT extbw 68000 "0100100010000ddd" $1.dsb $1.dsw ASSIGN_NNZ_BYTE
     "xlate_extbw")
(EXT extbl 68020 "0100100111000ddd" $1.dsb $1.dsl ASSIGN_NNZ_LONG
     "xlate_extbl")
(EXT extwl 68000 "0100100011000ddd" $1.dsw $1.dsl ASSIGN_NNZ_LONG
     "xlate_extwl")

(defopcode jmp_abs_w
  (list 68000 amode_implicit (ends_block)
	(list "0100111011111000" "wwwwwwwwwwwwwwww"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_jmp")
	(assign code (deref "uint16 **" code 0))))

(defopcode jmp_abs_l
  (list 68000 amode_implicit (ends_block)
	(list "0100111011111001" "llllllllllllllll" "llllllllllllllll"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_jmp")
	(assign code (deref "uint16 **" code 0))))

(defopcode jmp_a
  (list 68000 amode_implicit (ends_block next_block_dynamic)
	(list "0100111011010ddd"))
  (list "-----" "-----" dont_expand
	(assign code (call "code_lookup" $1.aul))))

(defopcode jmp
  (list 68000 amode_control (ends_block next_block_dynamic)
	(list "0100111011mmmmmm"))
  (list "-----" "-----" dont_expand
	(assign code (call "code_lookup "
			   (call "CLEAN" $1.puw)))))


(define (jsr_common targ_addr)
  (list
   "syn68k_addr_t target_addr, rts_addr"

   ; Grab the m68k addresses to which we are branching and returning.
   ; Note that the target_addr is in native byte order, while the rts
   ; addr is in big endian byte order.
   (assign "rts_addr" (call "CLEAN"
			    (call "READUL_UNSWAPPED_US" code)))
   (assign "target_addr" (call "CLEAN" targ_addr));

   (assign a7.ul (- a7.ul 4))
; Can't do this, because a7 holds the address in mac-memory space, but if
; we want to dereference it, we need to convert it to local space.
;  (assign (call "DEREF" "uint32" a7.ul) "rts_addr")
   (assign (call "READUL_UNSWAPPED" a7.ul) "rts_addr")
   (assign code (call "code_lookup " "target_addr"))))


(defopcode bsr
  (list 68000 "01100001xxxxxxxx"
	(ends_block next_block_dynamic skip_four_operand_words)
	(list "0aa00001xxxxxxxx"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_bsr")
	(list
	 (jsr_common (call "US_TO_SYN68K" (call "READUL_UNSWAPPED_US" 
						    (+ code 2))))
	 $1.sl)))  ; hack to give native code an a7

(defopcode jsr_absw
  (list 68000 "0100111010111000"
	(ends_block next_block_dynamic skip_two_operand_words)
	(list "0100111010aaa000" "wwwwwwwwwwwwwwww"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_jsr_abs_0")
	(list
	 (jsr_common (call "CLEAN" $2.sl))
	 $1.ul)))

(defopcode jsr_absl
  (list 68000 "0100111010111001"
	(ends_block next_block_dynamic skip_two_operand_words)
	(list "0100111010aaa001" "llllllllllllllllllllllllllllllll"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_jsr_abs_0")
	(list
	 (jsr_common (call "CLEAN" $2.sl))
	 $1.ul)))

(defopcode jsr_pcd16
  (list 68000 "0100111010111010"
	(ends_block next_block_dynamic skip_two_operand_words)
	(list "0100aaa010mmmmmm"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_jsr_pcd16_0")
	(list
	 (jsr_common (call "CLEAN" $2.puw))
	 $1.ul)))

; All jsr's to known addresses will self-modify to become a "fast" jsr
; that uses the jsr stack and ties all the blocks together directly.
; We do this for jsr abs.w, jsr abs.l, and jsr pc@d16.
(defopcode jsr_fixed
  (list 68000
	(union "xxxxxxxxxx11100x" "xxxxxxxxxx111010")
	(ends_block next_block_dynamic skip_two_operand_words)
	(list "0100111010mmmmmm"))
  (list "-----" "-----" dont_expand
	(jsr_common (call "CLEAN" $1.puw))))

(defopcode jsr_d16
  (list 68000 "0100111010101xxx" (ends_block skip_two_operand_words
					     next_block_dynamic)
	(list "0100aaa010101bbbwwwwwwwwwwwwwwww"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_jsr_d16_0_1")
	(list
	 (assign a7.ul (- a7.ul 4))
	 (assign (dereful a7.ul) (call "READUL_US" code))
	 (assign code (call "code_lookup "
			    (call "CLEAN" (+ $2.asl $3.sl))))
	 $1.ul)))

; jsr's to dynamic addresses are a lost cause.  FIXME - we could still
; use the jsr stack here since at least the return address is fixed.
(defopcode jsr_dynamic
  (list 68000 amode_control (ends_block skip_two_operand_words
					next_block_dynamic)
	(list "0100111010mmmmmm"))
  (list "-----" "-----" dont_expand
	(list
	 (assign a7.ul (- a7.ul 4))
	 (assign (dereful a7.ul) (call "READUL_US" code))
	 (assign code (call "code_lookup "
			    (call "CLEAN" $1.puw))))))


(defopcode lea_w
  (list 68000 amode_implicit ()
	(list "0100aaa111111000" "wwwwwwwwwwwwwwww"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_movel_imm_areg_1_0")
	(assign $1.asl $2.sl)))

(defopcode lea_l
  (list 68000 amode_implicit ()
	(list "0100aaa111111001" "llllllllllllllll" "llllllllllllllll"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_movel_imm_areg_1_0")
	(assign $1.asl $2.sl)))

(defopcode lea_d16
  (list 68000 amode_implicit ()
	(list "0100aaa111101sss" "llllllllllllllll"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_leal_indoff_areg_2_1_0")
	(assign $1.asl (+ $2.asl $3.sl))))

(defopcode lea_ind
  (list 68000 amode_implicit ()
	(list "0100aaa111010sss"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_movel_areg_areg_1_0")
	(assign $1.asl $2.asl)))

(defopcode lea_indix
  (list 68000 "xxxxxxxxxx110xxx" ()
	(list "0100aaa111ssssss"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_leal_indix_areg_1_0")
	(list
	 (assign $1.asl $2.psl)
	 $2.ul)))

(defopcode lea
  (list 68000 amode_control () (list "0100aaa111mmmmmm"))
  (list "-----" "-----" dont_expand
	(assign $1.aul $2.pul)))

(define (LINK name cpu amode_hack bit_pattern)
  (defopcode name
    (list cpu amode_hack () bit_pattern)
    (list "-----" "-----" dont_expand
	  (native_code "xlate_link")
	  (list
	   (assign a7.ul (- a7.ul 4))
	   (assign (dereful a7.ul) $2.aul)
	   (assign $2.aul a7.ul)
	   (assign a7.ul (+ a7.ul $3.sl))
	   $1.sl))))  ; hack for native code to get an a7 operand

(LINK linkw 68000 "xxxx111xxxxxx" (list "0100zzz001010aaa" "wwwwwwwwwwwwwwww"))
(LINK linkl 68020 "x1xxxxxxxxxxxxxx"
      (list "0z00100000001aaa" "llllllllllllllll" "llllllllllllllll"))


; move16 has to come before f-line trap, because move16's high nibble is 0xF!

; We copy the bytes in a different order than the 68040 does, but they can't
; tell (as long as the transfer is atomic)
(define (MOVE16 name bits src dst preamble postamble)
  (defopcode name
    (list 68040 amode_implicit () bits)
    (list "-----" "-----" dont_expand
	  (list
	   preamble

	   ; Assume that gcc inlines memcpy of constant size
	   (call "memcpy"
		 (cast "long *" (call "SYN68K_TO_US" (& dst (~ 15))))
		 (cast "long *" (call "SYN68K_TO_US" (& src (~ 15))))
		 16)

	   postamble))))

; The use of tmp.ul is necessary here because we must handle the case where
; $1 == $2; in that case, the address register is only incremented by 16
; bytes, and NOT 32 bytes as a naive implementation that added 16 to each
; register would do.
(MOVE16 move16_ax@+,ay@+ (list "1111011000100xxx" "1yyy000000000000")
	$1.aul tmp.ul
	(assign tmp.ul $2.aul)
	(list
	 (assign $1.aul (+ $1.aul 16))
	 (assign $2.aul (+ tmp.ul 16))))

(MOVE16 move16_ay@+,absl
	(list "1111011000000yyy" "llllllllllllllll" "llllllllllllllll")
	$1.aul $2.ul (list) (assign $1.aul (+ $1.aul 16)))

(MOVE16 move16_absl_ay@+
	(list "1111011000001yyy" "llllllllllllllll" "llllllllllllllll")
	$2.ul $1.aul (list) (assign $1.aul (+ $1.aul 16)))

(MOVE16 move16_ay@,absl
	(list "1111011000010yyy" "llllllllllllllll" "llllllllllllllll")
	$1.aul $2.ul (list) (list))

(MOVE16 move16_absl,ay@
	(list "1111011000011yyy" "llllllllllllllll" "llllllllllllllll")
	$2.ul $1.aul (list) (list))



(define (predec_src s_amode d_amode size)
  (if (= s_amode PREDEC)
      (if (no_reg_op_p d_amode)
	  (if (and (= size BYTE) (= $1.ul 7))
	      (assign a7.ul (- a7.ul 2))
	      (assign $1.aul (- $1.aul size)))
	  (if (and (= size BYTE) (= $2.ul 7))
	      (assign a7.ul (- a7.ul 2))
	      (assign $2.aul (- $2.aul size))))))

(define (postinc_src s_amode d_amode size)
  (if (= s_amode POSTINC)
      (if (no_reg_op_p d_amode)
	  (if (and (= size BYTE) (= $1.ul 7))
	      (assign a7.ul (+ a7.ul 2))
	      (assign $1.aul (+ $1.aul size)))
	  (if (and (= size BYTE) (= $2.ul 7))
	      (assign a7.ul (+ a7.ul 2))
	      (assign $2.aul (+ $2.aul size))))))

(define (amode_string s_amode d_amode)
  (if (= s_amode INDIX)
      (if (= d_amode INDIX)
	  "xxxxxxx110110xxx"
	  "xxxxxxxxxx110xxx")
      (if (= d_amode INDIX)
	  "xxxxxxx110xxxxxx"
	  "xxxxxxxxxxxxxxxx")))

(define (MOVE name size src_amode dst_amode order)
  (defopcode name
    (list 68000 (amode_string src_amode dst_amode) ()
	  (list "00" (if (= size BYTE) "01"
			 (if (= size WORD) "11" "10"))
		(switch dst_amode
			((+ ABSW    0) "000111")
			((+ ABSL    0) "001111")
			((+ REG     0) "ddd000")  ; no areg
			((+ AREG    0) "ddd001")
			((+ IND     0) "ddd010")
			((+ PREDEC  0) "ddd100")
			((+ POSTINC 0) "ddd011")
			((+ INDOFF  0) "ddd101")
			((+ INDIX   0) "dddddd"))
		(switch src_amode
			((+ IMM    0)
			 "111100"
			 (if (= size BYTE) "00000000aaaaaaaa"
			     (if (= size WORD)
				 "aaaaaaaaaaaaaaaa"
				 "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")))
			((+ ABSW   0) "111000aaaaaaaaaaaaaaaa")
			((+ ABSL   0) "111001aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")
			((+ REG    0) "000sss")
			((+ GREG   0) "00ssss")
			((+ AREG   0) "001sss")
			((+ IND    0) "010sss")
			((+ PREDEC 0) "100sss")
			((+ POSTINC 0) "011sss")
			((+ INDOFF 0)  "101sssaaaaaaaaaaaaaaaa")
			((+ INDIX   0) "ssssss"))
		(switch dst_amode
			((+ ABSW   0) "bbbbbbbbbbbbbbbb")
			((+ ABSL   0) "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb")
			((+ INDOFF 0) "bbbbbbbbbbbbbbbb")
			(default ""))))
    (list (if (= dst_amode AREG) "-----" "CNV-Z") "-----" dont_expand
	  (native_code "xlate_move"
		       (if (= size BYTE) "b"
			   (if (= size WORD) "w" "l"))
		       "_" (amode_name src_amode) "_" (amode_name dst_amode)
		       "_" order)
	  (list
	   (if (= size BYTE)
	       (list
		(predec_src src_amode dst_amode size)
		(assign tmp.sb (src_val src_amode dst_amode size false))
		(postinc_src src_amode dst_amode size)
		(if (= dst_amode PREDEC) 
		    (if (= $1.ul 7)
			(assign a7.ul (- a7.ul 2))
			(assign $1.aul (- $1.aul 1))))
		(if (<> dst_amode AREG)
		    (ASSIGN_C_N_V_NZ_BYTE tmp.sb))
		(assign (dst_val src_amode dst_amode size) tmp.sb))
	       (if (= size WORD)
		   (list
		    (predec_src src_amode dst_amode size)
		    (assign tmp2.sw (src_val src_amode dst_amode size false))
		    (postinc_src src_amode dst_amode size)
		    (if (= dst_amode PREDEC) 
			(assign $1.aul (- $1.aul 2)))
		    (if (<> dst_amode AREG)
			(ASSIGN_C_N_V_NZ_WORD tmp2.sw))
		    (assign (dst_val src_amode dst_amode size) tmp2.sw))
		   (list
		    (predec_src src_amode dst_amode size)
		    (assign tmp3.sl (src_val src_amode dst_amode size false))
		    (postinc_src src_amode dst_amode size)
		    (if (= dst_amode PREDEC) 
			(assign $1.aul (- $1.aul 4)))
		    (if (<> dst_amode AREG)
			(ASSIGN_C_N_V_NZ_LONG tmp3.sl))
		    (assign (dst_val src_amode dst_amode size) tmp3.sl))))
	   (if (= dst_amode POSTINC) 
	       (if (and (= size BYTE) (= $1.ul 7))
		   (assign a7.ul (+ a7.ul 2))
		   (assign $1.aul (+ $1.aul size))))))))


(MOVE moveb_imm_reg BYTE IMM REG "1_0")
(MOVE movew_imm_reg WORD IMM REG "1_0")
(MOVE movel_imm_reg LONG IMM REG "1_0")

(MOVE moveb_imm_absw BYTE IMM ABSW "0_1")
(MOVE movew_imm_absw WORD IMM ABSW "0_1")
(MOVE movel_imm_absw LONG IMM ABSW "0_1")

(MOVE moveb_imm_absl BYTE IMM ABSL "0_1")
(MOVE movew_imm_absl WORD IMM ABSL "0_1")
(MOVE movel_imm_absl LONG IMM ABSL "0_1")

(MOVE moveb_imm_predec BYTE IMM PREDEC "1_0")
(MOVE movew_imm_predec WORD IMM PREDEC "1_0")
(MOVE movel_imm_predec LONG IMM PREDEC "1_0")

(MOVE moveb_imm_postinc BYTE IMM POSTINC "1_0")
(MOVE movew_imm_postinc WORD IMM POSTINC "1_0")
(MOVE movel_imm_postinc LONG IMM POSTINC "1_0")

(MOVE moveb_imm_ind BYTE IMM IND "1_0")
(MOVE movew_imm_ind WORD IMM IND "1_0")
(MOVE movel_imm_ind LONG IMM IND "1_0")

(MOVE moveb_imm_indoff BYTE IMM INDOFF "1_2_0")
(MOVE movew_imm_indoff WORD IMM INDOFF "1_2_0")
(MOVE movel_imm_indoff LONG IMM INDOFF "1_2_0")

(MOVE moveb_imm_indix BYTE IMM INDIX "1_0")
(MOVE movew_imm_indix WORD IMM INDIX "1_0")
(MOVE movel_imm_indix LONG IMM INDIX "1_0")


(MOVE moveb_reg_reg BYTE REG REG "1_0")
(MOVE movew_reg_reg WORD GREG REG "1_0")
(MOVE movel_reg_reg LONG GREG REG "1_0")

(MOVE moveb_reg_absw BYTE REG ABSW "0_1")
(MOVE movew_reg_absw WORD GREG ABSW "0_1")
(MOVE movel_reg_absw LONG GREG ABSW "0_1")

(MOVE moveb_reg_absl BYTE REG ABSL "0_1")
(MOVE movew_reg_absl WORD GREG ABSL "0_1")
(MOVE movel_reg_absl LONG GREG ABSL "0_1")

(MOVE moveb_reg_predec BYTE REG PREDEC "1_0")
(MOVE movew_reg_predec WORD GREG PREDEC "1_0")
(MOVE movel_reg_predec LONG GREG PREDEC "1_0")

(MOVE moveb_reg_postinc BYTE REG POSTINC "1_0")
(MOVE movew_reg_postinc WORD GREG POSTINC "1_0")
(MOVE movel_reg_postinc LONG GREG POSTINC "1_0")

(MOVE moveb_reg_ind BYTE REG IND "1_0")
(MOVE movew_reg_ind WORD GREG IND "1_0")
(MOVE movel_reg_ind LONG GREG IND "1_0")

(MOVE moveb_reg_indoff BYTE REG INDOFF "1_2_0")
(MOVE movew_reg_indoff WORD GREG INDOFF "1_2_0")
(MOVE movel_reg_indoff LONG GREG INDOFF "1_2_0")

(MOVE moveb_reg_indix BYTE REG  INDIX "1_0")
(MOVE movew_reg_indix WORD GREG INDIX "1_0")
(MOVE movel_reg_indix LONG GREG INDIX "1_0")


(MOVE moveb_absw_reg BYTE ABSW REG "1_0")
(MOVE movew_absw_reg WORD ABSW REG "1_0")
(MOVE movel_absw_reg LONG ABSW REG "1_0")

(MOVE moveb_absw_absw BYTE ABSW ABSW "0_1")
(MOVE movew_absw_absw WORD ABSW ABSW "0_1")
(MOVE movel_absw_absw LONG ABSW ABSW "0_1")

(MOVE moveb_absw_absl BYTE ABSW ABSL "0_1")
(MOVE movew_absw_absl WORD ABSW ABSL "0_1")
(MOVE movel_absw_absl LONG ABSW ABSL "0_1")

(MOVE moveb_absw_predec BYTE ABSW PREDEC "1_0")
(MOVE movew_absw_predec WORD ABSW PREDEC "1_0")
(MOVE movel_absw_predec LONG ABSW PREDEC "1_0")

(MOVE moveb_absw_postinc BYTE ABSW PREDEC "1_0")
(MOVE movew_absw_postinc WORD ABSW PREDEC "1_0")
(MOVE movel_absw_postinc LONG ABSW PREDEC "1_0")

(MOVE moveb_absw_ind BYTE ABSW IND "1_0")
(MOVE movew_absw_ind WORD ABSW IND "1_0")
(MOVE movel_absw_ind LONG ABSW IND "1_0")

(MOVE moveb_absw_indoff BYTE ABSW INDOFF "1_2_0")
(MOVE movew_absw_indoff WORD ABSW INDOFF "1_2_0")
(MOVE movel_absw_indoff LONG ABSW INDOFF "1_2_0")


(MOVE moveb_absl_reg BYTE ABSL REG "1_0")
(MOVE movew_absl_reg WORD ABSL REG "1_0")
(MOVE movel_absl_reg LONG ABSL REG "1_0")

(MOVE moveb_absl_absw BYTE ABSL ABSW "0_1")
(MOVE movew_absl_absw WORD ABSL ABSW "0_1")
(MOVE movel_absl_absw LONG ABSL ABSW "0_1")

(MOVE moveb_absl_absl BYTE ABSL ABSL "0_1")
(MOVE movew_absl_absl WORD ABSL ABSL "0_1")
(MOVE movel_absl_absl LONG ABSL ABSL "0_1")

(MOVE moveb_absl_predec BYTE ABSL PREDEC "1_0")
(MOVE movew_absl_predec WORD ABSL PREDEC "1_0")
(MOVE movel_absl_predec LONG ABSL PREDEC "1_0")

(MOVE moveb_absl_postinc BYTE ABSL PREDEC "1_0")
(MOVE movew_absl_postinc WORD ABSL PREDEC "1_0")
(MOVE movel_absl_postinc LONG ABSL PREDEC "1_0")

(MOVE moveb_absl_ind BYTE ABSL IND "1_0")
(MOVE movew_absl_ind WORD ABSL IND "1_0")
(MOVE movel_absl_ind LONG ABSL IND "1_0")

(MOVE moveb_absl_indoff BYTE ABSL INDOFF "1_2_0")
(MOVE movew_absl_indoff WORD ABSL INDOFF "1_2_0")
(MOVE movel_absl_indoff LONG ABSL INDOFF "1_2_0")


(MOVE moveb_predec_reg BYTE PREDEC REG "1_0")
(MOVE movew_predec_reg WORD PREDEC REG "1_0")
(MOVE movel_predec_reg LONG PREDEC REG "1_0")

(MOVE moveb_predec_absw BYTE PREDEC ABSW "0_1")
(MOVE movew_predec_absw WORD PREDEC ABSW "0_1")
(MOVE movel_predec_absw LONG PREDEC ABSW "0_1")

(MOVE moveb_predec_absl BYTE PREDEC ABSL "0_1")
(MOVE movew_predec_absl WORD PREDEC ABSL "0_1")
(MOVE movel_predec_absl LONG PREDEC ABSL "0_1")

(MOVE moveb_predec_predec BYTE PREDEC PREDEC "1_0")
(MOVE movew_predec_predec WORD PREDEC PREDEC "1_0")
(MOVE movel_predec_predec LONG PREDEC PREDEC "1_0")

(MOVE moveb_predec_postinc BYTE PREDEC POSTINC "1_0")
(MOVE movew_predec_postinc WORD PREDEC POSTINC "1_0")
(MOVE movel_predec_postinc LONG PREDEC POSTINC "1_0")

(MOVE moveb_predec_ind BYTE PREDEC IND "1_0")
(MOVE movew_predec_ind WORD PREDEC IND "1_0")
(MOVE movel_predec_ind LONG PREDEC IND "1_0")

(MOVE moveb_predec_indoff BYTE PREDEC INDOFF "1_2_0")
(MOVE movew_predec_indoff WORD PREDEC INDOFF "1_2_0")
(MOVE movel_predec_indoff LONG PREDEC INDOFF "1_2_0")

(MOVE moveb_predec_indix BYTE PREDEC INDIX "1_0")
(MOVE movew_predec_indix WORD PREDEC INDIX "1_0")
(MOVE movel_predec_indix LONG PREDEC INDIX "1_0")


(MOVE moveb_postinc_reg BYTE POSTINC REG "1_0")
(MOVE movew_postinc_reg WORD POSTINC REG "1_0")
(MOVE movel_postinc_reg LONG POSTINC REG "1_0")

(MOVE moveb_postinc_absw BYTE POSTINC ABSW "0_1")
(MOVE movew_postinc_absw WORD POSTINC ABSW "0_1")
(MOVE movel_postinc_absw LONG POSTINC ABSW "0_1")

(MOVE moveb_postinc_absl BYTE POSTINC ABSL "0_1")
(MOVE movew_postinc_absl WORD POSTINC ABSL "0_1")
(MOVE movel_postinc_absl LONG POSTINC ABSL "0_1")

(MOVE moveb_postinc_predec BYTE POSTINC PREDEC "1_0")
(MOVE movew_postinc_predec WORD POSTINC PREDEC "1_0")
(MOVE movel_postinc_predec LONG POSTINC PREDEC "1_0")

(MOVE moveb_postinc_postinc BYTE POSTINC POSTINC "1_0")
(MOVE movew_postinc_postinc WORD POSTINC POSTINC "1_0")
(MOVE movel_postinc_postinc LONG POSTINC POSTINC "1_0")

(MOVE moveb_postinc_ind BYTE POSTINC IND "1_0")
(MOVE movew_postinc_ind WORD POSTINC IND "1_0")
(MOVE movel_postinc_ind LONG POSTINC IND "1_0")

(MOVE moveb_postinc_indoff BYTE POSTINC INDOFF "1_2_0")
(MOVE movew_postinc_indoff WORD POSTINC INDOFF "1_2_0")
(MOVE movel_postinc_indoff LONG POSTINC INDOFF "1_2_0")

(MOVE moveb_postinc_indix BYTE POSTINC INDIX "1_0")
(MOVE movew_postinc_indix WORD POSTINC INDIX "1_0")
(MOVE movel_postinc_indix LONG POSTINC INDIX "1_0")


(MOVE moveb_ind_reg BYTE IND REG "1_0")
(MOVE movew_ind_reg WORD IND REG "1_0")
(MOVE movel_ind_reg LONG IND REG "1_0")

(MOVE moveb_ind_absw BYTE IND ABSW "0_1")
(MOVE movew_ind_absw WORD IND ABSW "0_1")
(MOVE movel_ind_absw LONG IND ABSW "0_1")

(MOVE moveb_ind_absl BYTE IND ABSL "0_1")
(MOVE movew_ind_absl WORD IND ABSL "0_1")
(MOVE movel_ind_absl LONG IND ABSL "0_1")

(MOVE moveb_ind_predec BYTE IND PREDEC "1_0")
(MOVE movew_ind_predec WORD IND PREDEC "1_0")
(MOVE movel_ind_predec LONG IND PREDEC "1_0")

(MOVE moveb_ind_postinc BYTE IND POSTINC "1_0")
(MOVE movew_ind_postinc WORD IND POSTINC "1_0")
(MOVE movel_ind_postinc LONG IND POSTINC "1_0")

(MOVE moveb_ind_ind BYTE IND IND "1_0")
(MOVE movew_ind_ind WORD IND IND "1_0")
(MOVE movel_ind_ind LONG IND IND "1_0")

(MOVE moveb_ind_indoff BYTE IND INDOFF "1_2_0")
(MOVE movew_ind_indoff WORD IND INDOFF "1_2_0")
(MOVE movel_ind_indoff LONG IND INDOFF "1_2_0")

(MOVE moveb_ind_indix BYTE IND INDIX "1_0")
(MOVE movew_ind_indix WORD IND INDIX "1_0")
(MOVE movel_ind_indix LONG IND INDIX "1_0")


(MOVE moveb_indoff_reg BYTE INDOFF REG "2_1_0")
(MOVE movew_indoff_reg WORD INDOFF REG "2_1_0")
(MOVE movel_indoff_reg LONG INDOFF REG "2_1_0")

(MOVE moveb_indoff_absw BYTE INDOFF ABSW "1_0_2")
(MOVE movew_indoff_absw WORD INDOFF ABSW "1_0_2")
(MOVE movel_indoff_absw LONG INDOFF ABSW "1_0_2")

(MOVE moveb_indoff_absl BYTE INDOFF ABSL "1_0_2")
(MOVE movew_indoff_absl WORD INDOFF ABSL "1_0_2")
(MOVE movel_indoff_absl LONG INDOFF ABSL "1_0_2")

(MOVE moveb_indoff_predec BYTE INDOFF PREDEC "2_1_0")
(MOVE movew_indoff_predec WORD INDOFF PREDEC "2_1_0")
(MOVE movel_indoff_predec LONG INDOFF PREDEC "2_1_0")

(MOVE moveb_indoff_postinc BYTE INDOFF POSTINC "2_1_0")
(MOVE movew_indoff_postinc WORD INDOFF POSTINC "2_1_0")
(MOVE movel_indoff_postinc LONG INDOFF POSTINC "2_1_0")

(MOVE moveb_indoff_ind BYTE INDOFF IND "2_1_0")
(MOVE movew_indoff_ind WORD INDOFF IND "2_1_0")
(MOVE movel_indoff_ind LONG INDOFF IND "2_1_0")

(MOVE moveb_indoff_indoff BYTE INDOFF INDOFF "2_1_3_0")
(MOVE movew_indoff_indoff WORD INDOFF INDOFF "2_1_3_0")
(MOVE movel_indoff_indoff LONG INDOFF INDOFF "2_1_3_0")

(MOVE moveb_indoff_indix BYTE INDOFF INDIX "2_1_0")
(MOVE movew_indoff_indix WORD INDOFF INDIX "2_1_0")
(MOVE movel_indoff_indix LONG INDOFF INDIX "2_1_0")


(MOVE moveb_indix_reg BYTE INDIX REG "1_0")
(MOVE movew_indix_reg WORD INDIX REG "1_0")
(MOVE movel_indix_reg LONG INDIX REG "1_0")

(MOVE moveb_indix_ind BYTE INDIX IND "1_0")
(MOVE movew_indix_ind WORD INDIX IND "1_0")
(MOVE movel_indix_ind LONG INDIX IND "1_0")

(MOVE moveb_indix_predec BYTE INDIX PREDEC "1_0")
(MOVE movew_indix_predec WORD INDIX PREDEC "1_0")
(MOVE movel_indix_predec LONG INDIX PREDEC "1_0")

(MOVE moveb_indix_postinc BYTE INDIX POSTINC "1_0")
(MOVE movew_indix_postinc WORD INDIX POSTINC "1_0")
(MOVE movel_indix_postinc LONG INDIX POSTINC "1_0")



; Here is the addressing mode for the move instructions.  We can't use
; any of the "stock" addressing modes, because move instructions have two
; amode fields.
(define move_amode
  (intersect amode_all_combinations     ; src field
	     (union "xxxxxxx0x0xxxxxx"  ; dst field
		    "xxxxxxx011xxxxxx"
		    "xxxxxxx10xxxxxxx"
		    "xxxxxxx110xxxxxx"
		    "xxxx00x111xxxxxx")))



(defopcode moveb
  (list 68000 (intersect move_amode (not "xxxxxxxxxx001xxx")) ; can't moveb
	() (list "0001rrrrrrmmmmmm"))                         ; from addr reg
  (list "-----" "-----" dont_expand
	(assign $1.rub $2.mub))
  (list "CNV-Z" "-----" dont_expand
	(ASSIGN_C_N_V_NZ_BYTE (assign $1.rub $2.mub))))

(defopcode movew
  (list 68000 move_amode () (list "0011rrrrrrmmmmmm"))
  (list "-----" "-----" dont_expand
	(assign $1.ruw $2.muw))
  (list "CNV-Z" "-----" dont_expand
	(ASSIGN_C_N_V_NZ_WORD (assign $1.ruw $2.muw))))

(defopcode movel
  (list 68000 move_amode () (list "0010rrrrrrmmmmmm"))
  (list "-----" "-----" dont_expand
	(assign $1.rul $2.mul))
  (list "CNV-Z" "-----" dont_expand
	(ASSIGN_C_N_V_NZ_LONG (assign $1.rul $2.mul))))

(MOVE moveaw_imm_areg     WORD IMM     AREG "1_0")
(MOVE moveaw_reg_areg     WORD GREG    AREG "1_0")
(MOVE moveaw_absw_areg    WORD ABSW    AREG "1_0")
(MOVE moveaw_absl_areg    WORD ABSL    AREG "1_0")
(MOVE moveaw_predec_areg  WORD PREDEC  AREG "1_0")
(MOVE moveaw_postinc_areg WORD POSTINC AREG "1_0")
(MOVE moveaw_ind_areg     WORD IND     AREG "1_0")
(MOVE moveaw_indoff_areg  WORD INDOFF  AREG "2_1_0")

(MOVE moveal_imm_areg     LONG IMM     AREG "1_0")
(MOVE moveal_reg_areg     LONG GREG    AREG "1_0")
(MOVE moveal_absw_areg    LONG ABSW    AREG "1_0")
(MOVE moveal_absl_areg    LONG ABSL    AREG "1_0")
(MOVE moveal_predec_areg  LONG PREDEC  AREG "1_0")
(MOVE moveal_postinc_areg LONG POSTINC AREG "1_0")
(MOVE moveal_ind_areg     LONG IND     AREG "1_0")
(MOVE moveal_indoff_areg  LONG INDOFF  AREG "2_1_0")


(defopcode moveal_indix_areg
  (list 68000 "xxxxxxxxxx110xxx" () (list "0010aaa001mmmmmm"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_movel_indix_areg_1_0")
	(assign $1.aul $2.mul)))


(defopcode moveaw
  (list 68000 amode_all_combinations () (list "0011aaa001mmmmmm"))
  (list "-----" "-----" dont_expand
	(assign $1.asl $2.msw)))

(defopcode moveal
  (list 68000 amode_all_combinations () (list "0010aaa001mmmmmm"))
  (list "-----" "-----" dont_expand
	(assign $1.aul $2.mul)))

(define (nextreg_ptm_w reg)
  (list
   (if (& tmp.ul $2.ul)
       (list
	(assign tmp2.ul (- tmp2.ul 2))
	(assign (derefuw tmp2.ul) reg)))
   (assign tmp.ul (<< tmp.ul 1))))

(defopcode movemw_predec_to_memory
  (list 68000 amode_implicit ()
	(list "0100100010100ddd" "wwwwwwwwwwwwwwww"))
  (list "-----" "-----" dont_expand
	(list
	 (assign tmp.ul 1)
	 (assign tmp2.ul $1.aul)
	 (assign $1.aul (- $1.aul 2))
	 (assign tmp3.ul $2.ul)
	 (nextreg_ptm_w a7.uw)
	 (nextreg_ptm_w a6.uw)
	 (nextreg_ptm_w a5.uw)
	 (nextreg_ptm_w a4.uw)
	 (nextreg_ptm_w a3.uw)
	 (nextreg_ptm_w a2.uw)
	 (nextreg_ptm_w a1.uw)
	 (nextreg_ptm_w a0.uw)
	 (nextreg_ptm_w d7.uw)
	 (nextreg_ptm_w d6.uw)
	 (nextreg_ptm_w d5.uw)
	 (nextreg_ptm_w d4.uw)
	 (nextreg_ptm_w d3.uw)
	 (nextreg_ptm_w d2.uw)
	 (nextreg_ptm_w d1.uw)
	 (nextreg_ptm_w d0.uw)
	 (assign $1.aul tmp2.ul))))

(define (nextreg_ptm_l reg)
  (list
   (if (& tmp.ul 1)
       (list
	(assign tmp2.ul (- tmp2.ul 4))
	(assign (dereful tmp2.ul) reg)))
   (assign tmp.ul (>> tmp.ul 1))))

(defopcode moveml_predec_to_memory
  (list 68000 amode_implicit ()
	(list "0100100011100ddd" "wwwwwwwwwwwwwwww"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_moveml_reg_predec_0_1")
	(list
	 (assign tmp.ul $2.ul)
	 (assign tmp2.ul $1.aul)
	 (assign $1.aul (- $1.aul 4))
	 (nextreg_ptm_l a7.ul)
	 (nextreg_ptm_l a6.ul)
	 (nextreg_ptm_l a5.ul)
	 (nextreg_ptm_l a4.ul)
	 (nextreg_ptm_l a3.ul)
	 (nextreg_ptm_l a2.ul)
	 (nextreg_ptm_l a1.ul)
	 (nextreg_ptm_l a0.ul)
	 (nextreg_ptm_l d7.ul)
	 (nextreg_ptm_l d6.ul)
	 (nextreg_ptm_l d5.ul)
	 (nextreg_ptm_l d4.ul)
	 (nextreg_ptm_l d3.ul)
	 (nextreg_ptm_l d2.ul)
	 (nextreg_ptm_l d1.ul)
	 (nextreg_ptm_l d0.ul)
	 (assign $1.aul tmp2.ul))))

(define (nextreg_mtr_w reg)
  (list
   (if (& tmp.ul 1)
       (list
	(assign reg (derefsw tmp2.ul))
	(assign tmp2.ul (+ tmp2.ul 2))))
   (assign tmp.ul (>> tmp.ul 1))))

(define (nextreg_mtr_l reg)
  (list
   (if (& tmp.ul 1)
       (list
	(assign reg (dereful tmp2.ul))
	(assign tmp2.ul (+ tmp2.ul 4))))
   (assign tmp.ul (>> tmp.ul 1))))

(define (nextreg_rtm_w reg)
  (list
   (if (& tmp.ul 1)
       (list
	(assign (derefuw tmp2.ul) reg)
	(assign tmp2.ul (+ tmp2.ul 2))))
   (assign tmp.ul (>> tmp.ul 1))))

(define (nextreg_rtm_l reg)
  (list
   (if (& tmp.ul 1)
       (list
	(assign (dereful tmp2.ul) reg)
	(assign tmp2.ul (+ tmp2.ul 4))))
   (assign tmp.ul (>> tmp.ul 1))))

(define (MOVEMW name bit_pattern next amode base)
  (defopcode name
    (list 68000 amode (dont_postincdec_unexpanded)
	  (list bit_pattern "wwwwwwwwwwwwwwww"))
    (list "-----" "-----" dont_expand
	  (list
	   (assign tmp2.ul base)
	   (assign tmp.ul $2.ul)
	   (next d0.sl)
	   (next d1.sl)
	   (next d2.sl)
	   (next d3.sl)
	   (next d4.sl)
	   (next d5.sl)
	   (next d6.sl)
	   (next d7.sl)
	   (next a0.sl)
	   (next a1.sl)
	   (next a2.sl)
	   (next a3.sl)
	   (next a4.sl)
	   (next a5.sl)
	   (next a6.sl)
	   (next a7.sl)
	   (switch $1.ul
		   (0x18 (assign a0.ul tmp2.ul))   ; Switch is here so we don't
		   (0x19 (assign a1.ul tmp2.ul))   ; expand this opcode because
		   (0x1A (assign a2.ul tmp2.ul))   ; of a dollar reg lvalue
		   (0x1B (assign a3.ul tmp2.ul))
		   (0x1C (assign a4.ul tmp2.ul))
		   (0x1D (assign a5.ul tmp2.ul))
		   (0x1E (assign a6.ul tmp2.ul))
		   (0x1F (assign a7.ul tmp2.ul)))))))

(defopcode moveml_postinc_reg
  (list 68000 amode_implicit ()
	(list "0100110011011aaawwwwwwwwwwwwwwww"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_moveml_postinc_reg_0_1")
	(list
	 (assign tmp2.ul $1.aul)
	 (assign tmp.ul $2.ul)
	 (nextreg_mtr_l d0.ul)
	 (nextreg_mtr_l d1.ul)
	 (nextreg_mtr_l d2.ul)
	 (nextreg_mtr_l d3.ul)
	 (nextreg_mtr_l d4.ul)
	 (nextreg_mtr_l d5.ul)
	 (nextreg_mtr_l d6.ul)
	 (nextreg_mtr_l d7.ul)
	 (nextreg_mtr_l a0.ul)
	 (nextreg_mtr_l a1.ul)
	 (nextreg_mtr_l a2.ul)
	 (nextreg_mtr_l a3.ul)
	 (nextreg_mtr_l a4.ul)
	 (nextreg_mtr_l a5.ul)
	 (nextreg_mtr_l a6.ul)
	 (nextreg_mtr_l a7.ul)
	 (assign $1.aul tmp2.ul))))

(define (MOVEML name bit_pattern next amode base)
  (defopcode name
    (list 68000 amode (dont_postincdec_unexpanded)
	  (list bit_pattern "wwwwwwwwwwwwwwww"))
    (list "-----" "-----" dont_expand
	  (list
	   (assign tmp2.ul base)
	   (assign tmp.ul $2.ul)
	   (next d0.ul)
	   (next d1.ul)
	   (next d2.ul)
	   (next d3.ul)
	   (next d4.ul)
	   (next d5.ul)
	   (next d6.ul)
	   (next d7.ul)
	   (next a0.ul)
	   (next a1.ul)
	   (next a2.ul)
	   (next a3.ul)
	   (next a4.ul)
	   (next a5.ul)
	   (next a6.ul)
	   (next a7.ul)
	   (switch $1.ul
		   (0x18 (assign a0.ul tmp2.ul))   ; Switch is here so we don't
		   (0x19 (assign a1.ul tmp2.ul))   ; expand this opcode because
		   (0x1A (assign a2.ul tmp2.ul))   ; of a dollar reg lvalue
		   (0x1B (assign a3.ul tmp2.ul))
		   (0x1C (assign a4.ul tmp2.ul))
		   (0x1D (assign a5.ul tmp2.ul))
		   (0x1E (assign a6.ul tmp2.ul))
		   (0x1F (assign a7.ul tmp2.ul)))))))

; Do moves from memory to registers.
(MOVEMW movemw_to_reg "0100110010mmmmmm" nextreg_mtr_w
	(union amode_control "xxxxxxxxxx011xxx") ; ctl + postinc
	$1.puw)
(MOVEML moveml_to_reg "0100110011mmmmmm" nextreg_mtr_l
	(union amode_control "xxxxxxxxxx011xxx") ; ctl + postinc
	$1.pul)

; Do moves from registers to memory.
(MOVEMW movemw_to_mem "0100100010mmmmmm" nextreg_rtm_w
	(union amode_alterable_control "xxxxxxxxxx100xxx") ; altctl + predec
	$1.puw)
(MOVEML moveml_to_mem "0100100011mmmmmm" nextreg_rtm_l
	(union amode_alterable_control "xxxxxxxxxx100xxx") ; altctl + predec
	$1.pul)


(defopcode movepw_to_mem
  (list 68000 amode_implicit () (list "0000ddd110001aaa" "wwwwwwwwwwwwwwww"))
  (list "-----" "-----" dont_expand
	(list
	 (assign tmp.ul (+ $2.aul $3.sw))
	 (assign tmp2.uw $1.duw)
	 (assign (derefub tmp.ul) (>> tmp2.uw 8))
	 (assign (derefub (+ tmp.ul 2)) tmp2.ub))))

(defopcode movepl_to_mem
  (list 68000 amode_implicit () (list "0000ddd111001aaa" "wwwwwwwwwwwwwwww"))
  (list "-----" "-----" dont_expand
	(list
	 (assign tmp.ul (+ $2.aul $3.sw))
	 (assign tmp2.ul $1.dul)
	 (assign (derefub tmp.ul)       (>> tmp2.ul 24))
	 (assign (derefub (+ tmp.ul 2)) (>> tmp2.ul 16))
	 (assign (derefub (+ tmp.ul 4)) (>> tmp2.ul 8))
	 (assign (derefub (+ tmp.ul 6)) tmp2.ul))))

(defopcode movepw_to_reg
  (list 68000 amode_implicit () (list "0000ddd100001aaa" "wwwwwwwwwwwwwwww"))
  (list "-----" "-----" dont_expand
	(list
	 (assign tmp.ul (+ $2.aul $3.sw))
	 (assign $1.duw (| (<< (derefub tmp.ul) 8) (derefub (+ tmp.ul 2)))))))

(defopcode movepl_to_reg
  (list 68000 amode_implicit () (list "0000ddd101001aaa" "wwwwwwwwwwwwwwww"))
  (list "-----" "-----" dont_expand
	(list
	 (assign tmp.ul (+ $2.aul $3.sw))
	 (assign $1.dul (| (<< (derefub tmp.ul) 24)
			   (<< (derefub (+ tmp.ul 2)) 16)
			   (<< (derefub (+ tmp.ul 4)) 8)
			   (derefub (+ tmp.ul 6)))))))

(defopcode moveq
  (list 68000 amode_implicit () (list "0111ddd0bbbbbbbb"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_movel_imm_reg_1_0")
	(assign $1.dul $2.sl))
  (list "CNV-Z" "-----" dont_expand
	(native_code "xlate_movel_imm_reg_1_0")
	(ASSIGN_C_N_V_NZ_LONG (assign $1.dul $2.sl))))


(defopcode mulsw_imm_reg
  (list 68000 amode_data () (list "1100ddd111111100wwwwwwwwwwwwwwww"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_mulsw_imm_reg_1_0")
	(assign $1.dsl (* $1.dsw $2.sl)))
  (list "CNV-Z" "-----" dont_expand
	(native_code "xlate_mulsw_imm_reg_1_0")
	(ASSIGN_C_N_V_NZ_LONG (assign $1.dsl (* $1.dsw $2.sl)))))

(defopcode mulsw_reg_reg
  (list 68000 amode_data () (list "1100ddd111000sss"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_mulsw_reg_reg_1_0")
	(assign $1.dsl (* $1.dsw $2.dsw)))
  (list "CNV-Z" "-----" dont_expand
	(native_code "xlate_mulsw_reg_reg_1_0")
	(ASSIGN_C_N_V_NZ_LONG (assign $1.dsl (* $1.dsw $2.dsw)))))

(defopcode mulsw_absw_reg
  (list 68000 amode_data () (list "1100ddd111111000wwwwwwwwwwwwwwww"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_mulsw_abs_reg_1_0")
	(assign $1.dsl (* $1.dsw (derefsw $2.sl))))
  (list "CNV-Z" "-----" dont_expand
	(native_code "xlate_mulsw_abs_reg_1_0")
	(ASSIGN_C_N_V_NZ_LONG (assign $1.dsl (* $1.dsw (derefsw $2.sl))))))

(defopcode mulsw_absl_reg
  (list 68000 amode_data ()
	(list "1100ddd111111001llllllllllllllllllllllllllllllll"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_mulsw_abs_reg_1_0")
	(assign $1.dsl (* $1.dsw (derefsw $2.sl))))
  (list "CNV-Z" "-----" dont_expand
	(native_code "xlate_mulsw_abs_reg_1_0")
	(ASSIGN_C_N_V_NZ_LONG (assign $1.dsl (* $1.dsw (derefsw $2.sl))))))

(defopcode mulsw_ind_reg
  (list 68000 amode_data ()
	(list "1100ddd111010sss"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_mulsw_ind_reg_1_0")
	(assign $1.dsl (* $1.dsw (derefsw $2.aul))))
  (list "CNV-Z" "-----" dont_expand
	(native_code "xlate_mulsw_ind_reg_1_0")
	(ASSIGN_C_N_V_NZ_LONG (assign $1.dsl (* $1.dsw (derefsw $2.aul))))))

(defopcode mulsw_predec_reg
  (list 68000 amode_data ()
	(list "1100ddd111100sss"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_mulsw_predec_reg_1_0")
	(list
	 (assign $2.aul (- $2.aul 2))
	 (assign $1.dsl (* $1.dsw (derefsw $2.aul)))))
  (list "CNV-Z" "-----" dont_expand
	(native_code "xlate_mulsw_predec_reg_1_0")
	(list
	 (assign $2.aul (- $2.aul 2))
	 (ASSIGN_C_N_V_NZ_LONG (assign $1.dsl (* $1.dsw (derefsw $2.aul)))))))

(defopcode mulsw_postinc_reg
  (list 68000 amode_data ()
	(list "1100ddd111011sss"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_mulsw_postinc_reg_1_0")
	(list
	 (assign $1.dsl (* $1.dsw (derefsw $2.aul)))
	 (assign $2.aul (+ $2.aul 2))))
  (list "CNV-Z" "-----" dont_expand
	(native_code "xlate_mulsw_postinc_reg_1_0")
	(list
	 (ASSIGN_C_N_V_NZ_LONG (assign $1.dsl (* $1.dsw (derefsw $2.aul))))
	 (assign $2.aul (+ $2.aul 2)))))

(defopcode mulsw_indoff_reg
  (list 68000 amode_data ()
	(list "1100ddd111101ssswwwwwwwwwwwwwwww"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_mulsw_indoff_reg_2_1_0")
	(assign $1.dsl (* $1.dsw (derefsw (+ $2.asl $3.sl)))))
  (list "CNV-Z" "-----" dont_expand
	(native_code "xlate_mulsw_indoff_reg_2_1_0")
	(ASSIGN_C_N_V_NZ_LONG
	 (assign $1.dsl (* $1.dsw (derefsw (+ $2.asl $3.sl)))))))

(defopcode mulsw
  (list 68000 amode_data () (list "1100ddd111mmmmmm"))
  (list "-----" "-----" dont_expand
	(assign $1.dsl (* $1.dsw $2.msw)))
  (list "CNV-Z" "-----" dont_expand
	(ASSIGN_C_N_V_NZ_LONG (assign $1.dsl (* $1.dsw $2.msw)))))

(defopcode muluw_imm_reg
  (list 68000 amode_data () (list "1100ddd011111100wwwwwwwwwwwwwwww"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_muluw_imm_reg_1_0")
	(assign $1.dul (* $1.duw $2.ul)))
  (list "CNV-Z" "-----" dont_expand
	(native_code "xlate_muluw_imm_reg_1_0")
	(ASSIGN_C_N_V_NZ_LONG (assign $1.dul (* $1.duw $2.ul)))))

(defopcode muluw_reg_reg
  (list 68000 amode_data () (list "1100ddd011000sss"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_muluw_reg_reg_1_0")
	(assign $1.dul (* $1.duw $2.duw)))
  (list "CNV-Z" "-----" dont_expand
	(native_code "xlate_muluw_reg_reg_1_0")
	(ASSIGN_C_N_V_NZ_LONG (assign $1.dul (* $1.duw $2.duw)))))

(defopcode muluw_absw_reg
  (list 68000 amode_data () (list "1100ddd011111000wwwwwwwwwwwwwwww"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_muluw_abs_reg_1_0")
	(assign $1.dul (* $1.duw (derefuw $2.sl))))
  (list "CNV-Z" "-----" dont_expand
	(native_code "xlate_muluw_abs_reg_1_0")
	(ASSIGN_C_N_V_NZ_LONG (assign $1.dul (* $1.duw (derefuw $2.sl))))))

(defopcode muluw_absl_reg
  (list 68000 amode_data ()
	(list "1100ddd011111001llllllllllllllllllllllllllllllll"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_muluw_abs_reg_1_0")
	(assign $1.dul (* $1.duw (derefuw $2.sl))))
  (list "CNV-Z" "-----" dont_expand
	(native_code "xlate_muluw_abs_reg_1_0")
	(ASSIGN_C_N_V_NZ_LONG (assign $1.dul (* $1.duw (derefuw $2.sl))))))

(defopcode muluw_ind_reg
  (list 68000 amode_data ()
	(list "1100ddd011010sss"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_muluw_ind_reg_1_0")
	(assign $1.dul (* $1.duw (derefuw $2.aul))))
  (list "CNV-Z" "-----" dont_expand
	(native_code "xlate_muluw_ind_reg_1_0")
	(ASSIGN_C_N_V_NZ_LONG (assign $1.dul (* $1.duw (derefuw $2.aul))))))

(defopcode muluw_predec_reg
  (list 68000 amode_data ()
	(list "1100ddd011100sss"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_muluw_predec_reg_1_0")
	(list
	 (assign $2.aul (- $2.aul 2))
	 (assign $1.dul (* $1.duw (derefuw $2.aul)))))
  (list "CNV-Z" "-----" dont_expand
	(native_code "xlate_muluw_predec_reg_1_0")
	(list
	 (assign $2.aul (- $2.aul 2))
	 (ASSIGN_C_N_V_NZ_LONG (assign $1.dul (* $1.duw (derefuw $2.aul)))))))

(defopcode muluw_postinc_reg
  (list 68000 amode_data ()
	(list "1100ddd011011sss"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_muluw_postinc_reg_1_0")
	(list
	 (assign $1.dul (* $1.duw (derefuw $2.aul)))
	 (assign $2.aul (+ $2.aul 2))))
  (list "CNV-Z" "-----" dont_expand
	(native_code "xlate_muluw_postinc_reg_1_0")
	(list
	 (ASSIGN_C_N_V_NZ_LONG (assign $1.dul (* $1.duw (derefuw $2.aul))))
	 (assign $2.aul (+ $2.aul 2)))))

(defopcode muluw_indoff_reg
  (list 68000 amode_data ()
	(list "1100ddd011101ssswwwwwwwwwwwwwwww"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_muluw_indoff_reg_2_1_0")
	(assign $1.dul (* $1.duw (derefuw (+ $2.asl $3.sl)))))
  (list "CNV-Z" "-----" dont_expand
	(native_code "xlate_muluw_indoff_reg_2_1_0")
	(ASSIGN_C_N_V_NZ_LONG
	 (assign $1.dul (* $1.duw (derefuw (+ $2.asl $3.sl)))))))

(defopcode muluw
  (list 68000 amode_data () (list "1100ddd011mmmmmm"))
  (list "-----" "-----" dont_expand
	(assign $1.dul (* $1.duw $2.muw)))
  (list "CNV-Z" "-----" dont_expand
	(ASSIGN_C_N_V_NZ_LONG (assign $1.dul (* $1.duw $2.muw)))))

(define (mulsl_32_case reg)
  (list
   "{ int64 tmp64"
   (assign "tmp64" $1.msl)
   (ASSIGN_NNZ_LONG (assign reg (assign "tmp64" (* "tmp64" reg))))
   "\n#ifdef CCR_ELEMENT_8_BITS\n"
   (assign ccv (<> 0 (+ (>> "tmp64" 32) (>> (cast "uint32" reg) 31)))) ;hack
   "\n#else\n"
   (assign ccv (+ (>> "tmp64" 32) (>> (cast "uint32" ccnz) 31))) ;hack
   "\n#endif\n"
   "}"))

(define (mulul_32_case reg)
  (list
   "{ uint64 tmp64"
   (assign "tmp64" $1.mul)
   (ASSIGN_NNZ_LONG (assign reg (assign "tmp64" (* "tmp64" reg))))
   "\n#ifdef CCR_ELEMENT_8_BITS\n"
   (assign ccv (<> 0 (>> "tmp64" 32)))
   "\n#else\n"
   (assign ccv (>> "tmp64" 32))
   "\n#endif\n"
   "}"))

(define (mull_64_case reg mem decl64)
  (list
   decl64
   (assign "tmp64" mem)
   (assign reg (assign "tmp64" (* "tmp64" reg)))
   (assign ccnz (<> "tmp64" 0))
   (assign ccn (SIGN_LONG (assign tmp.ul (>> "tmp64" 32))))
   (assign $3.dul tmp.ul)
   (assign ccv 0)
   "}"))

(defopcode mull
  (list 68020 amode_data () (list "0100110000mmmmmm" "0iiiii0000000hhh"))
  (list "0NV-Z" "-----" dont_expand
	(switch $2.uw
		; mulul 32x32->32
		(0x00 (mulul_32_case d0.ul))
		(0x04 (mulul_32_case d1.ul))
		(0x08 (mulul_32_case d2.ul))
		(0x0C (mulul_32_case d3.ul))
		(0x10 (mulul_32_case d4.ul))
		(0x14 (mulul_32_case d5.ul))
		(0x18 (mulul_32_case d6.ul))
		(0x1C (mulul_32_case d7.ul))
		
		; mulsl 32x32->32
		(0x02 (mulsl_32_case d0.sl))
		(0x06 (mulsl_32_case d1.sl))
		(0x0A (mulsl_32_case d2.sl))
		(0x0E (mulsl_32_case d3.sl))
		(0x12 (mulsl_32_case d4.sl))
		(0x16 (mulsl_32_case d5.sl))
		(0x1A (mulsl_32_case d6.sl))
		(0x1E (mulsl_32_case d7.sl))
		
		; mulul 32x32->64
		(0x01 (mull_64_case d0.ul $1.mul "{ uint64 tmp64"))
		(0x05 (mull_64_case d1.ul $1.mul "{ uint64 tmp64"))
		(0x09 (mull_64_case d2.ul $1.mul "{ uint64 tmp64"))
		(0x0D (mull_64_case d3.ul $1.mul "{ uint64 tmp64"))
		(0x11 (mull_64_case d4.ul $1.mul "{ uint64 tmp64"))
		(0x15 (mull_64_case d5.ul $1.mul "{ uint64 tmp64"))
		(0x19 (mull_64_case d6.ul $1.mul "{ uint64 tmp64"))
		(0x1D (mull_64_case d7.ul $1.mul "{ uint64 tmp64"))
		
		; mulsl 32x32->64
		(0x03 (mull_64_case d0.sl $1.msl "{ int64 tmp64"))
		(0x07 (mull_64_case d1.sl $1.msl "{ int64 tmp64"))
		(0x0B (mull_64_case d2.sl $1.msl "{ int64 tmp64"))
		(0x0F (mull_64_case d3.sl $1.msl "{ int64 tmp64"))
		(0x13 (mull_64_case d4.sl $1.msl "{ int64 tmp64"))
		(0x17 (mull_64_case d5.sl $1.msl "{ int64 tmp64"))
		(0x1B (mull_64_case d6.sl $1.msl "{ int64 tmp64"))
		(0x1F (mull_64_case d7.sl $1.msl "{ int64 tmp64")))))


(defopcode nbcd
  (list 68000 amode_alterable_data () (list "0100100000mmmmmm"))
  (list "C??X<" "---X-" dont_expand
	(list
	 (assign tmp.ul $1.mub)
	 (if ccx
	     (list
	      (assign tmp.ul (+ tmp.ul 1))
	      (assign tmp2.ub (- 0x9A tmp.ul)))
	     (list
	      (assign tmp2.ub (- 0x9A tmp.ul))
	      (if (not (& tmp.ul 0xF))
		  (assign tmp2.ub (- tmp2.ub (call "NEGBCD_TABLE"
						   (>> tmp.ul 4)))))))
	 (assign $1.mub tmp2.ub)
	 (assign ccx (assign ccc (<> tmp.ul 0)))
	 (assign ccnz (| ccnz tmp2.ub)))))


(define (NEG name dst bit_pattern sign_func overflow native pre post)
  (defopcode name
    (list 68000 amode_alterable_data () (list bit_pattern))
    (list "-----" "-----" dont_expand
	  (native_code native)
	  (list
	   pre
	   (assign dst (- 0 dst))
	   post))
    (list "CNVXZ" "-----" dont_expand
	  (native_code native)
	  (list
	   pre
	   (assign ccv (= dst overflow))

	   "\n#ifdef CCR_ELEMENT_8_BITS\n"
	   (if (<> overflow 0x80)  ; reduces at compile time
	       (assign ccnz (assign ccx (assign ccc
						(<> (assign dst (- 0 dst))
						    0))))
	       (assign ccnz (assign ccx (assign ccc
						(assign dst (- 0 dst))))))
	   (assign ccn (sign_func dst))
	   "\n#else\n"
	   (assign ccn
		   (& overflow
		      (assign ccnz
			      (assign ccx
				      (assign ccc
					      (assign dst (- 0 dst)))))))
	   "\n#endif\n"
	   post))))
(define (NEG name dst bit_pattern sign_func overflow native)
  (NEG name dst bit_pattern sign_func overflow native (list) (list)))

(NEG negb_reg $1.dub "0100010000000mmm" SIGN_BYTE 0x80 "xlate_negb_reg_0")
(NEG negb_ind (derefub $1.aul) "0100010000010mmm" SIGN_BYTE 0x80
     "xlate_negb_ind_0")
(NEG negb_predec (derefub $1.aul) "0100010000100mmm" SIGN_BYTE 0x80
     "xlate_negb_predec_0"
     (if (= $1.ul 7)
	 (assign a7.ul (- a7.ul 2))
	 (assign $1.aul (- $1.aul 1)))
     (list))
(NEG negb_postinc (derefub $1.aul) "0100010000011mmm" SIGN_BYTE 0x80
     "xlate_negb_postinc_0"
     (list)
     (if (= $1.ul 7)
	 (assign a7.ul (+ a7.ul 2))
	 (assign $1.aul (+ $1.aul 1))))
(NEG negb_indoff (derefub (+ $1.asl $2.sl)) "0100010000101mmmwwwwwwwwwwwwwwww"
     SIGN_BYTE 0x80 "xlate_negb_indoff_1_0")

(NEG negw_reg $1.duw "0100010001000mmm" SIGN_WORD 0x8000 "xlate_negw_reg_0")
(NEG negw_ind (derefuw $1.aul) "0100010001010mmm" SIGN_WORD 0x8000
     "xlate_negw_ind_0")
(NEG negw_predec (derefuw $1.aul) "0100010001100mmm" SIGN_WORD 0x8000
     "xlate_negw_predec_0"
     (assign $1.aul (- $1.aul 2))
     (list))
(NEG negw_postinc (derefuw $1.aul) "0100010001011mmm" SIGN_WORD 0x8000
     "xlate_negw_postinc_0"
     (list)
     (assign $1.aul (+ $1.aul 2)))
(NEG negw_indoff (derefuw (+ $1.asl $2.sl)) "0100010001101mmmwwwwwwwwwwwwwwww"
     SIGN_WORD 0x8000 "xlate_negw_indoff_1_0")

(NEG negl_reg $1.dul "0100010010000mmm" SIGN_LONG 0x80000000
     "xlate_negl_reg_0")
(NEG negl_ind (dereful $1.aul) "0100010010010mmm" SIGN_LONG 0x80000000
     "xlate_negl_ind_0")
(NEG negl_predec (dereful $1.aul) "0100010010100mmm" SIGN_LONG 0x80000000
     "xlate_negl_predec_0"
     (assign $1.aul (- $1.aul 4))
     (list))
(NEG negl_postinc (dereful $1.aul) "0100010010011mmm" SIGN_LONG 0x80000000
     "xlate_negl_postinc_0"
     (list)
     (assign $1.aul (+ $1.aul 4)))
(NEG negl_indoff (dereful (+ $1.asl $2.sl)) "0100010010101mmmwwwwwwwwwwwwwwww"
     SIGN_LONG 0x80000000 "xlate_negl_indoff_1_0")

(NEG negb $1.mub "0100010000mmmmmm" SIGN_BYTE 0x80       "none")
(NEG negw $1.muw "0100010001mmmmmm" SIGN_WORD 0x8000     "none")
(NEG negl $1.mul "0100010010mmmmmm" SIGN_LONG 0x80000000 "none")

(define (NEGX name dst temp temp2 bit_pattern overflow)
  (defopcode name
    (list 68000 amode_alterable_data () (list bit_pattern))
    (list "-----" "---X-" dont_expand
	  (assign dst (- 0 dst (<> ccx 0))))
    (list "CNVXZ" "---X-" dont_expand
	  (list
	   (assign temp dst)
	   (assign temp2 (assign dst (- 0 temp (<> ccx 0))))
	   (assign ccn (<> 0 (& overflow temp2)))
	   (assign ccv (<> 0 (& temp temp2 overflow)))
	   (assign ccx (assign ccc (<> (& (| temp temp2) overflow) 0)))
	   (assign ccnz (| ccnz (<> 0 temp2)))))))

(NEGX negxb $1.mub tmp.ub tmp2.ub "0100000000mmmmmm" 0x80)
(NEGX negxw $1.muw tmp.uw tmp2.uw "0100000001mmmmmm" 0x8000)
(NEGX negxl $1.mul tmp.ul tmp2.ul "0100000010mmmmmm" 0x80000000)


(defopcode nop
  (list 68000 amode_implicit () (list "0100111001110001"))
  (list "-----" "-----" dont_expand
	(list)))

(define (NOT name dst bit_pattern cc_func)
  (defopcode name
    (list 68000 amode_alterable_data () (list bit_pattern))
    (list "-----" "-----" dont_expand
	  (assign dst (^ dst (~ 0))))
    (list "CNV-Z" "-----" dont_expand
	  (cc_func (assign dst (^ dst (~ 0)))))))

(NOT notb $1.mub "0100011000mmmmmm" ASSIGN_C_N_V_NZ_BYTE)
(NOT notw $1.muw "0100011001mmmmmm" ASSIGN_C_N_V_NZ_WORD)
(NOT notl $1.mul "0100011010mmmmmm" ASSIGN_C_N_V_NZ_LONG)

(defopcode pack_reg
  (list 68000 amode_implicit () (list "1000xxx101000yyy" "wwwwwwwwwwwwwwww"))
  (list "-----" "-----" dont_expand
	(list
	 (assign tmp.uw (+ $2.duw $3.uw))
	 (assign $1.dub (| (& tmp.uw 0xF) (& (>> tmp.uw 4) 0xF0))))))

; special case a7 so we predec it by two
(defopcode pack_mem_to_a7
  (list 68000 amode_implicit () (list "1000111101001yyy" "wwwwwwwwwwwwwwww"))
  (list "-----" "-----" dont_expand
	(list
	 
	 (assign $1.aul (- $1.aul 2))
	 "\n#ifdef QUADALIGN\n"
	 (assign tmp.uw (+ (| (<< (derefub $1.aul) 8) (derefub (+ $1.aul 1)))
			   $2.uw))
	 "\n#else\n"
	 (assign tmp.uw (+ (derefuw $1.aul) $2.uw))
	 "\n#endif\n"
	 (assign a7.ul (- a7.ul 2))
	 (assign (derefub a7.ul) (| (& tmp.uw 0xF) (& (>> tmp.uw 4) 0xF0))))))

(defopcode pack_mem
  (list 68000 amode_implicit () (list "1000xxx101001yyy" "wwwwwwwwwwwwwwww"))
  (list "-----" "-----" dont_expand
	(list
	 (assign $2.aul (- $2.aul 2))
	 "\n#ifdef QUADALIGN\n"
	 (assign tmp.uw (+ (| (<< (derefub $2.aul) 8) (derefub (+ $2.aul 1)))
			   $3.uw))
	 "\n#else\n"
	 (assign tmp.uw (+ (derefuw $2.aul) $3.uw))
	 "\n#endif\n"
	 (assign $1.aul (- $1.aul 1))
	 (assign (derefub $1.aul) (| (& tmp.uw 0xF) (& (>> tmp.uw 4) 0xF0))))))


(defopcode unpk_reg
  (list 68000 amode_implicit () (list "1000yyy110000xxx" "wwwwwwwwwwwwwwww"))
  (list "-----" "-----" dont_expand
	(assign $1.duw (+ (| (<< (& $2.dub 0xF0) 4) (& $2.dub 0x0F)) $3.uw))))

(defopcode unpk_mem_from_a7
  (list 68000 amode_implicit () (list "1000yyy110001111" "wwwwwwwwwwwwwwww"))
  (list "-----" "-----" dont_expand
	(list
	 (assign a7.ul (- a7.ul 2))
	 (assign tmp.ub (derefub a7.ul))
	 (assign $1.aul (- $1.aul 2))
	 (assign tmp2.uw (+ (| (<< (& tmp.ub 0xF0) 4) (& tmp.ub 0x0F)) $2.uw))
	 "\n#ifdef QUADALIGN\n"
	 (assign (derefub $1.aul) (>> tmp2.uw 8))
	 (assign (derefub (+ $1.aul 1)) tmp2.ub)
	 "\n#else\n"
	 (assign (derefuw $1.aul) tmp2.uw)
	 "\n#endif\n")))

(defopcode unpk_mem
  (list 68000 amode_implicit () (list "1000yyy110001xxx" "wwwwwwwwwwwwwwww"))
  (list "-----" "-----" dont_expand
	(list
	 (assign $2.aul (- $2.aul 1))
	 (assign tmp.ub (derefub $2.aul))
	 (assign $1.aul (- $1.aul 2))
	 (assign tmp2.uw (+ (| (<< (& tmp.ub 0xF0) 4) (& tmp.ub 0x0F)) $3.uw))
	 "\n#ifdef QUADALIGN\n"
	 (assign (derefub $1.aul) (>> tmp2.uw 8))
	 (assign (derefub (+ $1.aul 1)) tmp2.ub)
	 "\n#else\n"
	 (assign (derefuw $1.aul) tmp2.uw)
	 "\n#endif\n")))

(defopcode pea_w
  (list 68000 "0100100001111000" ()
	(list "0100100001aaa000" "wwwwwwwwwwwwwwww"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_movel_imm_predec_1_0")
	(list
	 (assign a7.aul (- a7.aul 4))
	 (assign (derefsl $1.aul) $2.sl))))

(defopcode pea_l
  (list 68000 "0100100001111001" ()
	(list "0100100001aaa001" "llllllllllllllll" "llllllllllllllll"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_movel_imm_predec_1_0")
	(list
	 (assign a7.ul (- a7.ul 4))
	 (assign (derefsl $1.aul) $2.sl))))

(defopcode pea_ind
  (list 68000 "0100100001010xxx" ()
	(list "010010000z010aaa"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_movel_areg_predec_1_0")
	(list
	 (assign (dereful (- a7.ul 4)) $2.aul)
	 (assign a7.ul (- a7.ul 4))
	 $1.sl))) ; hack to give native code an a7 operand

(defopcode pea_d16
  (list 68000 "0100100001101xxx" ()
	(list "010010000110zaaawwwwwwwwwwwwwwww"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_pea_indoff")
	(list
	 (assign (dereful (- a7.ul 4)) (+ $2.asl $3.sl))
	 (assign a7.ul (- a7.ul 4))
	 $1.sl))) ; hack to give native code an a7 operand

(defopcode pea
  (list 68000 amode_control () (list "0100100001mmmmmm"))
  (list "-----" "-----" dont_expand
	(list
	 (assign a7.ul (- a7.ul 4))
	 (assign (dereful a7.ul) $1.pul))))


(define (ROT_REG name bit_pattern sh1 sh2 cmsk size count shft val tmp nonzero)
  (defopcode name
    (list 68000 amode_implicit () (list bit_pattern))
    (list "-----" "-----" dont_expand
	  (if (or nonzero (& count 63))
	      (list
	       (assign tmp val)
	       (assign val (| (sh1 tmp shft) (sh2 tmp (- size shft)))))))
    (list "CN0-Z" "-----" dont_expand
	  (list
	   (assign tmp val)
	   (if (or nonzero (& count 63))
	       (list
		(assign val (assign tmp (| (sh1 tmp shft)
					   (sh2 tmp (- size shft)))))
		(assign ccc (<> (& tmp cmsk) 0)))
	       (assign ccc 0))
	   (assign ccnz (<> 0 tmp))
	   (assign ccn (& (>> tmp (- size 1)) 1))))))

(ROT_REG rolb_reg "1110sss100111vvv" << >> 1 8 $1.dul (& $1.dul 7)
	 $2.dub tmp.ub 0)
(ROT_REG rolb_reg_by_8 "1110000100011vvv" << >> 1 8 8 8 $1.dub tmp.ub 1)
(ROT_REG rolb_reg_by_const "1110sss100011vvv" << >> 1 8 $1.uw $1.uw
	 $2.dub tmp.ub 1)
(ROT_REG rolw_reg "1110sss101111vvv" << >> 1 16 $1.dul (& $1.dul 15)
	 $2.duw tmp.uw 0)
(ROT_REG rolw_reg_by_8 "1110000101011vvv" << >> 1 16 8 8 $1.duw tmp.uw 1)
(ROT_REG rolw_reg_by_const "1110sss101011vvv" << >> 1 16 $1.uw $1.uw
	 $2.duw tmp.uw 1)
(ROT_REG roll_reg "1110sss110111vvv" << >> 1 32 $1.dul (& $1.dul 31)
	 $2.dul tmp.ul 0)
(ROT_REG roll_reg_by_8 "1110000110011vvv" << >> 1 32 8 8 $1.dul tmp.ul 1)
(ROT_REG roll_reg_by_const "1110sss110011vvv" << >> 1 32 $1.uw $1.uw
	 $2.dul tmp.ul 1)

(ROT_REG rorb_reg "1110sss000111vvv" >> << 0x80 8 $1.dul (& $1.dul 7)
	 $2.dub tmp.ub 0)
(ROT_REG rorb_reg_by_8 "1110000000011vvv" >> << 0x80 8 8 8 $1.dub tmp.ub 1)
(ROT_REG rorb_reg_by_const "1110sss000011vvv" >> << 0x80 8 $1.uw $1.uw
	 $2.dub tmp.ub 1)
(ROT_REG rorw_reg "1110sss001111vvv" >> << 0x8000 16 $1.dul (& $1.dul 15)
	 $2.duw tmp.uw 0)
(ROT_REG rorw_reg_by_8 "1110000001011vvv" >> << 0x8000 16 8 8 $1.duw tmp.uw 1)
(ROT_REG rorw_reg_by_const "1110sss001011vvv" >> << 0x8000 16 $1.uw $1.uw
	 $2.duw tmp.uw 1)
(ROT_REG rorl_reg "1110sss010111vvv" >> << 0x80000000 32 $1.dul (& $1.dul 31)
	 $2.dul tmp.ul 0)
(ROT_REG rorl_reg_by_8 "1110000010011vvv" >> << 0x80000000 32 8 8
	 $1.dul tmp.ul 1)
(ROT_REG rorl_reg_by_const "1110sss010011vvv" >> << 0x80000000 32 $1.uw $1.uw
	 $2.dul tmp.ul 1)

(defopcode rolw_ea
  (list 68000 amode_alterable_data () (list "1110011111mmmmmm"))
  (list "CN0-Z" "-----" dont_expand
	(list
	 (assign tmp.uw $1.muw)
	 (ASSIGN_NNZ_WORD (assign tmp.uw (| (<< tmp.uw 1) (>> tmp.uw 15))))
	 (assign ccc (& tmp.uw 1))
	 (assign $1.muw tmp.uw))))

(defopcode rorw_ea
  (list 68000 amode_alterable_data () (list "1110011011mmmmmm"))
  (list "CN0-Z" "-----" dont_expand
	(list
	 (assign tmp.uw $1.muw)
	 (ASSIGN_NNZ_WORD (assign tmp.uw (| (>> tmp.uw 1) (<< tmp.uw 15))))
	 (assign ccc (SIGN_WORD tmp.uw))
	 (assign $1.muw tmp.uw))))


(define (ROXRBW_REG name bit_pattern count val size is_const)
  (defopcode name
    (list 68000 amode_implicit () (list bit_pattern))
    (list "CN0%Z" "---X-" dont_expand
	  (list
	   (assign tmp.ul (| val (<< (<> ccx 0) size)))
	   (assign tmp2.ul count)
	   (if (not is_const)
	       (assign tmp2.ul (% tmp2.ul (+ size 1))))
	   (if (or is_const (<> tmp2.ul 0))
	       (list
		(assign ccx (& (>> tmp.ul (- tmp2.ul 1)) 1))
		(assign val (| (>> tmp.ul tmp2.ul)
			       (<< tmp.ul (- (+ size 1) tmp2.ul))))))
	   (assign ccc ccx)
	   (assign ccnz (<> 0 val))
	   (assign ccn (& 1 (>> val (- size 1))))))))

(ROXRBW_REG roxrb_reg          "1110sss000110vvv" (& $1.dul 63) $2.dub 8  0)
(ROXRBW_REG roxrb_reg_by_8     "1110000000010vvv" 8             $1.dub 8  1)
(ROXRBW_REG roxrb_reg_by_const "1110sss000010vvv" $1.uw         $2.dub 8  1)
(ROXRBW_REG roxrw_reg          "1110sss001110vvv" (& $1.dul 63) $2.duw 16 0)
(ROXRBW_REG roxrw_reg_by_8     "1110000001010vvv" 8             $1.duw 16 1)
(ROXRBW_REG roxrw_reg_by_const "1110sss001010vvv" $1.uw         $2.duw 16 1)

(define (ROX_L_REG name bit_pattern count val sh1 sh2 shft is_const)
  (defopcode name
    (list 68000 amode_implicit () (list bit_pattern))
    (list "CN0%Z" "---X-" dont_expand
	  (list
	   (assign tmp.ul (<< (<> ccx 0) shft))
	   (assign tmp2.ul count)
	   (if (or is_const (and (<> tmp2.ul 0) (<> tmp2.ul 33)))
	       (list
		(if (and (not is_const) (>= tmp2.ul 33))
		    (assign tmp2.ul (- tmp2.ul 33)))
		(if (= tmp2.ul 1)
		    (list
		     (assign ccx (& (>> val (- 31 shft)) 1))
		     (assign val (| (sh1 val 1) tmp.ul)))
		    (list
		     (assign ccx (<> 0 (& val (sh2 (cast "uint32"
							 (<< 1 (- 31 shft)))
						   (- tmp2.ul 1)))))
		     (if (= tmp2.ul 32)    ; Avoid shifting by 32, not portable
			 (assign val (| (sh1 tmp.ul 31) (sh2 val 1)))
			 (assign val (| (sh1 val tmp2.ul)
					(sh1 tmp.ul (- tmp2.ul 1))
					(sh2 val (- 33 tmp2.ul)))))))))
	   (assign ccc ccx)
	   (ASSIGN_NNZ_LONG val)))))

(ROX_L_REG roxrl_reg "1110sss010110vvv" (& $1.dul 63)  $2.dul >> << 31 0)
(ROX_L_REG roxrl_reg_by_8     "1110000010010vvv" 8     $1.dul >> << 31 1)
(ROX_L_REG roxrl_reg_by_const "1110sss010010vvv" $1.uw $2.dul >> << 31 1)

(ROX_L_REG roxll_reg  "1110sss110110vvv" (& $1.dul 63) $2.dul << >> 0 0)
(ROX_L_REG roxll_reg_by_8     "1110000110010vvv" 8     $1.dul << >> 0 1)
(ROX_L_REG roxll_reg_by_const "1110sss110010vvv" $1.uw $2.dul << >> 0 1)

(defopcode roxrw_ea
  (list 68000 amode_alterable_data () (list "1110010011mmmmmm"))
  (list "CN0XZ" "---X-" dont_expand
	(list
	 (assign ccc (& (assign tmp.uw $1.muw) 1))
	 (ASSIGN_NNZ_WORD
	  (assign $1.muw
		  (assign tmp.uw (| (<< (<> ccx 0) 15) (>> tmp.uw 1)))))
	 (assign $1.muw tmp.uw)
	 (assign ccx ccc))))

(define (ROXLBW_REG name bit_pattern count val size is_const)
  (defopcode name
    (list 68000 amode_implicit () (list bit_pattern))
    (list "CN0%Z" "---X-" dont_expand
	  (list
	   (assign tmp2.ul count)
	   (if (not is_const)
	       (assign tmp2.ul (% tmp2.ul (+ size 1))))
	   (if (or is_const (<> tmp2.ul 0))
	       (list
		(assign tmp.ul (<> ccx 0))
		(assign ccx (<> 0
				(& val (>> (<< 1 (- size 1)) (- tmp2.ul 1)))))
		(assign val (| (<< val tmp2.ul)
			       (>> val (- (+ size 1) tmp2.ul))
			       (<< tmp.ul (- tmp2.ul 1))))))
	   (assign ccc ccx)
	   (assign ccnz (<> 0 val))
	   (assign ccn (& (>> val (- size 1)) 1))))))


(ROXLBW_REG roxlb_reg          "1110sss100110vvv" (& $1.dul 63) $2.dub 8  0)
(ROXLBW_REG roxlb_reg_by_8     "1110000100010vvv" 8             $1.dub 8  1)
(ROXLBW_REG roxlb_reg_by_const "1110sss100010vvv" $1.uw         $2.dub 8  1)
(ROXLBW_REG roxlw_reg          "1110sss101110vvv" (& $1.dul 63) $2.duw 16 0)
(ROXLBW_REG roxlw_reg_by_8     "1110000101010vvv" 8             $1.duw 16 1)
(ROXLBW_REG roxlw_reg_by_const "1110sss101010vvv" $1.uw         $2.duw 16 1)



(defopcode roxlw_ea
  (list 68000 amode_alterable_data () (list "1110010111mmmmmm"))
  (list "CN0XZ" "---X-" dont_expand
	(list
	 (assign ccc (>> (assign tmp.uw $1.muw) 15))
	 (ASSIGN_NNZ_WORD
	  (assign $1.muw
		  (assign tmp.uw (| (<> ccx 0) (<< tmp.uw 1)))))
	 (assign $1.muw tmp.uw)
	 (assign ccx ccc))))


(defopcode rtd
  (list 68010 amode_implicit (ends_block next_block_dynamic)
	(list "0100111001110100" "wwwwwwwwwwwwwwww"))
  (list "-----" "-----" dont_expand
	(list
	 (assign tmp.ul (dereful a7.ul))
	 (assign a7.ul (+ a7.ul 4 $1.sl))
	 (assign code (call "code_lookup" tmp.ul)))))

(defopcode rtr
  (list 68000 amode_implicit (ends_block next_block_dynamic)
	(list "0100111001110111"))
  (list "CNVXZ" "-----" dont_expand
	(list
	 "unsigned ix"
	 "const jsr_stack_elt_t *j"
	 (assign tmp.uw (derefuw a7.ul))
	 (assign a7.ul (+ a7.ul 2))
	 (assign ccc  (& tmp.uw 0x1))
	 (assign ccv  (& tmp.uw 0x2))
	 (assign ccn  (& tmp.uw 0x8))
	 (assign ccx  (& tmp.uw 0x10))
	 (assign ccnz (& (~ tmp.uw) 0x4))
	 (assign tmp2.ul (call "READUL_UNSWAPPED" a7.ul))
	 (assign a7.ul (+ a7.ul 4))
	 (assign "ix" "cpu_state.jsr_stack_byte_index")
	 (assign "j" "(jsr_stack_elt_t *)((char *)&cpu_state.jsr_stack + ix)")
	 (if (= "j->tag" tmp2.ul)
	     (list
	      (assign code "j->code")
	      (assign "cpu_state.jsr_stack_byte_index"
		      (% (+ "ix" "sizeof (jsr_stack_elt_t)")
			 "sizeof (cpu_state.jsr_stack)")))
	     (assign code (call "code_lookup"
				(call "SWAPUL_IFLE" tmp2.ul)))))))

(defopcode rts
  (list 68000 "0100111001110101" (ends_block next_block_dynamic)
	(list "0100aaa001110101"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_rts")
	(list
	 "unsigned ix"
	 "const jsr_stack_elt_t *j"
	 (assign tmp.ul (call "READUL_UNSWAPPED" a7.ul))
	 (assign a7.ul (+ a7.ul 4))
	 (assign "ix" "cpu_state.jsr_stack_byte_index")
	 (assign "j" "(jsr_stack_elt_t *)((char *)&cpu_state.jsr_stack + ix)")
	 (if (= "j->tag" tmp.ul)
	     (list
	      (assign code "j->code")
	      (assign "cpu_state.jsr_stack_byte_index"
		      (% (+ "ix" "sizeof (jsr_stack_elt_t)")
			 "sizeof cpu_state.jsr_stack")))
	     (assign code (call "code_lookup"
				(call "SWAPUL_IFLE" tmp.ul))))
	 $1.ul)))  ; hack to give native code an a7


(defopcode rte
  (list 68000 amode_implicit (ends_block next_block_dynamic)
	(list "0100111001110011"))
  (list "CNVXZ" "-----" dont_expand
	(list
	 "{ int done = 0"
	 "while (!done) {"  ; Loop in case we hit exception frame 1
	 (switch (>> (derefuw (+ a7.ul 6)) 12)

		 ; Normal 4 word frame
		 (0 (list
		     (assign tmp2.uw (derefuw a7.ul))
		     (assign tmp.ul (dereful (+ a7.ul 2)))
		     (assign a7.ul (+ a7.ul 8))
		     (LOAD_NEW_SR tmp2.uw)
		     (assign code (call "code_lookup" tmp.ul))
		     (assign "done" 1)
		     "break"))

		 ; Throwaway 4 word frame
		 (1 (list
		     (assign tmp2.uw (derefuw a7.ul))
		     (assign a7.ul (+ a7.ul 8))
		     (LOAD_NEW_SR tmp2.uw)
		     "continue"))

		 ; Normal 6 word frame
		 (2 (list
		     (assign tmp2.uw (derefuw a7.ul))
		     (assign tmp.ul (dereful (+ a7.ul 2)))
		     (assign a7.ul (+ a7.ul 12))
		     (LOAD_NEW_SR tmp2.uw)
		     (assign code (call "code_lookup" tmp.ul))
		     (assign "done" 1)
		     "break"))

		 (7 (list
		     (assign tmp2.uw (derefuw a7.ul))
		     (assign tmp.ul (dereful (+ a7.ul 2)))
		     (assign a7.ul (+ a7.ul 60))
		     (LOAD_NEW_SR tmp2.uw)
		     (assign code (call "code_lookup" tmp.ul))
		     (assign "done" 1)
		     "break"))

		 (8 (list
		     (assign tmp2.uw (derefuw a7.ul))
		     (assign tmp.ul (dereful (+ a7.ul 2)))
		     (assign a7.ul (+ a7.ul 58))
		     (LOAD_NEW_SR tmp2.uw)
		     (assign code (call "code_lookup" tmp.ul))
		     (assign "done" 1)
		     "break"))

		 (9 (list
		     (assign tmp2.uw (derefuw a7.ul))
		     (assign tmp.ul (dereful (+ a7.ul 2)))
		     (assign a7.ul (+ a7.ul 20))
		     (LOAD_NEW_SR tmp2.uw)
		     (assign code (call "code_lookup" tmp.ul))
		     (assign "done" 1)
		     "break"))

		 (10 (list
		      (assign tmp2.uw (derefuw a7.ul))
		      (assign tmp.ul (dereful (+ a7.ul 2)))
		      (assign a7.ul (+ a7.ul 32))
		      (LOAD_NEW_SR tmp2.uw)
		      (assign code (call "code_lookup" tmp.ul))
		      (assign "done" 1)
		      "break"))

		 (11 (list
		      (assign tmp2.uw (derefuw a7.ul))
		      (assign tmp.ul (dereful (+ a7.ul 2)))
		      (assign a7.ul (+ a7.ul 92))
		      (LOAD_NEW_SR tmp2.uw)
		      (assign code (call "code_lookup" tmp.ul))
		      (assign "done" 1)
		      "break"))

		 ; Unknown type - generate a format exception
		 (default
		   (list
		    (TRAP 14 (deref "uint32 *" code 0) 0)
		    (assign "done" 1)
		    "break")))
	 "} }")))


(define (SBCD name amode bit_pattern src dst preamble postamble)
  (defopcode name
    (list 68000 amode () (list bit_pattern))
    (list "C??X<" "---X-" dont_expand
	  (list
	   preamble
	   "{ int32 src, dst"
	   (assign ccx (<> ccx 0))
	   (assign "src" src)
	   (assign "dst" dst)

	   ; This only covers some of the cases
	   (assign ccc (> (+ ccx (- (& "src" 0xF) (& "dst" 0xF))
			     (* 10 (- (>> "src" 4)
				      (>> "dst" 4))))
			  0))

	   ; Compute rough guess at the value
	   (assign tmp.sb (- "dst" "src" ccx))

	   ; Correct it
	   (if (> (& "src" 0xF) (- (& "dst" 0xF) ccx))
	       (list
		(assign tmp.sb (- tmp.sb 6))
		(if (>= (& "src" 0xF0) (- (& "dst" 0xF0) ccx))
		    (assign tmp.sb (- tmp.sb 0x60))))
	       (list
		(if (> (& "src" 0xF0) (& "dst" 0xF0))
		    (list
		     (assign tmp.sb (- tmp.sb 0x60))
		     (assign ccx (assign ccc 1))))))

	   (assign ccnz (| ccnz tmp.sb))
	   (assign ccx ccc)
	   (assign ccv 0)                ; For 68020; left alone on 68040
	   (assign ccn (& tmp.sb 0x80))  ; For 68020; left alone on 68040
	   (assign dst tmp.sb)
	   postamble
	   "}"))))

(SBCD sbcd_dreg amode_implicit "1000xxx100000yyy" $2.dub $1.dub (list) (list))

; Handle the sbcd an@-,an@- case (annoying)
(define (SAME_SBCD name reg bits size)
  (SBCD name amode_same_reg bits
	(derefub reg)
	(derefub (- reg size))
	(assign reg (- reg size))
	(assign reg (- reg size))))

(SAME_SBCD sbcd_a7@-,a7@- a7.ul "1000111100001111" 2)
(SAME_SBCD sbcd_an@-,an@- $1.aul "1000aaa100001aaa" 1)

(SBCD sbcd_an@-,a7@- amode_implicit "1000111100001yyy"
      (derefub $1.aul)
      (derefub a7.ul)
      (list
       (assign $1.aul (- $1.aul 1))
       (assign a7.ul (- a7.ul 2)))
      (list))

(SBCD sbcd_a7@-,an@- amode_implicit "1000aaa100001111"
      (derefub a7.ul)
      (derefub $1.aul)
      (list
       (assign $1.aul (- $1.aul 1))
       (assign a7.ul (- a7.ul 2)))
      (list))

(SBCD sbcd_mem amode_implicit "1000xxx100001yyy"
      (derefub $2.aul)
      (derefub $1.aul)
      (list
       (assign $1.aul (- $1.aul 1))
       (assign $2.aul (- $2.aul 1)))
      (list))


(define (SCC_REG name strname cc_req bit_pattern condn)
  (defopcode name
    (list 68000 amode_implicit () (list bit_pattern))
    (list "-----" cc_req dont_expand
	  (native_code "xlate_" strname "_reg_0")
	  (assign $1.dub (- 0 condn)))))

(define (SCC name cc_req bit_pattern condn)
  (defopcode name
    (list 68000 amode_alterable_data () (list bit_pattern))
    (list "-----" cc_req dont_expand
	  (assign $1.mub (- 0 condn)))))

(SCC_REG st  "st"  "-----" "0101000011000mmm" 1)
(SCC_REG sf  "sf"  "-----" "0101000111000mmm" 0)
(SCC_REG shi "shi" "C---Z" "0101001011000mmm" CC_HI)
(SCC_REG sls "sls" "C---Z" "0101001111000mmm" CC_LS)
(SCC_REG scc "scc" "C----" "0101010011000mmm" CC_CC)
(SCC_REG scs "scs" "C----" "0101010111000mmm" CC_CS)
(SCC_REG sne "sne" "----Z" "0101011011000mmm" CC_NE)
(SCC_REG seq "seq" "----Z" "0101011111000mmm" CC_EQ)
(SCC_REG svc "svc" "--V--" "0101100011000mmm" CC_VC)
(SCC_REG svs "svs" "--V--" "0101100111000mmm" CC_VS)
(SCC_REG spl "spl" "-N---" "0101101011000mmm" CC_PL)
(SCC_REG smi "smi" "-N---" "0101101111000mmm" CC_MI)
(SCC_REG sge "sge" "-NV--" "0101110011000mmm" CC_GE)
(SCC_REG slt "slt" "-NV--" "0101110111000mmm" CC_LT)
(SCC_REG sgt "sgt" "-NV-Z" "0101111011000mmm" CC_GT)
(SCC_REG sle "sle" "-NV-Z" "0101111111000mmm" CC_LE)

(SCC st  "-----" "0101000011mmmmmm" 1)
(SCC sf  "-----" "0101000111mmmmmm" 0)
(SCC shi "C---Z" "0101001011mmmmmm" CC_HI)
(SCC sls "C---Z" "0101001111mmmmmm" CC_LS)
(SCC scc "C----" "0101010011mmmmmm" CC_CC)
(SCC scs "C----" "0101010111mmmmmm" CC_CS)
(SCC sne "----Z" "0101011011mmmmmm" CC_NE)
(SCC seq "----Z" "0101011111mmmmmm" CC_EQ)
(SCC svc "--V--" "0101100011mmmmmm" CC_VC)
(SCC svs "--V--" "0101100111mmmmmm" CC_VS)
(SCC spl "-N---" "0101101011mmmmmm" CC_PL)
(SCC smi "-N---" "0101101111mmmmmm" CC_MI)
(SCC sge "-NV--" "0101110011mmmmmm" CC_GE)
(SCC slt "-NV--" "0101110111mmmmmm" CC_LT)
(SCC sgt "-NV-Z" "0101111011mmmmmm" CC_GT)
(SCC sle "-NV-Z" "0101111111mmmmmm" CC_LE)

(defopcode swap
  (list 68000 amode_implicit () (list "0100100001000ddd"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_swap")
	(list
	 (assign tmp.ul (<< $1.dul 16))
	 (assign $1.dul (| (>> $1.dul 16) tmp.ul))))
  (list "CNV-Z" "-----" dont_expand
	(native_code "xlate_swap")
	(list
	 (assign tmp.ul (| (<< $1.dul 16) (>> $1.dul 16)))
	 (assign $1.dul tmp.ul)
	 (ASSIGN_C_N_V_NZ_LONG tmp.ul))))

(defopcode tas
  (list 68000 amode_alterable_data () (list "0100101011mmmmmm"))
  (list "CNV-Z" "-----" dont_expand
	(list
	 (ASSIGN_C_N_V_NZ_BYTE $1.mub)
	 (assign $1.mub (| $1.mub 0x80)))))

(defopcode trap
  (list 68000 amode_implicit
	(ends_block next_block_dynamic skip_two_operand_words)
	(list "010011100100vvvv"))
  (list "-----" "-----" dont_expand
	(TRAP (+ $1.uw 32) (deref "uint32 *" code 0) 0)))

(defopcode trapv
  (list 68000 amode_implicit (ends_block next_block_dynamic)
	(list "0100111001110110"))
  (list "-----" "---V-" dont_expand
	(list
	 (assign tmp.ul (deref "uint32 *" code 0))
	 (if ccv
	     (TRAP 7 tmp.ul (- tmp.ul 2))
	     (assign code
		     (call "code_lookup" tmp.ul))))))

(define (TRAPCC name cc bit_pattern expr size)
  (defopcode name
    (list 68020 amode_implicit (ends_block next_block_dynamic) bit_pattern)
    (list "-----" cc dont_expand
	  (list
	   (assign tmp.ul (deref "uint32 *" code 0))
	   (if expr
	       (TRAP 7 tmp.ul (- tmp.ul size))
	       (assign code (call "code_lookup" tmp.ul)))))))

; No optional arguments
(TRAPCC trapt  "-----" (list "0101000011111100") 1     2)
(TRAPCC trapf  "-----" (list "0101000111111100") 0     2)
(TRAPCC traphi "C---Z" (list "0101001011111100") CC_HI 2)
(TRAPCC trapls "C---Z" (list "0101001111111100") CC_LS 2)
(TRAPCC trapcc "C----" (list "0101010011111100") CC_CC 2)
(TRAPCC trapcs "C----" (list "0101010111111100") CC_CS 2)
(TRAPCC trapne "----Z" (list "0101011011111100") CC_NE 2)
(TRAPCC trapeq "----Z" (list "0101011111111100") CC_EQ 2)
(TRAPCC trapvc "--V--" (list "0101100011111100") CC_VC 2)
(TRAPCC trapvs "--V--" (list "0101100111111100") CC_VS 2)
(TRAPCC trappl "-N---" (list "0101101011111100") CC_PL 2)
(TRAPCC trapmi "-N---" (list "0101101111111100") CC_MI 2)
(TRAPCC trapge "-NV--" (list "0101110011111100") CC_GE 2)
(TRAPCC traplt "-NV--" (list "0101110111111100") CC_LT 2)
(TRAPCC trapgt "-NV-Z" (list "0101111011111100") CC_GT 2)
(TRAPCC traple "-NV-Z" (list "0101111111111100") CC_LE 2)

; With optional word
(TRAPCC trapwt  "-----" (list "0101000011111010" "wwwwwwwwwwwwwwww") 1     4)
(TRAPCC trapwf  "-----" (list "0101000111111010" "wwwwwwwwwwwwwwww") 0     4)
(TRAPCC trapwhi "C---Z" (list "0101001011111010" "wwwwwwwwwwwwwwww") CC_HI 4)
(TRAPCC trapwls "C---Z" (list "0101001111111010" "wwwwwwwwwwwwwwww") CC_LS 4)
(TRAPCC trapwcc "C----" (list "0101010011111010" "wwwwwwwwwwwwwwww") CC_CC 4)
(TRAPCC trapwcs "C----" (list "0101010111111010" "wwwwwwwwwwwwwwww") CC_CS 4)
(TRAPCC trapwne "----Z" (list "0101011011111010" "wwwwwwwwwwwwwwww") CC_NE 4)
(TRAPCC trapweq "----Z" (list "0101011111111010" "wwwwwwwwwwwwwwww") CC_EQ 4)
(TRAPCC trapwvc "--V--" (list "0101100011111010" "wwwwwwwwwwwwwwww") CC_VC 4)
(TRAPCC trapwvs "--V--" (list "0101100111111010" "wwwwwwwwwwwwwwww") CC_VS 4)
(TRAPCC trapwpl "-N---" (list "0101101011111010" "wwwwwwwwwwwwwwww") CC_PL 4)
(TRAPCC trapwmi "-N---" (list "0101101111111010" "wwwwwwwwwwwwwwww") CC_MI 4)
(TRAPCC trapwge "-NV--" (list "0101110011111010" "wwwwwwwwwwwwwwww") CC_GE 4)
(TRAPCC trapwlt "-NV--" (list "0101110111111010" "wwwwwwwwwwwwwwww") CC_LT 4)
(TRAPCC trapwgt "-NV-Z" (list "0101111011111010" "wwwwwwwwwwwwwwww") CC_GT 4)
(TRAPCC trapwle "-NV-Z" (list "0101111111111010" "wwwwwwwwwwwwwwww") CC_LE 4)

; With optional long
(TRAPCC traplt  "-----" (list "0101000011111011" "llllllllllllllll"
			      "llllllllllllllll") 1 6)
(TRAPCC traplf  "-----" (list "0101000111111011" "llllllllllllllll"
			      "llllllllllllllll") 0 6)
(TRAPCC traplhi "C---Z" (list "0101001011111011" "llllllllllllllll"
			      "llllllllllllllll") CC_HI 6)
(TRAPCC traplls "C---Z" (list "0101001111111011" "llllllllllllllll"
			      "llllllllllllllll") CC_LS 6)
(TRAPCC traplcc "C----" (list "0101010011111011" "llllllllllllllll"
			      "llllllllllllllll") CC_CC 6)
(TRAPCC traplcs "C----" (list "0101010111111011" "llllllllllllllll"
			      "llllllllllllllll") CC_CS 6)
(TRAPCC traplne "----Z" (list "0101011011111011" "llllllllllllllll"
			      "llllllllllllllll") CC_NE 6)
(TRAPCC trapleq "----Z" (list "0101011111111011" "llllllllllllllll"
			      "llllllllllllllll") CC_EQ 6)
(TRAPCC traplvc "--V--" (list "0101100011111011" "llllllllllllllll"
			      "llllllllllllllll") CC_VC 6)
(TRAPCC traplvs "--V--" (list "0101100111111011" "llllllllllllllll"
			      "llllllllllllllll") CC_VS 6)
(TRAPCC traplpl "-N---" (list "0101101011111011" "llllllllllllllll"
			      "llllllllllllllll") CC_PL 6)
(TRAPCC traplmi "-N---" (list "0101101111111011" "llllllllllllllll"
			      "llllllllllllllll") CC_MI 6)
(TRAPCC traplge "-NV--" (list "0101110011111011" "llllllllllllllll"
			      "llllllllllllllll") CC_GE 6)
(TRAPCC trapllt "-NV--" (list "0101110111111011" "llllllllllllllll"
			      "llllllllllllllll") CC_LT 6)
(TRAPCC traplgt "-NV-Z" (list "0101111011111011" "llllllllllllllll"
			      "llllllllllllllll") CC_GT 6)
(TRAPCC traplle "-NV-Z" (list "0101111111111011" "llllllllllllllll"
			      "llllllllllllllll") CC_LE 6)

(define (TST name bit_pattern mode dst cc_func native pre post)
  (defopcode name
    (list 68000 mode () (list bit_pattern))
    (list "CNV-Z" "-----" dont_expand
	  (native_code native)
	  (list
	   pre
	   (cc_func dst)
	   post))))
(define (TST name bit_pattern mode dst cc_func native)
  (TST name bit_pattern mode dst cc_func native (list) (list)))


(TST tstb_reg "0100101000000mmm" amode_implicit
     $1.dub ASSIGN_C_N_V_NZ_BYTE "xlate_tstb_reg_0")
(TST tstb_ind "0100101000010mmm" amode_implicit
     (derefub $1.aul) ASSIGN_C_N_V_NZ_BYTE "xlate_tstb_ind_0")
(TST tstb_predec "0100101000100mmm" amode_implicit
     (derefub $1.aul) ASSIGN_C_N_V_NZ_BYTE "xlate_tstb_predec_0"
     (if (= $1.ul 7)
	 (assign a7.ul (- a7.ul 2))
	 (assign $1.aul (- $1.aul 1)))
     (list))
(TST tstb_postinc "0100101000011mmm" amode_implicit
     (derefub $1.aul) ASSIGN_C_N_V_NZ_BYTE "xlate_tstb_postinc_0"
     (list)
     (if (= $1.ul 7)
	 (assign a7.ul (+ a7.ul 2))
	 (assign $1.aul (+ $1.aul 1))))
(TST tstb_indoff "0100101000101mmmwwwwwwwwwwwwwwww" amode_implicit
     (derefub (+ $1.asl $2.sl)) ASSIGN_C_N_V_NZ_BYTE "xlate_tstb_indoff_1_0")

(TST tstw_reg "0100101001000mmm" amode_implicit
     $1.duw ASSIGN_C_N_V_NZ_WORD "xlate_tstw_reg_0")
(TST tstw_areg "0100101001001mmm" amode_implicit
     $1.auw ASSIGN_C_N_V_NZ_WORD "xlate_tstw_areg_0")
(TST tstw_ind "0100101001010mmm" amode_implicit
     (derefuw $1.aul) ASSIGN_C_N_V_NZ_WORD "xlate_tstw_ind_0")
(TST tstw_predec "0100101001100mmm" amode_implicit
     (derefuw $1.aul) ASSIGN_C_N_V_NZ_WORD "xlate_tstw_predec_0"
     (assign $1.aul (- $1.aul 2))
     (list))
(TST tstw_postinc "0100101001011mmm" amode_implicit
     (derefuw $1.aul) ASSIGN_C_N_V_NZ_WORD "xlate_tstw_postinc_0"
     (list)
     (assign $1.aul (+ $1.aul 2)))
(TST tstw_indoff "0100101001101mmmwwwwwwwwwwwwwwww" amode_implicit
     (derefuw (+ $1.asl $2.sl)) ASSIGN_C_N_V_NZ_WORD "xlate_tstw_indoff_1_0")

(TST tstl_reg "0100101010000mmm" amode_implicit
     $1.dul ASSIGN_C_N_V_NZ_LONG "xlate_tstl_reg_0")
(TST tstl_areg "0100101010001mmm" amode_implicit
     $1.aul ASSIGN_C_N_V_NZ_LONG "xlate_tstl_areg_0")
(TST tstl_ind "0100101010010mmm" amode_implicit
     (dereful $1.aul) ASSIGN_C_N_V_NZ_LONG "xlate_tstl_ind_0")
(TST tstl_predec "0100101010100mmm" amode_implicit
     (dereful $1.aul) ASSIGN_C_N_V_NZ_LONG "xlate_tstl_predec_0"
     (assign $1.aul (- $1.aul 4))
     (list))
(TST tstl_postinc "0100101010011mmm" amode_implicit
     (dereful $1.aul) ASSIGN_C_N_V_NZ_LONG "xlate_tstl_postinc_0"
     (list)
     (assign $1.aul (+ $1.aul 4)))
(TST tstl_indoff "0100101010101mmmwwwwwwwwwwwwwwww" amode_implicit
     (dereful (+ $1.asl $2.sl)) ASSIGN_C_N_V_NZ_LONG "xlate_tstl_indoff_1_0")

(defopcode tstb_indix
    (list 68000 "xxxxxxxxxx110xxx" ()
	  (list "0100101000mmmmmm"))
    (list "CNV-Z" "-----" dont_expand
	  (native_code "xlate_cmpb_zero_indix_0")
	  (ASSIGN_C_N_V_NZ_BYTE $1.mub)))
(defopcode tstw_indix
    (list 68000 "xxxxxxxxxx110xxx" ()
	  (list "0100101001mmmmmm"))
    (list "CNV-Z" "-----" dont_expand
	  (native_code "xlate_cmpw_zero_indix_0")
	  (ASSIGN_C_N_V_NZ_WORD $1.muw)))
(defopcode tstl_indix
    (list 68000 "xxxxxxxxxx110xxx" ()
	  (list "0100101010mmmmmm"))
    (list "CNV-Z" "-----" dont_expand
	  (native_code "xlate_cmpl_zero_indix_0")
	  (ASSIGN_C_N_V_NZ_LONG $1.mul)))

(TST tstb "0100101000mmmmmm" (intersect amode_data (not amode_immediate))
     $1.mub ASSIGN_C_N_V_NZ_BYTE "none")
(TST tstw "0100101001mmmmmm" (intersect amode_all_combinations
					(not amode_immediate))
     $1.muw ASSIGN_C_N_V_NZ_WORD "none")
(TST tstl "0100101010mmmmmm" (intersect amode_all_combinations
					(not amode_immediate))
     $1.mul ASSIGN_C_N_V_NZ_LONG "none")

(defopcode unlk
  (list 68000 "0100111001011xxx" () (list "0100aaa001011ddd"))
  (list "-----" "-----" dont_expand
	(native_code "xlate_unlk")
	(list
	 (assign a7.ul $2.aul)
	 (assign $2.aul (dereful a7.ul))
	 (assign a7.ul (+ a7.ul 4))
	 $1.ul)))  ; hack to get a7 operand for native code

(defopcode cpush
  (list 68040 (union "11110100xx101xxx" "11110100xx110xxx" "11110100xx111xxx")
	() (list "11110100--1-----"))
  (list "-----" "-----" dont_expand
	(list
	 "{ syn68k_addr_t next_addr"
	 (assign "next_addr" (call "CLEAN"
				   (call "READUL_UNSWAPPED_US" code)))
	 (call "destroy_blocks" 0 -1)
	 (assign code (call "code_lookup " (+ "next_addr" 2)))
	 "}")))

(defopcode f_line_trap
  (list 68000 amode_implicit (ends_block next_block_dynamic)
	(list "1111xxxxxxxxxxxx"))
  (list "%%%%%" "CNVXZ" dont_expand
	(TRAP 11 (deref "uint32 *" code 0) 0)))

