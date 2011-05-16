#! /usr/bin/perl

print "Target_Blanks,Side,Blanks,Low,High,Avg\n";
for (my $blanks = 100; $blanks <= 2000; $blanks += 100) {
    my $mines = 0.5 * $blanks;
    my $dim = int (sqrt ($blanks / .4) + .5);
    my $total_time = 0;
    my $low = 100000000000;
    my $high= 0;

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
        open (SOLVE, "./ms_solve -t 1 tmp.ms |");
        while (<SOLVE>) {
            #print $_;
            if ($_ =~ /Time elapsed.*(\d+)/) {
                #print $_;
                my $time = $1;
                if ($time < $low) { $low = $time }
                if ($time > $high) { $high = $time }
                $total_time += $time;
            }
        }
    }
    my $avg = $total_time /= 500;
    my $true_blanks = int (.4 * $dim * $dim + .5);
    #printf ("%d x %d, $true_blanks blanks:\n  Low: %f\n  High: %f\n  Avg: %f\n",
    #        $dim, $dim, $low, $high, $avg);
    print "$blanks,$dim,$true_blanks,$low,$high,$avg\n";
}
