#! /usr/bin/perl

use warnings;
use strict;
sub run_test;


if (@ARGV > 0 && $ARGV[0] =~ /all/) {
    print "RUNNING ALL TESTS\n\n";
    print "Base case:\n";
    #run_test ("-t 1 -p 1");
    print "\n\n";

    print "Pre-resolve:\n";
    #run_test ("-r -t 1 -p 1");
    print "\n\n";

    print "Forward checking:\n";
    #run_test ("-f -t 1 -p 1");
    print "\n\n";

    print "Variable ordering:\n";
    #run_test ("-s -t 1 -p 1");
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

    print "16 threads\n";
    run_test ("-t 8 -p 1");
    print "\n\n";

    print "All Optimizations (1 thread)\n";
    #run_test ("-r -t 1 -p 1");
    print "\n\n";

    print "All Optimizations (8 threads)\n";
    #run_test ("-r -t 8 -p 1");
    print "\n\n";
} elsif (@ARGV > 0 && $ARGV[0] =~ /test/) {
    #for (my $pct = 0; $pct <= 100; $pct += 10) {
    my $pct = 100;
        print "Blank %: $pct\n";
        run_test ("", $pct);
    #}
} else {
    my $arg = join (" ", @ARGV);
    print "Running with arguments: $arg\n";
    run_test ($arg);
}

sub run_test () {
    my $extra_args = shift;
    my $blank_pct = shift;
    if (!defined $blank_pct) {
        $blank_pct = 0.4;
    }
    my $test_mode = shift;

    print "Target_Blanks,Side,Blanks,Pre_Time,Search_Time,Elapsed_Time,Avg_Preres\n";
    for (my $blanks = 10; $blanks <= 100; $blanks += 10) {
        my $dim = int (sqrt ($blanks / $blank_pct) + .5);
        my $mines = int ($dim * $dim * 0.2);
        my $total_elapsed = 0;
        my $total_preproc = 0;
        my $total_search = 0;
        my $total_preres = 0;
        my $executed = 0;

        # Run 500 times, get average run time, as well as low and high.
        for (my $j = 0; $j < 500; $j++) {
            # Generate a grid.
            open (GRID, "./ms_gen.pl $dim $dim $mines $blank_pct |");
            open (TMP, ">", "./tmp.ms");
            while (<GRID>) {
                print TMP $_;
            }
            close GRID;
            close TMP;

            # Time ms_solve with the brute force algorithm.
            open (SOLVE, "./ms_solve $extra_args -m $mines tmp.ms |");
            while (<SOLVE>) {
                if ($_ =~ /Elapsed time: (\d+.\d+)/) {
                    $total_elapsed += $1;
                    $executed++;
                } elsif ($_ =~ /Preprocess time: (\d+.\d+)/) {
                    $total_preproc += $1;
                } elsif ($_ =~ /Search time: (\d+.\d+)/) {
                    $total_search += $1;
                } elsif ($_ =~ /Pre-resolved unknowns: (\d+)/) {
                    $total_preres += $1;
                }
            }
        }

        my $preproc_time = $total_preproc / $executed;
        my $search_time = $total_search / $executed;
        my $elapsed_time = $total_elapsed / $executed;
        my $avg_preres = $total_preres / $executed;
        my $true_blanks = int (.4 * $dim * $dim + .5);
        print "$blanks,$dim,$true_blanks,$preproc_time,$search_time,$elapsed_time,$avg_preres\n";
    }
}
