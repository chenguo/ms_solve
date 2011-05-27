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
#define TILE_ZERO 1000000
#define TILE_MAP(val) (val - '0')  // Numbered tiles are mapped to
#define TILE_UNMAP(val) (val + '0')      // 1000000 - 1000008
#define UNKNOWN 9
#define MINE_ON 10
#define MINE_OFF -10
#define UNKNOWN_CHAR '?'
#define MINE_ON_CHAR '*'
#define MINE_OFF_CHAR '-'
#define LOCK {pthread_mutex_lock (thr_lock);}
#define UNLOCK {pthread_mutex_unlock (thr_lock);}

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
};

/* Struct used for sorting unknown tiles. */
struct sort_tile
{
  struct ind index;           // Index of unknown tile.
  int number_tiles;           // Number of surrounding tiles with numbers.
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

/* Argument settings. */
static bool force = false;       // Force unknown states during search.
static bool preresolve = false;  // Preresolve uknowns before search.
static bool single = true;       // Find all solutions (opposed to just one)
static bool sort = false;        // Sort unknown order.
static int mine_target = -1;     // Number of desired mines in solution.
enum { PRINT_NONE, PRINT_MIN, PRINT_BASIC, PRINT_ALL, PRINT_DEBUG };
static int print = PRINT_BASIC;   // Print boards.

/* For thread control */
static int max_threads = 1;      // Threads to use.
static int avail_threads = 1;    // Available threads.
static struct search *thr_data;  // Array of search data, for threads.
static pthread_mutex_t *thr_lock;    // Thread control mutex.
static pthread_cond_t *thr_cond; // Thread control cond var.
static __thread int thread_num;  // Thread number.

/* Function prototypes. */
/* Preprocess functions. */
static void preprocess_grid ();

/* Grid solver functions. */
static void * solve_tree_thr (void *);
static int solve_tree (int, int, struct buffers);
static int solve_subtree (int, int, struct buffers, bool);
static bool consistency_check (struct ind, int **, int);
static int find_unknowns (int **, struct ind *);
static void clear_unknowns (int, struct ind *, int **);

/* Thread control functions. */
static void thread_alloc ();
static void thread_struct_alloc ();
static int thread_find ();
static void thread_free ();
static void thread_copy (int, int);

/* Misc functions. */
static inline int force_on (int);
static inline int force_off (int);
static inline bool is_mine (int);
static inline int mine_src (int);
static void board_print (int **);
static void parse_input (char *);
static void help ();


int main (int argc, char **argv)
{
  // Parse arguments.
  char c;
  while ((c = getopt (argc, argv, "afhm:p:rst:")) != -1)
    {
      switch (c)
        {
          // Find all solutions.
        case 'a':
          single = false;
          break;

          // Force unknown state during search.
        case 'f':
          force = true;
          break;

          // Help. Does not return.
        case 'h':
          help ();

          // Target number of mines.
        case 'm':
          mine_target = atoi (optarg);
          break;

          // Print.
        case 'p':
          print = atoi (optarg);
          break;

          // Pre-resolve unknowns.
        case 'r':
          preresolve = true;
          break;

          // Sort unknowns by number of surrounding tiles.
        case 's':
          sort = true;
          break;

          // Threads tp use.
        case 't':
          max_threads = atoi(optarg);
          // Current thread is a thread, so max-1 are available.
          avail_threads = max_threads;
          break;
        }
    }

  int num_goals = 0;
  char *file = argv[optind];
  if (print >= PRINT_BASIC)
    printf ("Processing file %s\n", file);

  // Allocate structures for threads.
  thread_alloc ();

  // Parse the input file. The buffers for each thread structure
  // is allocated here.
  parse_input (file);

  // Begin processing file. Start timer.
  struct timeval timer_start, timer_pre, timer_end;
  double total_time, pre_time, search_time;
  if (print >= PRINT_MIN)
    gettimeofday (&timer_start, NULL);

  // Preprocess the grid to prepare for search. Assigns global value
  // TOTAL_UNKNOWNS.
  preprocess_grid ();
  if (print >= PRINT_MIN)
    gettimeofday (&timer_pre, NULL);

  // NOTE: in defines, mapping tile values to 1000000 - 1000008 assumes that
  // there will NEVER be more than 1 million unknowns.
  if (total_unknowns >= 1000000)
    fprintf (stderr, "Too many unknowns, search not performed.\n");
  else if (total_unknowns > 0)
    {
      // Attempt to find a solution, or multiple solutions.
      // Spin off search thread in thread 0, and wait.
      struct thr_args args = {0, 0, 0};
      pthread_t thread;
      LOCK;
      pthread_create (&thread, NULL, solve_tree_thr, &args);
      pthread_detach (thread);
      pthread_cond_wait (thr_cond, thr_lock);
      UNLOCK;
    }
  else
    {
      // Trivially a goal state.
      if (print >= PRINT_ALL)
        board_print (thr_data[0].bufs.grid);
      goal_states++;
    }

  if (print >= PRINT_BASIC)
    printf ("Number of goal states: %d\n", goal_states);

  // Find elapsed time in us.
  if (print >= PRINT_MIN)
    {
      gettimeofday (&timer_end, NULL);

      pre_time = (timer_pre.tv_sec * 1000.0) + (timer_pre.tv_usec/1000.0)
        - (timer_start.tv_sec * 1000.0) - (timer_start.tv_usec / 1000.0);
      printf ("Preprocess time: %f ms\n", pre_time);

      search_time = (timer_end.tv_sec * 1000.0) + (timer_end.tv_usec/1000.0)
        - (timer_pre.tv_sec * 1000.0) - (timer_pre.tv_usec / 1000.0);
      printf ("Search time: %f ms\n", search_time);


      total_time = (timer_end.tv_sec * 1000.0) + (timer_end.tv_usec/1000.0)
        - (timer_start.tv_sec * 1000.0) - (timer_start.tv_usec / 1000.0);
      printf ("Elapsed time: %f ms\n", total_time);
    }

  // Free thread resources.
  //thread_free ();
}



/*****************************************************************************
 *
 *  Preprocess functions.
 *
 ****************************************************************************/

/* Given a tile, if it is a numbered tile, count the mines and unknowns around
   it. If the count matches the tile number, then all unknowns around must be
   mines. Conversely, if the count matches the mine number, then all unknowns
   must be off.
   SOURCE denotes the index of the unknown tile that this resolution stems
   from. */
static int resolve_tile (int row, int col, int **grid, int source, bool rec)
{

  struct ind ind[8];
  int unknowns = 0;
  int mines = 0;
  int resolved = 0;
  int i, j;

  int tile_num = grid[row][col];
  if (tile_num < 0 || tile_num > 8)
    return 0;

  for (i = -1; i < 2; i++)
    for (j = -1; j < 2; j++)
      {
        // Upon finding an unknown, record it's position.
        if (grid[row+i][col+j] == UNKNOWN)
          {
            ind[unknowns].row = row + i;
            ind[unknowns++].col = col + j;
          }
        else if (is_mine (grid[row+i][col+j]))
          mines++;
      }

  // If TILE_NUM matches the sum of mines and unkowns, all the unknowns are
  // mines. If TILE_NUM matches the mines, then all unknowns are off. Set
  // the unknowns accordingly, and check the tiles around the former
  // unknowns.
  bool turn_on;
  if (tile_num == mines + unknowns)
    {
      turn_on = true;
      int k;
      for (k = 0; k < unknowns; k++)
        {
          if (print >= PRINT_DEBUG)
            fprintf (stderr, "[%d][%d] forced on\n", ind[k].row, ind[k].col);
          grid[ind[k].row][ind[k].col] = force_on (source);
        }
    }
  else if (tile_num == mines)
    {
      turn_on = false;
      int k;
      for (k = 0; k < unknowns; k++)
        {
          if (print >= PRINT_DEBUG)
            fprintf (stderr, "[%d][%d] forced off\n", ind[k].row, ind[k].col);
          grid[ind[k].row][ind[k].col] = force_off (source);
        }
    }
  else
    return 0;

  resolved = unknowns;
  return resolved;

  // Check for recursive mode.
  // TODO: control direction of frontier in recursion, for effcieny (i.e. don't
  // recheck what your parent just checked. */
  if (rec)
    {
      // For each unknown that was resolved, check it's neighbors.
      int k;
      for (k = 0; k < unknowns; k++)
        {
          row = ind[k].row;
          col = ind[k].col;
          for (i = -1; i < 2; i++)
            for (j = -1; j < 2; j++)
              resolved += resolve_tile (row + i, col + j, grid, source, rec);
        }
    }
  return resolved;
}

/* This function attempts to resolve some unknown tiles before we start the
   search. Essentially, this will reduce the depth of the search tree. */
static int preresolve_grid ()
{
  int i, j;
  int resolved = 0;
  int **grid = thr_data[0].bufs.grid;
  for (i = 1; i < nrows - 1; i++)
    for (j = 1; j < ncols - 1; j++)
      resolved += resolve_tile (i, j, grid, -1, false);
  return resolved;
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
            //rintf (stderr, "UNKNOWN at [%d][%d]\n", i, j);
          }
      }
  ind[n].row = -1;
  ind[n].col = -1;
  return n;
}

