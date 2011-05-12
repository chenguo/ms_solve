/* Minesweeper Consistency Problem solver.
   Written by Chen Guo, UCLA CS 261A project spring 2011.
*/

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

/* Defines. */
#define INBUF_SIZ 1024
#define TILE_ADJUST 0
#define UNKNOWN '?'
#define MINE_ON '*'
#define MINE_OFF '-'
#define LOCK {pthread_mutex_lock (&thr_lock);}
#define UNLOCK {pthread_mutex_unlock (&thr_lock);}

/*****************************************************************************
 *
 *  Structures
 *
 ****************************************************************************/

/* Index in 2D grid. */
struct ind
{
  int row;
  int col;
};

/* Buffers used for search. */
struct buffers
{
  int *buf;                   // Array for grid.
  int **grid;                 // 2D array for grid.
  struct ind *ind;            // Array of indices to unknowns.
};

/* Individual thread data. */
struct search
{
  pthread_t thread;           // Thread object.
  bool avail;                 // Available flag.
  struct buffers bufs;        // Thread individual buffers used for search.
};

/* solve_tree () arguments for threading. */
struct thr_args
{
  int thread_num;             // Number of thread being used.
  int unknown_num;            // Number of unknown being inspected.
  int mine_count;             // Number of mines turned on thus far.
  struct buffers bufs;        // Buffers used for searching.
};

/*****************************************************************************
 *
 *  Globals
 *
 ****************************************************************************/

/* Globals. */
static int nrows;
static int ncols;
static int ntiles;
static int total_unknowns;       // Total possible mine positions.
static int goal_states = 0;      // Total goal states found, with constraints.
static bool print = false;       // Print boards.
static bool single = true;       // Find all solutions (opposed to just one)
static int mine_target = -1;        // Number of desired mines in solution.

/* For thread control */
static int max_threads = 1;      // Threads to use.
static int avail_threads = 1;    // Available threads.
static struct search *thr_data;  // Array of search data, for threads.
static pthread_mutex_t thr_lock; // Thread control mutex.
static pthread_cond_t thr_cond;  // Thread control cond var.

/* Function prototypes. */
static void optimize_grid ();
static void * solve_tree_thr (void *);
static int solve_tree (int, int, struct buffers);
static bool consistency_check (struct ind, int **grid);
static int find_unknowns (int **, struct ind *);
static void clear_unknowns (int, struct ind *, int **);

static void thread_alloc ();
static void thread_struct_alloc ();
static void thread_free ();
static void thread_copy (int, int);

static void board_print (int **);
static void parse_input (char *);
static void help ();


int main (int argc, char **argv)
{
  char c;
  bool brute = false;

  // Parse arguments.
  while ((c = getopt (argc, argv, "abhmopt")) != -1)
    {
      switch (c)
        {
          // Find all solutions.
        case 'a':
          single = false;
          break;

          // Brute force.
        case 'b':
          brute = true;
          break;

          // Help. Does not return.
        case 'h':
          help ();

          // Target number of mines.
        case 'm':
          mine_target = atoi (argv[optind++]);
          break;

          // Optimized.
        case 'o':
          brute = false;
          break;

          // Print.
        case 'p':
          print = true;
          break;

          // Threads tp use.
        case 't':
          max_threads = atoi(argv[optind++]);
          // Current thread is a thread, so max-1 are available.
          avail_threads = max_threads - 1;
          printf ("Using %d threads\n", avail_threads);
          break;
        }
    }

  int num_goals = 0;
  char *file = argv[optind];
  printf ("Processing file %s\n", file);

  // Allocate structures for threads.
  thread_alloc ();

  // Parse the input file. The buffers for each thread structure
  // is allocated here.
  parse_input (file);

  // Begin processing file. Start timer.
  struct timeval timer_start;
  gettimeofday (&timer_start, NULL);

  // If not brutefoce, optimize board first.
  if (!brute)
    optimize_grid ();

  total_unknowns =
    find_unknowns (thr_data[0].bufs.grid, thr_data[0].bufs.ind);

  // Attempt to find a solution, or multiple solutions. Spin off thread in
  // thread 0, and wait for results.
  LOCK;
  struct thr_args args = {0, 0, 0, thr_data[0].bufs};
  pthread_create (&thr_data[0].thread, NULL, solve_tree_thr, &args);
  pthread_detach (&thr_data[0].thread);
  pthread_cond_wait (&thr_cond, &thr_lock);
  UNLOCK;

  printf ("Number of goal states: %d\n", goal_states);

  // Find elapsed time in us.
  struct timeval timer_end;
  gettimeofday (&timer_end, NULL);
  long usec = timer_end.tv_sec * 1000000 + timer_end.tv_usec
    - timer_start.tv_sec * 1000000 - timer_start.tv_usec;
  printf ("Time elapsed: %ld us\n", usec);

  // Free thread resources.
  thread_free ();
}



