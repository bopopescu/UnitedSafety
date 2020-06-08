/* Handle so called `shell archives'.
   Copyright (C) 1994, 1995, 1996, 2002, 2005 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "liballoca.h"

#include "system.h"
#include "basename.h"
#include "error.h"
#include "exit.h"
#include "xalloc.h"
#include "xgetcwd.h"

#include "scripts.x"

#if HAVE_LOCALE_H
# include <locale.h>
#else
# define setlocale(Category, Locale)
#endif
#include "gettext.h"

/*
 *  _() and N_() strings are both extracted into the .pot file.
 */
#define _(str)  gettext (str)
#define N_(str) gettext_maybe (str)

#ifndef NUL
#  define NUL '\0'
#endif

static const char *cut_mark_line
  = "---- Cut Here and feed the following to sh ----\n";

/* Delimiter to put after each file.  */
#define	DEFAULT_HERE_DELIMITER "SHAR_EOF"

/* Character which goes in front of each line.  */
#define DEFAULT_LINE_PREFIX_1 'X'

/* Character which goes in front of each line if here_delimiter[0] ==
   DEFAULT_LINE_PREFIX_1.  */
#define DEFAULT_LINE_PREFIX_2 'Y'

/* Shell command able to count characters from its standard input.  We
   have to take care for the locale setting because wc in multi-byte
   character environments get different results.  */
#define CHARACTER_COUNT_COMMAND "LC_ALL=C wc -c <"

/* Maximum length for a text line before it is considered binary.  */
#define MAXIMUM_NON_BINARY_LINE 200

/* System related declarations.  */

#include <ctype.h>

#if STDC_HEADERS
# define ISASCII(Char) 1
#else
# ifdef isascii
#  define ISASCII(Char) isascii (Char)
# else
#  if HAVE_ISASCII
#   define ISASCII(Char) isascii (Char)
#  else
#   define ISASCII(Char) ((Char) & 0x7f == (unsigned char) (Char))
#  endif
# endif
#endif

#include <time.h>

struct tm *localtime ();

#ifdef HAVE_LIMITS_H
# include <limits.h>
#endif

/* Determine whether an integer type is signed, and its bounds.
   This code assumes two's (or one's!) complement with no holes.  */

/* The extra casts work around common compiler bugs,
   e.g. Cray C 5.0.3.0 when t == time_t.  */
#ifndef TYPE_SIGNED
# define TYPE_SIGNED(t) (! ((t) 0 < (t) -1))
#endif
#ifndef TYPE_MINIMUM
# define TYPE_MINIMUM(t) ((t) (TYPE_SIGNED (t) \
			       ? ~ (t) 0 << (sizeof (t) * CHAR_BIT - 1) \
			       : (t) 0))
#endif
#ifndef TYPE_MAXIMUM
# define TYPE_MAXIMUM(t) ((t) (~ (t) 0 - TYPE_MINIMUM (t)))
#endif

#if !NO_WALKTREE

/* Declare directory reading routines and structures.  */

#ifdef __MSDOS__
# include "msd_dir.h"
#else
# include DIRENT_HEADER
#endif

#if HAVE_DIRENT_H
# define NAMLEN(dirent) (strlen((dirent)->d_name))
#else
# define NAMLEN(dirent) ((dirent)->d_namlen)
# ifndef __MSDOS__
#  define dirent direct
# endif
#endif

#endif /* !NO_WALKTREE */

/* Option variables.  */

#include "getopt.h"
#include "inttostr.h"
#include "md5.h"

/* No Brown-Shirt mode.  */
static int vanilla_operation_mode = 0;

/* Mixed text and binary files.  */
static int mixed_uuencoded_file_mode = -1;

/* Flag for binary files.  */
static int uuencoded_file_mode = -1;

/* Run input files through gzip (requires uuencoded_file_mode).  */
static int gzipped_file_mode = -1;

/* -N option to gzip.  */
static int gzip_compression_level = 9;

/* Run input files through bzip2 (requires uuencoded_file_mode).  */
static int bzipped_file_mode = -1;

/* Run input files through compress (requires uuencoded_file_mode).  */
static int compressed_file_mode = -1;

/* -bN option to compress */
static int bits_per_compressed_byte = 12;

/* Generate $shar_touch commands.  */
static int timestamp_mode = 1;

/* Option to provide wc checking.  */
static int character_count_mode = 1;

/* Option to provide MD5 sum checking.  */
static int md5_count_mode = 1;

/* Use temp file instead of pipe to feed uudecode.  This gives better
   error detection, at expense of disk space.  This is also necessary for
   those versions of uudecode unwilling to read their standard input.  */
static int inhibit_piping_mode = 0;

/* --no-i18n option to prevent internationalized shell archives.  */
static int no_i18n = 0;

/* Character to get at the beginning of each line.  */
static int line_prefix = '\0';

/* Option to generate "Archive-name:" headers.  */
static int net_headers_mode = 0;

/* Documentation name for archive.  */
static char *archive_name = NULL;

/* Option to provide append feedback at shar time.  */
static int quiet_mode = 0;

/* Option to provide extract feedback at unshar time.  */
static int quiet_unshar_mode = 0;

/* Pointer to delimiter string.  */
static const char *here_delimiter = DEFAULT_HERE_DELIMITER;

/* Value of strlen (here_delimiter).  */
static size_t here_delimiter_length = 0;

/* Use line_prefix even when first char does not force it.  */
static int mandatory_prefix_mode = 0;

/* Option to provide cut mark.  */
static int cut_mark_mode = 0;

/* Check if file exists.  */
static int check_existing_mode = 1;

/* Interactive overwrite.  */
static int query_user_mode = 0;

/* Allow positional parameters.  */
static int intermixed_parameter_mode = 0;

/* Strip directories from filenames.  */
static int basename_mode;

/* Switch for debugging on.  */
#if DEBUG
static int debugging_mode = 0;
#endif

/* Split files in the middle.  */
static int split_file_mode = 0;

/* File size limit in bytes.  */
static off_t file_size_limit = 0;

/* Other global variables.  */

/* The name this program was run with. */
const char *program_name;

/* If non-zero, display usage information and exit.  */
static int show_help = 0;

/* If non-zero, print the version on standard output and exit.  */
static int show_version = 0;

/* File onto which the shar script is being written.  */
static FILE *output = NULL;

/* Position for archive type message.  */
static off_t archive_type_position = 0;

/* Position for first file in the shar file.  */
static off_t first_file_position = 0;

/* Base for output filename.  */
static char *output_base_name = NULL;

/* Actual output filename.  */
static char *output_filename = NULL;

static char *submitter_address = NULL;

/* Output file ordinal.  FIXME: also flag for -o.  */
static int part_number = 0;

/* Table saying whether each character is binary or not.  */
static unsigned char byte_is_binary[256];

/* For checking file type and access modes.  */
static struct stat struct_stat;

/* Nonzero if special NLS option (--print-text-domain-dir) is selected.  */
static int print_text_dom_dir = 0;

/* The number used to make the intermediate files unique.  */
static int sharpid = 0;

/* scribble space. */
static size_t scribble_size = 1024 + (BUFSIZ * 2);
static char* scribble = NULL;

static int translate_script = 0;

static char*
gettext_maybe( char* pz )
{
  if (translate_script)
    return gettext (pz);
  return pz;
}

#if DEBUG
# define DEBUG_PRINT(Format, Value) \
    if (debugging_mode)					\
      {							\
	char buf[INT_BUFSIZE_BOUND (off_t)];		\
	printf (Format, offtostr (Value, buf));		\
      }
#else
# define DEBUG_PRINT(Format, Value)
#endif

static void open_output __P ((void));
static void close_output __P ((void));
static void usage __P ((int))
#if defined __GNUC__ && ((__GNUC__ == 2 && __GNUC_MINOR__ >= 5) || __GNUC__ > 2)
     __attribute__ ((noreturn))
#endif
;

/* Walking tree routines.  */

/* Define a type just for easing ansi2knr's life.  */
typedef int (*walker_t) __P ((const char *, const char *));