/* Comparator function to pass to qsort (), when sorting unknown tiles. */
int comp_tiles (const void *arg1, const void *arg2)
{
  struct sort_tile *a = (struct sort_tile *) arg1;
  struct sort_tile *b = (struct sort_tile *) arg2;

  // Compare the NUMBER_TILES field.
  if (a->number_tiles > b->number_tiles)
    return -1;
  else if (a->number_tiles < b->number_tiles)
    return 1;
  else
    return 0;
}

/* Sort the unknown tiles by constraint order. The unknowns with more
   surrounding numbered tiles will come first, and thus be searched first. */
static void sort_unknowns (int **grid, struct ind *ind)
{
  struct sort_tile *unknown_tiles =
    (struct sort_tile *) malloc (total_unknowns * sizeof *unknown_tiles);

  // Count the number of mines around each unknown tile.
  int i;
  for (i = 0; i < total_unknowns; i++)
    {
      unknown_tiles[i].index = ind[i];
      int number_tiles = 0;
      int row = ind[i].row;
      int col = ind[i].col;
      int j, k;
      for (j = -1; j < 2; j++)
        for (k = -1; k < 2; k++)
          {
            int tile_num = grid[row+j][col+k];
            if (0 <= tile_num && tile_num <= 8)
              number_tiles++;
          }
      unknown_tiles[i].number_tiles = number_tiles;
    }

  // Sort the unknown tiles.
  qsort (unknown_tiles, total_unknowns,
         sizeof (struct sort_tile), comp_tiles);

  // Copy sorted results back into IND.
  for (i = 0; i < total_unknowns; i++)
    ind[i] = unknown_tiles[i].index;
}