/*****************************************************************************
 *
 *  Solver functions.
 *
 ****************************************************************************/

/* This function attempts to resolve some unknown tiles before we start the
   search. Essentially, this will reduce the depth of the search tree. */
static void optimize_grid ()
{}

/* Threaded function call for solve_tree. */
static void * solve_tree_thr (void *data)
{
  struct thr_args *args = (struct thr_args *) data;
  int num_goals = solve_tree (args->unknown_num, args->mine_count, args->bufs);

  // Critical section.
  LOCK;
  // Update the number of goals found.
  goal_states += num_goals;

  // Indicate our thread as being availabe.
  thr_data[args->thread_num].avail = true;

  // If we are the last thread to exit, execution is complete.
  if (++avail_threads == max_threads)
    pthread_cond_signal (&thr_cond);

  UNLOCK;

  return NULL;
}

/* Solve the game board with brute force algorithm. The algorithm takes a trial
   and error approach, placing one mine at a time. Backtracking and pruning of
   the search tree are employed when an inconsistent game board is encountered.
   This algorithm is based on Neville Mehta's, ported from Lisp to C++ by
   Meredith Kadlac, but is much more optimized and performs more than 20x
   faster. */
static int solve_tree (int unknown_num, int mine_count, struct buffers bufs)
{
  int row = bufs.ind[unknown_num].row;
  int col = bufs.ind[unknown_num].col;
  bool consis;
  int num_goals = 0;

  // Keep mine off. Check for consistency.
  bufs.grid[row][col] = MINE_OFF;
  consis = consistency_check (bufs.ind[unknown_num], bufs.grid);
  if (consis)
    {
      // The MINE_OFF state for this mine is valid.
      if (unknown_num < total_unknowns - 1
          && (mine_target == -1 || mine_count <= mine_target))
        {
          // A subtree exists, and if MINE_TARGET is specified, we have
          // not exceeded it. Check subtree for valid solutions.
          num_goals = solve_tree (unknown_num + 1, mine_count, bufs);
        }
      else if (unknown_num == total_unknowns - 1
               && (mine_target == -1 || mine_count == mine_target))
        {
          // 1) All unknowns have been assigned a valid state.
          // 2) MINE_TARGET wasn't specified, or it has been reached.
          // Thus, we have found a solution.
          num_goals++;
          if (print)
            board_print (bufs.grid);
        }
      // The remaining cases are
      // 1) MINE_TARGET is specified and exceeded
      // 2) MINE_TARGET is specified and not reached, but unknown tiles have
      //    all been assigned.
      // In both of these cases, no solution is found. NUM_GOALS remains 0.
    }

  // Turn mine on. This case is invalid if:
  // 1) SINGLE is set and a goal state has been found.
  // 2) MINE_TARGET is set and has been reached.
  if (single && num_goals || (mine_target >= 0 && mine_count == mine_target))
      return num_goals;

  // Turn mine on. Check for consistency.
  clear_unknowns (unknown_num + 1, bufs.ind, bufs.grid);
  bufs.grid[row][col] = MINE_ON;
  consis = consistency_check (bufs.ind[unknown_num], bufs.grid);
  if (consis)
    {
      // The MINE_ON state for this mine is valid.
      mine_count++;

      if (unknown_num < total_unknowns -1
          && (mine_target == -1 || mine_count <= mine_target))
        {
          num_goals += solve_tree (unknown_num + 1, mine_count, bufs);
        }
      else if (unknown_num == total_unknowns - 1
               && (mine_target == -1 || mine_count == mine_target))
        {
          num_goals++;
          if (print)
            board_print (bufs.grid);
        }
    }

  return num_goals;
}

