#!/usr/bin/perl

require "x86_recog.pl";

sub intersect {
    local (@sets) = @_;
    local ($set, $elem);
    local (%mark);

    foreach $set (@sets) {
	foreach $elem (split (' ', $set))
	{
	    local (@keys);
	    $mark{$elem} = $mark{$elem};
	    @keys = keys %mark;
	    grep (do { $mark{$_}++ if /$elem/; }, @keys);
	    grep (do { $mark{$elem}++ if $elem =~ /$_/; }, @keys);
	    $mark{$elem}--;
	    # grep (do { $mark{$elem}-- if $elem eq $_; }, @keys);
	}
    }

    return &union (join (' ', grep ($mark{$_} >= @sets, keys (%mark))));
}

sub union {
    local (@sets) = @_;
    local (@from, @to);
    local ($elem);

    @from = split (' ', join (' ', @sets));

    while ($elem = shift @from)
    {
	next if grep ($elem =~ /$_/, @from);
	next if grep ($elem =~ /$_/, @to);
	push (@to, $elem);
    }
    return join (' ', sort (@to));
}

sub is_subset {
    local ($little, $big) = @_;

    return ($little eq &intersect ($little, $big));
}

sub make_parents {
    local($lineno, $earlier);
    local($seeking, $not_seeking, $found);

    foreach $lineno ($first_line .. $last_line)
    {
	local (*par_ref) = 'parents' . $lineno;
	@par_ref = ();
	undef $not_seeking;
	$seeking = $inputs[$lineno];
	next if (!$seeking);

	foreach $earlier (reverse ($first_line .. $lineno-1))
	{
	    $found = &intersect ($seeking, $outputs[$earlier]);
	    next if (&is_subset ($found, $not_seeking));
	    $not_seeking = &union ($not_seeking, $found);
	    if ($found) {
#print $lineno . " " . $earlier . "\n";
		push (@par_ref, $earlier);
	    }
	}
    }
}

sub bad_order {
    local ($first, $middle, $last) = @_;

    if (($first < $middle) && ($middle < $last)) {
	return 0;
    }

    if (!defined ($cached_bad{$first, $middle, $last})) {
	if (&intersect ($outputs[$first],
			&intersect($outputs[$middle],
				   $inputs[$last])))
	{
	    $cached_bad{$first, $middle, $last} = 1;
	}
	else
	{
	    $cached_bad{$first, $middle, $last} = 0;
	}
    }

    return $cached_bad{$first, $middle, $last};
}

sub max
{
    local (@values) = @_;
    local ($result);

    grep (do { $result = $_ if $result < $_ } && 0, @values);

    return $result;
}

