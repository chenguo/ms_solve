/* Minesweeper Consistency Problem solver.
   Written by Chen Guo, UCLA CS 261A project spring 2011.
*/

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Function prototypes. */
static void help ();
static void parse_input ();

/* Defines. */
#define INBUF_SIZ 1024
#define MAX_TILES 1000000

/* Globals. */
static char *tiles;
static int dim_x;
static int dim_y;

int main (int argc, char **argv)
{
  char c;
  bool brute = false;

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
      char *file = argv[optind++];
      printf ("Processing file %s\n", file);
      parse_input (file);
    }
}

static void board_print ()
{
  int i, j;
  char *buf_ptr = tiles;

  printf ("+");
  for (j = 0; j < dim_x; j++)
    printf ("---+");

  for (i = 0; i < dim_y; i++)
    {
      printf ("\n|");
      for (j = 0; j < dim_x; j++)
        printf (" %c |", *buf_ptr++);
      printf ("\n+");
      for (j = 0; j < dim_x; j++)
        printf ("---+");
    }
  printf ("\n");
}

static void parse_input (char *file)
{
  FILE *fh = fopen (file, "r");
  char inbuf[INBUF_SIZ];
  char *buf_ptr;

  // First line: dimensions.
  dim_x = 0;
  dim_y = 0;
  fgets (inbuf, INBUF_SIZ, fh);
  if (sscanf (inbuf, "%d%*[x ]%d", &dim_x, &dim_y) != 2)
    {
      fprintf (stderr, "Invalid dimensions format. ");
      fprintf (stderr, "Please use \"W x H\" format\n");
      return;
    }
  printf ("Dimensions: %d, %d\n", dim_x, dim_y);
  {
    int ntiles = dim_x * dim_y;
    if (ntiles <= 0 || ntiles > MAX_TILES)
      {
        fprintf (stderr, "Invalid dimensions.\n");
        return;
      }
    tiles = malloc (ntiles);
  }

  buf_ptr = tiles;
  while (!feof (fh) && !ferror (fh))
    {
      fgets (inbuf, INBUF_SIZ, fh);
      memcpy (buf_ptr, inbuf, dim_x);
      buf_ptr += dim_x;
    }
  board_print ();
}

static void help ()
{
  printf ("\
Minesweeper solver. Written by Chen Guo.\n\
Options:\n\
  -b  Brute force algorithm (default).\n\
  -h  Display help information.\n\
  -o  Optimized algorithm.\n");
}
