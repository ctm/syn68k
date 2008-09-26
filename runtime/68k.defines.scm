; All		   = No restrictions
; Alt		   = No #, No PC
; Data		   = No An
; DataNoIMM	   = No An, No #
; DataAlt	   = No An, No #, No PC
; MemAlt	   = No Dn, No An, No #, No PC
; Ctrl		   = No Dn, No An, No (An)+, No -(An), No #
; Move		   = Source: All, Dest: DataAlt
; Moveb	   = Source: Data, Dest: DataAlt
; CtrlAltOrPreDec = No Dn, No An, No (An)+, No #, No PC
; CtrlOrPostInc   = No Dn, No An, No -(An), No #
; DRegOrCtrl	   = No An, No (An)+, No -(An), No #
; DRegOrCtrlAlt   = No An, No (An)+, No -(An), No #, No PC


; Each of these macros must evaluate to a number between 0 and 0xFF if
; the appropriate sign bit is set, else 0.
(define (SIGN_BYTE expr) (& expr 0x80))
(define (SIGN_WORD expr) (>> (<< (cast "uint32" expr) 16) 31))
(define (SIGN_LONG expr) (>> expr 31))

(define (SET_C_N_V_NZ c n v nz)
  (list
   "\n#ifdef CCR_ELEMENT_8_BITS\n"
   (assign (dereful (call "US_TO_SYN68K" "&cpu_state.ccnz")) (| (<< nz 24)
								(| (<< n 16)
								   (| (<< c 8)
								      v))))
   "\n#else\n"
   (assign ccnz nz)
   (assign ccn n)
   (assign ccc c)
   (assign ccv v)
   "\n#endif\n"))

(define (SET_N_NZ n nz)
  (list
   "\n#ifdef CCR_ELEMENT_8_BITS\n"
   (assign (derefuw (call "US_TO_SYN68K" "&cpu_state.ccnz")) (| (<< nz 8) n))
   "\n#else\n"
   (assign ccnz nz)
   (assign ccn n)
   "\n#endif\n"))


; These macros set the n and z bits based on whether the value of an
; expression of a certain size is negative or zero, respectively.
(define (ASSIGN_NNZ_BYTE expr)
  (assign ccn (SIGN_BYTE (assign ccnz expr))))
(define (ASSIGN_NNZ_WORD expr)
  (list
   "\n#ifdef CCR_ELEMENT_8_BITS\n"
   "{ uint16 assign_tmp"
   (assign "assign_tmp" expr)
   (assign ccnz (<> "assign_tmp" 0))
   (assign ccn (>> "assign_tmp" 15))
   "}"
   "\n#else\n"
   (assign ccn (& 0x8000 (assign ccnz expr)))
   "\n#endif\n"))
(define (ASSIGN_NNZ_LONG expr)
  (list
   "\n#ifdef CCR_ELEMENT_8_BITS\n"
   "{ uint32 assign_tmp"
   (assign "assign_tmp" expr)
   (assign ccnz (<> "assign_tmp" 0))
   (assign ccn (>> "assign_tmp" 31))
   "}"
   "\n#else\n"
   (assign ccn (>> (assign ccnz expr) 31))
   "\n#endif\n"))


(define (ASSIGN_C_N_V_NZ_BYTE expr)
  (list
   "\n#ifdef FAST_CC_FUNCS\n"
   (call "inline_compute_c_n_v_nz_byte" expr)
   "\n#else\n"
   (ASSIGN_NNZ_BYTE expr)
   (assign ccc (assign ccv 0))
   "\n#endif\n"))
(define (ASSIGN_C_N_V_NZ_WORD expr)
  (list
   "\n#ifdef FAST_CC_FUNCS\n"
   (call "inline_compute_c_n_v_nz_word" expr)
   "\n#else\n"
   (ASSIGN_NNZ_WORD expr)
   (assign ccc (assign ccv 0))
   "\n#endif\n"))
(define (ASSIGN_C_N_V_NZ_LONG expr)
  (list
   "\n#ifdef FAST_CC_FUNCS\n"
   (call "inline_compute_c_n_v_nz_long" expr)
   "\n#else\n"
   (ASSIGN_NNZ_LONG expr)
   (assign ccc (assign ccv 0))
   "\n#endif\n"))
   