sub static_eval {
    return ($start_time[$schedule[$#schedule]] + 1 + $last_line -
	    $first_line + 1 - @schedule);
}

sub push_schedule {
    local ($lineno) = @_;
    local (@parents_times);
    local ($parent);

    if ($lineno == 0)
    {
	$start_time[$lineno] = 0;
    }
    else
    {
	local (*par_ref) = 'parents' . $lineno;
	foreach $parent (@par_ref)
	{
	    push (@parents_times, $start_time[$parent] + $latency[$parent]);
	}

	$start_time[$lineno] = &max ($start_time[$schedule[$#schedule]] + 1,
				     @parents_times);
    }
    push (@schedule, $lineno);
}

sub pop_schedule {
    pop (@schedule);
}

sub dump_schedule
{
    local ($lineno);
    foreach $lineno (@_)
    {
	print "$lines[$lineno]\t; $start_time[$lineno]\n"
    }
}

sub create_greedy_lookup
{
    local (@which_lines) = @_;
    local ($lineno);

    foreach $lineno (@which_lines)
    {
	&push_schedule ($lineno);
	$greedy_lookup[$lineno] = &static_eval ();
	&pop_schedule ();
    }
}

sub bygreed
{
    local ($foo);

    $foo = $greedy_lookup[$a] <=> $greedy_lookup[$b];

    return ($a <=> $b) if (!$foo);
    return $foo;
}

sub parents_scheduled
{
    local ($lineno) = @_;
    local (*par_ref) = 'parents' . $lineno;

    return !grep (!$scheduled[$_], @par_ref);
}

sub create_schedule {
    local ($lineno, $unscheduled_line);
    local ($static_value);
    local (@valid_next_lines);
    local (@unscheduled);
    local (@possible_problems);

    return if ($best_so_far == $last_line - $first_line + 1);

    $static_value = &static_eval();

    return if ($static_value >= $best_so_far);

    if (@schedule == $last_line - $first_line + 1)
    {
#	&dump_schedule (@schedule);
#	print " Value: $static_value\n";
	if ($static_value <= $best_so_far)
	{
	    @best_schedule = @schedule;
	    $best_so_far = $static_value;
	}
	return;
    }

    @unscheduled = grep (!$scheduled[$_], ($first_line .. $last_line));

    @valid_next_lines = grep (&parents_scheduled ($_), @unscheduled);

    &create_greedy_lookup (@valid_next_lines);
    foreach $lineno (sort bygreed (@valid_next_lines))
    {
	local ($bad);
	$bad = 0;

	foreach $unscheduled_line (@unscheduled)
	{
	    local ($possible_problem);
	    local (*par_ref) = 'parents' . $unscheduled_line;

	    next if $unscheduled_line == $lineno;

	    @possible_problems = grep ($scheduled[$_], @par_ref);

	    foreach $possible_problem (@possible_problems)
	    {
		next if (!&bad_order ($possible_problem, $lineno,
				      $unscheduled_line));
		$bad = 1;
		last;
	    }

	    last if $bad;
	}
	next if $bad;

	&push_schedule ($lineno);
	$scheduled[$lineno] = 1;
	&create_schedule ();
	&pop_schedule ();
	$scheduled[$lineno] = 0;
    }
}

@parents0 = @parents1 = @parents2 = @parents3 = ();
@parents4 = @parents5 = @parents6 = @parents7 = ();
@parents8 = @parents9 = @parents10 = @parents11 = ();

while (<>) {
    next if /^\s*$/;
    next if /^#(NO_)?APP/;

    if (!@lines && /^\s*\./)
    {
	print;
	next;
    }

    chop;

    if (!@lines) {
	# Prepend a dummy instruction
	push (@lines, '@@@@bogo@@@@');
	$inputs[$#lines] = "";
	$outputs[$#lines] = "";
	$latency[$#lines] = 0;
    }

    push (@lines, $_);

    ($inputs[$#lines], $outputs[$#lines], $latency[$#lines]) =
	&recognize ($_);

    if (@lines >= 10 || eof() || $inputs[$#lines] eq '.*'
	|| $outputs[$#lines] =~ /eip/)
    {
	local ($saved_outputs);
	$saved_outputs = $outputs[0];
	$outputs[0] = '.*';

	# Append a dummy instruction
	push (@lines, '@@@@bogo@@@@');
	$inputs[$#lines] = ".*";
	$outputs[$#lines] = ".*";
	$latency[$#lines] = 0;

	$first_line = 0;
	$last_line = $#lines;

	&make_parents ();

	$best_so_far = 999999;
	undef %cached_bad;
	undef @schedule;
	undef @start_time;
	undef @best_schedule;
	undef @scheduled;

#	print "\nInput block:\n", join ("\n", @lines), "\n";
	&create_schedule ();
#	print "\nOutput block:\n", join ("\n", @lines[@best_schedule]), "\n\n";

	$outputs[0] = $saved_outputs;
	# Renumber everything
	@lines = @lines[@best_schedule];
	@inputs = @inputs[@best_schedule];
	@outputs = @outputs[@best_schedule];
	@latency = @latency[@best_schedule];

	while ($line = shift @lines)
	{
	    local ($foo, $bar);
	    $foo = shift @inputs;
	    $bar = shift @outputs;
	    shift @latency;
	    print "$line\n" if ($line !~ /\@\@\@\@bogo\@\@\@\@/);
	    print STDERR "$line\n" if ($line =~ /^\w*:\s*$/);
#	    print " Inputs: $foo\n Outputs: $bar\n";
#	    last if ((@lines <= 5) && !eof() && !($inputs[$#lines] eq '.*'));
	}
    }
}
