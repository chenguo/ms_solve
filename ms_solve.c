/* Minesweeper Consistency Problem solver.
   Written by Chen Guo, UCLA CS 261A project spring 2011.
*/

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Function prototypes. */
static int solve_brute ();
static int solve_opt ();
static int solve_subtree (int, bool);
static bool consistency_check (int);
static int find_mines ();
static void clear_mines (int);

static void board_print ();
static void parse_input ();
static void help ();

/* Defines. */
#define INBUF_SIZ 1024
#define MINE_UNKNOWN '?'
#define MINE_ON '*'
#define MINE_OFF '-'

/* Globals. */
static char *grid;            // Minesweeper grid.
static char **mines;          // Pointers to mines.
static int total_mines;       // Total possible mine positions.
static int dim_x;
static int dim_y;
static int ntiles;
static int goal_states;

int main (int argc, char **argv)
{
  char c;
  bool brute = true;

  /* Parse arguments. */
  while ((c = getopt (argc, argv, "bho")) != -1)
    {
      switch (c)
        {
          /* Brute force. */
        case 'b':
          brute = true;
          break;

          /* Help. Does not return. */
        case 'h':
          help ();

          /* Optimized. */
        case 'o':
          brute = false;
          break;
        }
    }

  /* Cycle through input files. */
  while (argv[optind])
    {
      int num_goals = 0;
      char *file = argv[optind++];
      printf ("Processing file %s\n", file);

      /* Parse the file and allocate grids buffer. */
      parse_input (file);
      total_mines = find_mines ();

      /* Attempt to solve the board. */
      if (brute)
        num_goals = solve_brute ();
      else
        num_goals = solve_opt ();

      printf ("Number of goal states: %d\n", num_goals);
    }
}

/* Solve the game board with brute force algorithm. The algorithm takes a trial
   and error approach, placing one mine at a time. Backtracking and pruning of
   the search tree are employed when an inconsistent game board is encountered.
   This algorithm is based on Neville Mehta's, ported from Lisp to C++ by
   Meredith Kadlac. */
static int solve_brute ()
{
  // Set first mine to off and explore subtree.
  int num_goals = solve_subtree (0, false);

  // Set first mine to on and explore subtree.
  clear_mines (0);
  num_goals += solve_subtree (0, true);

  return num_goals;
}

/* Solve a subtree of the mine possibilities. */
static int solve_subtree (int mine_num, bool mine_on)
{
  int num_goals = 0;

  // Turn the mine on or off.
  *mines[mine_num] = (mine_on)? MINE_ON : MINE_OFF;

  // Check consistency.
  bool consis = consistency_check (mine_num++);
  if (consis && mine_num < total_mines)
    {
      // Set next mine to off and explore subtree.
      num_goals = solve_subtree (mine_num, false);

      // Set next mine to on and explore subtree.
      clear_mines (mine_num);
      num_goals += solve_subtree (mine_num, true);
    }
  else if (consis && mine_num == total_mines)
    {
      board_print ();
      num_goals = 1;
    }

  return num_goals;
}

static int solve_opt ()
{}

/*
   This function only checks the tiles around the mine that was most recently
   turned on/off, since the validity of other tiles would not be affected.

   Given:
   N: number on tile.
   M: mines surrounding tile.
   Q: unknown tiles surrounding tile.
   A tile is consistent if M <= N <= M + Q
*/
static bool consistency_check (int mine_num)
{
  // Derive coordinates of mine.
  int mine_offset = mines[mine_num] - grid;
  int row = mine_offset / dim_y;
  int col = mine_offset % dim_y;

  // For each numbered tile around mine.
  int i;
  int j;
  for (i = -1; i < 2; i++)
    for (j = -1; j < 2; j++)
      {
        int tile_offset = (row + i) * dim_x + col + j;
        int tile_num = grid[tile_offset] - '0';
        if (0 <= tile_num && tile_num <= 8)
          {
            int mines = 0;
            int unknowns = 0;
            int k;
            int l;

            // Count number of mines and unknowns around tile.
            for (k = -1; k < 2; k++)
              for (l = -1; l < 2; l++)
                {
                  int probe_offset = tile_offset + k * dim_x + l;
                  if (grid[probe_offset] == MINE_ON)
                    mines++;
                  else if (grid[probe_offset] == MINE_UNKNOWN)
                    unknowns++;
                }
            // Check if tile is consistent.
            if (tile_num < mines || tile_num > mines + unknowns)
              return false;
          }
      }

  return true;
}

