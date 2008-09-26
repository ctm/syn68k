#include "testqsort.h"
#include "callemulator.h"
#include "syn68k_public.h"
#include <stdlib.h>
#include <stdio.h>

#define ARRAY_SIZE 103

#if defined(mc68000)
static int emulator_array[ARRAY_SIZE], real_array[ARRAY_SIZE];

/* qsort helper function. */
static int
compare_ints (const void *n1, const void *n2)
{
  return *(const int *)n1 - *(const int *)n2;
}
#endif

#ifdef USE_STUB
static void
stub ()
{
  qsort (emulator_array, sizeof emulator_array / sizeof emulator_array[0],
	 sizeof emulator_array[0], compare_ints);
}
#endif


void
test_qsort ()
{
#ifndef mc68000
  fputs ("You can't test qsort on a non-68000.\n", stderr);
#else
  int i;

  
  /* Randomize the array. */
  for (i = 0; i < sizeof real_array / sizeof real_array[0]; i++)
    emulator_array[i] = real_array[i] = rand ();

  /* Sort it on the real CPU. */
  qsort (real_array, sizeof real_array / sizeof real_array[0],
	 sizeof real_array[0], compare_ints);

#ifndef USE_STUB
  /* Sort it under the emulator. */
  call_emulator ((int (*)())qsort, emulator_array,
		 sizeof emulator_array / sizeof emulator_array[0],
		 sizeof emulator_array[0], compare_ints);
#else
  {
    char stack[8192];
    EM_A7 = EM_A6 = &stack[6000];
    CALL_EMULATOR ((syn68k_addr_t) stub);
  }
#endif


  /* Compare the results. */
  if (memcmp (real_array, emulator_array, sizeof real_array))
    fprintf (stderr, "test_qsort failed; emulator gets different "
	     "results than the real 680x0.\n");
  else
    fprintf (stderr, "test_qsort succeeded.\n");
#endif  /* mc68000 */
}
