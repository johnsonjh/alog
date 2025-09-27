/* alog - Control for a log file */

#define _ILS_MACROS

#include "alog.h"
#include "endian.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/statfs.h>
#include <sys/types.h>
#include <unistd.h>

int result = 0;      /* holds global exit value */
char c_flag = FALSE; /* flag for SMIT processing */

int
main (int argc, char *argv[])
{

  /*********************************************************************/
  /*********************************************************************/
  /*********************************************************************/

  FILE *fout, *fcon; /* File pointer */
  struct statfs statbuf;
  char *fnull; /* Pointer to /dev/null name */

  int i, j;          /* Temp vars */
  int op;            /* option return from getopt */
  int verbose_valid; /* true if verbosity is 0-9 */
  int free_bytes;    /* free bytes in filesystem */
  extern char *optarg;
  extern int opterr;

  /**************** Default Values/Definitions */

  char *log_file_name; /* Pointer to Log file name */
  char *log_file_type; /* Pointer to Log file type */
  char *log_verbose;   /* Pointer to verbosity level */
  char *chg_verb;      /* Pointer to new verbosity level */
  int log_size;        /* Current size of log */
  int log_shrink = 0;  /* We are shrinking the log size */

  /***************** Buffers & storage ptrs */
  char tname[128];        /* Temp file name for shrink */
  char t_type[128];       /* Hold optarg log type */
  char t_verb[128];       /* Hold optarg log verbosity */
  char log_size_arg[128]; /* Hold optarg log size */
  char cmd[256];          /* Used to build command */
  char inbuf[BUFSIZ];     /* Input buffer */

  /**************** Flags & control vars */
  char C_flag = FALSE;     /* Change attribute flag */
  char f_flag = FALSE;     /* File name flag */
  char L_flag = FALSE;     /* List attributes flag */
  char o_flag = FALSE;     /* Output log flag */
  char q_flag = FALSE;     /* Quiet logging flag */
  char s_flag = FALSE;     /* Change size flag */
  char t_flag = FALSE;     /* Type flag */
  char V_flag = FALSE;     /* Return verbosity value*/
  char w_flag = FALSE;     /* Change verbosity flag */
  char valid_type = FALSE; /* Valid Type indicator */
  char state = FALSE;      /* logging state control */

  struct bl_head lp; /* Setup control structure */
  int bytes_inbuf;   /* bytes in from buffer */
  int systemrc = 0;

  /*********************************************************************/
  /*********************************************************************/
  /*********************************************************************/

  (void)setlocale (LC_ALL, ""); /* get locale env values */

  /* Set all control vals to zero */
  log_size = 0;
  log_file_name = NULL;
  log_file_type = NULL;
  log_verbose = NULL;
  chg_verb = NULL;
  opterr = 0;

  /* Get command line options */
  while ((op = getopt (argc, argv, "Cf:Loqcs:t:Vw:-H")) != EOF)
    {
      switch (op)
        {
        case 'c':
          c_flag = TRUE;
          break;

        case 'C': /* user specified change flag */
          C_flag = TRUE;
          break;

        case 'f': /* user specified log name */
          f_flag = TRUE;
          log_file_name = malloc (strlen (optarg) + 1);
          if (log_file_name == NULL)
            {
              fprintf (stderr, "alog: Out of memory.\n");
              exit (1);
            }
          strcpy (log_file_name, optarg);
          break;

        case 'L': /* user specified list flag */
          L_flag = TRUE;
          break;

        case 'o': /* output log to screen */
          o_flag = TRUE;
          break;

        case 'q': /* quiet logging - no cons output */
          q_flag = TRUE;
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
                s_flag = TRUE;
              }
          }
          break;

        case 't': /* user specified log type */
          t_flag = TRUE;
          strcpy (t_type, optarg);
          log_file_type = t_type;
          break;

        case 'V': /* get verbosity level */
          V_flag = TRUE;
          break;

        case 'w': /* user specified verbosity */
          w_flag = TRUE;
          strcpy (t_verb, optarg);
          if (isdigit (t_verb[0]) && !(t_verb[1])
              && (strcmp (t_verb, "00") != 0))
            chg_verb = t_verb;
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

  /* If there are syntax errors, then put out */
  /* syntax msg in the following cases: */
  /* - the L_flag or o_flag flag is set */
  /* Only put out the syntax message in these cases (and the */
  /* cases already handled above) so that when alog is being used */
  /* as a pipe, alog won't interupt the operation of the command */
  /* that is calling it. */
  if ((result == 1) && (C_flag || L_flag || o_flag))
    syntax ();

  /* Flag combination check */

  /* the t flag is required with the C flag */
  /* the s, f, or w flag is required with C and t */
  /* the other flags are not valid with the C flag */
  if (C_flag)
    if ((!t_flag) || (!(s_flag || f_flag || w_flag))
        || (L_flag || o_flag || q_flag || V_flag))
      set_result (1); /* syntax return code */

  /* the L and V flags are not valid with the f flag */
  if (f_flag)
    if (L_flag || V_flag)
      set_result (1); /* syntax return code */

  /* these flags are not valid with the L flag */
  if (L_flag)
    if (C_flag || f_flag || o_flag || s_flag || q_flag || V_flag || w_flag)
      set_result (1); /* syntax return code */

  /* either the f or t flag is required with the o flag */
  /* the other flags are not valid with the o flag */
  if (o_flag)
    if ((!f_flag && !t_flag)
        || (C_flag || L_flag || s_flag || q_flag || V_flag || w_flag))
      set_result (1); /* syntax return code */

  /* either the f or t flag is required with the q flag */
  /* the other flags are not valid with the q flag */
  if (q_flag)
    if ((!f_flag && !t_flag)
        || (C_flag || L_flag || o_flag || V_flag || w_flag))
      set_result (1); /* syntax return code */

  /* either the f or t flag is required with the s flag */
  /* the other flags are not valid with the s flag */
  if (s_flag)
    if ((!f_flag && !t_flag) || (L_flag || o_flag || V_flag))
      set_result (1); /* syntax return code */

  /* the t flag is required with the V flag */
  /* the other flags are not valid with the V flag */
  if (V_flag)
    if ((!t_flag)
        || (C_flag || f_flag || L_flag || o_flag || s_flag || q_flag
            || w_flag))
      set_result (1); /* syntax return code */

  /* the C and t flags are required with the w flag */
  /* the other flags are not valid with the w flag */
  if (w_flag)
    if ((!C_flag || !t_flag) || (L_flag || o_flag || q_flag || V_flag))
      set_result (1); /* syntax return code */

  /* only put out the bad flag combo for when the output flags are */
  /* specified. */
  if ((result == 1) && (C_flag || L_flag || o_flag))
    {
      fprintf (stderr, "alog: Invalid combination \
of flags.\n");
      syntax ();
    }

  /* Make sure that the verbosity is a value 0 through 9. */
  if ((w_flag) && (chg_verb == NULL))
    {
      set_result (1);
      fprintf (stderr, "alog: The verbosity \
'%s' is not a valid verbosity value.\n\
The verbosity value must be within the range 0 to 9.\n",
               t_verb);
      syntax ();
    }

  /* Check for type and change/retrieve info from ODM database if needed */
  if (log_file_type != NULL) /* user specified log type */
    {
        if (C_flag || L_flag || o_flag)
          {
            fprintf (stderr, "alog: %s is not an alog type.\n",
                     log_file_type);
            exit (2); /* exit so no more processing is done */
          }
    } /* end if */

  /*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=**=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
          VALIDATE - Make all fields have an appropriate value
                     Set values in case of failure to insure that
                     alog always runs to completion.
   *=*=*=*=*=*=*=*=*=*=*=*=*=*=*=**=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/
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
          fprintf (stderr, "alog: Invalid file name.\n\
Possible cause(s):\n\t\
- The file name associated with the specified type\n\t\
  was not found in the ODM database, or it does not have a value.\n\t\
- /dev/null was specified.\n");
          exit (2);
        }
      else
        {
          output_log (log_file_name);
          exit (0);
        }
    }

  /* Check to see if list attributes mode is selected without -t */
  /* This is for when a log type was not entered. */
  if (L_flag)
    {
      // set_result(output_attr(log_file_type));
      exit (result);
    }

  /* Check to see if verbose level display mode is selected */
  /* Only return verbosity level to stdout if a valid_type is entered */
  /* and there were no syntax errors. */
  if ((V_flag) && (!f_flag))
    {
      exit (2); /* the type was not valid so exit */
    }

  /*
   *----------------------------------------------------------------
   * Check to see the current verbosity level. If verbosity is
   * turned off then set log_file_name to /dev/null. This overrides
   * the filename and prevents any log from being generated.
   *----------------------------------------------------------------
   */

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

  /* Open the log file    */
  if ((fout = fopen (log_file_name, "r+")) != NULL)
    {
      /* Read header from file     */
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
          set_result (2); /* return error because file was */
                          /* not an alog file */
        }
    } /* end if */
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
      s_flag = FALSE; /* Turn off s_flag flag */
    } /* end else */
  /*
   *-------------------------------------------------------
   * Time to check size to see if a new size has been specified
   * as a parameter on input
   *-------------------------------------------------------
   */

  /* if we're logging then see what size to use */
  if (s_flag)
    {
      fclose (fout);
      fout = NULL;
      if (lp.size < log_size) /* Increase size of log         */
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
              /* Extend the log with zeros   */
              for (i = 0; i < j; i++)
                fputc (0, fout);
              lp.size = log_size; /* put new size in log header */
            } /* end else */
        } /* end if */
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
        } /* end if */
      /* ready */
      if ((fout = fopen (log_file_name, "r+")) == NULL)
        {
          /* Could not open (resized) log file, so log to /dev/null */
          if ((fout = fopen (fnull, "r+")) == NULL)
            /* Could not open /dev/null */
            exit (-1);
          set_result (2); /* set result for later return code */
        } /* end if */

      /* Only write to the header if we are increasing the log. */
      /* If we are shrinking, the log pointers have already been */
      /* updated by the call to alog. */
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
    } /* end if */

  /*
   *-------------------------------------------------------
   * Now we're all set up, let's begin
   * 1. Ignore the kill signal while reading/writing
   * 2. Read from stdin and write to fcon and fout
   * 3. When we reach the end of the log (lp.size),
   *    go back to the top and set the bottom pointer
   *    to the size of the log.
   *-------------------------------------------------------
   */

  /* ignore the kill signal because we're starting to write to the log */
  signal (SIGINT, SIG_IGN);
  fseek (fout, lp.current, 0); /* Goto starting point in log */

  j = 0;

  /* Get input from stdin  */
  while ((bytes_inbuf = read (0, inbuf, BUFSIZ)) > 0)
    {
      for (i = 0; i < bytes_inbuf; i++) /* Output to file(s) */
        {
          /* check to see if it is time to start writing to the */
          /* top of the log again. */
          if (lp.current < lp.size)
            {
              putc (inbuf[i], fout);
              putc (inbuf[i], fcon);
              j++;
              lp.current++;
            } /* end if */
          else /* wrap logic active when we reach the size of the log */
            { /* if we are logging to a file */
              fseek (fout, lp.top, 0); /* position pointer to top of log */
              lp.current = lp.top;     /* position current to top of log */
              putc (inbuf[i], fout);
              putc (inbuf[i], fcon);
              j++;
              lp.current++;
              lp.bottom = lp.size; /* set bottom to size (end of log) */
            } /* end else */
        } /* end for loop */
    } /* end while loop */

  if (lp.current > lp.bottom) /* adjust bottom of log */
    lp.bottom = lp.current;

  /* Update the header */
  fseek (fout, 0, 0);
  to_big_endian (&lp);
  fwrite (&lp, sizeof (struct bl_head), 1, fout);
  to_big_endian (&lp);

  /* All done let's sync & close  */
  fclose (fout);
  if (fcon != stdout)
    {
      fclose (fcon);
    }
  exit (result); /* return value of previous problems */
} /* end main */

/*
 * FUNCTION: This function sets the global result code if it's
 *              not already set.
 */

void
set_result (int new_result)
{
  if (result)
    return;
  result = new_result;
}
