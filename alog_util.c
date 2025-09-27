/* alog_util.c - This file contains support routines for the alog routine  */

#include <stdint.h>
#include "alog.h"
#include "endian.h"

int is_little_endian (void);
uint32_t swap_uint32 (uint32_t val);

void
to_big_endian (struct bl_head *h)
{
  if (is_little_endian ())
    {
      h->magic = swap_uint32 (h->magic);
      h->top = swap_uint32 (h->top);
      h->current = swap_uint32 (h->current);
      h->bottom = swap_uint32 (h->bottom);
      h->size = swap_uint32 (h->size);
    }
}

/*
 *-------------------------------------------------------
 * output_log() - Routine to output file in correct order
 *-------------------------------------------------------
 */

void
output_log (char *log_file_name)
{
  int i;
  struct bl_head lp;
  FILE *fin;

  if ((fin = fopen (log_file_name, "r")) != NULL)
    {
      if (fread (&lp, sizeof (struct bl_head), 1, fin) != 1)
        {
          fprintf (stderr, "alog: Error reading log file header from %s.\n",
                   log_file_name);
          exit (2);
        }
      to_big_endian (&lp);
      if (lp.magic != ALOG_MAGIC)
        {
          fprintf (stderr, "alog: %s is not an alog file.\n", log_file_name);
          exit (2);
        }
      if (fseek (fin, lp.current, 0) != 0)
        {
          fprintf (stderr, "alog: Error seeking in log file %s.\n",
                   log_file_name);
          exit (2);
        }
    }
  else
    {
      fprintf (stderr, "alog: Could not open file, %s.\n", log_file_name);
      exit (2);
    }

  /* Output from current to bottom */
  if (lp.current != lp.bottom)
    {
      for (i = lp.current; i < lp.bottom; i++)
        {
          int c = fgetc (fin);
          if (c == EOF)
            {
              if (ferror (fin))
                {
                  fprintf (stderr, "alog: Error reading from log file %s.\n",
                           log_file_name);
                }
              break;
            }
          putchar (c);
        }
    }

  /* Output from top to current     */
  if (fseek (fin, lp.top, 0) != 0)
    {
      fprintf (stderr, "alog: Error seeking in log file %s.\n", log_file_name);
      exit (2);
    }
  for (i = lp.top; i < lp.current; i++)
    {
      int c = fgetc (fin);
      if (c == EOF)
        {
          if (ferror (fin))
            {
              fprintf (stderr, "alog: Error reading from log file %s.\n",
                       log_file_name);
            }
          break;
        }
      putchar (c);
    }

  fclose (fin);
} /* end output_log */

/*
 *-------------------------------------------------------
 * syntax() - Display the syntax message
 *-------------------------------------------------------
 */

void
syntax (void)
{
  fprintf (stderr, "Usage:\n\t\
alog -f File [-o] | [ [-s Size] [-q] ]\n\t\
alog -t LogType [-f File] [-o] | [ [-s Size] [-q] ]\n\t\
alog -t LogType -V\n\t\
alog -C -t LogType { [-f File] [-s Size] [-w Verbosity] }\n\t\
alog -L [-t LogType]\n\t\
alog -H\n");
  exit (1);
} /* end syntax */
