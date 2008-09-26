#NO_APP
.lcomm	_saved_regs,64
.lcomm	_jump_address,4
.text
.globl _run_code_on_68000
_run_code_on_68000:
	link a6,#0
	moveml #0xFFFF,_saved_regs
	movel a6@(8),_jump_address
	movel a6@(12),a0
	movel #L0,a0@
	moveml _cpu_state,#0xFFFF
	jmp @(_jump_address)@(0)
L0:
	moveml #0xFFFF,_cpu_state
	moveml _saved_regs,#0xFFFF
	unlk a6
	rts
