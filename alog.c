/* alog */

/*****************************************************************************/

#if defined (_AIX)
# if !defined _ALL_SOURCE
#  define _ALL_SOURCE
# endif
#endif

/*****************************************************************************/

#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

/*****************************************************************************/

#define DEF_SIZE   4096       /* log size default */
#define ALOG_MAGIC 0xf9f3f9f4 /* magic number for alog files */

/*****************************************************************************/

static int result = 0; /* global exit value */

/*****************************************************************************/

struct bl_head /* log file header */
{
  int magic;   /* magic number */
  int top;     /* top of log */
  int current; /* current position in log */
  int bottom;  /* bottom of log */
  int size;    /* size of log */
};

/*****************************************************************************/

static int
is_little_endian (void)
{
  uint16_t i = 1;

  return *(char *)& i;
}

/*****************************************************************************/

static uint32_t
swap_uint32 (uint32_t val)
{
  val = ((val << 8) & 0xFF00FF00) |
        ((val >> 8) & 0xFF00FF);

  return (val << 16) | (val >> 16);
}

/*****************************************************************************/

static void
to_big_endian (struct bl_head * h)
{ /* cppcheck-suppress knownConditionTrueFalse */
  if (is_little_endian ())
    {
      h -> magic   = (int)swap_uint32 ((uint32_t)h -> magic);
      h -> top     = (int)swap_uint32 ((uint32_t)h -> top);
      h -> current = (int)swap_uint32 ((uint32_t)h -> current);
      h -> bottom  = (int)swap_uint32 ((uint32_t)h -> bottom);
      h -> size    = (int)swap_uint32 ((uint32_t)h -> size);
    }
}

/*****************************************************************************/

static void
syntax (int ret)
{
  (void)fprintf (stderr,
                 "Usage:\n"
                 "\talog [ -f File [ -s Size ] ] [ -o ] [ -q ]\n"
                 "\talog -H\n");
  exit (ret);
}

/*****************************************************************************/

static void
set_result (int new_result)
{ /* sets the global result code if not already set. */
  if (result)
    return;

  result = new_result;
}

/*****************************************************************************/

static void
output_log (const char * log_file_name)
{ /* routine to output file in correct order */
  int i;
  struct bl_head lp;
  FILE * fin;

  if ((fin = fopen (log_file_name, "r")) != NULL)
    {
      if (fread (& lp, sizeof (struct bl_head), 1, fin) != 1)
        {
          (void)fprintf (stderr,
                         "alog: Error reading log file header from %s.\n",
                         log_file_name);

          exit (2);
        }

      to_big_endian (& lp);

      if ((uint32_t)lp.magic != ALOG_MAGIC)
        {
          (void)fprintf (stderr, "alog: %s is not an alog file.\n",
                         log_file_name);

          exit (2);
        }

      if (fseek (fin, lp.current, 0) != 0)
        {
          (void)fprintf (stderr, "alog: Error seeking in log file %s.\n",
                         log_file_name);

          exit (2);
        }
    }
  else
    {
      (void)fprintf (stderr, "alog: Could not open file, %s.\n",
                     log_file_name);

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
                (void)fprintf (stderr,
                               "alog: Error reading from log file %s.\n",
                               log_file_name);

              break;
            }

          (void)putchar (c);
        }
    }

  /* Output from top to current */
  if (fseek (fin, lp.top, 0) != 0)
    {
      (void)fprintf (stderr, "alog: Error seeking in log file %s.\n",
                     log_file_name);

      exit (2);
    }

  for (i = lp.top; i < lp.current; i++)
    {
      int c = fgetc (fin);

      if (c == EOF)
        {
          if (ferror (fin))
            (void)fprintf (stderr,
                           "alog: Error reading from log file %s.\n",
                           log_file_name);

          break;
        }

      (void)putchar (c);
    }

  (void)fclose (fin);
}

/*****************************************************************************/

static int
round_log_size (int size)
{ /* round up to a whole multiple of the default log size */
  return ((size / DEF_SIZE) + ((size % DEF_SIZE != 0) ? 1 : 0)) * DEF_SIZE;
}

/*****************************************************************************/

