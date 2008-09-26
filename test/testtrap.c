#include "testtrap.h"
#include "syn68k_public.h"
#include "callemulator.h"
#include <stdio.h>

#ifdef mc68000
static void test_aline_traps (void);
#endif
static void test_callbacks (void);

void
test_traps ()
{
#ifdef mc68000
  test_aline_traps ();
#endif
  test_callbacks ();
}


static uint32
handle_callback (uint32 addr, void *arg)
{
  printf ("Callback: addr == 0x%08lX, arg == %p\n", addr, arg);
  return MAGIC_EXIT_EMULATOR_ADDRESS;
}


static void
test_callbacks ()
{
  uint32 callback_addr[10];
  int i, j;

  for (i = 0; i < 10; i++)
    callback_addr[i] = callback_install (handle_callback, (void *) i);

  for (j = 0; j < 2; j++)
    for (i = 0; i < 10; i++)
      interpret_code (hash_lookup_code_and_create_if_needed
		      (callback_addr[i]));
}

#ifdef mc68000
static uint32
catch_aline_trap (uint32 m68k_addr, void *unused)
{
  printf ("Hit a-line trap 0x%04X\n",
	  (unsigned)*(uint16 *)(SYN68K_TO_US (m68k_addr)));
  return m68k_addr + 2;
}


static int
aline_trap_death ()
{
  int i, result = 91;
  asm (".word 0xA000");
  for (i = 0; i < 5; i++)
    {
      asm (".word 0xA003");
      result += i * 3;
    }
  asm (".word 0xA001");
  return result;
}


static void
test_aline_traps ()
{
  trap_install_handler (10, catch_aline_trap, NULL);
  call_emulator (aline_trap_death);
}
#endif