static void
echo_status( const char*  test_pz,
	     const char*  ok_format_pz,
	     const char*  bad_format_pz,
	     const char*  what_pz,
	     int die_on_failure )
{
  static const char test_okay_z[] = "if %s\nthen ${echo} '";
  static const char test_else_z[] = "'\nelse ${echo} '";
  static const char test_fail_z[] = "if %s\nthen : ; else ${echo} '";
  static const char test_exit_z[] = "'\n  exit 1\nfi\n";
  static const char test_fi_z[]   = "'\nfi\n";

  if (ok_format_pz != NULL)
    {
      fprintf (output, test_okay_z, test_pz);
      fprintf (output, ok_format_pz, what_pz);

      if (bad_format_pz != NULL)
        {
          fputs (test_else_z, output);
          fprintf (output, bad_format_pz, what_pz);
        }
    }
  else
    {
      fprintf (output, test_fail_z, test_pz);
      fprintf (output, bad_format_pz, what_pz);
    }

  if (die_on_failure)
    fputs (test_exit_z, output);
  else
    fputs (test_fi_z, output);
}

static void
echo_text( const char* format_pz, const char* arg_pz, int cascade )
{
  fputs ("  ${echo} 'x -", output);
  fprintf (output, format_pz, arg_pz);
  fputs (cascade ? "' &&\n" : "'\n", output);
}

#if !NO_WALKTREE

/*--------------------------------------------------------------------------.
| Recursively call ROUTINE on each entry, down the directory tree.  NAME    |
| is the path to explore.  RESTORE_NAME is the name that will be later	    |
| relative to the unsharing directory.  ROUTINE may also assume		    |
| struct_stat is set, it accepts updated values for NAME and RESTORE_NAME.  |
`--------------------------------------------------------------------------*/

static int
walkdown (routine, local_name, restore_name)
     walker_t routine;
     const char *local_name;
     const char *restore_name;
{
  DIR *directory;		/* directory being scanned */
  int status;			/* status to return */

  char *local_name_copy;	/* writeable copy of local_name */
  size_t local_name_length;	/* number of characters in local_name_copy */
  size_t sizeof_local_name;	/* allocated size of local_name_copy */

  char *restore_name_copy;	/* writeable copy of restore_name */
  int    restore_offset;	/* passdown copy of restore_name */
  size_t restore_name_length;	/* number of characters in restore_name_copy */
  size_t sizeof_restore_name;	/* allocated size of restore_name_copy */

  if (stat (local_name, &struct_stat))
    {
      error (0, errno, local_name);
      return 1;
    }

  if (!S_ISDIR (struct_stat.st_mode & S_IFMT))
    return (*routine) (local_name, restore_name);

  if (directory = opendir (local_name), !directory)
    {
      error (0, errno, local_name);
      return 1;
    }

  status = 0;

  /* include trailing '/' in length */

  local_name_length = strlen (local_name) + 1;
  sizeof_local_name = local_name_length + 32;
  local_name_copy   = xmalloc (sizeof_local_name);
  memcpy (local_name_copy, local_name, local_name_length-1);
  local_name_copy[ local_name_length-1 ] = '/';
  local_name_copy[ local_name_length   ] = NUL;

  restore_name_length = strlen (restore_name) + 1;
  sizeof_restore_name = restore_name_length + 32;
  restore_name_copy   = xmalloc (sizeof_restore_name);
  memcpy (restore_name_copy, restore_name, restore_name_length-1);
  restore_name_copy[ restore_name_length-1 ] = '/';
  restore_name_copy[ restore_name_length   ] = NUL;

  {
    size_t sz = (sizeof_local_name < sizeof_restore_name)
      ? sizeof_restore_name : sizeof_local_name;
    sz = 1024 + (sz * 2);
    if (scribble_size < sz)
      {
        scribble_size = (sz + 4096) & 0x0FFF;
        scribble = xrealloc (scribble, scribble_size);
      }
  }

  if ((restore_name_copy[0] == '.') && (restore_name_copy[1] == '/'))
    restore_offset = 2;
  else
    restore_offset = 0;

  for (;;)
    {
      struct dirent *entry = readdir (directory);
      const char* pzN;
      int space_need;

      if (entry == NULL)
	break;

      /* append the new file name after the trailing '/' char.
         If we need more space, add in a buffer so we needn't
         allocate over and over.  */

      pzN = entry->d_name;
      if (*pzN == '.')
	{
	  if (pzN[1] == NUL)
	    continue;
	  if ((pzN[1] == '.') && (pzN[2] == NUL))
	    continue;
	}

      space_need = 1 + NAMLEN (entry);
      if (local_name_length + space_need > sizeof_local_name)
	{
	  sizeof_local_name = local_name_length + space_need + 16;
	  local_name_copy = (char *)
	    xrealloc (local_name_copy, sizeof_local_name);
	}
      strcpy (local_name_copy + local_name_length, pzN);

      if (restore_name_length + space_need > sizeof_restore_name)
	{
	  sizeof_restore_name = restore_name_length + space_need + 16;
	  restore_name_copy = (char *)
	    xrealloc (restore_name_copy, sizeof_restore_name);
	}
      strcpy (restore_name_copy + restore_name_length, pzN);

      status = walkdown (routine, local_name_copy,
			 restore_name_copy + restore_offset);
      if (status != 0)
	break;
    }

  /* Clean up.  */

  free (local_name_copy);
  free (restore_name_copy);

#if CLOSEDIR_VOID
  closedir (directory);
#else
  if (closedir (directory))
    {
      error (0, errno, local_name);
      return 1;
    }
#endif

  return status;
}

#endif /* !NO_WALKTREE */

/*------------------------------------------------------------------.
| Walk through the directory tree, calling ROUTINE for each entry.  |
| ROUTINE may also assume struct_stat is set.			    |
`------------------------------------------------------------------*/

static int
walktree (routine, local_name)
     walker_t routine;
     const char *local_name;
{
  const char *restore_name;
  char *local_name_copy;

  /* Remove crumb at end.  */
  {
    int len = strlen (local_name);
    char *cursor;

    local_name_copy = (char *) alloca (len + 1);
    memcpy (local_name_copy, local_name, len + 1);
    cursor = local_name_copy + len - 1;

    while (*cursor == '/' && cursor > local_name_copy)
      *(cursor--) = NUL;
  }

  /* Remove crumb at beginning.  */

  if (basename_mode)
    restore_name = basename (local_name_copy);
  else if (!strncmp (local_name_copy, "./", 2))
    restore_name = local_name_copy + 2;
  else
    restore_name = local_name_copy;

#if NO_WALKTREE

  /* Just act on current entry.  */

  {
    int status = stat (local_name_copy, &struct_stat);

    if (status != 0)
      error (0, errno, local_name_copy);
    else
      status = (*routine) (local_name_copy, restore_name);

    return status;
  }

#else

  /* Walk recursively.  */

  return walkdown (routine, local_name_copy, restore_name);

#endif
}

/* Generating parts of shar file.  */

/*---------------------------------------------------------------------.
| Build a `drwxrwxrwx' string corresponding to MODE into MODE_STRING.  |
`---------------------------------------------------------------------*/

static char *
mode_string (mode)
     unsigned mode;
{
  static char result[12];

  strcpy (result, "----------");

  if (mode & 00400)
    result[1] = 'r';
  if (mode & 00200)
    result[2] = 'w';
  if (mode & 00100)
    result[3] = 'x';
  if (mode & 04000)
    result[3] = 's';
  if (mode & 00040)
    result[4] = 'r';
  if (mode & 00020)
    result[5] = 'w';
  if (mode & 00010)
    result[6] = 'x';
  if (mode & 02000)
    result[6] = 's';
  if (mode & 00004)
    result[7] = 'r';
  if (mode & 00002)
    result[8] = 'w';
  if (mode & 00001)
    result[9] = 'x';

  return result;
}

/*-----------------------------------------------------------------------.
| Generate shell code which, at *unshar* time, will study the properties |
| of the unpacking system and set some variables accordingly.		 |
`-----------------------------------------------------------------------*/

