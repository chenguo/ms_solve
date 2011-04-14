# Minesweeper grid generator.

my $ARGC = @ARGV;
if ($ARGC < 4)
{
    print STDERR "Not enough arguments.\n";
    exit 1;
}

my $dim_x = $ARGV[0];
my $dim_y = $ARGV[1];
my $mines = $ARGV[2];
my $q_pct = $ARGV[3];

my $ntiles = $dim_x * $dim_y;
if ($mines > $ntiles)
{
    print STDERR "Too many mines for grid dimensions.\n";
    exit 1;
}

if ($q_pct < 1)
{
    $q_pct *= 100;
}

if ($q_pct < 0 || $q_pct > 100)
{
    print STDERR "Invalid blank percentage: $q_pct%\n";
}

#print "Width: $dim_x  Height: $dim_y  Mines: $mines  Blanks: $q_pct%\n";
print "$dim_x x $dim_y\n";

# Randomly generate bombs. Create list of available grid slots and choose from
# the list.
my @rand_tiles = (0 .. $ntiles - 1);
fisher_yates_shuffle (\@rand_tiles);
my @grid = ();
for ($i = 0; $i < $mines; $i++)
{
    $grid[$rand_tiles[$i]] = '*';
}
fill_grid ($dim_x, $dim_y, \@grid);
#print_grid ($dim_x, $dim_y, \@grid);

# Randomly add in blanks.
fisher_yates_shuffle (\@rand_tiles);
my $num_blanks = int ($q_pct * $ntiles / 100);
for ($i = 0; $i < $num_blanks; $i++)
{
    $grid[$rand_tiles[$i]] = '?';
}
print_grid ($dim_x, $dim_y, \@grid);
exit 0;


# Fill the minesweeper grid with numbers.
sub fill_grid
{
    my ($dim_x, $dim_y, $grid) = @_;

    # For each tile.
    for ($i = 0; $i < $dim_x; $i++) {
        for ($j = 0; $j < $dim_y; $j++) {
            # If it is not a mine.
            if ($$grid[$i * $dim_x + $j] ne '*') {
                my $num_mines = 0;
                # For all the tiles around it.
                for ($k = -1; $k < 2; $k++) {
                    for ($l = -1; $l < 2; $l++) {
                        # If the tile is out of bounds, ignore it.
                        my $row = $i + $k;
                        my $col = $j + $l;
                        if ($row < 0 || $row >= $dim_y || $col < 0 || $col >= $dim_x) {next;}

                        my $index = $row * $dim_x + $col;
                        if ($$grid[$index] eq '*') {$num_mines++;}
                    }
                }
                $$grid[$i * $dim_x + $j] = $num_mines;
            }
        }
    }
}

# Print the minesweeper grid.
sub print_grid
{
    my ($dim_x, $dim_y, $grid) = @_;
    for ($i = 0; $i < $dim_y; $i++)
    {
        for ($j = 0; $j < $dim_x; $j++)
        {
            print "$$grid[$i * $dim_x + $j]";
        }
        print "\n";
    }
    print "\n";
}


# Permute an array in place.
sub fisher_yates_shuffle
{
    my $array = shift;
    my $i;
    for ($i = @$array; --$i; )
    {
        my $j = int rand ($i+1);
        next if $i == $j;
        @$array[$i,$j] = @$array[$j,$i];
    }
}

# Print an array. Used for debugging.
sub print_array
{
    my $array = shift;
    foreach (@$array)
    {
        print "$_ ";
    }
    print "\n";
}
