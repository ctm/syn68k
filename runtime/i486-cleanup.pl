#!/usr/bin/perl -ni

# This perl script strips out the extra cruft created as a side effect of
# splitting syn68k.c into many functions.  This will make the resulting
# syn68k.o file smaller and also force the real branch targets to be
# aligned optimally.

# Delete the header for each function
if (/^\.globl _?s68k_handle_opcode_0x/ .. /^[#\/]APP$|^_?S68K_HANDLE_0x....:$/)
{
    print if (/^[#\/]APP/ || /^L/ || /^_?S68K_HANDLE_0x....:$/);
}

# Delete the trailer for each function
elsif (/_S68K_DONE_WITH/ .. /^\s*ret$|^\s*jmp\s+_s68k_handle_opcode_dummy$/)
{
    print if (!/\s*movl %ebp,%esp$/
	      && !/\s*leal\s*(-?\d+)?\(%ebp\),%esp$/
	      && !/\s*movl\s*(-?\d+)?\(%ebp\),%edi$/
              && !/\s*popl/
              && !/\s*ret$/
              && !/^_S68K_DONE_WITH/
              && !/s68k_handle_opcode/
              && !/^\s*jmp\s+_s68k_handle_opcode_dummy$/);
}
elsif (/^\s*lods/)
{
  die "lods not allowed; we're punting cld's these days.";
}
# elsif (/^\s*call/ && !/s68k_handle_opcode/)
# {  # no longer necessary since we punted lodsl
#     print "$_\tcld\n";
# }
elsif (!/s68k_handle_opcode/)
{
    print;
}