static void
generate_configure ()
{
  if (md5_count_mode)
    fprintf (output, md5check_z, N_("\
Note: not verifying md5sums.  Consider installing GNU coreutils."));

  if (no_i18n)
    fputs ("echo=echo\n", output);
  else
    {
      fputs (i18n_z, output);
      /* Above the name of the program of the package which supports the
	 --print-text-domain-dir option has to be given.  */
    }

  if (!quiet_unshar_mode)
    {
      if (vanilla_operation_mode)
	fputs ("shar_tty= shar_n= shar_c='\n'\n",
	       output);
      else
	{
	  if (query_user_mode)
	    /* Check if /dev/tty exists.  If yes, define shar_tty to
	       `/dev/tty', else, leave it empty.  */

	    fputs (dev_tty_check_z, output);

	  /* Try to find a way to echo a message without newline.  Set
	     shar_n to `-n' or nothing for an echo option, and shar_c
	     to `\c' or nothing for a string terminator.  */

	  fputs (echo_checks_z, output);
	}
    }

  if (timestamp_mode)
    {
      fprintf (output, timestamp_z, N_("\
WARNING: not restoring timestamps.  Consider getting and"),
	       N_("\
installing GNU `touch'\\'', distributed in GNU coreutils..."));
    }

  if (file_size_limit == 0 || part_number == 1)
    {
      echo_status ("test ! -d ${lock_dir}", NULL,
                   N_("lock directory '${lock_dir}' exists"), NULL, 1);

      /* Create locking directory.  */
      if (vanilla_operation_mode)
	echo_status ("mkdir ${lock_dir}", NULL,
		     N_("failed to create lock directory"), NULL, 1);
      else
	{
	  const char* did_pz =
	    N_("x - created lock directory `'%s\\''.");
	  const char* not_pz =
	    N_("x - failed to create lock directory `'%s\\''.");
	  echo_status ("mkdir ${lock_dir}", did_pz, not_pz, "${lock_dir}", 1);
	}
    }

  if (query_user_mode)
    {
      fprintf (output, query_answers_z,
               N_("yes"),  N_("overwrite this file"),
               N_("no"),   N_("skip this file"),
               N_("all"),  N_("overwrite all files"),
               N_("none"), N_("overwrite no files"),
               N_("help"), N_("explain choices"),
               N_("quit"), N_("exit immediately"));
    }
}

/*----------------------------------------------.
| generate_mkdir                                |
| Make sure it is done only once for each dir   |
`----------------------------------------------*/

static int    mkdir_alloc_ct = 0;
static int    mkdir_already_ct = 0;
static char** mkdir_already;

static void
generate_mkdir (path)
     const char *path;
{
  /* If already generated code for this dir creation, don't do again.  */

  {
    int    ct = mkdir_already_ct;
    char** pp = mkdir_already;

    while (--ct > 0)
      {
        if (strcmp (*(pp++), path) == 0)
          return;
      }
  }

  /* Haven't done this one.  */

  if (++mkdir_already_ct > mkdir_alloc_ct)
    {
      /*
       *  We need more name space.  Get larger and larger chunks of space.
       *  The bound is when integers go negative.  Too many directories.  :)
       *
       *  16, 40, 76, 130, 211, 332, 514, 787, 1196, 1810, 2731, 4112, ...
       */
      mkdir_alloc_ct += 16 + (mkdir_alloc_ct/2);
      if (mkdir_alloc_ct < 0)
        error (EXIT_FAILURE, 0, _("Too many directories for mkdir generation"));

      if (mkdir_already != NULL)
        mkdir_already =
          xrealloc (mkdir_already, mkdir_alloc_ct * sizeof (char*));
      else
        mkdir_already = xmalloc (mkdir_alloc_ct * sizeof (char*));
    }

  /* Add the directory into our "we've done this already" table */

  mkdir_already[ mkdir_already_ct-1 ] = xstrdup (path);

  /* Generate the text.  */

  fprintf (output, "if test ! -d '%s'; then\n", path);
  if (!quiet_unshar_mode)
    {
      const char* did_pz =
	N_("x - created directory `%s'\\''.");
      const char* not_pz =
	N_("x - failed to create directory `%s'\\''.");
      fprintf (output, "  mkdir '%s'\n", path);
      echo_status ("test $? -eq 0", did_pz, not_pz, path, 1);
    }
  else
    fprintf (output, "  mkdir '%s' || exit 1\n", path);
  fputs ("fi\n", output);
}

static void
clear_mkdir_already (void)
{
  char** pp = mkdir_already;
  int    ct = mkdir_already_ct;

  mkdir_already_ct = 0;
  while (--ct >= 0)
    {
      free (*pp);
      *(pp++) = NULL;
    }
}

/*---.
| ?  |
`---*/

static void
generate_mkdir_script (path)
     const char *path;
{
  char *cursor;

  for (cursor = strchr (path, '/'); cursor; cursor = strchr (cursor + 1, '/'))
    {

      /* Avoid empty string if leading or double '/'.  */

      if (cursor == path || *(cursor - 1) == '/')
	continue;

      /* Omit '.'.  */

      if (cursor[-1] == '.' && (cursor == path + 1 || cursor[-2] == '/'))
	continue;

      /* Temporarily terminate string.  FIXME!  */

      *cursor = 0;
      generate_mkdir (path);
      *cursor = '/';
    }
}

/* Walking routines.  */

/*---.
| ?  |
`---*/

static int
check_accessibility (local_name, restore_name)
     const char *local_name;
     const char *restore_name;
{
  if (access (local_name, 4))
    {
      error (0, 0, _("Cannot access %s"), local_name);
      return 1;
    }

  return 0;
}

/*---.
| ?  |
`---*/

static int
generate_one_header_line (local_name, restore_name)
     const char *local_name;
     const char *restore_name;
{
  char buf[INT_BUFSIZE_BOUND (off_t)];
  fprintf (output, "# %6s %s %s\n", offtostr (struct_stat.st_size, buf),
	   mode_string (struct_stat.st_mode), restore_name);
  return 0;
}

/*---.
| ?  |
`---*/

static void
generate_full_header (argc, argv)
     int argc;
     char *const *argv;
{
  char *current_directory;
  time_t now;
  struct tm *local_time;
  char buffer[80];		/* FIXME: No fix limit in GNU... */
  int warned_once;
  int counter;

  warned_once = 0;
  for (counter = 0; counter < argc; counter++)
    {

      /* Skip positional parameters.  */

      if (intermixed_parameter_mode &&
	  (strcmp (argv[counter], "-B") == 0 ||
	   strcmp (argv[counter], "-T") == 0 ||
	   strcmp (argv[counter], "-M") == 0 ||
	   strcmp (argv[counter], "-z") == 0 ||
	   strcmp (argv[counter], "-Z") == 0 ||
	   strcmp (argv[counter], "-C") == 0))
	{
	  if (!warned_once && strcmp (argv[counter], "-C") == 0)
	    {
	      error (0, 0, _("-C is being deprecated, use -Z instead"));
	      warned_once = 1;
	    }
	  continue;
	}

      if (walktree (check_accessibility, argv[counter]))
	exit (EXIT_FAILURE);
    }

  if (net_headers_mode)
    {
      fprintf (output, "Submitted-by: %s\n", submitter_address);
      fprintf (output, "Archive-name: %s%s%02d\n\n",
	       archive_name, (strchr (archive_name, '/')) ? "" : "/part",
	       part_number ? part_number : 1);
    }

  if (cut_mark_mode)
    fputs (cut_mark_line, output);
  {
    char* pz = archive_name ? archive_name : "";
    char* ch = archive_name ? ", a shell" : "a shell";

    fprintf (output, file_leader_z, pz, ch, PACKAGE, VERSION, sharpid);
  }

  time (&now);
  local_time = localtime (&now);
  strftime (buffer, 79, "%Y-%m-%d %H:%M %Z", local_time);
  fprintf (output, "# Made on %s by <%s>.\n",
	   buffer, submitter_address);

  current_directory = xgetcwd ();
  if (current_directory)
    {
      fprintf (output, "# Source directory was `%s'.\n",
	       current_directory);
      free (current_directory);
    }
  else
    error (0, errno, _("Cannot get current directory name"));

  fputs ("#\n# Existing files ", output);
  if (check_existing_mode)
    fputs ("will *not* be overwritten, unless `-c' is specified.\n",
	   output);
  else if (query_user_mode)
    fputs ("MAY be overwritten.\n", output);
  else
    fputs ("WILL be overwritten.\n", output);

  if (query_user_mode)
    fputs ("# The unsharer will be INTERACTIVELY queried.\n",
	   output);

  if (vanilla_operation_mode)
    {
      fputs (
        "# This format requires very little intelligence at unshar time.\n# ",
	     output);
      if (check_existing_mode || split_file_mode)
	fputs ("\"if test\", ", output);
      if (split_file_mode)
	fputs ("\"cat\", \"rm\", ", output);
      fputs ("\"echo\", \"mkdir\", and \"sed\" may be needed.\n", output);
    }

  if (split_file_mode)
    {

      /* May be split, explain.  */

      fputs ("#\n", output);
      archive_type_position = ftello (output);
      fprintf (output, "%-75s\n%-75s\n", "#", "#");
    }

  fputs (contents_z, output);

  for (counter = 0; counter < argc; counter++)
    {

      /* Output names of files but not parameters.  */

      if (intermixed_parameter_mode &&
	  (strcmp (argv[counter], "-B") == 0 ||
	   strcmp (argv[counter], "-T") == 0 ||
	   strcmp (argv[counter], "-M") == 0 ||
	   strcmp (argv[counter], "-z") == 0 ||
	   strcmp (argv[counter], "-Z") == 0 ||
	   strcmp (argv[counter], "-C") == 0))
	continue;

      if (walktree (generate_one_header_line, argv[counter]))
	exit (EXIT_FAILURE);
    }
  fputs ("#\n", output);

  generate_configure ();

  if (split_file_mode)
    {
      /* Now check the sequence.  */
      echo_status ("test ! -r ${lock_dir}/seq", NULL,
                   N_("Archives must be unpacked in sequence!\n"
                      "Please unpack part '`cat ${lock_dir}/seq`' next."),
                   NULL, 1);
    }
}

