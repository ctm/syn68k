#ifdef PROFILE

#include "profile.h"
#include "mapping.h"
#include "blockinfo.h"
#include <stdio.h>

static uint32 instruction_frequency[65536];

/* Global variable to tweak with gdb to turn profiling on and off. */
int should_profile = 1;

void
profile_block (Block *b)
{
  syn68k_addr_t addr = b->m68k_start_address;

  if ((addr < MAGIC_ADDRESS_BASE || addr > 0x00FFFFFF)
      && should_profile)
    {
      while (addr < b->m68k_start_address + b->m68k_code_length)
	{
	  uint16 m68kop = READUW (addr);
	  ++instruction_frequency[m68kop];
	  addr += 2 * (instruction_size
		       (SYN68K_TO_US (addr),
			&opcode_map_info[opcode_map_index[m68kop]]));
	}
    }
}


void
dump_profile (const char *file)
{
  FILE *fp;
#ifdef LITTLEENDIAN
  int i;
  uint32 *p;
#endif

  /* Choose default filename. */
  if (file == NULL)
    file = "/tmp/instrfreq";

  fp = fopen (file, "wb");
  if (fp == NULL)
    {
      fprintf (stderr, "Unable to write to %s", file);
      perror ("");
      return;
    }

  /* Assure that we always write out the profiling info in big endian order. */
#ifdef LITTLEENDIAN
  for (i = 65535, p = instruction_frequency; i >= 0; p++, i--)
    *p = SWAPUL (*p);
#endif

  fwrite (instruction_frequency, sizeof instruction_frequency[0],
	  sizeof instruction_frequency / sizeof instruction_frequency[0],
	  fp);

  /* Undo byte swaps and put it back into native order. */
#ifdef LITTLEENDIAN
  for (i = 65535, p = instruction_frequency; i >= 0; p++, i--)
    *p = SWAPUL (*p);
#endif

  fclose (fp);
}

#endif  /* PROFILE */