/* Allocate mine position array, and find the mines in the original grid.
   Returns the number of unknown tiles on the grid.
*/
static int find_mines ()
{
  char *grid_iter = grid;
  char **mine_iter = mines;
  int mine_count = 0;

  /* Search for question marks and save pointers to them. */
  while (1)
    {
      *mine_iter = strchr (grid_iter, MINE_UNKNOWN);
      if (*mine_iter)
        {
          grid_iter = *mine_iter++ + 1;
          mine_count++;
        }
      else
        break;
    }

  return mine_count;
}

/* Set mines to MINE_UNKNOWN from MINE_NUM onwards. */
static inline void clear_mines (int mine_num)
{
  char **mine_iter = mines + mine_num;
  while (*mine_iter)
    **(mine_iter++) = MINE_UNKNOWN;
}


///////////////////////////////////////////////////////////////////////////////
//
//  Misc. functions.
//
//////////////////////////////////////////////////////////////////////////////

/* Print the game board. */
static void board_print ()
{
  int i, j;
  int index = dim_x + 1;

  /*
  printf ("+");
  for (j = 0; j < dim_x - 2; j++)
    printf ("---+");
  */

  for (i = 1; i < dim_y - 1; i++)
    {
      for (j = 1; j < dim_x - 1; j++)
        {
          printf ("%c", grid[index++]);
        }
      printf ("\n");
      index += 2;
      /*printf ("\n|");
      for (j = 1; j < dim_x - 1; j++)
        printf (" %c |", grid[i * dim_x + j]);
      printf ("\n+");
      for (j = 0; j < dim_x - 2; j++)
      printf ("---+");
      */
    }
  printf ("\n\n");
}

/* Parse input file for initial tile layout. */
static void parse_input (char *file)
{
  FILE *fh = fopen (file, "r");
  char inbuf[INBUF_SIZ];
  char *buf_ptr;
  int i;

  // First line: dimensions.
  fgets (inbuf, INBUF_SIZ, fh);
  if (sscanf (inbuf, "%d%*[x ]%d", &dim_x, &dim_y) != 2)
    {
      fprintf (stderr, "Invalid dimensions format. ");
      fprintf (stderr, "Please use \"W x H\" format\n");
      return;
    }
  printf ("Dimensions: %d, %d\n", dim_x, dim_y);

  // Allocate buffer for original grid.
  dim_x += 2;
  dim_y += 2;
  ntiles = dim_x * dim_y;
  if (ntiles <= 0)
    {
      fprintf (stderr, "Invalid dimensions.\n");
      return;
    }
  grid = malloc ((ntiles + 1) * sizeof (*grid));

  // Set everything to off.
  for (i = 0; i < ntiles; i++)
    grid[i] = MINE_OFF;
  grid[ntiles] = '\0';

  // Fill in original grid.
  i = 0;
  buf_ptr = grid + dim_x + 1;
  while (i++ < dim_y - 2)
    {
      fgets (inbuf, INBUF_SIZ, fh);
      memcpy (buf_ptr, inbuf, dim_x - 2);
      if (feof (fh) || ferror (fh))
          break;
      buf_ptr += dim_x;
    }
  board_print ();

  // Allocate space for pointers to potential mines.
  mines = malloc ((ntiles + 1) * sizeof (*mines));
}

/* Print help message. */
static void help ()
{
  printf ("\
Minesweeper solver. Written by Chen Guo.\n\
Options:\n\
  -b  Brute force algorithm (default).\n\
  -h  Print this help message.\n\
  -o  Optimized algorithm.\n");
}
