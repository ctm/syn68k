#include "trap.h"
#include "callback.h"
#include <stdio.h>
#include <stdlib.h>

/* Pointer to the array of trap vectors. */
uint32 *trap_vector_array;

void
trap_init ()
{
  volatile TrapHandlerInfo *thi;
  int i;

  cpu_state.vbr = US_TO_SYN68K (trap_vector_array);
  for (i = 63, thi = &cpu_state.trap_handler_info[63]; i >= 0; thi--, i--)
    {
      thi->callback_address = callback_install (trap_forwarded, (void *) thi);
      WRITE_LONG (&trap_vector_array[i], thi->callback_address);
      thi->func = NULL;
      thi->arg = NULL;
    }
}


void
trap_install_handler (unsigned trap_number, callback_handler_t func,
		      void *arbitrary_argument)
{
  cpu_state.trap_handler_info[trap_number].func = func;
  cpu_state.trap_handler_info[trap_number].arg  = arbitrary_argument;
}


void
trap_remove_handler (unsigned trap_number)
{
  cpu_state.trap_handler_info[trap_number].func = NULL;
}


uint32
trap_forwarded (uint32 m68k_address, void *arg)
{
  const TrapHandlerInfo *thi = (TrapHandlerInfo *) arg;

  if (thi->func == NULL)   /* If no handler installed, just rte. */
    {
      static const char *trap_description[] = {
	/* I've only put the really interesting ones in. */
	NULL, NULL, NULL, NULL,
	"Illegal instruction",
	"Integer divide by zero",
	"CHK/CHK2",
	"FTRAPcc, TRAPcc, TRAPV",
	"Privilege violation",
	"Trace",
	"A-line",
	"F-line",
	NULL,
	NULL,
	"Format error"
	};
      int which_trap = thi - cpu_state.trap_handler_info;

      fprintf (stderr, "Unhandled trap %d!", which_trap);
      if (which_trap >= 0
	  && which_trap < sizeof trap_description / sizeof trap_description[0]
	  && trap_description[which_trap] != NULL)
	fprintf (stderr, " (%s)", trap_description[which_trap]);
      putc ('\n', stderr);

      return MAGIC_RTE_ADDRESS;
    }
  return thi->func (READUL (EM_A7 + 2), thi->arg);
}


uint32
trap_direct (uint32 trap_number, uint32 exception_pc,
	     uint32 exception_address)
{
  const volatile TrapHandlerInfo *thi;
  uint16 *p, old_sr;
  uint32 trap_addr;
  uint32 retval;

  /* Fetch trap address from vector table. */
  trap_addr = READUL (cpu_state.vbr + (trap_number << 2));

  switch (trap_number) {
  case 4:   /* Illegal instruction. */
  case 8:   /* Privilege violation. */
  case 10:  /* A-line. */
  case 11:  /* F-line. */
  case 14:  /* Format error. */
  case 15:  /* Uninitialized interrupt. */
  case 24:  /* Spurious interrupt. */
  case 25:  /* Level 1 interrupt autovector. */
  case 26:  /* Level 2 interrupt autovector. */
  case 27:  /* Level 3 interrupt autovector. */
  case 28:  /* Level 4 interrupt autovector. */
  case 29:  /* Level 5 interrupt autovector. */
  case 30:  /* Level 6 interrupt autovector. */
  case 31:  /* Level 7 interrupt autovector. */
  case 32:  /* TRAP #0 vector. */
  case 33:  /* TRAP #1 vector. */
  case 34:  /* TRAP #2 vector. */
  case 35:  /* TRAP #3 vector. */
  case 36:  /* TRAP #4 vector. */
  case 37:  /* TRAP #5 vector. */
  case 38:  /* TRAP #6 vector. */
  case 39:  /* TRAP #7 vector. */
  case 40:  /* TRAP #8 vector. */
  case 41:  /* TRAP #9 vector. */
  case 42:  /* TRAP #10 vector. */
  case 43:  /* TRAP #11 vector. */
  case 44:  /* TRAP #12 vector. */
  case 45:  /* TRAP #13 vector. */
  case 46:  /* TRAP #14 vector. */
  case 47:  /* TRAP #15 vector. */
    /* Save away the old SR. */
    old_sr = COMPUTE_SR_FROM_CPU_STATE ();

    /* Clear TR0 and TR1. */
    cpu_state.sr &= 0x3FFF;

    /* If this is an actual interrupt, adjust the SR appropriately. */
    if (trap_number >= 15 && trap_number <= 31)
      {
	/* Bump up interrupt mask priority. */
	if (trap_number >= 24 && trap_number <= 31)
	  cpu_state.sr = ((cpu_state.sr & ~(7 << 8))
			  | ((trap_number - 24) << 8));

	/* Switch into interrupt mode. */
	cpu_state.sr &= ~SR_MASTER_BIT;
      }

    cpu_state.sr |= SR_SUPERVISOR_BIT;

    /* Switch stacks appropriately. */
    SWITCH_A7 (old_sr, cpu_state.sr);

    /* Push the exception frame. */
    EM_A7 -= 8;
    p = SYN68K_TO_US (CLEAN (EM_A7));
    WRITE_WORD (p, old_sr);
    WRITE_LONG (p + 1, exception_pc);
    WRITE_WORD (p + 3, 0x0000 | (trap_number << 2));
    break;

  case 3:   /* Address error. */
  case 6:   /* CHK/CHK2 instruction. */
  case 7:   /* FTRAPcc, TRAPcc, TRAPV instructions. */
  case 9:   /* Trace. */
  case 5:   /* Integer divide by zero. */
    /* Save away the old SR. */
    old_sr = COMPUTE_SR_FROM_CPU_STATE ();

    /* Clear TR0 and TR1. */
    cpu_state.sr &= 0x3FFF;

    /* Switch into supervisor mode. */
    cpu_state.sr |= SR_SUPERVISOR_BIT;

    /* Switch stacks. */
    SWITCH_A7 (old_sr, cpu_state.sr);

    EM_A7 -= 12;
    p = SYN68K_TO_US (CLEAN (EM_A7));
    WRITE_WORD (p, old_sr);
    WRITE_LONG (p + 1, exception_pc);
    WRITE_WORD (p + 3, 0x2000 | (trap_number << 2));
    WRITE_LONG (p + 4, exception_address);
    break;
    
    /* I don't know how to handle these or they can't happen. */
  case 0:   /* Reset initial interrupt stack pointer. */
  case 1:   /* Reset initial program counter. */
  case 2:   /* Access fault */
  case 13:  /* ? */
  case 48:  /* FP Branch or Set on Unordered Condition. */
  case 49:  /* FP Inexact result. */
  case 50:  /* FP Divide by zero. */
  case 51:  /* FP Underflow. */
  case 52:  /* FP Operand error. */
  case 53:  /* FP Overflow. */
  case 54:  /* FP Signaling NAN. */
  case 55:  /* FP Unimplemented Data Type. */
  default:
    abort ();
  }

  /* See if they haven't set up their own vector.  If they haven't,
   * and we are interested in the trap, dispatch it directly to the handling
   * function.
   */
  thi = &cpu_state.trap_handler_info[trap_number];

  if (trap_addr == thi->callback_address && thi->func != NULL)
    retval = thi->func (exception_pc, thi->arg);
  else  /* Otherwise, trap back to m68k space. */
    retval = trap_addr;

#ifdef USE_BIOS_TIMER
  /* Just to be safe, guarantee that %fs points to conventional memory. */
  asm volatile ("movw %0,%%fs"
		: : "g" (dos_memory_selector));
#endif  /* USE_BIOS_TIMER */

  return retval;
}
