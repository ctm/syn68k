#include "interrupt.h"

#ifdef SYNCHRONOUS_INTERRUPTS

#include "trap.h"

#ifdef USE_BIOS_TIMER
uint16 dos_memory_selector;
uint32 dos_interrupt_flag_addr;
#endif


void
interrupt_generate (unsigned priority)
{
  /* Calling this with a weird priority will just note that an interrupt
   * should be checked for.
   */
  if (priority >= 1 && priority <= 7)
    cpu_state.interrupt_pending[priority] = TRUE;

  SET_INTERRUPT_STATUS (INTERRUPT_STATUS_CHANGED);
}


/* This function turns on the global "check for an interrupt" bit if
 * any interrupts are currently pending.
 */
void
interrupt_note_if_present ()
{
  int i;

  for (i = 1; i <= 7; i++)
    if (cpu_state.interrupt_pending[i])
      {
	SET_INTERRUPT_STATUS (INTERRUPT_STATUS_CHANGED);
	break;
      }
}


/* This function looks to see if any interrupts of high enough
 * priority are pending, and if they are it processes the highest
 * priority interrupt.  It adjusts the machine state appropriately for
 * the interrupt and returns the 68k PC at which execution should
 * resume.  The INTERRUPT_PC parameter specifies the address
 * of the 68k instruction about to be executed when the interrupt
 * was detected.
 */
syn68k_addr_t
interrupt_process_any_pending (syn68k_addr_t interrupt_pc)
{
  int priority;
  syn68k_addr_t continuation_pc;
  int cpu_priority;

  /* First note that the interrupt has been processed.  The RTE
   * will end up causing another check when it reloads the SR, so
   * we shouldn't miss any interrupts.
   */
  SET_INTERRUPT_STATUS (INTERRUPT_STATUS_UNCHANGED);

  /* Determine if any interrupt with high enough priority is pending. */
  cpu_priority = (cpu_state.sr >> 8) & 7;
  if (cpu_state.interrupt_pending[7])
    priority = 7;  /* Priority 7 interrupt cannot be masked. */
  else
    {
      for (priority = 6; priority > cpu_priority; priority--)
	if (cpu_state.interrupt_pending[priority])
	  break;
      if (priority <= cpu_priority)
	priority = -1;
    }

#ifdef USE_BIOS_TIMER
  /* The BIOS has no way of specifying the interrupt priority when
   * an interrupt comes in.  Therefore if we don't see any other
   * explicitly specified interrupts, we'll assume it must have been
   * a timer interrupt and act accordingly.
   */
  if (priority == -1 && cpu_priority < M68K_TIMER_PRIORITY)
    priority = M68K_TIMER_PRIORITY;
#endif
  
  /* Did we find an interrupt of high enough priority? */
  if (priority != -1)
    {
      /* Process the interrupt. */
      cpu_state.interrupt_pending[priority] = 0;
      continuation_pc =  trap_direct (24 + priority, interrupt_pc, 0);
    }
  else
    {
      /* Nothing interesting after all, so just return unchanged. */
      continuation_pc = interrupt_pc;
    }

  return continuation_pc;
}

#endif  /* SYNCHRONOUS_INTERRUPTS */
