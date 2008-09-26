#!/usr/bin/perl
$* = 1;   # Enable multi-line patterns

print STDERR "Parsing file...";


# Read the entire file into core, splitting it up into pseudo-basic blocks
while (<>){
    next if /^\s*$/;   # Skip blank lines

    if (/\s*\.align/ || !$started)
    {
        if ($current_block)
        {
            push (@blocks, $current_block);
            push (@block_names, $current_block_name);
        }

        $current_block = $_;
        $current_block_name = "";
        $started = 1;
    }
    elsif ($current_block && !$current_block_name && /^\w.*:$/)
    {
        $current_block .= "xx#BLOCK_NAME_HERE\n";
        $current_block_name = $_;
        chop $current_block_name;
        $current_block_name =~ s/://g;
    }
    elsif ($current_block)
    {
        # Note: we don't add the name of the block to the block itself.
        $current_block .= $_;
    }
}

# Clean up any stuff left around at end of file
if ($current_block)
{
    push (@blocks, $current_block);
    push (@block_names, $current_block_name);
}


print STDERR "done.\n";


sub eliminate_duplicates
{
    local (%blocks_found, $old, $new, $old_name, $new_name, $b, $bname, $n,
           $x, $clobbered_blocks, $clobbered_lines, @clobbered_old,
           @clobbered_new, @clobbered_old_s68k, @clobbered_new_s68k,
           @clobbered_old_non_s68k, @clobbered_new_non_s68k);

    print STDERR "Pass $pass:\t";

    # Loop over all the blocks, looking for duplicates.
    $new = 0;
    $clobbered_blocks = 0;
    $clobbered_lines = 0;
    foreach $old (0 .. $#blocks)
    {
        $b = $blocks[$old];
        $bname = $block_names[$old];

        # If this block is redundant, add it to the clobbered list.
        if ($bname && $blocks_found{$b})
        {
            # Create regexps for the old/new to replace
            $old_name = $bname;            $old_name =~ s/(\W)/\\$1/g;
            $new_name = $blocks_found{$b}; $new_name =~ s/(\W)/\\$1/g;
            push (@clobbered_old, $old_name);
            push (@clobbered_new, $new_name);
            $clobbered_blocks++;
            $clobbered_lines += split (' ', $b);
        }
        else    # Save this block
        {
            $blocks[$new] = $b;
            $block_names[$new] = $bname;
            $blocks_found{$b} = $bname;
            $new++;
        }
    }

    # Print out a status report.
    print STDERR "eliminating $clobbered_blocks/", $#blocks + 1, " blocks ",
    "($clobbered_lines lines)...";

    # If we found something to clobber, clean everything up.
    if ($clobbered_blocks)
    {
        # Truncate the blocks array to the new number of blocks.
        $#blocks = $new - 1;
        $#block_names = $new - 1;

        # Divide up the replace strings into two classes, for speed.
        foreach $n (0 .. $#clobbered_old)
        {
            if (@clobbered_old[$n] =~ /_S68K_/)
            {
                push (@clobbered_old_s68k, @clobbered_old[$n]);
                push (@clobbered_new_s68k, @clobbered_new[$n]);
            }
            else
            {
                push (@clobbered_old_non_s68k, @clobbered_old[$n]);
                push (@clobbered_new_non_s68k, @clobbered_new[$n]);
            }
        }

        foreach $n (0 .. $#blocks)
        {
            # Dump out this block if it's unique or special.
            if (!$block_names[$n]
                || $blocks_found{@blocks[$n]} eq $block_names[$n])
            {
                $b = $blocks[$n];
                $b =~ s/xx#BLOCK_NAME_HERE/$block_names[$n]:/g;

                # Replace any _S68K_ labels (if there are any here).
                if ($b =~ /_S68K_/)
                {
                    foreach $x (0 .. $#clobbered_old_s68k)
                    {
                        $old_name = $clobbered_old_s68k[x];
                        $new_name = $clobbered_new_s68k[x];
                        if ($b =~ /$old_name/)
                        {
                            $b =~ s/$old_name,/$new_name,/g;
                            $b =~ s/$old_name$/$new_name$/g;
                            
                            # If we didn't eliminate the old label, fail!
                            die "I'm afraid to replace \"$old_name\" in this ",
                            "block:\n", $b if ($b =~ /$old_name/);
                        }
                    }
                }

                # Replace any non-_S68K_ labels.
                foreach $x (0 .. $#clobbered_old_non_s68k)
                {
                    $old_name = $clobbered_old_non_s68k[x];
                    $new_name = $clobbered_new_non_s68k[x];
                    if ($b =~ /$old_name/)
                    {
                        $b =~ s/$old_name,/$new_name,/g;
                        $b =~ s/$old_name$/$new_name$/g;

                        # If we didn't eliminate the old label, fail!
                        die "I'm afraid to replace \"$old_name\" in this ",
                          "block:\n", $b if ($b =~ /$old_name/);
                    }
                }
                $blocks[$n] = $b;
            }
        }
    }

    print STDERR "done.\n";
    return $clobbered_blocks;
}


# Keep eliminating duplicates until nothing changes.
$pass = 1;
while (&eliminate_duplicates ())
{
    $pass++;
}


# Print out all of the blocks.
foreach $n (0 .. $#blocks)
{
    print $blocks[$n];
}