/* Count the number of current existing mines on the field,
   and adjusts the MINE_TARGET field accordingly.

   This function also calls a few functions that scan through the unknown
   tiles, and these functions can be combined, but that is neglected because
   the small time spend iterating through the grid is negligible compared to
   the exponential time needed to solve the consistency problem. */
static void preprocess_grid ()
{
  int **grid = thr_data[0].bufs.grid;
  struct ind *ind = thr_data[0].bufs.ind;

  // Preprocess grid, if specified.
  int resolved = 0;
  if (preresolve)
    resolved = preresolve_grid ();
  if (print >= PRINT_MIN)
    printf ("Pre-resolved unknowns: %d\n", resolved);
  if (print >= PRINT_BASIC && preresolve)
    board_print (thr_data[0].bufs.grid);

  // If no MINE_TARGET was set, there is no need to adjust it.
  if (mine_target > -1)
    {
      int i, j;
      for (i = 1; i < nrows - 1; i++)
        for (j = 1; j < ncols - 1; j++)
          if (is_mine (grid[i][j]))
            mine_target--;
    }

  // Find all the unknowns.
  total_unknowns = find_unknowns (grid, ind);

  // Sort the unknowns (by surrounding tiles) if specified.
  if (sort)
    sort_unknowns (grid, ind);
}



/*****************************************************************************
 *
 *  Solver functions.
 *
 ****************************************************************************/

