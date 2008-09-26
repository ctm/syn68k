#!/usr/bin/perl

sub mem_range
{
    local ($base, $offset, $size) = @_;
    local ($result, $real_size);

    $real_size = 1 if ($size eq "b");
    $real_size = 2 if ($size eq "w");
    $real_size = 4 if ($size eq "l");

    foreach (0 .. $real_size-1)
    {
	$result .= $base . ($offset + $_) . ' ';
    }

    chop $result;
    return $result;
}

sub break_it_down
{
    local($operand, $size) = @_;
    local ($result);

    # No operand
    if (!$operand)
    {
	return "";
    }

    # _cpu_state+offset
    if ($operand =~ /^_cpu_state(\+\d+)?$/)
    {
	return &mem_range ("mem_cs_", $1, $size);
    }

    # _cpu_state...
    if ($operand =~ /^_cpu_state/)
    {
	return "mem_cs_.*";
    }

    # %reg
    if ($operand =~ /^%(.*)$/)
    {
	die "No info for $1, $size"
	    if (!defined ($register_info{$1, $size}));
	return $register_info{$1, $size};
    }

    # Memory ref
    if ($operand =~ /^[_A-Za-z]/)
    {
	return "mem_.*";
    }

    # offset(%reg)
    if ($operand =~ /^([^%()]*)\(%(e[a-z][a-z])\)$/)
    {
	return &mem_range ("mem_code_", $1, $size) if ($2 eq 'esi');
	return &mem_range ("mem_bp_", $1, $size) if ($2 eq 'ebp');

	return "mem_.*";
    }

    # ($reg, $reg, n)
    if ($operand =~ /$\(%e[a-z][a-z],%e[a-z][a-z],[24])\)/)
    {
	return ("mem_.* ");
    }

    # ($reg, $reg)
    if ($operand =~ /$\(%e[a-z][a-z],%e[a-z][a-z])\)/)
    {
	return ("mem_.*");
    }

    # $constant
    if ($operand =~ /^\$.*$/)
    {
	return "";
    }

    # offset(,$reg,const)
    if ($operand =~ /^(-?\d+)?\(,%e[a-z][a-z],\d+\)$/)
    {
	return "mem_.*";
    }

    die "Unrecognized operand $operand\n";
}

# Recognize an instruction.  Returns (inputs, outputs, latency).
sub recognize
{
    local ($insn) = @_;
    local ($operand_regexp) = '((.*\(.*\).*)|([^\(\)]+))';
    local ($mnem, @operands);
    local ($inputs, $outputs, $latency, $reg);
    local ($no_union_inputs);

    $_ = $insn;
    $no_union_inputs = 0;

# First, grab the mnemonic and the operand(s)
  grab:
    {
	if (/^\t([a-z]+) $operand_regexp,\s*$operand_regexp$/)
	{
	    $mnem = $1;
	    @operands = ($2, $5);

#	    print "$insn\n";
#	    print " Operand 1: $operands[0]  Operand 2: $operands[1]\n";
	    last grab;
	}

	elsif (/^\t([a-z]+) $operand_regexp$/)
	{
	    $mnem = $1;
	    @operands = ($2);
#	    print "$insn\n";
#	    print " Operand: $2\n";
	    last grab;
	}

	elsif (/^\t([a-z]+)$/)
	{
	    $mnem = $1;
#	    print " No operands\n";
	    last grab;
	}

	elsif (/^([^\t]|\t\.)/)
	{
	    last grab;
	}

	die "Unable to figure out $insn\n";
    }

    # Barrier if we couldn't find the mnemonic.
    return (".*", ".*", 1) if (!$mnem);

    $inputs .= "eip ";   # everyone takes program counter as input

    # Find registers in parens and add to inputs
    foreach $reg (keys %regs)
    {
	if (/\([^\)]*%$reg[^\)]*\)/)
	{
	    $inputs .= &break_it_down ("%$reg", "l") . " ";
	}
    }


    $_ = $mnem;