void
change_files (const char *restore_name, off_t remaining_size)
{
  /* Change to another file.  */

  DEBUG_PRINT (_("New file, remaining %s, "), remaining_size);
  DEBUG_PRINT (_("Limit still %s\n"), file_size_limit);

  /* Close the "&&" and report an error if any of the above
     failed.  */

  fputs (" :\n", output);
  echo_status ("test $? -ne 0", N_("restore of %s failed"), NULL,
	       restore_name, 0);

  fputs ("'\n${echo} '", output);
  fprintf (output, N_("End of part %d, continue with part %d"),
	   part_number, part_number+1);
  fputs ("'\nexit 0\n", output);

  close_output ();

  /* Clear mkdir_already in case the user unshars out of order.  */

  clear_mkdir_already ();

  /* Form the next filename.  */

  open_output ();
  if (!quiet_mode)
    fprintf (stderr, _("Starting file %s\n"), output_filename);

  if (net_headers_mode)
    {
      fprintf (output, "Submitted-by: %s\n", submitter_address);
      fprintf (output, "Archive-name: %s%s%02d\n\n", archive_name,
	       strchr (archive_name, '/') ? "" : "/part",
	       part_number ? part_number : 1);
    }

  if (cut_mark_mode)
    fputs (cut_mark_line, output);

  {
    static const char part_z[] =
      "part %02d of %s ";
    char *nm = archive_name ? archive_name : "a multipart";
    char *pz = xmalloc (sizeof(part_z) + strlen(nm) + 16);
    sprintf (pz, part_z, part_number, nm);
    fprintf (output, file_leader_z, pz, "", PACKAGE, VERSION, sharpid);
    free (pz);
  }

  generate_configure ();

  first_file_position = ftello (output);
}

/* Prepare a shar script.  */

/*---.
| ?  |
`---*/

