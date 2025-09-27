/* alog - Control for a log file */

#define _ILS_MACROS

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <locale.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statfs.h>
#include <sys/types.h>
#include <unistd.h>

#undef HAS_INCLUDE
#if defined __has_include
# define HAS_INCLUDE(inc) __has_include(inc)
#else
# define HAS_INCLUDE(inc) 0
#endif

#if HAS_INCLUDE(<getopt.h>)
# include <getopt.h>
#endif

#define DEF_SIZE 4096 /* log size Define */
#define ALOG_MAGIC 0xf9f3f9f4 /* magic number for alog files */

static int result = 0; /* holds global exit value */

struct bl_head /* log file header */
{
  int magic;   /* magic number */
  int top;     /* top of log */
  int current; /* current position in log */
  int bottom;  /* bottom of log */
  int size;    /* size of log */
};

static int
is_little_endian (void)
{
  uint16_t i = 1;
  return *(char *)&i;
}

static uint32_t
swap_uint32 (uint32_t val)
{
  val = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0xFF00FF);
  return (val << 16) | (val >> 16);
}

static void
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

static void
syntax (void)
{
  fprintf (stderr, "Usage:\n\talog -f File [-o] | [ [-s Size] [-q] ]\n\talog -H\n");
  exit (1);
} /* end syntax */

static void
set_result (int new_result)
{ /* sets the global result code if not already set. */
  if (result)
    return;
  result = new_result;
}