/* Threaded function call for solve_tree. */
static void * solve_tree_thr (void *data)
{
  struct thr_args *args = (struct thr_args *) data;
  thread_num = args->thread_num;
  int tmp = args->thread_num;
  struct buffers bufs = thr_data[thread_num].bufs;

  int num_goals = solve_tree (args->unknown_num, args->mine_count, bufs);

  // Critical section.
  // 1) Update goals found.
  // 2) Mark thread as available.
  // 3) Check for end of execution.
  bool signal = false;
  LOCK;
  goal_states += num_goals;
  thr_data[tmp].avail = true;
  avail_threads++;
  if (avail_threads == max_threads || (single && num_goals))
    pthread_cond_signal (thr_cond);
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
  int num_goals = 0;
  int row = bufs.ind[unknown_num].row;
  int col = bufs.ind[unknown_num].col;

  if (mine_src (bufs.grid[row][col]) >= 0)
    {
      // Unknown was pre-assigned. Just move on to next unknown.
      if (is_mine (bufs.grid[row][col]))
        mine_count++;
      if (unknown_num == total_unknowns - 1
          && (mine_target == -1 || mine_count == mine_target))
        {
          num_goals++;
          if (print >= PRINT_ALL)
            board_print (bufs.grid);
        }
      else
        num_goals = solve_tree (unknown_num+1, mine_count, bufs);
    }
  else if (mine_count == mine_target)
    {
      // All mines are used up, just check the MINE_OFF subtree. Do not thread.
      bufs.grid[row][col] = force_off (unknown_num);
      num_goals = solve_subtree (unknown_num, mine_count, bufs, false);
    }
  else if (mine_target == -1 || mine_count < mine_target)
    {
      // Check both subtrees. For the first subtree, instruct it to spin off a
      // thread if a thread is available.

      // Determine which subtree to check first.
      bool mine_on = false;

      // Check the first subtree. Thread if possible.
      if (mine_on)
        {
          if (print >= PRINT_DEBUG)
            fprintf (stderr, "[%d][%d] on\n", row, col);
          bufs.grid[row][col] = force_on (unknown_num);
          mine_count++;
        }
      else
        {
          if (print >= PRINT_DEBUG)
            fprintf (stderr, "[%d][%d] off\n", row, col);
          bufs.grid[row][col] = force_off (unknown_num);
        }
      num_goals = solve_subtree (unknown_num, mine_count, bufs, true);

      // If only a single solution is desired, and it's been found,
      // then we're done.
      if (single && num_goals)
        return num_goals;

      // Check the other subtree. Do not thread.
      mine_on = !mine_on;
      if (mine_on)
        {
          if (print >= PRINT_DEBUG)
            fprintf (stderr, "[%d][%d] on\n", row, col);
          bufs.grid[row][col] = force_on (unknown_num);
          mine_count++;
        }
      else
        {
          if (print >= PRINT_DEBUG)
            fprintf (stderr, "[%d][%d] off\n", row, col);
          bufs.grid[row][col] = force_off (unknown_num);
        }
      clear_unknowns (unknown_num, bufs.ind, bufs.grid);
      num_goals += solve_subtree (unknown_num, mine_count, bufs, false);
    }
  return num_goals;
}