static int
shar (local_name, restore_name)
     const char *local_name;
     const char *restore_name;
{
  char buffer[BUFSIZ];
  FILE *input;
  off_t remaining_size;
  int split_flag = 0;		/* file split flag */
  const char *file_type;	/* text of binary */
  const char *file_type_remote;	/* text or binary, avoiding locale */
  struct tm *restore_time;

  /* Check to see that this is still a regular file and readable.  */

  if (!S_ISREG (struct_stat.st_mode & S_IFMT))
    {
      error (0, 0, _("%s: Not a regular file"), local_name);
      return 1;
    }
  if (access (local_name, 4))
    {
      error (0, 0, _("Cannot access %s"), local_name);
      return 1;
    }

  /* If file_size_limit set, get the current output length.  */

  if (file_size_limit)
    {
      off_t current_size = ftello (output);
      remaining_size = file_size_limit - current_size - 1024;
      DEBUG_PRINT (_("In shar: remaining size %s\n"), remaining_size);

      if (!split_file_mode && current_size > first_file_position
	  && ((uuencoded_file_mode
	       ? struct_stat.st_size + struct_stat.st_size / 3
	       : struct_stat.st_size)
	      > remaining_size))
        change_files (restore_name, remaining_size);
    }
  else
    remaining_size = 0;		/* give some value to the variable */

  fprintf (output, "# ============= %s ==============\n",
	   restore_name);

  generate_mkdir_script (restore_name);

  if (struct_stat.st_size == 0)
    {
      file_type = _("empty");
      file_type_remote = N_("(empty)");
      input = NULL;		/* give some value to the variable */
    }
  else
    {

      /* If mixed, determine the file type.  */

      if (mixed_uuencoded_file_mode)
	{

	  /* Uuencoded was once decided through calling the `file'
	     program and studying its output: the method was slow and
	     error prone.  There is only one way of doing it correctly,
	     and this is to read the input file, seeking for one binary
	     character.  Considering the average file size, even reading
	     the whole file (if it is text) would be usually faster than
	     calling `file'.  */

	  int character;
	  int line_length;

	  if (input = fopen (local_name, "rb"), input == NULL)
	    {
	      error (0, errno, _("Cannot open file %s"), local_name);
	      return 1;
	    }

	  /* Assume initially that the input file is text.  Then try to prove
	     it is binary by looking for binary characters or long lines.  */

	  uuencoded_file_mode = 0;
	  line_length = 0;
	  while (character = getc (input), character != EOF)
	    if (character == '\n')
	      line_length = 0;
	    else if (
#ifdef __CHAR_UNSIGNED__
		     byte_is_binary[character]
#else
		     byte_is_binary[character & 0xFF]
#endif
		     || line_length == MAXIMUM_NON_BINARY_LINE)
	      {
		uuencoded_file_mode = 1;
		break;
	      }
	    else
	      line_length++;
	  fclose (input);

	  /* Text files should terminate by an end of line.  */

	  if (line_length > 0)
	    uuencoded_file_mode = 1;
	}

      if (uuencoded_file_mode)
	{
	  static int pid, pipex[2];

	  file_type = (compressed_file_mode ? _("compressed")
		       : gzipped_file_mode ? _("gzipped")
		       : bzipped_file_mode ? _("bzipped")
		       : _("binary"));
	  file_type_remote = (compressed_file_mode ? N_("(compressed)")
			      : gzipped_file_mode ? N_("(gzipped)")
			      : bzipped_file_mode ? N_("(bzipped)")
			      : N_("(binary)"));

	  /* Fork a uuencode process.  */

	  pipe (pipex);
	  fflush (output);

	  if (pid = fork (), pid != 0)
	    {

	      /* Parent, create a file to read.  */

	      if (pid < 0)
		error (EXIT_FAILURE, errno, _("Could not fork"));
	      close (pipex[1]);
	      input = fdopen (pipex[0], "r");
	      if (!input)
		{
		  error (0, errno, _("File %s (%s)"), local_name, file_type);
		  return 1;
		}
	    }
	  else
	    {

	      /* Start writing the pipe with encodes.  */

	      FILE *outptr;

	      if (compressed_file_mode)
		{
		  sprintf (buffer, "compress -b%d < '%s'",
			   bits_per_compressed_byte, local_name);
		  input = popen (buffer, "r");
		}
	      else if (gzipped_file_mode)
		{
		  sprintf (buffer, "gzip -%d < '%s'",
			   gzip_compression_level, local_name);
		  input = popen (buffer, "r");
		}
	      else if (bzipped_file_mode)
		{
		  sprintf (buffer, "bzip2 < '%s'", local_name);
		  input = popen (buffer, "r");
		}
	      else
		input = fopen (local_name, "rb");

	      outptr = fdopen (pipex[1], "w");
	      fputs ("begin 600 ", outptr);
	      if (compressed_file_mode)
		fprintf (outptr, "_sh%05d/cmp\n", sharpid);
	      else if (gzipped_file_mode)
		fprintf (outptr, "_sh%05d/gzi\n", sharpid);
	      else if (bzipped_file_mode)
		fprintf (outptr, "_sh%05d/bzi\n", sharpid);
	      else
		fprintf (outptr, "%s\n", restore_name);
	      copy_file_encoded (input, outptr);
	      fprintf (outptr, "end\n");
	      if (compressed_file_mode || gzipped_file_mode
		  || bzipped_file_mode)
		pclose (input);
	      else
		fclose (input);

	      exit (EXIT_SUCCESS);
	    }
	}
      else
	{
	  file_type = _("text");
	  file_type_remote = N_("(text)");

	  input = fopen (local_name, "r");
	  if (!input)
	    {
	      error (0, errno, _("File %s (%s)"), local_name, file_type);
	      return 1;
	    }
	}
    }

  /* Protect existing files.  */

  if (check_existing_mode)
    {
      fprintf (output, "if test -f '%s' && test \"$first_param\" != -c; then\n",
	       restore_name);

      if (query_user_mode)
	{
          char* pzOverwriting = scribble;
          char* pzOverwrite;

          sprintf (pzOverwriting, N_("overwriting %s"), restore_name);
          pzOverwrite = scribble + strlen(pzOverwriting) + 2;
          sprintf (pzOverwrite, N_("overwrite %s"), restore_name);

          fprintf (output, query_user_z, pzOverwriting, pzOverwrite);

          sprintf (pzOverwriting, N_("SKIPPING %s"), restore_name);
          fprintf (output, query_check_z, N_("extraction aborted"),
		   pzOverwriting, pzOverwriting);
	}
      else
        echo_text (N_("SKIPPING %s (file already exists)"),
                   restore_name, 0);

      if (split_file_mode)
	fputs ("  rm -f ${lock_dir}/new\nelse\n  > ${lock_dir}/new\n", output);
      else
	fputs ("else\n", output);
    }

  if (!quiet_mode)
    error (0, 0, _("Saving %s (%s)"), local_name, file_type);

  if (!quiet_unshar_mode)
    {
      sprintf (scribble, N_("x - extracting %s %s"),
               restore_name, file_type_remote);
      fprintf (output, echo_string_z, scribble);
    }

  if (struct_stat.st_size == 0)
    {

      /* Just touch the file, or empty it if it exists.  */

      fprintf (output, "  > '%s' &&\n",
	       restore_name);
    }
  else
    {
      /* Run sed for non-empty files.  */

      if (uuencoded_file_mode)
	{

	  /* Run sed through uudecode (via temp file if might get split).  */

	  fprintf (output, "  sed 's/^%c//' << '%s' ",
		   line_prefix, here_delimiter);
	  if (inhibit_piping_mode)
	    fprintf (output, "> ${lock_dir}/uue &&\n");
	  else
	    fputs ("| uudecode &&\n", output);
	}
      else
	{

	  /* Just run it into the file.  */

	  fprintf (output, "  sed 's/^%c//' << '%s' > '%s' &&\n",
		   line_prefix, here_delimiter, restore_name);
	}

      while (fgets (buffer, BUFSIZ, input))
	{

	  /* Output a line and test the length.  */

	  if (!mandatory_prefix_mode
	      && ISASCII (buffer[0])
#ifdef isgraph
	      && isgraph (buffer[0])
#else
	      && isprint (buffer[0]) && !isspace (buffer[0])
#endif
	      /* Protect lines already starting with the prefix.  */
	      && buffer[0] != line_prefix

	      /* Old mail programs interpret ~ directives.  */
	      && buffer[0] != '~'

	      /* Avoid mailing lines which are just `.'.  */
	      && buffer[0] != '.'

#if STRNCMP_IS_FAST
	      && strncmp (buffer, here_delimiter, here_delimiter_length)

	      /* unshar -e: avoid `exit 0'.  */
	      && strncmp (buffer, "exit 0", 6)

	      /* Don't let mail prepend a `>'.  */
	      && strncmp (buffer, "From", 4)
#else
	      && (buffer[0] != here_delimiter[0]
		  || strncmp (buffer, here_delimiter, here_delimiter_length))

	      /* unshar -e: avoid `exit 0'.  */
	      && (buffer[0] != 'e' || strncmp (buffer, "exit 0", 6))

	      /* Don't let mail prepend a `>'.  */
	      && (buffer[0] != 'F' || strncmp (buffer, "From", 4))
#endif
	      )
	    fputs (buffer, output);
	  else
	    {
	      fprintf (output, "%c%s", line_prefix, buffer);
	      remaining_size--;
	    }

	  /* Try completing an incomplete line, but not if the incomplete
	     line contains no character.  This might occur with -T for
	     incomplete files, or sometimes when switching to a new file.  */

	  if (*buffer && buffer[strlen (buffer) - 1] != '\n')
	    {
	      putc ('\n', output);
	      remaining_size--;
	    }

	  if (split_file_mode
#if MSDOS
	      /* 1 extra for CR.  */
	      && (remaining_size -= strlen (buffer) + 1) < 0
#else
	      && (remaining_size -= strlen (buffer)) < 0
#endif
	      )
	    {

	      /* Change to another file.  */

	      DEBUG_PRINT (_("New file, remaining %s, "), remaining_size);
	      DEBUG_PRINT (_("Limit still %s\n"), file_size_limit);

	      fprintf (output, "%s\n", here_delimiter);

	      /* Close the "&&" and report an error if any of the above
		 failed.  */

	      fputs (" :\n", output);
	      echo_status ("test $? -ne 0", N_("restore of %s failed\n"), NULL,
			   restore_name, 0);

	      if (check_existing_mode)
		fputs ("fi\n", output);

	      if (quiet_unshar_mode)
                {
                  sprintf (scribble,
                           N_("End of part %ld, continue with part %ld"),
			   (long)part_number, (long)part_number + 1);
                  fprintf (output, echo_string_z, scribble);
                }
	      else
		{
                  sprintf (scribble, N_("End of %s part %d"),
                           archive_name ? archive_name : N_("archive"),
                           part_number);
                  fprintf (output, echo_string_z, scribble);

                  sprintf (scribble, N_("File %s is continued in part %d"),
                           restore_name, (long)part_number + 1);
                  fprintf (output, echo_string_z, scribble);
		}

	      fprintf (output, "echo %d > ${lock_dir}/seq\nexit 0\n",
		       part_number + 1);

	      if (part_number == 1)
		{

		  /* Rewrite the info lines on the first header.  */

		  fseeko (output, archive_type_position, SEEK_SET);
		  fprintf (output, "%-75s\n%-75s\n",
		   "# This is part 1 of a multipart archive.",
		   "# Do not concatenate these parts, unpack them in order with `/bin/sh'.");
                }
	      close_output ();

	      /* Next! */

	      open_output ();

	      if (net_headers_mode)
		{
		  fprintf (output, "Submitted-by: %s\n", submitter_address);
		  fprintf (output, "Archive-name: %s%s%02d\n\n",
			   archive_name,
			   strchr (archive_name, '/') ? "" : "/part",
			   part_number ? part_number : 1);
		}

	      if (cut_mark_mode)
		fputs (cut_mark_line, output);

	      fprintf (output, continue_archive_z,
		       basename (output_filename), part_number,
		       archive_name ? archive_name : "a multipart archive",
		       restore_name, sharpid);

	      generate_configure ();

	      fprintf (output, seq_check_z,
		       N_("Please unpack part 1 first!"),
		       part_number);

              echo_status ("test $? -eq 0", NULL,
                           N_("Please unpack part '${shar_sequence}' next!"),
                           NULL, 1);

	      if (check_existing_mode)
		{
		  if (quiet_unshar_mode)
		    fprintf (output, "if test -f ${lock_dir}/new; then\n");
		  else
                    {
                      fputs ("if test ! -f ${lock_dir}/new\nthen echo 'x - ",
                             output);
                      fprintf (output, N_("STILL SKIPPING %s"), restore_name);
                      fputs ("'\nelse\n", output);
                    }
		}

	      if (!quiet_mode)
		fprintf (stderr, _("Starting file %s\n"), output_filename);
	      if (!quiet_unshar_mode)
                echo_text (N_("continuing file %s"), restore_name, 0);
	      fprintf (output, "  sed 's/^%c//' << '%s' >> ",
		       line_prefix, here_delimiter);
	      if (uuencoded_file_mode)
		fprintf (output, "${lock_dir}/uue &&\n");
	      else
		fprintf (output, "%s &&\n", restore_name);
	      remaining_size = file_size_limit;
	      split_flag = 1;
	    }
	}

      fclose (input);
      while (wait (NULL) >= 0)
	;

      fprintf (output, "%s\n", here_delimiter);
      if (split_flag && !quiet_unshar_mode)
        echo_text (N_("File %s is complete"), restore_name, 1);

      /* If this file was uuencoded w/Split, decode it and drop the temp.  */

      if (uuencoded_file_mode && inhibit_piping_mode)
	{
	  if (!quiet_unshar_mode)
            echo_text (N_("uudecoding file %s"), restore_name, 1);

	  fprintf (output, "  uudecode ${lock_dir}/uue < ${lock_dir}/uue &&\n");
	}

      /* If this file was compressed, uncompress it and drop the temp.  */

      if (compressed_file_mode)
	{
	  if (!quiet_unshar_mode)
            echo_text (N_("uncompressing file %s"), restore_name, 1);

	  fprintf (output, "  compress -d < ${lock_dir}/cmp > '%s' &&\n",
		   restore_name);
	}
      else if (gzipped_file_mode)
	{
	  if (!quiet_unshar_mode)
            echo_text (N_("gunzipping file %s"), restore_name, 1);

	  fprintf (output, "  gzip -d < ${lock_dir}/gzi > '%s' &&\n",
		   restore_name);
	}
      else if (bzipped_file_mode)
	{
	  if (!quiet_unshar_mode)
            echo_text (N_("bunzipping file %s"), restore_name, 1);

	  fprintf (output, "  bzip2 -d < ${lock_dir}/bzi > '%s' &&\n",
		   restore_name);
	}
    }

  if (timestamp_mode)
    {

      /* Set the dates as they were.  */

      restore_time = localtime (&struct_stat.st_mtime);
      fprintf (output, "\
  (set %02d %02d %02d %02d %02d %02d %02d '%s'; eval \"$shar_touch\") &&\n",
	       (restore_time->tm_year + 1900) / 100,
	       (restore_time->tm_year + 1900) % 100,
	       restore_time->tm_mon + 1, restore_time->tm_mday,
	       restore_time->tm_hour, restore_time->tm_min,
	       restore_time->tm_sec, restore_name);
    }

  if (vanilla_operation_mode)
    {

      /* Close the "&&" and report an error if any of the above
	 failed.  */
      fputs (":\n", output);
      echo_status ("test $? -ne 0", N_("restore of %s failed"), NULL,
		   restore_name, 0);
    }
  else
    {
      unsigned char md5buffer[16];
      FILE *fp = NULL;
      int did_md5 = 0;

      /* Set the permissions as they were.  */

      fprintf (output, "  chmod %04o '%s'\n",
	       (unsigned) (struct_stat.st_mode & 0777), restore_name);

      /* Report an error if any of the above failed.  */

      echo_status ("test $? -ne 0", N_("restore of %s failed"), NULL,
		   restore_name, 0);

      if (md5_count_mode && (fp = fopen (local_name, "r")) != NULL
	  && md5_stream (fp, md5buffer) == 0)
	{
	  /* Validate the transferred file using `md5sum' command.  */
	  size_t cnt;
	  did_md5 = 1;

	  fprintf (output, md5test_z, restore_name,
		   N_("MD5 check failed"), here_delimiter);

	  for (cnt = 0; cnt < 16; ++cnt)
	    fprintf (output, "%02x", md5buffer[cnt]);

	  fprintf (output, " %c%s\n%s\n",
		   ' ', restore_name, here_delimiter);
	  /* This  ^^^ space is not necessarily a parameter now.  But it
	     is a flag for binary/text mode and will perhaps be used later.  */
	}

      if (fp != NULL)
	fclose (fp);

      if (character_count_mode)
	{
	  /* Validate the transferred file using simple `wc' command.  */

	  FILE *pfp;
	  char command[BUFSIZ];

	  sprintf (command, "LC_ALL=C wc -c < '%s'", local_name);
	  if (pfp = popen (command, "r"), pfp)
	    {
	      char wc[BUFSIZ];

	      if (did_md5)
		{
		  fputs ("  else\n", output);
		}

              {
                char* pz = wc;
                for (;;)
                  {
		    int ch = (unsigned int)fgetc (pfp);
		    if (! isspace (ch))
		      {
			*(pz++) = ch;
			break;
		      }
                  }
                if (pz[-1] != NUL) do
                  {
                    int ch = (unsigned int)fgetc (pfp);
                    if ((ch < 0) || isspace (ch))
                      break;
                    *pz = ch;
                  } while (++pz < wc+BUFSIZ-1);
                *pz = NUL;
              }

	      fprintf (output,
                       "test `LC_ALL=C wc -c < '%s'` -ne %s && \\\n  ${echo} ",
                       restore_name, wc);
              fprintf (output,
                       N_("'restoration warning:  size of %s is not %s'\n"),
                       restore_name, wc);
	      pclose (pfp);
	    }
	}
      if (did_md5)
	fputs ("  fi\n", output);
    }

  /* If the exists option is in place close the if.  */

  if (check_existing_mode)
    fputs ("fi\n", output);

  return 0;
}