static void
output_log (char *log_file_name)
{ /* routine to output file in correct order */
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

  /* Output from top to current */
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

int
main (int argc, char *argv[])
{
  FILE *fout, *fcon;
  struct statfs statbuf;
  char *fnull;
  int i, j;            /* Temp vars */
  int op;              /* option return from getopt */
  int free_bytes;      /* free bytes in filesystem */

  /**************** Default Values/Definitions */
  char *log_file_name; /* Pointer to Log file name */
  int log_size = 0;    /* Current size of log */
  int log_shrink = 0;  /* We are shrinking the log size */

  /***************** Buffers & storage ptrs */
  char tname[128];     /* Temp file name for shrink */
  char cmd[256];       /* Used to build command */
  char inbuf[BUFSIZ];  /* Input buffer */

  /**************** Flags & control vars */
  bool o_flag = false; /* Output log flag */
  bool q_flag = false; /* Quiet logging flag */
  bool s_flag = false; /* Change size flag */

  struct bl_head lp;   /* Setup control structure */
  int bytes_inbuf;     /* bytes in from buffer */
  int systemrc = 0;

  (void)setlocale (LC_ALL, ""); /* get locale env values */

  /* Set all control vals to zero */
  log_file_name = NULL;
  opterr = 0;

  /* Get command line options */
  while ((op = getopt (argc, argv, "f:oqs:H")) != EOF)
    {
      switch (op)
        {
        case 'f': /* user specified log name */
          log_file_name = malloc (strlen (optarg) + 1);
          if (log_file_name == NULL)
            {
              fprintf (stderr, "alog: Out of memory.\n");
              exit (1);
            }
          strcpy (log_file_name, optarg);
          break;

        case 'o': /* output log to screen */
          o_flag = true;
          break;

        case 'q': /* quiet logging - no cons output */
          q_flag = true;
          break;

        case 's': /* user specified log size */
          {
            char *endptr;
            long size_arg;

            errno = 0;
            size_arg = strtol (optarg, &endptr, 10);

            if (errno == ERANGE)
              {
                fprintf (stderr, "alog: Out of range error for size '%s'.\n", optarg);
                set_result (1);
                break;
              }

            if (endptr == optarg || *endptr != '\0')
              {
                fprintf (stderr, "alog: Invalid number for size '%s'.\n", optarg);
                set_result (1);
                break;
              }

            if (size_arg < INT_MIN || size_arg > INT_MAX)
              {
                fprintf (stderr, "alog: Limits exceeded for size '%s'.\n", optarg);
                set_result (1);
                break;
              }

            if (size_arg > 2147483647) //-V547
              {
                log_size = 2147483647;
              }
            else
              {
                log_size = (int)size_arg;
              }

            if (log_size <= 0)
              {
                set_result (1); /* invalid size specified */
              }
            else
              {
                s_flag = true;
              }
          }
          break;

        case '?': /* put syntax/usage msg */
          set_result (1);
          break;

        case 'H': /* help */
          syntax ();
          break;

        default:          /* set syntax return code */
          set_result (1); /* for future use */
          break;
        } /* end case */
    } /* end while */

  /* If there are syntax errors, put out syntax msg in the following cases: */
  /* - the L_flag or o_flag flag is set */
  /* Only put out the syntax message in these cases (and the cases already */
  /* handled above) so that when alog is being used as a pipe, alog won't */
  /* interupt the operation of the command that is calling it. */
  if (result == 1)
    syntax ();

  /* VALIDATE - Make all fields have an appropriate value set values in */
  /* case of failure to insure that alog always runs to completion. */
  fnull = "/dev/null"; /* set /dev/null filename var */

  if (log_file_name == NULL) /* user did not specify log name */
    {
      log_file_name = fnull; /* set log_file_name to /dev/null */
      set_result (2);        /* had a problem with log name */
    }

  if (log_size < DEF_SIZE)
    log_size = DEF_SIZE; /* for -s flag usage */

  /* Check to see if output mode is selected */
  if (o_flag)
    {
      /* make sure we have a log file name */
      if (strcmp (fnull, log_file_name) == 0)
        {
          fprintf (stderr, "alog: Invalid file name.\n");
          exit (2);
        }
      else
        {
          output_log (log_file_name);
          exit (0);
        }
    }

  /* Setup file pointers */

  /* If the quiet logging flag was specified ... */
  /* try to open /dev/null to use as the "console" */
  if (q_flag)
    {
      fcon = fopen (fnull, "a");
    }
  else /* normal logging, so assign stdout to fcon, the "console" */
    fcon = stdout;

  if (fcon == NULL)
    /* Could not open /dev/null up above */
    exit (2);

  /* Open the log file */
  if ((fout = fopen (log_file_name, "r+")) != NULL)
    {
      /* Read header from file */
      fread (&lp, sizeof (struct bl_head), 1, fout);
      to_big_endian (&lp);
      /* See if magic number is the right value */
      if (lp.magic != ALOG_MAGIC)
        { /* the header is not correct */
          fclose (fout);
          if ((fout = fopen (fnull, "w")) == NULL) /* log to dev/null */
            /* Could not open /dev/null */
            exit (2);
          /* build a header to use */
          lp.magic = ALOG_MAGIC;
          lp.top = 0;
          lp.current = 0;
          lp.bottom = 0;
          lp.size = 4096; /* Let's not do wrap too often */
          set_result (2); /* return error because file was not an alog file */
        }
    }
  else
    {

      /* set up the header */
      lp.magic = ALOG_MAGIC;            /* Create the magic number */
      lp.top = sizeof (struct bl_head); /* point to 1st char */
      lp.current = lp.top;
      lp.bottom = lp.current;
      j = log_size;

      if ((fout = fopen (log_file_name, "w")) == NULL)
        {
          /* Could not open log file, so log to /dev/null */
          if ((fout = fopen (fnull, "w")) == NULL)
            /* Could not open /dev/null */
            exit (2);
          set_result (2); /* set result for later return code */
        } /* end if */

      /* Make the log size a multiple of the default size */
      log_size = ((j / DEF_SIZE) + ((j % DEF_SIZE != 0) * 1)) * DEF_SIZE;
      if (statfs (log_file_name, &statbuf) == 0)
        {
          free_bytes = (((statbuf.f_bfree * 4) * 1000));
          if ((log_size > free_bytes) && (free_bytes > DEF_SIZE))
            {
              log_size = DEF_SIZE;
              set_result (2);
            }
        }
      lp.size = log_size;

      /* Initialize the header and fill the log with zeroes */
      fseek (fout, 0, 0);
      to_big_endian (&lp);
      fwrite (&lp, sizeof (struct bl_head), 1, fout);
      to_big_endian (&lp);
      fseek (fout, lp.top, 0);
      for (i = 0; i < (lp.size - sizeof (struct bl_head));
           i++) /* Fill the log with zeros */
        {
          fputc ('0', fout);
        }
      s_flag = false; /* Turn off s_flag flag */
    }

  /* check size to see if a new size has been specified as input parameter */

  /* if we're logging then see what size to use */
  if (s_flag)
    {
      fclose (fout);
      fout = NULL;
      if (lp.size < log_size) /* Increase size of log */
        {
          if ((fout = fopen (log_file_name, "a")) == NULL)
            ; /* Could not change size of log */
          else
            {
              j = log_size;
              /* Make the log size a multiple of the default size */
              log_size
                  = ((j / DEF_SIZE) + ((j % DEF_SIZE != 0) * 1)) * DEF_SIZE;
              if (statfs (log_file_name, &statbuf) == 0)
                {
                  free_bytes = (((statbuf.f_bfree * 4) * 1000));
                  if ((log_size - lp.size) > free_bytes)
                    {
                      log_size = lp.size;
                      set_result (2);
                    }
                }
              j = log_size - lp.size;
              /* Extend the log with zeros */
              for (i = 0; i < j; i++)
                fputc (0, fout);
              lp.size = log_size; /* put new size in log header */
            }
        }

      if (lp.size > log_size) /* Shrink the log file */
        {
          log_shrink = 1;
          j = log_size;
          /* Make the log size a multiple of the default size */
          log_size = ((j / DEF_SIZE) + ((j % DEF_SIZE != 0) * 1)) * DEF_SIZE;

          /* create temp file name */
          sprintf (tname, "/var/tmp/alogtmp%d", getpid ());

          /* create call to alog to change the size of the log using */
          /* the log name, temp file, and new log size */
          sprintf (cmd, "alog -f %s -o | alog -f %s -s %d -q", log_file_name,
                   tname, log_size);
          system (cmd);

          /* Move the temporary file to the log name */
          sprintf (cmd, "mv -f %s %s\n", tname, log_file_name);
          systemrc = system (cmd);

          if (systemrc != 0) /* the original log will be used */
            {
              remove (tname); /* remove temporary file */
              set_result (2);
            }
        }

      /* ready */
      if ((fout = fopen (log_file_name, "r+")) == NULL)
        {
          /* Could not open (resized) log file, so log to /dev/null */
          if ((fout = fopen (fnull, "r+")) == NULL)
            /* Could not open /dev/null */
            exit (-1);
          set_result (2); /* set result for later return code */
        }

      /* Only write header if we are increasing the log. If shrinking, the */
      /* log pointers have already been updated by the call to alog. */
      if (!log_shrink)
        {
          fseek (fout, 0, 0);
          to_big_endian (&lp);
          fwrite (&lp, sizeof (struct bl_head), 1, fout);
          to_big_endian (&lp);
        }
      else
        {
          fseek (fout, 0, 0);
          fread (&lp, sizeof (struct bl_head), 1, fout);
        }
    }

  /* 1. Ignore the kill signal while reading/writing
   * 2. Read from stdin and write to fcon and fout
   * 3. When we reach the end of the log (lp.size), go back to the top
   *    and set the bottom pointer to the size of the log. */

  /* ignore the kill signal because we're starting to write to the log */
  signal (SIGINT, SIG_IGN);
  fseek (fout, lp.current, 0); /* goto starting point in log */

  j = 0;

  /* Get input from stdin */
  while ((bytes_inbuf = read (0, inbuf, BUFSIZ)) > 0)
    {
      for (i = 0; i < bytes_inbuf; i++) /* Output to file(s) */
        {
          /* check to see if its time to start writing the top of log again. */
          if (lp.current < lp.size)
            {
              putc (inbuf[i], fout);
              putc (inbuf[i], fcon);
              j++;
              lp.current++;
            }
          else /* wrap logic active when we reach the size of the log */
            { /* if we are logging to a file */
              fseek (fout, lp.top, 0); /* position pointer to top of log */
              lp.current = lp.top;     /* position current to top of log */
              putc (inbuf[i], fout);
              putc (inbuf[i], fcon);
              j++;
              lp.current++;
              lp.bottom = lp.size; /* set bottom to size (end of log) */
            }
        }
    }

  if (lp.current > lp.bottom) /* adjust bottom of log */
    lp.bottom = lp.current;

  /* Update the header */
  fseek (fout, 0, 0);
  to_big_endian (&lp);
  fwrite (&lp, sizeof (struct bl_head), 1, fout);
  to_big_endian (&lp);

  /* All done let's sync & close */
  fclose (fout);
  if (fcon != stdout)
    fclose (fcon);
  exit (result); /* return value of previous problems */
}