/*
   This function only checks the tiles around the mine that was most recently
   turned on/off, since the validity of other tiles would not be affected.

   Given:
   N: number on tile.
   M: mines surrounding tile.
   Q: unknown tiles surrounding tile.
   A tile is consistent if M <= N <= M + Q
*/
static bool consistency_check (struct ind index, int **grid)
{
  int row = index.row;
  int col = index.col;
  int i;
  int j;

  for (i = -1; i < 2; i++)
    for (j = -1; j < 2; j++)
      {
        // For each numbered tile around mine:
        int tile_num = grid[row][col] + TILE_ADJUST;
        if (0 <= tile_num && tile_num <= 8)
          {
            int local_mines = 0;
            int local_unknowns = 0;
            int k;
            int l;

            // Count number of mines and unknowns aroudn tile.
            for (k = -1; k < 2; k++)
              for (l = -1; l < 2; l++)
                {
                  if (grid[i+k][l+k] == MINE_ON)
                    local_mines++;
                  else if (grid[i+k][l+k] == UNKNOWN)
                    local_unknowns++;
                }

            // Perform the consistency check.
            if (tile_num < local_mines
                || tile_num > local_mines + local_unknowns)
              return false;
          }
      }
  return true;
}

/* Find the unknowns in the grid, and establish the indices. */
static int find_unknowns (int **grid, struct ind *ind)
{
  int i, j, n = 0;
  for (i = 1; i < nrows - 1; i++)
    for (j = 1; j < ncols - 1; j++)
      {
        if (grid[i][j] == UNKNOWN)
          {
            ind[n].row = i;
            ind[n++].col = j;
          }
      }
  ind[n].row = -1;
  ind[n].col = -1;
  return n;
}

/* Set mines to UNKNOWN from I onwards. */
static inline void clear_unknowns (int i, struct ind *ind, int **grid)
{
  while (i < total_unknowns)
    grid[ind[i].row][ind[i++].col] = UNKNOWN;
}


/*****************************************************************************
 *
 *  Thread control functions.
 *
 ****************************************************************************/

/* Allocate thread memory. Called at the start of the program. */
static void thread_alloc ()
{
  pthread_mutex_init (&thr_lock, NULL);
  pthread_cond_init (&thr_cond, NULL);
  thr_data = (struct search *) malloc (avail_threads * sizeof *thr_data);
}

/* Call after input file is read, and thus board dimensions are known. We can
   now appropriately allocate the remaning structures.
   Note that here, the first struct has been allocated for us by the parsing
   function. */
static void thread_struct_alloc ()
{
  int i;
  for (i = 0; i < max_threads; i++)
    {
      thr_data[i].avail = true;

      // Allocate memory for buffers.
      thr_data[i].bufs.buf = (int *) malloc (ntiles * sizeof (int));
      thr_data[i].bufs.grid = (int **) malloc (nrows * sizeof (int *));
      thr_data[i].bufs.ind =
        (struct ind *) malloc ((ntiles + 1) * sizeof (struct ind));

      // GRID is a 2D array of [row][col] ordering, so GRID is an array of pointers
      // to the start of each row.
      int j;
      int *buf_iter = thr_data[i].bufs.buf;
      int **grid = thr_data[i].bufs.grid;
      for (j = 0; j < nrows; j++)
        {
          grid[j] = buf_iter;
          buf_iter += ncols;
        }
    }
}



/* At the end of the program, free thread memory. */
static void thread_free ()
{
  pthread_mutex_destroy (&thr_lock, NULL);
  pthread_cond_destroy (&thr_cond, NULL);
  int i;
  for (i = 0; i < max_threads; i++)
    {
      free (thr_data[i].bufs.buf);
      free (thr_data[i].bufs.grid);
      free (thr_data[i].bufs.ind);
    }
  free (thr_data);
}