/* Main control.  */

/*-----------------------------------------------------------------------.
| Set file mode, accepting a parameter 'M' for mixed uuencoded mode, 'B' |
| for uuencoded mode, 'z' for gzipped mode, 'j' for bzipped mode, or 'Z' |
| for compressed mode.							 |
| Any other value yields text mode.					 |
`-----------------------------------------------------------------------*/

static void
set_file_mode (mode)
     int mode;
{
  if (mode == 'B' && uuencoded_file_mode)
    /* Selecting uuencode mode should not contradict compression.  */
    return;

  mixed_uuencoded_file_mode = mode == 'M';
  uuencoded_file_mode = mode == 'B';
  gzipped_file_mode = mode == 'z';
  bzipped_file_mode = mode == 'j';
  compressed_file_mode = mode == 'Z';

  if (gzipped_file_mode || bzipped_file_mode || compressed_file_mode)
    uuencoded_file_mode = 1;
}

/*-------------------------------------------.
| Open the next output file, or die trying.  |
`-------------------------------------------*/

static void
open_output ()
{
  if (output_filename == NULL)
    error (EXIT_FAILURE, ENXIO, _("allocating output file name"));

  sprintf(output_filename, output_base_name, ++part_number);
  output = fopen (output_filename, "w");

  if (!output)
    error (EXIT_FAILURE, errno, _("Opening `%s'"), output_filename);
}

/*-----------------------------------------------.
| Close the current output file, or die trying.	 |
`-----------------------------------------------*/

static void
close_output ()
{
  if (fclose (output) != 0)
    error (EXIT_FAILURE, errno, _("Closing `%s'"), output_filename);
}

/*----------------------------------.
| Output a command format message.  |
`----------------------------------*/

