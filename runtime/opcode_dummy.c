#include <stdio.h>

/*
 * s68k_handle_opcode_dummy is used to detect portions of assembly in
 * syn68k.s that are unreachable and should be removed via
 * i486-cleanup.pl.  The call to s68k_handle_opcode_dummy should never
 * be made.  It won't be reachable even if i486-cleanup.pl isn't run,
 * but it will be assembled and linked in, so that's why we actually
 * supply s68k_handle_opcode_dummy but expect it to never be called.
 */

void s68k_handle_opcode_dummy (void)
{
  fprintf (stderr, "This function should never be called\n");
  abort ();
}
