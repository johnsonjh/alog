/* alog.h -- Header file for alog utility */

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <locale.h>
#include <string.h>

#include "endian.h"

#define TRUE 1
#define FALSE 0

/*-------------------------------------
 * This define controls rounding of
 * log size for expanding or shrinking
 * the log. Set to 4096 for arbitrary
 * log sizes.
 *-------------------------------------*/

#define DEF_SIZE 4096         /* log size Define */
#define ALOG_MAGIC 0xf9f3f9f4 /* magic number for alog files */

/* The log file header contains information about the log file */
struct bl_head /* log file header */
{
  int magic;   /* magic number */
  int top;     /* top of log */
  int current; /* current position in log */
  int bottom;  /* bottom of log */
  int size;    /* size of log */
};

void syntax (void);
void set_result (int new_result);
void output_log (char *log_file_name);
void to_big_endian (struct bl_head *h);