; Macros for common addressing modes.
(define amode_control
  (union "xxxxxxxxxxx10xxx" "xxxxxxxxxx101xxx" "xxxxxxxxxx1110xx"))
(define amode_memory
  (union amode_control "xxxxxxxxxx011xxx" "xxxxxxxxxx100xxx"
	 "xxxxxxxxxx111100"))
(define amode_reg "xxxxxxxxxx00xxxx")
(define amode_dreg "xxxxxxxxxx000xxx")
(define amode_data (union amode_memory amode_dreg))
(define amode_alterable
  (union "xxxxxxxxxx0xxxxx" "xxxxxxxxxxx0xxxx"
	 "xxxxxxxxxxxx0xxx" "xxxxxxxxxx11100x"))
(define amode_alterable_memory
  (intersect amode_alterable amode_memory))
(define amode_alterable_control
  (intersect amode_alterable amode_control))
(define amode_implicit "xxxxxxxxxxxxxxxx")
(define amode_immediate "xxxxxxxxxx111100")
(define amode_alterable_data
  (intersect amode_alterable amode_data))
(define amode_all_combinations (union amode_data "xxxxxxxxxx001xxx"))

; Not strictly an addressing mode, but handy when you want the two reg
; fields to be the same register.
(define amode_same_reg
  (union "xxxx000xxxxxx000" "xxxx001xxxxxx001" "xxxx010xxxxxx010"
	 "xxxx011xxxxxx011" "xxxx100xxxxxx100" "xxxx101xxxxxx101"
	 "xxxx110xxxxxx110" "xxxx111xxxxxx111"))

;(define dont_expand  "----------------")
;(define fully_expand "xxxxxxxxxxxxxxxx")
(define dont_expand (list))
(define fully_expand (list "xxxxxxxxxxxxxxxx"))


(define BYTE 1)
(define WORD 2)
(define LONG 4)

(define IMM     0)
(define ABSW    1)
(define ABSL    2)
(define REG     3)
(define GREG    4)
(define AREG    5)
(define IND     6)
(define PREDEC  7)
(define POSTINC 8)
(define INDOFF  9)
(define INDIX   10)

(define (amode_name mode)
  (switch mode
	  ((+ IMM     0) "imm")
	  ((+ ABSW    0) "abs")
	  ((+ ABSL    0) "abs")
	  ((+ REG     0) "reg")
	  ((+ GREG    0) "reg")
	  ((+ AREG    0) "areg")
	  ((+ IND     0) "ind")
	  ((+ PREDEC  0) "predec")
	  ((+ POSTINC 0) "postinc")
	  ((+ INDOFF  0) "indoff")
	  ((+ INDIX   0) "indix")
	  (default "?? unknown amode ??")))


(define (no_reg_op_p mode)
  (or (= mode IMM) (or (= mode ABSW) (= mode ABSL))))