static long long
fs_free_bytes (const char * path)
{ /* free bytes on the filesystem holding path, or -1 if it can't be told */
  struct statvfs sb;
  unsigned long bs;

  if (statvfs (path, & sb) != 0)
    return -1;

  bs = (sb.f_frsize ? sb.f_frsize : sb.f_bsize);

  return (long long)sb.f_bfree * (long long)bs;
}

/*****************************************************************************/

static int
shrink_log (char * log_file_name, struct bl_head * lp, int new_size)
{
  struct bl_head nlp, disk;
  struct stat st;
  FILE * fout;
  char * data = NULL;
  char * tname;
  long datalen;
  long i;
  int hdr = (int)sizeof (struct bl_head);
  int have_mode = 0;

  if (lp -> top < hdr               ||
      lp -> current < lp -> top     ||
      lp -> bottom  < lp -> current ||
      lp -> bottom  > lp -> size)
    return -1;

  datalen = (long)lp -> bottom - (long)lp -> top; /* total bytes of live data */

  if (datalen > 0)
    {
      FILE * fin;

      long seg1 = (long)lp -> bottom  - (long)lp -> current;
      long seg2 = (long)lp -> current - (long)lp -> top;

      if ((data = (char *)calloc ((size_t)datalen, 1)) == NULL)
        return -1;

      if ((fin = fopen (log_file_name, "r")) == NULL)
        {
          free (data);

          return -1;
        }

      if ((seg1 > 0
           && (fseek (fin, lp -> current, SEEK_SET) != 0
               || fread (data, 1, (size_t)seg1, fin) != (size_t)seg1))
          || (seg2 > 0
              && (fseek (fin, lp -> top, SEEK_SET) != 0
                  || fread (data + seg1, 1, (size_t)seg2, fin)
                       != (size_t)seg2)))
        {
          (void)fclose (fin);
          free (data);

          return -1;
        }

      (void)fclose (fin);
    }

  nlp.magic   = (int)ALOG_MAGIC;
  nlp.top     = hdr;
  nlp.current = hdr;
  nlp.bottom  = hdr;
  nlp.size    = new_size;

  if ((tname = (char *)malloc (strlen (log_file_name) + 32)) == NULL)
    {
      free (data);

      return -1;
    }

  (void)sprintf (tname, "%s.alogtmp.%ld", log_file_name, (long)getpid ());

  if ((fout = fopen (tname, "w+")) == NULL)
    {
      free (data);
      free (tname);

      return -1;
    }

  disk = nlp;
  to_big_endian (& disk);
  (void)fwrite (& disk, sizeof (struct bl_head), 1, fout);
  (void)fseek (fout, nlp.top, SEEK_SET);

  for (i = 0; i < (long)new_size - hdr; i++)
    (void)fputc ('0', fout);

  (void)fseek (fout, nlp.current, SEEK_SET);

  for (i = 0; data != NULL && i < datalen; i++)
    {
      if (nlp.current >= nlp.size) /* wrap back to the top of the log */
        {
          (void)fseek (fout, nlp.top, SEEK_SET);
          nlp.current = nlp.top;
          nlp.bottom  = nlp.size;
        }

      (void)putc (data [i], fout);
      nlp.current++;
    }

  if (nlp.current > nlp.bottom)
    nlp.bottom = nlp.current;

  (void)fseek (fout, 0, SEEK_SET);
  disk = nlp;
  to_big_endian (& disk);
  (void)fwrite (& disk, sizeof (struct bl_head), 1, fout);

  if (fclose (fout) != 0)
    {
      (void)remove (tname);
      free (data);
      free (tname);

      return -1;
    }

  if (stat (log_file_name, & st) == 0)
    have_mode = 1;

  if (have_mode)
    (void)chmod (tname, st.st_mode & 07777);

  if (rename (tname, log_file_name) != 0)
    {
      (void)remove (tname);
      free (data);
      free (tname);

      return -1;
    }

  * lp = nlp;
  free (data);
  free (tname);

  return 0;
}

/*****************************************************************************/

