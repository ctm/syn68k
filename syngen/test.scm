(include "68k.defines.scm")

(define (ADDB name bit_pattern expand amode src dst)
  (defopcode name 
    (list 68000 amode () bit_pattern)
    (list "-----" "-----" expand
	  (assign dst (+ dst src)))
    (list "CNVXZ" "-----" dont_expand
	  (list
	   (assign tmp2.ub dst)
	   (assign ccv (& (~ (^ tmp2.ub src)) 0x80))
	   (ASSIGN_NNZ_BYTE (assign dst (assign tmp.uw (+ dst src))))
	   (assign ccx (assign ccc (>> tmp.uw 8)))
	   (assign ccv (& ccv (^ tmp.ub tmp2.ub)))))))

(define add_sub_expand (list "----xxx---00xxxx"   ; [ad]n, [ad]n
			     "----xxx---010xxx"   ; an@, [ad]n
			     "----xxx---101xxx")) ; an@(d16), [ad]n

(ADDB addb_dest_reg (list "1101ddd000mmmmmm") add_sub_expand
      (intersect amode_all_combinations (not "xxxxxxxxxx001xxx"))  ; no an
      $2.mub $1.dub)