(define (src_val s_amode d_amode size force_unsigned_p)
  (if (= size BYTE)
      (switch s_amode
	      ((+ IMM   0)
	       (if (no_reg_op_p d_amode)
		   $1.ub
		   $2.ub))
	      ((+ ABSW   0)
	       (if (no_reg_op_p d_amode)
		   (derefub $1.sw)
		   (derefub $2.sw)))
	      ((+ ABSL   0)
	       (if (no_reg_op_p d_amode)
		   (derefub $1.ul)
		   (derefub $2.ul)))
	      ((+ REG    0)
	       (if (no_reg_op_p d_amode)
		   $1.dub
		   $2.dub))
	      ((+ GREG   0)
	       (if (no_reg_op_p d_amode)
		   $1.gub
		   $2.gub))
	      ((+ IND    0)
	       (if (no_reg_op_p d_amode)
		   (derefub $1.aul)
		   (derefub $2.aul)))
	      ((+ PREDEC 0)
	       (if (no_reg_op_p d_amode)
		   (derefub $1.aul)
		   (derefub $2.aul)))
	      ((+ POSTINC 0)
	       (if (no_reg_op_p d_amode)
		   (derefub $1.aul)
		   (derefub $2.aul)))
	      ((+ INDOFF 0)
	       (if (no_reg_op_p d_amode)
		   (derefub (+ $1.asl $2.sw))
		   (derefub (+ $2.asl $3.sw))))
	      ((+ INDIX  0)
	       (if (no_reg_op_p d_amode)
		   $1.msb
		   $2.msb)))
      (if (= size WORD)
	  (switch s_amode
		  ((+ IMM   0)
		   (if force_unsigned_p
		       (if (no_reg_op_p d_amode)
			   $1.uw
			   $2.uw)
		       (if (no_reg_op_p d_amode)
			   $1.sw
			   $2.sw)))
		  ((+ ABSW   0)
		   (if force_unsigned_p
		       (if (no_reg_op_p d_amode)
			   (derefuw $1.sw)
			   (derefuw $2.sw))
		       (if (no_reg_op_p d_amode)
			   (derefsw $1.sw)
			   (derefsw $2.sw))))
		  ((+ ABSL   0)
		   (if force_unsigned_p
		       (if (no_reg_op_p d_amode)
			   (derefuw $1.ul)
			   (derefuw $2.ul))
		       (if (no_reg_op_p d_amode)
			   (derefsw $1.ul)
			   (derefsw $2.ul))))
		  ((+ AREG    0)
		   (if force_unsigned_p
		       (if (no_reg_op_p d_amode)
			   $1.auw
			   $2.auw)
		       (if (no_reg_op_p d_amode)
			   $1.asw
			   $2.asw)))
		  ((+ REG    0)
		   (if force_unsigned_p
		       (if (no_reg_op_p d_amode)
			   $1.duw
			   $2.duw)		
		       (if (no_reg_op_p d_amode)
			   $1.dsw
			   $2.dsw)))
		  ((+ GREG   0)
		   (if force_unsigned_p
		       (if (no_reg_op_p d_amode)
			   $1.guw
			   $2.guw)
		       (if (no_reg_op_p d_amode)
			   $1.gsw
			   $2.gsw)))
		  ((+ IND    0)
		   (if force_unsigned_p
		       (if (no_reg_op_p d_amode)
			   (derefuw $1.aul)
			   (derefuw $2.aul))
		       (if (no_reg_op_p d_amode)
			   (derefsw $1.aul)
			   (derefsw $2.aul))))
		  ((+ PREDEC 0)
		   (if force_unsigned_p
		       (if (no_reg_op_p d_amode)
			   (derefuw $1.aul)
			   (derefuw $2.aul))
		       (if (no_reg_op_p d_amode)
			   (derefsw $1.aul)
			   (derefsw $2.aul))))
		  ((+ POSTINC 0)
		   (if force_unsigned_p
		       (if (no_reg_op_p d_amode)
			   (derefuw $1.aul)
			   (derefuw $2.aul))
		       (if (no_reg_op_p d_amode)
			   (derefsw $1.aul)
			   (derefsw $2.aul))))
		  ((+ INDOFF 0)
		   (if force_unsigned_p
		       (if (no_reg_op_p d_amode)
			   (derefuw (+ $1.asl $2.sw))
			   (derefuw (+ $2.asl $3.sw)))
		       (if (no_reg_op_p d_amode)
			   (derefsw (+ $1.asl $2.sw))
			   (derefsw (+ $2.asl $3.sw)))))
		  ((+ INDIX 0)
		   (if (no_reg_op_p d_amode)
		       $1.msw
		       $2.msw)))
	  (switch s_amode   ; LONG op
		  ((+ IMM   0)
		   (if (no_reg_op_p d_amode)
		       $1.ul
		       $2.ul))
		  ((+ ABSW   0)
		   (if (no_reg_op_p d_amode)
		       (dereful $1.sw)
		       (dereful $2.sw)))
		  ((+ ABSL   0)
		   (if (no_reg_op_p d_amode)
		       (dereful $1.ul)
		       (dereful $2.ul)))
		  ((+ AREG    0)
		   (if (no_reg_op_p d_amode)
		       $1.asl
		       $2.asl))
		  ((+ REG    0)
		   (if (no_reg_op_p d_amode)
		       $1.dul
		       $2.dul))
		  ((+ GREG   0)
		   (if (no_reg_op_p d_amode)
		       $1.gul
		       $2.gul))
		  ((+ IND    0)
		   (if (no_reg_op_p d_amode)
		       (dereful $1.aul)
		       (dereful $2.aul)))
		  ((+ PREDEC 0)
		   (if (no_reg_op_p d_amode)
		       (dereful $1.aul)
		       (dereful $2.aul)))
		  ((+ POSTINC 0)
		   (if (no_reg_op_p d_amode)
		       (dereful $1.aul)
		       (dereful $2.aul)))
		  ((+ INDOFF 0)
		   (if (no_reg_op_p d_amode)
		       (dereful (+ $1.asl $2.sw))
		       (dereful (+ $2.asl $3.sw))))
		  ((+ INDIX 0)
		   (if (no_reg_op_p d_amode)
		       $1.msl
		       $2.msl))))))


