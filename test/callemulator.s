#NO_APP
.text
	.align 1
.globl _call_emulator
_call_emulator:
	link a6,#0
	pea 100:w
	pea a6@(8)
	pea a6@(-2048)
	jbsr _memcpy
	lea a6@(-2048),a0
	movel #0x00FFF000,a0@
	movel a0,_cpu_state+60
	movel a6,_cpu_state+56
	movel a6@(8),sp@-
	jbsr _hash_lookup_code_and_create_if_needed
	movel d0,sp@-
	jbsr _interpret_code
	movel _cpu_state,d0
	unlk a6
	rts