int
main (int argc, char * argv [])
{
  FILE * fout, * fcon;
  char * fnull;
  int i, j;             /* Temp vars */
  int op;               /* option return from getopt */

  /**************** Default Values/Definitions */
  char * log_file_name; /* Pointer to Log file name */
  int log_size = 0;     /* Current size of log */

  /***************** Buffers & storage ptrs */
  char inbuf [BUFSIZ];  /* Input buffer */

  /**************** Flags & control vars */
  bool o_flag = false;  /* Output log flag */
  bool q_flag = false;  /* Quiet logging flag */
  bool s_flag = false;  /* Change size flag */

  struct bl_head lp;    /* Setup control structure */
  ssize_t bytes_inbuf;  /* bytes in from buffer */

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
          log_file_name = (char *)malloc (strlen (optarg) + 1);

          if (log_file_name == NULL)
            {
              (void)fprintf (stderr, "alog: Out of memory.\n");

              exit (1);
            }

          (void)strcpy (log_file_name, optarg);

          break;

        case 'o': /* output log to screen */
          o_flag = true;

          break;

        case 'q': /* quiet logging - no cons output */
          q_flag = true;

          break;

        case 's': /* user specified log size */
          {
            char * endptr;
            long size_arg;

            errno = 0;
            size_arg = strtol (optarg, & endptr, 10);

            if (errno == ERANGE)
              {
                (void)fprintf (stderr,
                               "alog: Out of range error for size '%s'.\n",
                               optarg);
                set_result (1);

                break;
              }

            if (endptr == optarg || * endptr != '\0')
              {
                (void)fprintf (stderr,
                               "alog: Invalid number for size '%s'.\n",
                               optarg);
                set_result (1);

                break;
              }

            if (size_arg < INT_MIN || size_arg > INT_MAX)
              {
                (void)fprintf (stderr,
                               "alog: Limits exceeded for size '%s'.\n",
                               optarg);
                set_result (1);

                break;
              }

            log_size = (int)size_arg;

            if (log_size <= 0)
              set_result (1); /* invalid size specified */
            else
              s_flag = true;
          }

          break;

        case '?': /* put syntax/usage msg */
          set_result (1);

          break;

        case 'H': /* help */
          syntax (0);

          break;

        default:          /* set syntax return code */
          set_result (1); /* for future use */

          break;
        }
    }

  /* If there are syntax errors, put out syntax msg in the following cases: */
  /* - the L_flag or o_flag flag is set */

  /* Only put out the syntax message in these cases (and the cases already */
  /* handled above) so that when alog is being used as a pipe, alog won't */
  /* interupt the operation of the command that is calling it. */

  if (result == 1)
    syntax (1);

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
    { /* make sure we have a log file name */
      if (strcmp (fnull, log_file_name) == 0)
        {
          (void)fprintf (stderr, "alog: Invalid file name.\n");

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
    fcon = fopen (fnull, "a");
  else /* normal logging, so assign stdout to fcon, the "console" */
    fcon = stdout;

  if (fcon == NULL) /* Could not open /dev/null up above */
    exit (2);

  /* Open the log file */
  if ((fout = fopen (log_file_name, "r+")) != NULL)
    { /* Read header from file */
      if (fread (& lp, sizeof (struct bl_head), 1, fout) != 1)
        lp.magic = 0; /* short/unreadable header: not an alog file */

      to_big_endian (& lp);

      /* See if magic number is the right value */
      if ((uint32_t)lp.magic != ALOG_MAGIC)
        { /* the header is not correct */
          (void)fclose (fout);

          if ((fout = fopen (fnull, "w")) == NULL) /* log to dev/null */
            exit (2); /* Could not open /dev/null */

          /* build a header to use */
          lp.magic   = (int)ALOG_MAGIC;
          lp.top     = 0;
          lp.current = 0;
          lp.bottom  = 0;
          lp.size    = 4096; /* Let's not do wrap too often */
          set_result (2);    /* return error; not an alog file */
        }
    }
  else
    { /* set up the header */
      lp.magic   = (int)ALOG_MAGIC;         /* Create the magic number */
      lp.top     = sizeof (struct bl_head); /* point to 1st char */
      lp.current = lp.top;
      lp.bottom  = lp.current;
      j          = log_size;

      if ((fout = fopen (log_file_name, "w")) == NULL)
        { /* Could not open log file, so log to /dev/null */
          if ((fout = fopen (fnull, "w")) == NULL)
            exit (2); /* Could not open /dev/null */

          set_result (2); /* set result for later return code */
        }

      /* Make the log size a multiple of the default size */
      log_size = round_log_size (j);

      {
        long long free_bytes = fs_free_bytes (log_file_name);

        /* free_bytes is -1 when unknown; the > DEF_SIZE test rejects that. */
        if (log_size > free_bytes && free_bytes > DEF_SIZE)
          {
            log_size = DEF_SIZE;
            set_result (2);
          }
      }

      lp.size = log_size;

      /* Initialize the header and fill the log with zeroes */
      (void)fseek (fout, 0, 0);
      to_big_endian (& lp);
      (void)fwrite (& lp, sizeof (struct bl_head), 1, fout);
      to_big_endian (& lp);
      (void)fseek (fout, lp.top, 0);

      for (i = 0; i < lp.size - (int)sizeof (struct bl_head); i++)
        (void)fputc ('0', fout); /* Fill the log with zeros */

      s_flag = false; /* Turn off s_flag flag */
    }

  /* check size to see if a new size has been specified as input parameter */

  /* if we're logging then see what size to use */
  if (s_flag)
    {
      (void)fclose (fout);
      fout = NULL;

      log_size = round_log_size (log_size);

      if (lp.size < log_size) /* Increase the size of the log */
        {
          long long free_bytes = fs_free_bytes (log_file_name);

          if (free_bytes >= 0 && (long long)(log_size - lp.size) > free_bytes)
            {
              log_size = lp.size; /* not enough free space; keep size */
              set_result (2);
            }

          if (log_size > lp.size
              && (fout = fopen (log_file_name, "a")) != NULL)
            {
              j = log_size - lp.size;

              /* Extend the log with filler */
              for (i = 0; i < j; i++)
                (void)fputc ('0', fout);

              lp.size = log_size;   /* put new size in log header */
              (void)fclose (fout);  /* flush and release the append handle */
              fout = NULL;
            }
        }
      else if (lp.size > log_size) /* Shrink the log file */
        {
          if (shrink_log (log_file_name, & lp, log_size) != 0)
            set_result (2); /* on failure the original log is kept */
        }

      /* Reopen the (possibly resized) log file. */
      if ((fout = fopen (log_file_name, "r+")) == NULL)
        { /* Could not open resized log file, so log to /dev/null */
          if ((fout = fopen (fnull, "r+")) == NULL)
            exit (-1); /* Could not open /dev/null */

          set_result (2); /* set result for later return code */
        }

      /* Write the up-to-date header (lp is already in host byte order). */
      (void)fseek (fout, 0, 0);
      to_big_endian (& lp);
      (void)fwrite (& lp, sizeof (struct bl_head), 1, fout);
      to_big_endian (& lp);
    }

  /*
   * 1. Ignore the kill signal while reading/writing
   * 2. Read from stdin and write to fcon and fout
   * 3. When we reach the end of the log (lp.size), go back to the top
   *    and set the bottom pointer to the size of the log.
   */

  /* ignore the kill signal because we're starting to write to the log */
  (void)signal (SIGINT, SIG_IGN);
  (void)fseek (fout, lp.current, 0); /* goto starting point in log */

  j = 0;

  /* Get input from stdin */
  while ((bytes_inbuf = read (0, inbuf, BUFSIZ)) > 0)
    {
      for (i = 0; i < bytes_inbuf; i++) /* Output to file(s) */
        { /* check to see if its time to start writing the top of log again. */
          if (lp.current < lp.size)
            {
              (void)putc (inbuf [i], fout);
              (void)putc (inbuf [i], fcon);
              j++;
              lp.current++;
            }
          else /* wrap logic active when we reach the size of the log */
            { /* if we are logging to a file */
              (void)fseek (fout, lp.top, 0); /* position pointer to top */
              lp.current = lp.top;           /* position current to top */
              (void)putc (inbuf [i], fout);
              (void)putc (inbuf [i], fcon);
              j++;
              lp.current++;
              lp.bottom = lp.size; /* set bottom to size (end of log) */
            }
        }
    }

  if (lp.current > lp.bottom) /* adjust bottom of log */
    lp.bottom = lp.current;

  /* Update the header */
  (void)fseek (fout, 0, 0);
  to_big_endian (& lp);
  (void)fwrite (& lp, sizeof (struct bl_head), 1, fout);
  to_big_endian (& lp);

  /* All done let's sync & close */
  (void)fclose (fout);

  if (fcon != stdout)
    (void)fclose (fcon);

  exit (result); /* return value of previous problems */
}

/*****************************************************************************/
