#ifndef _trap_h_
#define _trap_h_

#include "syn68k_private.h"

extern syn68k_addr_t trap_direct (uint32 trap_number,
				  syn68k_addr_t exception_pc,
				  syn68k_addr_t exception_address);
extern uint32 trap_forwarded (syn68k_addr_t m68k_address, void *arg);
extern void trap_init (void);
extern void trap_install_handler (unsigned trap_number,
				  callback_handler_t func,
				  void *arbitrary_argument);
extern void trap_remove_handler (unsigned trap_number);

extern uint32 *trap_vector_array;

#define SR_SUPERVISOR_BIT 0x2000
#define SR_MASTER_BIT     0x1000

#define USE_USP(sr) (!((sr) & SR_SUPERVISOR_BIT))
#define USE_MSP(sr) (((sr) & (SR_SUPERVISOR_BIT | SR_MASTER_BIT)) \
		     == (SR_SUPERVISOR_BIT | SR_MASTER_BIT))
#define USE_ISP(sr) (((sr) & (SR_SUPERVISOR_BIT | SR_MASTER_BIT)) \
		     == SR_SUPERVISOR_BIT)

#define FETCH_USP() (USE_USP (cpu_state.sr) ? EM_A7 : cpu_state.usp)
#define FETCH_MSP() (USE_MSP (cpu_state.sr) ? EM_A7 : cpu_state.msp)
#define FETCH_ISP() (USE_ISP (cpu_state.sr) ? EM_A7 : cpu_state.isp)

#define SET_USP(n) \
(USE_USP (cpu_state.sr) ? (EM_A7 = (n)) : (cpu_state.usp = (n)))
#define SET_MSP(n) \
(USE_MSP (cpu_state.sr) ? (EM_A7 = (n)) : (cpu_state.msp = (n)))
#define SET_ISP(n) \
(USE_ISP (cpu_state.sr) ? (EM_A7 = (n)) : (cpu_state.isp = (n)))

#define SWITCH_A7(old_sr, new_sr)					\
do									\
{									\
  if (((old_sr) ^ (new_sr)) & (SR_SUPERVISOR_BIT | SR_MASTER_BIT))	\
    {									\
      /* Save away the old a7. */					\
      if (USE_USP (old_sr))						\
	cpu_state.usp = EM_A7;						\
      else if (USE_MSP (old_sr))					\
	cpu_state.msp = EM_A7;						\
      else								\
	cpu_state.isp = EM_A7;						\
									\
      /* Load up the new value. */					\
      if (USE_USP (new_sr))						\
	EM_A7 = cpu_state.usp;						\
      else if (USE_MSP (new_sr))					\
	EM_A7 = cpu_state.msp;						\
      else								\
	EM_A7 = cpu_state.isp;						\
    }									\
} while (0)

#endif  /* Not _trap_h_ */