(define (dst_val s_amode d_amode size)
  (if (= size BYTE)
      (switch d_amode
	      ((+ ABSW   0)
	       (if (or (= s_amode INDOFF) (= s_amode INDIX))
		   (derefub $3.sw)
		   (derefub $2.sw)))
	      ((+ ABSL   0)
	       (if (or (= s_amode INDOFF) (= s_amode INDIX))
		   (derefub $3.ul)
		   (derefub $2.ul)))
	      ((+ REG     0) $1.dub)
	      ((+ GREG    0) $1.gub)
	      ((+ IND     0) (derefub $1.aul))
	      ((+ PREDEC  0) (derefub $1.aul))
	      ((+ POSTINC 0) (derefub $1.aul))
	      ((+ INDOFF  0)
	       (if (or (= s_amode INDOFF) (= s_amode INDIX))
		   (derefub (+ $1.asl $4.sw))
		   (derefub (+ $1.asl $3.sw))))
	      ((+ INDIX 0) $1.rub))
      (if (= size WORD)
	  (switch d_amode
	      ((+ ABSW   0)
	       (if (or (= s_amode INDOFF) (= s_amode INDIX))
		   (derefuw $3.sw)
		   (derefuw $2.sw)))
	      ((+ ABSL   0)
	       (if (or (= s_amode INDOFF) (= s_amode INDIX))
		   (derefuw $3.ul)
		   (derefuw $2.ul)))
	      ((+ REG     0) $1.duw)
	      ((+ GREG    0) $1.guw)
	      ((+ AREG    0) $1.asl)  ; Yes, a long!  Word moves are sexted.
	      ((+ IND     0) (derefuw $1.aul))
	      ((+ PREDEC  0) (derefuw $1.aul))
	      ((+ POSTINC 0) (derefuw $1.aul))
	      ((+ INDOFF  0)
	       (if (or (= s_amode INDOFF) (= s_amode INDIX))
		   (derefuw (+ $1.asl $4.sw))
		   (derefuw (+ $1.asl $3.sw))))
	      ((+ INDIX 0) $1.ruw))
	  (switch d_amode   ; LONG op
	      ((+ ABSW   0)
	       (if (or (= s_amode INDOFF) (= s_amode INDIX))
		   (dereful $3.sw)
		   (dereful $2.sw)))
	      ((+ ABSL   0)
	       (if (or (= s_amode INDOFF) (= s_amode INDIX))
		   (dereful $3.ul)
		   (dereful $2.ul)))
	      ((+ REG     0) $1.dul)
	      ((+ AREG    0) $1.aul)
	      ((+ GREG    0) $1.gul)
	      ((+ IND     0) (dereful $1.aul))
	      ((+ PREDEC  0) (dereful $1.aul))
	      ((+ POSTINC 0) (dereful $1.aul))
	      ((+ INDOFF  0)
	       (if (or (= s_amode INDOFF) (= s_amode INDIX))
		   (dereful (+ $1.asl $4.sw))
		   (dereful (+ $1.asl $3.sw))))
	      ((+ INDIX 0) $1.rul)))))

; For add/sub/cmp/and/or/eor...reuse the tricky code above.
(define (ea_val amode size force_unsigned_p)
	(src_val amode REG size force_unsigned_p))
