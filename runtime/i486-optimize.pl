#!/usr/bin/perl -ni

chop;
next if (/^[\#\/](NO_)?APP/ || /^\s*$/);
$cur = $_;
$first_changed = 0;


# First we handle mapping cases like:
#  _S68K_HANDLE_0x00B5:
#  	movw _cpu_state+4,%ax
#	movw (%esi),%bx
#
# to:
#
#  _S68K_HANDLE_0x00B5:
#  	movl _cpu_state+4,%eax
#	movl (%esi),%ebx
#
# Because we know cpu_state and %esi-relative pointers will be long-aligned,
# and long ops are faster and more pairable than word ops.

if ($prev_first_changed
    && ((($mem, $reg) = /^\s*movw (_cpu_state\+?\d*),%(ax|bx|cx|dx|di)$/)
	|| (($mem, $reg) = /^\s*movw (\d*\(%esi\)),%(ax|bx|cx|dx|di)$/))
    && ($prev !~ /$reg/)) {
    $cur = "\tmovl " . $mem . ",%e" . $reg;
}
if ($prev =~ /^_S68K_HANDLE_/) {
    if ((($mem, $reg) = /^\s*mov[wl] (_cpu_state\+?\d*),%e?(ax|bx|cx|dx|di)$/) 
	|| (($mem, $reg) = /^\s*mov[wl] (\d*\(%esi\)),%e?(ax|bx|cx|dx|di)$/)) {
	$cur = "\tmovl " . $mem . ",%e" . $reg;
	$first_changed = 1;
    }
}

# Next we'll handle brain damage like:
#	movl %eax,_cpu_state+80
#	movl _cpu_state+80,%eax
#
# by deleting the second line.  Yes, this really happens.
# We have to be conservative here so we don't botch cases like:
#	movb _cpu_state(%eax),%al
#	movb %al,_cpu_state(%eax)


if (((($size1, $src1, $dst1)
      = ($prev =~ /^\s+mov([bwl])\s+(_cpu_state\+?\d*),\s*(%[a-z]+)$/))
     || (($size1, $src1, $dst1)
	 = ($prev =~ /^\s+mov([bwl])\s+(%[a-z]+),\s*(_cpu_state\+?\d*)$/))
     || (($size1, $src1, $dst1)
	 = ($prev =~ /^\s+mov([bwl])\s+(-?\d*\(%ebp\)),\s*(%[a-z]+)$/))
     || (($size1, $src1, $dst1)
	 = ($prev =~ /^\s+mov([bwl])\s+(%[a-z]+),\s*(-?\d*\(%ebp\))$/)))
    && (($size2, $src2, $dst2)
	= /^\s+mov([bwl])\s+([^,]+),\s*([^,]+)$/)
    && ($size1 eq $size2)
    && ($src1 eq $dst2)
    && ($src2 eq $dst1)) {
# This second line is completely useless, so delete it.
   $cur = "";
}


if (!($cur eq "")) {
    print $cur . "\n";
}
$prev = $cur;
$prev_first_changed = $first_changed;