/* Copy thread ORIG's data to thread CPY's thread data structure. */
static void thread_copy (int cpy, int orig)
{
  memcpy (thr_data[cpy].bufs.buf, thr_data[orig].bufs.buf,
          ntiles * sizeof (int));
  memcpy (thr_data[cpy].bufs.ind, thr_data[orig].bufs.ind,
          (total_unknowns + 1) * sizeof (struct ind));

  // GRID is a 2D array of [row][col] ordering, so GRID is an array of pointers
  // to the start of each row.
  int i;
  int *buf_iter = thr_data[cpy].bufs.buf;
  int **grid = thr_data[cpy].bufs.grid;
  for (i = 0; i < nrows; i++)
    {
      grid[i] = buf_iter;
      buf_iter += ncols;
    }
}


/*****************************************************************************
 *
 *  Miscellaneous functions.
 *
 ****************************************************************************/

/* Print the game board. */
static void board_print (int **grid)
{
  int i, j;
  for (i = 1; i < nrows - 1; i++)
    {
      for (j = 1; j < ncols - 1; j++)
        printf ("%c", grid[i][j]);
      printf ("\n");
    }
  printf ("\n\n");
}

/* Parse input file for initial tile layout. This is done at the beginning
   of processing each file, so the only thread that should be in use is
*/
static void parse_input (char *file)
{
  char inbuf[INBUF_SIZ];
  FILE *fh = fopen (file, "r");

  // Get the dimensions.
  fgets (inbuf, INBUF_SIZ, fh);
  if (sscanf (inbuf, "%d%*[x ]%d", &nrows, &ncols) != 2)
    {
      fprintf (stderr, "Invalid dimensions format. ");
      fprintf (stderr, "Please use \"W x H\" format\n");
      exit (1);
    }
  if (ncols >= INBUF_SIZ-1)
    {
      fprintf (stderr, "NCOLS too big for input buffer.\n");
      exit (1);
    }
  printf ("Dimensions: %d, %d\n", nrows, ncols);

  // Add buffer to dimensions.
  nrows += 2;
  ncols += 2;
  ntiles = nrows * ncols;
  if (ntiles <= 0)
    {
      fprintf (stderr, "Invalid dimensions.\n");
      exit (1);
    }

  // Now that the dimensions are known, we can finish allocating
  // buffers for each thread structure.
  thread_struct_alloc ();

  // Thread 0's buffers will now be filled. Mark it as in use.
  thr_data[0].avail = false;
  avail_threads--;

  // Set outside boundary to off. For efficiency, loops are not combined.
  int i;
  int **grid = thr_data[0].bufs.grid;
  for (i = 0; i < ncols; i++)
    grid[0][i] = MINE_OFF;
  for (i = 0; i < ncols; i++)
    grid[nrows-1][i] = MINE_OFF;
  for (i = 1; i < nrows - 1; i++)
    {
      grid[i][0] = MINE_OFF;
      grid[i][ncols-1] = MINE_OFF;
    }


  // Fill in original grid.
  int j;
  for (i = 1; i < nrows; i++)
    {
      // Read in a row as CHAR.
      fgets (inbuf, INBUF_SIZ, fh);
      if (feof (fh))
        break;
      if (ferror (fh))
        {
          fprintf (stderr, "Error reading file.\n");
          exit (1);
        }

      // Copy to grid mapped to INT.
      for (j = 0; j < ncols-2; j++)
        grid[i][j+1] = inbuf[j];
    }

  board_print (grid);
}

/* Print help message. */
static void help ()
{
  printf ("\
Minesweeper solver. Written by Chen Guo.\n\
Usage:\n\
  ms_solve [OPTION]... [FILE]\n\n\
Options:\n\
  -a  Find all solutions.\n\
  -b  Brute force algorithm.\n\
  -h  Print this help message.\n\
  -m  Set a target number of mines.\n\
  -o  Optimized algorithm.\n\
  -p  Print solutions.\n\
  -t  Threads to use.\n\n");
}