/* */
static int solve_subtree (int unknown_num, int mine_count, struct buffers bufs,
                          bool create_thread)
{
  int num_goals = 0;
  int row = bufs.ind[unknown_num].row;
  int col = bufs.ind[unknown_num].col;

  bool consis = consistency_check (bufs.ind[unknown_num], bufs.grid,
                                   unknown_num);
  if (consis)
    {
      // The mine state is valid. Check for subtrees, or if this branch of
      // the search tree is exhausted.
      if (unknown_num < total_unknowns - 1
          && (mine_target == -1 || mine_count <= mine_target))
        {
          // A subtree exists:
          // 1) Not all unknowns are assigned.
          // 2) If MINE_TARGET is specified, it has not been exceeded.

          // Check if we should create a new thread.
          if (create_thread
              && (mine_target == -1 || mine_count < mine_target)
              && (total_unknowns - unknown_num > 10))
            {
              LOCK;
              if (avail_threads <= 0)
                {
                  UNLOCK;
                  create_thread = false;
                }
              else
                {
                  // Find a thread and claim it.
                  int new_thr = thread_find (0);
                  thr_data[new_thr].avail = false;
                  avail_threads--;
                  UNLOCK;

                  // Copy buffers into new thread.
                  thread_copy (new_thr, thread_num);
                  struct thr_args *args =
                    (struct thr_args *) malloc (sizeof *args);
                  args->thread_num = new_thr;
                  args->unknown_num = unknown_num + 1;
                  args->mine_count = mine_count;
                  pthread_create (&thr_data[new_thr].thread, NULL,
                                  solve_tree_thr, args);
                  pthread_detach (thr_data[new_thr].thread);
                }
            }
          else
            {
              create_thread = false;
            }
          if (!create_thread)
            {
              //fprintf (stderr, "Rec call\n");
              num_goals = solve_tree (unknown_num + 1, mine_count, bufs);
            }
        }
      else if (unknown_num == total_unknowns - 1
               && (mine_target == -1 || mine_count == mine_target))
        {
          // Solution has been found:
          // 1) All unknowns have been assigned a valid state.
          // 2) MINE_TARGET, if specified, has been matched.
          num_goals++;
          if (print >= PRINT_ALL)
            board_print (bufs.grid);
        }
      else
        {
          // The remaining case is that all unknown tiles have been assigned,
          // and MINE_TARGET was specified but not reached. Here, there is
          // nothing to be done.
        }
    }
  else
    {
      //fprintf (stderr, "[%d][%d] consis failed\n", row, col);
      //board_print (bufs.grid);

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
static bool consistency_check (struct ind ind, int **grid, int unknown_num)
{
  int row = ind.row;
  int col = ind.col;
  int i;
  int j;

  //fprintf (stderr, "Checking [%d][%d]\n", row, col);
  for (i = -1; i < 2; i++)
    for (j = -1; j < 2; j++)
      {
        // For each numbered tile around mine:
        int tile_num = grid[row+i][col+j];
        if (0 <= tile_num && tile_num <= 8)
          {
            int local_mines = 0;
            int local_unknowns = 0;
            int k;
            int l;

            // Count number of mines and unknowns around tile.
            for (k = -1; k < 2; k++)
              for (l = -1; l < 2; l++)
                {
                  if (is_mine (grid[row+i+k][col+j+l]))
                    local_mines++;
                  else if (grid[row+i+k][col+j+l] == UNKNOWN)
                    local_unknowns++;
                }

            //fprintf (stderr, "  grid[%d][%d]: %d:  mines %d  unknowns %d\n",
            //         row + i, col + j, tile_num, local_mines, local_unknowns);

            // Perform the consistency check.
            if (tile_num < local_mines
                || tile_num > local_mines + local_unknowns)
              {
                //fprintf (stderr, "[%d+%d][%d+%d] inconsis: tile %d  mines %d  unknowns %d\n",
                //         row, i, col, j, tile_num, local_mines, local_unknowns);
                board_print (grid);
                return false;
              }

            // If specified, see if we can force neighboring mine states.
            //if (force && (i == 1 || (i == 0 && j == 1)))
            //  {
            //    resolve_tile (row+i, col+j, grid, unknown_num, false);
            //  }
          }
      }

  if (force)
    {
      resolve_tile (row, col + 1, grid, unknown_num, false);
      for (j = -1; j < 2; j++)
        resolve_tile (row + 1, col + j, grid, unknown_num, false);
    }

  return true;
}

/* Set mines to UNKNOWN from the one after index I. However, if a mine is
   forced by an unknown indexed earlier than I, don't touch it. */
static inline void clear_unknowns (int i, struct ind *ind, int **grid)
{
  int init = i++;
  if (!force)
    {
      while (i < total_unknowns)
        grid[ind[i].row][ind[i++].col] = UNKNOWN;
    }
  else
    {
      while (i < total_unknowns)
        {
          int src = mine_src (grid[ind[i].row][ind[i].col]);
          if (src >= init)
            {
              grid[ind[i].row][ind[i].col] = UNKNOWN;
            }
          i++;
        }
    }
}


/*****************************************************************************
 *
 *  Thread control functions.
 *
 ****************************************************************************/

/* Allocate thread memory. Called at the start of the program. */
static void thread_alloc ()
{
  thr_lock = (pthread_mutex_t *) malloc (sizeof *thr_lock);
  thr_cond = (pthread_cond_t *) malloc (sizeof *thr_cond);
  pthread_mutex_init (thr_lock, NULL);
  pthread_cond_init (thr_cond, NULL);
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

/* Find a free thread. Assumes there is a thread available. */
static int thread_find ()
{
  int i;
  for (i = 0; i < max_threads; i++)
    if (thr_data[i].avail)
      return i;

  // If we're still here, no threads are available despite
  // avail_threads being positive.
  fprintf (stderr, "Error: Thread control corruption.\n");
  exit (1);
}

/* At the end of the program, free thread memory. */
static void thread_free ()
{
  pthread_mutex_destroy (thr_lock, NULL);
  pthread_cond_destroy (thr_cond, NULL);
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
}


/*****************************************************************************
 *
 *  Miscellaneous functions.
 *
 ****************************************************************************/

/* When an unknown is forced on/off, use this function to set the value.
   Later, we can determine which unknown forced this unknown to turn on/off. */
static inline int force_on (int unknown_num)
{
  return unknown_num + MINE_ON + 1;
}
static inline int force_off (int unknown_num)
{
  return -force_on (unknown_num);
}

/* Check if a mapped tile value denotes if the mine is on. False is returned
   otherwise. This function is not meant to be used when the tile being checked
   is not a mine. */
static inline bool is_mine (int tile_val)
{
  return (tile_val >= MINE_ON);
}

/* Returns the index of the unknown tile that forced this mine to be on or
   off. -1 is returned if TILE_VAL denotes an unknown or a number, or if this
   mine was forced to be on/off from the start. */
static inline int mine_src (int tile_val)
{
  int n = abs (tile_val);
  if (n <= MINE_ON)
    return -1;
  else
    return (n - MINE_ON - 1);
}

/* Print the game board. */
static void board_print (int **grid)
{
  int i, j;
  for (i = 1; i < nrows - 1; i++)
    {
      for (j = 1; j < ncols - 1; j++)
        {
          char c;
          if (0 <= grid[i][j] && grid[i][j] < UNKNOWN)
            c = TILE_UNMAP(grid[i][j]);
          else if (grid[i][j] == UNKNOWN)
            c = UNKNOWN_CHAR;
          else if (is_mine (grid[i][j]))
            c = MINE_ON_CHAR;
          else
            c = MINE_OFF_CHAR;
          printf ("%c", c);
        }

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
  if (print >= PRINT_BASIC)
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

  // Fill in original grid.
  int i, j;
  int **grid = thr_data[0].bufs.grid;
  for (i = 1; i < nrows - 1; i++)
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
        {
          int grid_val;
          switch (inbuf[j])
            {
            case UNKNOWN_CHAR:
              grid_val = UNKNOWN;
              break;
            case MINE_ON_CHAR:
              grid_val = force_on (-1);
              break;
            case MINE_OFF_CHAR:
              grid_val = force_off (-1);
              break;
            default:
              grid_val = TILE_MAP(inbuf[j]);
              break;
            }

          grid[i][j+1] = grid_val;
        }
    }

  // Set outside boundary to off. For efficiency, loops are not combined.
  for (i = 0; i < ncols; i++)
    grid[0][i] = MINE_OFF;
  for (i = 0; i < ncols; i++)
    grid[nrows-1][i] = MINE_OFF;
  for (i = 1; i < nrows - 1; i++)
    {
      grid[i][0] = MINE_OFF;
      grid[i][ncols-1] = MINE_OFF;
    }

  if (print >= PRINT_BASIC)
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
  -a                Find all solutions.\n\
  -f                During search, whenever possible, force the state of an\n\
                    unknown to be on or off. This effectively reduces the\n\
                    depth of the search.\n\
  -h                Print this help message.\n\
  -m MINE_TARGET    Set a target number of mines.\n\
  -p PRINT          Print solutions.\n\
                      0    Print nothing.\n\
                      1    Print time elapsed.\n\
                      2    Print basic information.\n\
                      3    Print found solutions.\n\
  -r                Pre-resolve unknowns.\n\
  -s                Sort unknowns before searching. Unknowns are sorted in\n\
                    increasing order by the number of surrounding numbered\n\
                    tiles they have.\n\
  -t THREADS        Number of threads to use.\n\n");
}
