#! /usr/bin/perl

use warnings;
use strict;
sub run_test;


if (@ARGV > 0 && $ARGV[0] =~ /all/) {
    print "RUNNING ALL TESTS\n\n";
    print "Base case:\n";
    run_test ("-t 1 -p 1");
    print "\n\n";

    print "Pre-resolve:\n";
    run_test ("-r -t 1 -p 1");
    print "\n\n";

    print "2 threads\n";
    run_test ("-t 2 -p 1");
    print "\n\n";

    print "4 threads\n";
    run_test ("-t 4 -p 1");
    print "\n\n";

    print "8 threads\n";
    run_test ("-t 8 -p 1");
    print "\n\n";

    print "All Optimizations\n";
    run_test ("-r -t 8 -p 1");
    print "\n\n";
} else {
    my $arg = join (" ", @ARGV);
    print "Running with arguments: $arg -m $mines\n";
    run_test ($arg);
}

sub run_test () {
    my $extra_args = shift;

    print "Target_Blanks,Side,Blanks,Low_Time,High_Time,Avg_Time,Avg_Preres\n";
    for (my $blanks = 100; $blanks <= 1000; $blanks += 100) {
        my $mines = 0.5 * $blanks;
        my $dim = int (sqrt ($blanks / .4) + .5);
        my $total_time = 0;
        my $low_time = 100000000000;
        my $high_time = 0;
        my $total_preres = 0;

        # Run 500 times, get average run time, as well as low and high.
        for (my $j = 0; $j < 500; $j++) {
            # Generate a grid.
            open (GRID, "./ms_gen.pl $dim $dim $mines 40 |");
            open (TMP, ">", "./tmp.ms");
            while (<GRID>) {
                print TMP $_;
            }
            close GRID;
            close TMP;

            # Time ms_solve with the brute force algorithm.
            open (SOLVE, "./ms_solve $extra_args -m $mines tmp.ms |");
            while (<SOLVE>) {
                if ($_ =~ /Time elapsed: (\d+.\d+)/) {
                    my $time = $1;
                    if ($time < $low_time) { $low_time = $time }
                    if ($time > $high_time) { $high_time = $time }
                    $total_time += $time;
                }
                elsif ($_ =~ /Pre-resolved unknowns: (\d+)/) {
                    my $preres = $1;
                    $total_preres += $preres;
                }
            }
        }
        my $avg_time = $total_time /= 500;
        my $avg_preres = $total_preres /= 500;
        my $true_blanks = int (.4 * $dim * $dim + .5);
        print "$blanks,$dim,$true_blanks,$low_time,$high_time,$avg_time,$avg_preres\n";
    }
}