the_switch:
    {
	local ($op, $len, $srclen, $dstlen);

	# Easy binops
	if (($op, $len) = /^(adc|add|cmp|sbb|sub|test|and|or|xor)([bwl])$/) {
	    die if (@operands != 2);
	    $inputs .= "reg_ccc " if ($op = /^adc/);
	    $outputs .= "reg_cco reg_ccs reg_ccz reg_cca reg_ccp reg_ccc ";
	    $inputs .= &break_it_down ($operands[0], $len) . " ";
	    $inputs .= &break_it_down ($operands[1], $len) . " ";
	    if ($op !~ /^cmp|test$/)
	    {
		$outputs .= &break_it_down ($operands[1], $len) . " ";
	    }
	    last the_switch;
	}
	
	# Easy uniops
	if (($op, $len) = /^(dec|inc|neg|not|bswap)([bwl])?$/) {
	    die if (@operands != 1);
	    $len = "l" if (!$len);
	    $outputs .= "reg_cco reg_ccs reg_ccz reg_cca reg_ccp reg_ccc "
		if ($op =~ /^neg$/);
	    $outputs .= "reg_cco reg_ccs reg_ccz reg_cca reg_ccp "
		if ($op =~ /^dec|inc/);
	    $inputs .= &break_it_down ($operands[0], $len) . " ";
	    $outputs .= &break_it_down ($operands[0], $len) . " ";
	    last the_switch;
	}

	# Set
	if (($op) = /^(set.*)$/) {
	    die if (@operands != 1);
	    $inputs .= "reg_cco reg_ccs reg_ccz reg_cca reg_ccp reg_ccc ";
	    $outputs .= &break_it_down ($operands[0], "b") . " ";
	    last the_switch;
	}

	# When we jmp to the next instruction, note that nearly everything
	# is dead.
	if ((/^jmp$/) && (@operands == 1) && ($operands[0] =~ /^\*%edi$/)) {
	    $inputs .= "reg_di_.* ";
	    $outputs .= "eip reg_cco reg_ccs reg_ccz reg_cca reg_ccp reg_ccc "
		. "reg_a_.* reg_b_.* reg_c_.* reg_d_.* reg_di_.* ";
	    $no_union_inputs = 1;
	    last the_switch;
	}

	# Barriers
	if (/^[j.].*|call|ret|leave$/) {
	    $inputs .= ".* ";
	    $outputs .= "eip ";   # program counter
	    last the_switch;
	}

	# Shifts
	if (($op, $len) = /^(rc|ro|sa|sh)[rl]([bwl])$/) {
	    die if (@operands != 2);
	    $inputs .= "reg_ccc "
		if ($op =~ /^rc/);  #FIXME - not sure if this is right
	    $outputs .= "reg_cco reg_ccs reg_ccz reg_cca reg_ccp reg_ccc ";
	    $inputs .= &break_it_down ($operands[0], "b") . " ";
	    $inputs .= &break_it_down ($operands[1], $len) . " ";
	    $outputs .= &break_it_down ($operands[1], $len) . " ";
	    last the_switch;
	}
	
	# Divide
	if (($op, $len) = /^(div|idiv|mul|imul)([bwl])$/) {
	    local ($inout);
	    die if (@operands != 1 && @operands != 2);
	    $inout .= "reg_cco reg_ccs reg_ccz reg_cca reg_ccp reg_ccc ";
	    if (@operands == 1)
	    {
		if ($len eq "b")
		{
		    $inout .= &break_it_down ("%ax", "w") . " ";
		}
		elsif ($len eq "w")
		{
		    $inout .= (&break_it_down ("%ax", "w") . " "
			       . &break_it_down ("%dx", "w") . " ");
		}
		elsif ($len eq "l")
		{
		    $inout .= (&break_it_down ("%eax", "l") . " "
			       . &break_it_down ("%edx", "l") . " ");
		}
	    }
	    else
	    {
		$inout .= ".* ";   # be paranoid, who cares.
	    }
	    $inputs .= $inout;
	    $outputs .= $inout;
	    last the_switch;
	}

	# LEA and MOV
	if (($op, $len) = /^(lea|mov)([bwl])$/) {
	    die if (@operands != 2);
	    if ($op eq "mov")
	    {
		$inputs .= &break_it_down ($operands[0], $len) . " ";
	    }
	    $outputs .= &break_it_down ($operands[1], $len) . " ";
	    last the_switch;
	}

	# Move and extend
	if (($op, $srclen, $dstlen) = /^(mov[sz])([bwl])([bwl])$/) {
	    die if (@operands != 2);
	    $inputs .= &break_it_down ($operands[0], $srclen) . " ";
	    $outputs .= &break_it_down ($operands[1], $dstlen) . " ";
	    last the_switch;
	}

	# Push and pop
	if (($op, $len) = /^(push|pop)([bwl])$/) {
	    die if (@operands != 1);
	    $inputs .= &break_it_down ("%esp", "l") . " ";
	    $outputs .= &break_it_down ("%esp", "l") . " ";
	    if ($op eq "push")
	    {
		$inputs .= &break_it_down ($operands[0], $len) . " ";
		$outputs .= &break_it_down ("(%esp)", $len) . " ";
	    }
	    else
	    {
		$inputs .= &break_it_down ("(%esp)", $len) . " ";
		$outputs .= &break_it_down ($operands[0], $len) . " ";
	    }
	    last the_switch;
	}

	# LODS
#	if (($op, $len) = /^(lods)([bwl])$/) {
#	    die if (@operands != 0);
#	    $inputs .= &break_it_down ("%esi", "l") . " ";
#	    $outputs .= &break_it_down ("%esi", "l") . " ";
#	    $inputs .= &break_it_down ("(%esi)", $len) . " ";
#	    $outputs .= &break_it_down ("%eax", $len) . " ";
#	    $inputs .= "reg_ccd";
#	    last the_switch;
#	}

	# CWTL
	if (($op, $len) = /^(cwt)(l)$/) {
	    die if (@operands != 0);
	    $inputs .= &break_it_down ("%ax", "w") . " ";
	    $outputs .= &break_it_down ("%eax", "l") . " ";
	    last the_switch;
	}

	# CLTD
	if (/^cltd$/) {
	    die if (@operands != 0);
	    $inputs .= &break_it_down ("%eax", "l") . " ";
	    $outputs .= &break_it_down ("%edx", "l") . " ";
	    last the_switch;
	}

	die "Unrecognized instruction $insn\n";
    }

    if (!$no_union_inputs) {
	$inputs .= join (' ', grep (/\*/, split (' ', $outputs))) . " ";
    }
    
    chop $inputs;
    chop $outputs;
    $inputs = &union ($inputs);
    $outputs = &union ($outputs);
#    print "$insn\n Inputs: $inputs\n Outputs: $outputs\n";
    return ($inputs, $outputs, 2);
}

sub compute_info
{
    local($lineno) = @_;

    ($inputs[$lineno], $outputs[$lineno], $latency[$lineno]) =
	&recognize ($lines[$lineno]);
}

open (REG, "reg") || die;

while (<REG>)
{
    local ($reg, $len, $info);
    chop;
    ($reg, $len, $info) = split (/\t+/);
    $register_info{$reg, $len} = $info;
    $regs{$reg}++;
}

close (REG);
