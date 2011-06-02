#! /usr/bin/perl

use warnings;
use strict;
sub run_test;
sub run_test_set;
sub run_test_loop;
sub get_dim;
sub filter;
sub stats;


if (@ARGV > 0 && $ARGV[0] =~ /all/) {
    # Test all options.

    print "RUNNING ALL TESTS\n\n";
    print "Base case:\n";
    run_test_loop ("-t 1 -p 1");
    print "\n\n";

    print "Pre-resolve:\n";
    run_test_loop ("-r -t 1 -p 1");
    print "\n\n";

    print "Forward checking:\n";
    run_test_loop ("-f -t 1 -p 1");
    print "\n\n";

    #print "Variable ordering:\n";
    #run_test ("-s -t 1 -p 1");
    #print "\n\n";

    print "2 threads\n";
    run_test_loop ("-t 2 -p 1");
    print "\n\n";

    print "4 threads\n";
    run_test_loop ("-t 4 -p 1");
    print "\n\n";

    print "8 threads\n";
    run_test_loop ("-t 8 -p 1");
    print "\n\n";

    print "16 threads\n";
    run_test_loop ("-t 8 -p 1");
    print "\n\n";

    print "All Optimizations (8 threads)\n";
    run_test_loop ("-r -t 8 -p 1 -f");
    print "\n\n";
} elsif (@ARGV > 0 && $ARGV[0] =~ /hard/) {
    # Test for problem hardness.
    # Methodology: 1 set of runs: 19 runs with blank_pct from 5% to 95%
    # 620 runs sets done, 20 run sets per blank count, starting from 20.


    my $blanks = $ARGV[1];
    if (!defined $blanks) { $blanks = 30 }
    #print "Rows,Cols,Blanks,Time,Pre-Resolved\n";

    # Array of number of times this blank_pct had the worst run time, where
    # array[0] is associated with blank_pct = 5%, array[1] with 10%, etc.
    my @worst_count = (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    my @total_times = ();
    for (my $i = 0; $i < 19; $i++) { push (@total_times, []); }

    my $runs = 250;
    while ($runs--) {
        my $worst_time = 0;
        my $worst = 0;
        for (my $blank_pct = 0.05, my $index = 0; $blank_pct < 1; $blank_pct += .05, $index++) {
            # Run a single test.
            (my $rows, my $cols, $blanks) = get_dim ($blanks, $blank_pct);
            my $mines = int ($rows * $cols * 0.2);
            (my $time,) = run_test ("", $rows, $cols, $mines, $blank_pct);
            my $tmp_aref = $total_times[$index];
            push (@$tmp_aref, $time);

            # Check if it has the worst runtime so far.
            if ($time >= $worst_time) {
                $worst_time = $time;
                $worst = $index;
            }
         }
        $worst_count[$worst]++;
        #if ($runs % 10 == 0) { $blanks++; }
    }

    for (my $index = 0; $index < 19; $index++) {
        my $pct = 5 * ($index + 1);
        my $count = $worst_count[$index];
        my $tmp_aref = $total_times[$index];
        my $avg_time = filter (@$tmp_aref);
        print "$pct $count $avg_time\n";
    }
} else {
    # Run 100 samples with give arguments.

    my $arg = join (" ", @ARGV);
    print "Running with arguments: $arg\n";
    run_test_loop ($arg, 0.8);
}





# Subs

sub run_test_loop {
    my $extra_args = shift;
    my $blank_pct = shift;
    if (!defined $blank_pct) {
        $blank_pct = 0.8;
    }

    print "Target_Blanks,Side,Blanks,Pre_Time,Search_Time,Elapsed_Time,Avg_Preres\n";
    for (my $blanks = 5; $blanks <= 50; $blanks += 5) {
        run_test_set ($extra_args, $blank_pct, $blanks);
    }
}

sub run_test_set {
    my $extra_args = shift;
    my $blank_pct = shift;
    if (!defined $blank_pct) {
        $blank_pct = 0.4;
    }
    my $blanks = shift;

    # Find dimensions that'll get us our target number of blanks,
    # with rows and cols as equal as possible.
    (my $rows, my $cols, $blanks) = get_dim ($blanks, $blank_pct);
    my $mines = int ($rows * $cols * 0.2);
    my @times = ();
    my $total_preres = 0;
    my $seed = "";

    # Run 100 times, get average run time, as well as low and high.
    for (my $j = 0; $j < 100; $j++) {
        (my $time, my $preres) = run_test ($extra_args, $rows, $cols, $mines, $blank_pct, $seed);
        push (@times, $time);
        $total_preres += $preres;
        #$seed++;
    }

    my $avg_time = filter (@times);
    my $avg_preres = $total_preres / 100;
    print "$rows,$cols,$blanks,$avg_time,$avg_preres\n";
    return $avg_time;
}

sub run_test {
    (my $args, my $rows, my $cols, my $mines, my $blank_pct, my $seed) = @_;
    if (!defined $seed) { $seed = ""; }
    my $time = 0;
    my $preres = 0;

    # Generate a grid.
    open (GRID, "./ms_gen.pl $rows $cols $mines $blank_pct $seed |");
    open (TMP, ">", "./tmp.ms");
    while (<GRID>) {
        print TMP $_;
    }
    close GRID;
    close TMP;

    # Time ms_solve with the brute force algorithm.
    open (SOLVE, "./ms_solve $args -m $mines tmp.ms |");
    while (<SOLVE>) {
        if ($_ =~ /Elapsed time: (\d+.\d+)/) {
            $time = $1;
        } elsif ($_ =~ /Pre-resolved unknowns: (\d+)/) {
            $preres = $1;
        }
    }
    return ($time, $preres);
}

sub get_dim {
    my $blanks = shift;
    my $blank_pct = shift;

    my $rows = int (sqrt ($blanks / $blank_pct) + .5);
    my $cols = $rows;
    my $true_blanks = int ($blank_pct * $rows * $cols + .5);
    my $adj_dir = 0;
    if ($true_blanks > $blanks) { $adj_dir = -1; }
    else { $adj_dir = 1; }

    #print "  Row: $rows  Col: $cols  Blanks: $true_blanks\n";
    # First adjust rows.
    while (($true_blanks - $blanks) * $adj_dir < 0) {
        my $prev_rows = $rows;
        my $prev_blanks = $true_blanks;
        $rows += $adj_dir;
        $true_blanks = int ($blank_pct * $rows * $cols + .5);

        #print "  Row: $rows  Col: $cols  Blanks: $true_blanks\n";

        # If new value is further than old value, continue with old value.
        if (abs ($true_blanks - $blanks) > abs ($prev_blanks - $blanks)) {
            $rows = $prev_rows;
            $true_blanks = $prev_blanks;
            last;
        }
    }

    #print "  Row locked: $rows\n";

    # Flip the adjustment direction for cols.
    #print "  Row: $rows  Col: $cols  Blanks: $true_blanks\n";
    $adj_dir *= -1;
    while (($true_blanks - $blanks) * $adj_dir < 0) {
        my $prev_cols = $cols;
        my $prev_blanks = $true_blanks;
        $cols += $adj_dir;
        $true_blanks = int ($blank_pct * $rows * $cols + .5);

        #print "  Row: $rows  Col: $cols  Blanks: $true_blanks\n";

        # If new value is further than old value, continue with old value.
        if (abs ($true_blanks - $blanks) > abs ($prev_blanks - $blanks)) {
            $cols = $prev_cols;
            $true_blanks = $prev_blanks;
            last;
        }
    }
    #print "  Col locked: $cols\n";
    #print "  Blanks: $true_blanks\n";
    return ($rows, $cols, $true_blanks);
}

sub filter {
    my @times = shift;
    (my $mean, my $stdev) = stats (@times);
    my @new_times = ();

    # Filter out values > 3 stdevs away
    foreach my $time (@times) {
        if (abs ($time - $mean) <= 3 * $stdev) {
            push (@new_times, $time);
        }
    }

    (my $avg_time,) = stats (@new_times);
    return $avg_time;
}

sub stats {
    my @times = @_;
    my $n = @times;

    # Get mean.
    my $sum = 0;
    foreach (@times) {
        $sum += $_;
    }
    my $mean = $sum / $n;

    # Get standard deviation.
    $sum = 0;
    foreach (@times) {
        my $diff = $mean - $_;
        $sum += $diff * $diff;
    }
    my $stdev = sqrt ($sum / $n);

    return ($mean, $stdev);
}