static void
usage (status)
     int status;
{
  if (status != EXIT_SUCCESS)
    fprintf (stderr, _("Try `%s --help' for more information.\n"),
	     program_name);
  else
    {
      printf (_("Usage: %s [OPTION]... [FILE]...\n"), program_name);
      fputs (_("\
Mandatory arguments to long options are mandatory for short options too.\n"),
	     stdout);
      fputs (_("\
\n\
Giving feedback:\n\
      --help              display this help and exit\n\
      --version           output version information and exit\n\
  -q, --quiet, --silent   do not output verbose messages locally\n"),
	     stdout);
#if HAVE_COMPRESS
      fputs (_("\
\n\
Selecting files:\n\
  -p, --intermix-type     allow -[BTzZ] in file lists to change mode\n\
  -S, --stdin-file-list   read file list from standard input\n"),
	     stdout);
#else
      fputs (_("\
\n\
Selecting files:\n\
  -p, --intermix-type     allow -[BTz] in file lists to change mode\n\
  -S, --stdin-file-list   read file list from standard input\n"),
	     stdout);
#endif
      fputs (_("\
\n\
Splitting output:\n\
  -o, --output-prefix=PREFIX    output to file PREFIX.01 through PREFIX.NN\n\
  -l, --whole-size-limit=SIZE   split archive, not files, to SIZE kilobytes\n\
  -L, --split-size-limit=SIZE   split archive, or files, to SIZE kilobytes\n"),
	     stdout);
      fputs (_("\
\n\
Controlling the shar headers:\n\
  -n, --archive-name=NAME   use NAME to document the archive\n\
  -s, --submitter=ADDRESS   override the submitter name\n\
  -a, --net-headers         output Submitted-by: & Archive-name: headers\n\
  -c, --cut-mark            start the shar with a cut line\n\
  -t, --translate           translate messages in the script\n\
\n\
Selecting how files are stocked:\n\
  -M, --mixed-uuencode         dynamically decide uuencoding (default)\n\
  -T, --text-files             treat all files as text\n\
  -B, --uuencode               treat all files as binary, use uuencode\n\
  -z, --gzip                   gzip and uuencode all files\n\
  -g, --level-for-gzip=LEVEL   pass -LEVEL (default 9) to gzip\n\
  -j, --bzip2                  bzip2 and uuencode all files\n"),
	     stdout);
#if HAVE_COMPRESS
      fputs (_("\
  -Z, --compress               compress and uuencode all files\n\
  -b, --bits-per-code=BITS     pass -bBITS (default 12) to compress\n"),
	     stdout);
#endif
      fputs (_("\
\n\
Protecting against transmission:\n\
  -w, --no-character-count      do not use `wc -c' to check size\n\
  -D, --no-md5-digest           do not use `md5sum' digest to verify\n\
  -F, --force-prefix            force the prefix character on every line\n\
  -d, --here-delimiter=STRING   use STRING to delimit the files in the shar\n\
\n\
Producing different kinds of shars:\n\
  -V, --vanilla-operation   produce very simple and undemanding shars\n\
  -P, --no-piping           exclusively use temporary files at unshar time\n\
  -x, --no-check-existing   blindly overwrite existing files\n\
  -X, --query-user          ask user before overwriting files (not for Net)\n\
  -m, --no-timestamp        do not restore file modification dates & times\n\
  -Q, --quiet-unshar        avoid verbose messages at unshar time\n\
  -f, --basename            restore in one directory, despite hierarchy\n\
      --no-i18n             do not produce internationalized shell script\n"),
	     stdout);
      fputs (_("\n\
Option -o is required with -l or -L, option -n is required with -a.\n"),
	     stdout);
#if HAVE_COMPRESS
      fputs (_("Option -g implies -z, option -b implies -Z.\n"),
	     stdout);
#else
      fputs (_("Option -g implies -z.\n"),
	     stdout);
#endif
      /* TRANSLATORS: add the contact address for your translation team! */
      printf (_("Report bugs to <%s>.\n"), PACKAGE_BUGREPORT);
    }
  exit (status);
}

/*--------------------------------------.
| Decode options and launch execution.  |
`--------------------------------------*/

static const struct option long_options[] =
{
  {"archive-name", required_argument, NULL, 'n'},
  {"basename", no_argument, NULL, 'f'},
  {"bits-per-code", required_argument, NULL, 'b'},
  {"bzip2", no_argument, NULL, 'j'},
  {"compress", no_argument, NULL, 'Z'},
  {"cut-mark", no_argument, NULL, 'c'},
  {"force-prefix", no_argument, NULL, 'F'},
  {"gzip", no_argument, NULL, 'z'},
  {"here-delimiter", required_argument, NULL, 'd'},
  {"intermix-type", no_argument, NULL, 'p'},
  {"level-for-gzip", required_argument, NULL, 'g'},
  {"mixed-uuencode", no_argument, NULL, 'M'},
  {"net-headers", no_argument, NULL, 'a'},
  {"no-character-count", no_argument, &character_count_mode, 0},
  {"no-check-existing", no_argument, NULL, 'x'},
  {"no-i18n", no_argument, &no_i18n, 1},
  {"no-md5-digest", no_argument, &md5_count_mode, 0},
  {"no-piping", no_argument, NULL, 'P'},
  {"no-timestamp", no_argument, NULL, 'm'},
  {"output-prefix", required_argument, NULL, 'o'},
  {"print-text-domain-dir", no_argument, &print_text_dom_dir, 1},
  {"query-user", no_argument, NULL, 'X'},
  {"quiet", no_argument, NULL, 'q'},
  {"quiet-unshar", no_argument, NULL, 'Q'},
  {"split-size-limit", required_argument, NULL, 'L'},
  {"stdin-file-list", no_argument, NULL, 'S'},
  {"submitter", required_argument, NULL, 's'},
  {"text-files", no_argument, NULL, 'T'},
  {"translate", no_argument, NULL, 't'},
  {"uuencode", no_argument, NULL, 'B'},
  {"vanilla-operation", no_argument, NULL, 'V'},
  {"whole-size-limit", required_argument, NULL, 'l'},

  {"help", no_argument, &show_help, 1},
  {"version", no_argument, &show_version, 1},

  { NULL, 0, NULL, 0 },
};

/* Limit file sizes to LIMIT KiB.  */

static void
set_file_size_limit (char const *limit)
{
  char *numend;
  intmax_t lim = strtoimax (limit, &numend, 10);
  if (*numend || ! (0 < lim && lim <= TYPE_MAXIMUM (off_t) / 1024))
    error (EXIT_FAILURE, 0, _("invalid file size limit `%s'"), limit);
  lim *= 1024;
  if (! file_size_limit || lim < file_size_limit)
    file_size_limit = lim;
}


char *
parse_output_base_name(char *arg)
{
  int c;
  int hadarg = 0;
  char *fmt, *p;
  int base_name_len = 128;

  for (p = arg ; (c = *p++) != 0; )
    {
      base_name_len++;
      if (c != '%')
	continue;
      c = *p++;
      if (c == '%')
	continue;
      if (hadarg)
	return NULL;
      while (c != 0 && strchr("#0+- 'I", c) != 0)
	c = *p++;
      if (c == 0)
	return NULL;
      if (c >= '0' && c <= '9')
	{
	  long v;
	  errno = 0;
	  v = strtol(p-1, &fmt, 10);
	  if ((v == 0) || (v > 16) || (errno != 0))
	    {
	      fprintf(stderr, _("invalid format (count field too wide): '%s'\n"),
		      arg);
	      return NULL;
	    }
	  p = fmt;
	  c = *p++;
	  base_name_len += v;
	}
      if (c == '.')
	{
	  c = *p++;
	  while (c != 0 && c >= '0' && c <= '9')
	    c = *p++;
	}
      if (c == 0 || strchr("diouxX", c) == 0)
	return NULL;
      hadarg = 1;
    }
  fmt = xmalloc(strlen(arg) + (hadarg ? 1 : 6));
  strcpy(fmt, arg);
  if (!hadarg)
    strcat(fmt, ".%02d");
  output_filename = xmalloc(base_name_len);
  return fmt;
}


/*---.
| ?  |
`---*/

int
main (argc, argv)
     int argc;
     char *const *argv;
{
  int status = EXIT_SUCCESS;
  int stdin_file_list = 0;
  int optchar;

  program_name = argv[0];
  sharpid = (int) getpid ();
  setlocale (LC_ALL, "");

  /* Set the text message domain.  */
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  while (optchar = getopt_long (argc, argv,
				"+$BDFL:MPQSTVXZab:cd:fg:hjl:mn:o:pqs:wxz",
				long_options, NULL),
	 optchar != EOF)
    switch (optchar)
      {
      case 0: case 1: /* handled by getopt_long() */
	break;

      case '$':
#if DEBUG
	debugging_mode = 1;
#else
	error (0, 0, _("DEBUG was not selected at compile time"));
#endif
	break;

      case 'B':
	set_file_mode ('B');
	break;

      case 'D':
	md5_count_mode = 0;
	break;

      case 'F':
	mandatory_prefix_mode = 1;
	break;

      case 'L':
	set_file_size_limit (optarg);
	split_file_mode = 1;
	inhibit_piping_mode = 1;
	DEBUG_PRINT (_("Hard limit %s\n"), file_size_limit);
	break;

      case 'M':
	set_file_mode ('M');
	break;

      case 'P':
	inhibit_piping_mode = 1;
	break;

      case 'Q':
	quiet_unshar_mode = 1;
	break;

      case 'S':
	stdin_file_list = 1;
	break;

      case 'V':
	vanilla_operation_mode = 1;
	break;

      case 't':
        translate_script = 1;
        break;

      case 'T':
	set_file_mode ('T');
	break;

      case 'X':
	query_user_mode = 1;
	check_existing_mode = 1;
	break;

      case 'b':
	bits_per_compressed_byte = atoi (optarg);
	/* Fall through.  */

      case 'Z':
#ifndef HAVE_COMPRESS
	error (EXIT_FAILURE, 0, _("\
This system doesn't support -Z ('compress'), use -z instead"));
#endif
	set_file_mode ('Z');
	break;

      case 'a':
	net_headers_mode = 1;
	break;

      case 'c':
	cut_mark_mode = 1;
	break;

      case 'd':
	here_delimiter = optarg;
	break;

      case 'f':
	basename_mode = 1;
	break;

      case 'h':
	usage (EXIT_SUCCESS);
	break;

      case 'j':
	set_file_mode ('j');
	break;

      case 'l':
	set_file_size_limit (optarg);
	split_file_mode = 0;
	DEBUG_PRINT (_("Soft limit %s\n"), file_size_limit);
	break;

      case 'm':
	timestamp_mode = 0;
	break;

      case 'n':
	archive_name = optarg;
	break;

      case 'o':
        output_base_name = parse_output_base_name(optarg);
        if (!output_base_name)
	  {
	    fprintf (stderr, _("illegal output prefix\n"));
	    exit (EXIT_FAILURE);
	  }
	part_number = 0;
	open_output ();
	break;

      case 'p':
	intermixed_parameter_mode = 1;
	break;

      case 'q':
	quiet_mode = 1;
	break;

      case 's':
	submitter_address = optarg;
	break;

      case 'w':
	character_count_mode = 0;
	break;

      case 'x':
	check_existing_mode = 0;
	break;

      case 'g':
	gzip_compression_level = atoi (optarg);
	/* Fall through.  */

      case 'z':
	set_file_mode ('z');
	break;

      default:
	usage (EXIT_FAILURE);
      }

  /* Internationalized shell scripts are not vanilla.  */
  if (vanilla_operation_mode)
    no_i18n = 1;

  if (show_version)
    {
      printf ("%s (GNU %s) %s\n", basename (program_name), PACKAGE, VERSION);
      /* xgettext: no-wrap */
      printf (_("Copyright (C) %s Free Software Foundation, Inc.\n\
This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
"),
	      "1994, 1995, 1996, 2004, 2005");
      exit (EXIT_SUCCESS);
    }

  if (show_help)
    usage (EXIT_SUCCESS);

  if (print_text_dom_dir != 0)
    {
      /* Support for internationalized shell scripts is only usable with
	 GNU gettext.  If we don't use it simply mark it as not available.  */
#if !defined ENABLE_NLS || defined HAVE_CATGETS \
    || (defined HAVE_GETTEXT && !defined __USE_GNU_GETTEXT)
      exit (EXIT_FAILURE);
#else
      puts (LOCALEDIR);
      exit (EXIT_SUCCESS);
#endif
    }

  line_prefix = (here_delimiter[0] == DEFAULT_LINE_PREFIX_1
		 ? DEFAULT_LINE_PREFIX_2
		 : DEFAULT_LINE_PREFIX_1);

  here_delimiter_length = strlen (here_delimiter);

  if (vanilla_operation_mode)
    {
      if (mixed_uuencoded_file_mode < 0)
	set_file_mode ('T');

      /* Implies -m, -w, -D, -F and -P.  */

      timestamp_mode = 0;
      character_count_mode = 0;
      md5_count_mode = 0;
      mandatory_prefix_mode = 1;
      inhibit_piping_mode = 1;

      /* Forbids -X.  */

      if (query_user_mode)
	{
	  error (0, 0, _("WARNING: No user interaction in vanilla mode"));
	  query_user_mode = 0;
	}

      /* Diagnose if not in -T state.  */

      if (mixed_uuencoded_file_mode
	  || uuencoded_file_mode
	  || gzipped_file_mode
	  || bzipped_file_mode
	  || compressed_file_mode
	  || intermixed_parameter_mode)
	error (0, 0, _("WARNING: Non-text storage options overridden"));
    }

  /* Set defaults for unset options.  */

  if (mixed_uuencoded_file_mode < 0)
    set_file_mode ('M');

  if (!submitter_address)
    submitter_address = get_submitter (NULL);

  if (!output)
    output = stdout;

  /* Maybe prepare to decide dynamically about file type.  */

  if (mixed_uuencoded_file_mode || intermixed_parameter_mode)
    {
      memset ((char *) byte_is_binary, 1, 256);
      byte_is_binary['\b'] = 0;
      byte_is_binary['\t'] = 0;
      byte_is_binary['\f'] = 0;
      memset ((char *) byte_is_binary + 32, 0, 127 - 32);
    }

  /* Maybe read file list from standard input.  */

  if (stdin_file_list)
    {
      char stdin_buf[258];	/* FIXME: No fix limit in GNU... */
      char **list;
      int max_argc;

      argc = 0;
      max_argc = 32;
      list = (char **) xmalloc (max_argc * sizeof (char *));
      stdin_buf[0] = 0;
      while (fgets (stdin_buf, sizeof (stdin_buf), stdin))
	{
	  if (argc == max_argc)
	    list = (char **) xrealloc (list,
				       (max_argc *= 2) * sizeof (char *));
	  if (stdin_buf[0] != NUL)
	    stdin_buf[strlen (stdin_buf) - 1] = 0;
	  list[argc] = xstrdup (stdin_buf);
	  ++argc;
	  stdin_buf[0] = 0;
	}
      argv = list;
      optind = 0;
    }

  /* Diagnose various usage errors.  */

  if (optind >= argc)
    {
      error (0, 0, _("No input files"));
      usage (EXIT_FAILURE);
    }

  if (net_headers_mode && !archive_name)
    {
      error (0, 0, _("Cannot use -a option without -n"));
      usage (EXIT_FAILURE);
    }

  if (file_size_limit && !part_number)
    {
      error (0, 0, _("Cannot use -l or -L option without -o"));
      usage (EXIT_FAILURE);
    }

  /* Start making the archive file.  */

  generate_full_header (argc - optind, &argv[optind]);

  if (query_user_mode)
    {
      quiet_unshar_mode = 0;
      if (net_headers_mode)
	error (0, 0, _("PLEASE avoid -X shars on Usenet or public networks"));

      fputs ("shar_wish=\n", output);
    }

  first_file_position = ftello (output);

  /* Process positional parameters and files.  */

  scribble = xmalloc (scribble_size);

  for (; optind < argc; optind++)
    {
      char* arg = argv[optind];

      if (intermixed_parameter_mode && (arg[0] == '-') && (arg[2] == NUL))
        {
          switch (arg[1]) {
          case 'B':
          case 'T':
          case 'M':
          case 'z':
          case 'Z':
            set_file_mode (arg[1]);
            continue;

          case 'C':
            set_file_mode ('Z');
            continue;
          }
        }

      if (walktree (shar, arg))
        status = EXIT_FAILURE;
    }

  /* Delete the sequence file, if any.  */

  if (split_file_mode && part_number > 1)
    {
      fprintf (output, "$echo '%s'\n",
	       N_("You have unpacked the last part"));
      if (quiet_mode)
	fprintf (stderr, _("Created %d files\n"), part_number);
    }

  {
    const char* did_pz =
      N_("x - removed lock directory `'%s\\''.");
    const char* not_pz =
      N_("x - failed to remove lock directory `'%s\\''.");

    echo_status ("rm -fr ${lock_dir}", did_pz, not_pz, "${lock_dir}", 1);
  }
  fputs ("exit 0\n", output);
  exit (status);
}
/*
 * Local Variables:
 * mode: C
 * c-file-style: "gnu"
 * indent-tabs-mode: nil
 * End:
 * end of agen5/autogen.c */
