/* lockfile-progs.c

   Copyright 1998-2009 Rob Browning <rlb@defaultvalue.org>

   This code is covered under the terms of the Gnu Public License.
   See the accompanying COPYING file for details.

  To do:

   It might be useful at some point to support a --user option to
   mail-lock that can only be used by the superuser (of course, they
   could just use lockfile-create with an appropriate path...

*/

#define _GNU_SOURCE

#include <errno.h>
#include <lockfile.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <stdarg.h>
#include <string.h>
#include <getopt.h>
#include <pwd.h>
#include <sys/types.h>

static const char *action = NULL;
static char *target_file = NULL;
static int retry_count_specified = 0;
static int retry_count = 9; /* This will be a maximum of 3 minutes */
static int touchlock_oneshot = 0;
static int use_pid = 0;

/* not used yet, so not documented... */
static int lockfile_verbosity = 1;
static int lockfile_add_dot_lock_to_name = 1;

static volatile int exit_status = 0;

static int
msg(FILE *f, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));

static int
msg(FILE *f, const char *fmt, ...)
{
  int rc = 0;
  if(lockfile_verbosity > 0)
  {
    va_list args;
    va_start(args, fmt);
    rc = vfprintf(f, fmt, args);
    va_end(args);
  }
  return rc;
}

static void
chk(const int test, const char *fmt, ...)
  __attribute__ ((format (printf, 2, 3)));

static void
chk(const int test, const char *fmt, ...)
{
  if(!test)
  {
    int rc = 0;
    if(lockfile_verbosity > 0)
    {
      va_list args;
      va_start(args, fmt);
      rc = vfprintf(stderr, fmt, args);
      va_end(args);
    }
    exit(1);
  }
}

static void
usage(const char *command_name, FILE *file)
{
  if(strcmp(command_name, "mail-lock") == 0)
  {
    msg(file, "usage: mail-lock [--use-pid] [--retry retry-count]\n");
  }
  else if(strcmp(command_name, "mail-unlock") == 0)
  {
    msg(file, "usage: mail-unlock\n");
  }
  else if(strcmp(command_name, "mail-touchlock") == 0)
  {
    msg(file, "usage: mail-touchlock [--oneshot]\n");
  }
  else if(strcmp(command_name, "lockfile-create") == 0)
  {
    msg(file,"usage: lockfile-create"
        " [--use-pid] [--retry retry-count] [--lock-name] file\n");
  }
  else if(strcmp(command_name, "lockfile-remove") == 0)
  {
    msg(file, "usage: lockfile-remove [--lock-name] file\n");
  }
  else if(strcmp(command_name, "lockfile-touch") == 0)
  {
    msg(file, "usage: lockfile-touch [--oneshot] [--lock-name] file\n");
  }
  else if(strcmp(command_name, "lockfile-check") == 0)
  {
    msg(file, "usage: lockfile-check [--use-pid] [--lock-name] file\n");
  }
  else
  {
    msg(stderr, "lockfile: big problem - unknown command name: %s\n",
        command_name);
    exit(1);
  }
}

static void
parse_arguments(const int argc, char *argv[]) {

  int usage_error = 0;
  int opt_result;
  const char *short_opts = "r:olqv";
  struct option long_opts[] = {
    { "retry", required_argument, NULL, 'r' },
    { "oneshot", no_argument, NULL, 'o' },
    { "use-pid", no_argument, NULL, 'p' },
    { "lock-name", no_argument, NULL, 'l' },
    { "quiet", no_argument, NULL, 'q' },
    { "verbose", no_argument, NULL, 'v' },
    { NULL, 0, NULL, 0 }
  };

  char *cmd_name = rindex(argv[0], '/');
  int mail_cmd_p = 0;

  if(cmd_name != NULL) {
    /* Skip the '/' */
    cmd_name++;
  } else {
    cmd_name = argv[0];
  }

  while((opt_result = getopt_long(argc, argv,
                                  short_opts, long_opts, NULL)) != -1)
  {
    switch(opt_result)
    {
      case 'o':
        touchlock_oneshot = 1;
        break;
      case 'p':
        use_pid = 1;
        break;
      case 'q':
        lockfile_verbosity = 0;
        break;
      case 'v':
        lockfile_verbosity = 2;
        break;
      case 'l':
        lockfile_add_dot_lock_to_name = 0;
        break;
      case 'r':
        {
          char *rest_of_string;
          long tmp_value = strtol(optarg, &rest_of_string, 10);

          retry_count_specified = 1;

          if((tmp_value == 0) && (rest_of_string == optarg))
          {
            /* Bad value */
            msg(stderr, "%s: bad retry-count value\n", cmd_name);
            usage(cmd_name, stderr);
            exit(1);
          }
          else
            retry_count = tmp_value;
        }
        break;
      case '?':
        usage(cmd_name, stderr);
        exit(1);
        break;
      default:
        msg(stderr, "%s: getopt returned impossible value 0%o.\n",
            cmd_name, opt_result);
        exit(1);
        break;
    }
  }

  if(strcmp(cmd_name, "mail-lock") == 0)
  {
    action = "lock";
    mail_cmd_p = 1;
  }
  else if(strcmp(cmd_name, "mail-unlock") == 0)
  {
    action = "unlock";
    mail_cmd_p = 1;
  }
  else if(strcmp(cmd_name, "mail-touchlock") == 0)
  {
    action = "touch";
    mail_cmd_p = 1;
  }
  else if(strcmp(cmd_name, "lockfile-create") == 0)
    action = "lock";
  else if(strcmp(cmd_name, "lockfile-remove") == 0)
    action = "unlock";
  else if(strcmp(cmd_name, "lockfile-touch") == 0)
    action = "touch";
  else if(strcmp(cmd_name, "lockfile-check") == 0)
    action = "check";
  else
    usage_error = 1;

  if(retry_count_specified && (strcmp("lock", action) != 0))
    usage_error = 1;

  if(use_pid
     && (strcmp("lock", action) != 0)
     && (strcmp("check", action) != 0))
    usage_error = 1;

  if(touchlock_oneshot && (strcmp(action, "touch") != 0))
    usage_error = 1;

  if(mail_cmd_p && lockfile_add_dot_lock_to_name)
    usage_error = 1;

  if(usage_error)
  {
    usage(cmd_name, stderr);
    exit(1);
  }

  if(mail_cmd_p)
  {
    if(optind == argc) {
      uid_t user_id = geteuid();
      struct passwd *user_info = getpwuid(user_id);

      if(user_info == NULL) {
        msg(stderr, "%s: fatal error, can't find info for user id %ud\n",
            cmd_name, user_id);
        exit(1);
      }

      if(asprintf(&target_file, "/var/spool/mail/%s",
                  user_info->pw_name) == -1) {
        msg(stderr, "asprintf failed: line %d\n", __LINE__);
        exit(1);
      }
    } else {
      usage(cmd_name, stderr);
      exit(1);
    }
  } else {
    if((argc - optind) != 1) {
      usage(cmd_name, stderr);
      exit(1);
    }
    target_file = argv[optind];
  }
}


static void
handle_touchdeath(int sig)
{
  exit(exit_status);
}


/* Must be called right after liblockfile call (b/c it calls strerror())*/
static char*
get_status_code_string(int status)
{
  switch (status)
  {
    case L_SUCCESS:
      return strdup("success");
      break;

    case L_NAMELEN:
      return strdup("name too long");
      break;

    case L_TMPLOCK:
      return strdup("cannot create temporary lockfile");
      break;

    case L_TMPWRITE:
      return strdup("cannot write PID lockfile");
      break;

    case L_MAXTRYS:
      return strdup("exceeded maximum number of lock attempts");
      break;

    case L_ERROR:
      return strdup(strerror(errno));;
      break;

    default:
      return 0L;
      break;
  }
}


static int
cmd_unlock(const char *lockfilename)
{
  int rc = lockfile_remove(lockfilename);
  if((rc != L_SUCCESS) && (lockfile_verbosity > 0))
    perror("lockfile removal failed");
  return rc;
}


static int
cmd_lock(const char *lockfilename, int retry_count)
{
  int rc = lockfile_create(lockfilename, retry_count, (use_pid ? L_PID : 0));
  const char *rc_str = get_status_code_string(rc);

  if(rc != L_SUCCESS)
    msg(stderr, "lockfile creation failed: %s\n", rc_str);

  if(rc_str) free((void *) rc_str);
  return rc;
}


static int
cmd_touch(const char *lockfilename, int touchlock_oneshot)
{
  int rc = 0;
  signal(SIGTERM, handle_touchdeath);

  if(touchlock_oneshot)
    rc = lockfile_touch(lockfilename);
  else
  {
    while(1 && (rc == 0))
    {
      rc = lockfile_touch(lockfilename);
      sleep(60);
    }
  }

  return rc;
}


static int
cmd_check(const char *lockfilename)
{
  int rc = lockfile_check(lockfilename, (use_pid ? L_PID : 0));
  return rc;
}


int
main(int argc, char *argv[])
{
  const char *lock_name_pattern = "%s";
  char *lockfilename = NULL;

  chk(L_SUCCESS == 0, "liblockfile's L_SUCCESS != 0 (aborting)");

  parse_arguments(argc, argv);

  if(lockfile_add_dot_lock_to_name)
    lock_name_pattern = "%s.lock";

  if(asprintf(&lockfilename, lock_name_pattern, target_file) == -1)
  {
    msg(stderr, "asprintf failed: line %d\n", __LINE__);
    exit(1);
  }

  if(strcmp(action, "unlock") == 0)
    exit_status = cmd_unlock(lockfilename);
  else if(strcmp(action, "lock") == 0)
    exit_status = cmd_lock(lockfilename, retry_count);
  else if(strcmp(action, "touch") == 0)
    exit_status = cmd_touch(lockfilename, touchlock_oneshot);
  else if(strcmp(action, "check") == 0)
    exit_status = cmd_check(lockfilename);

  if(lockfilename) free(lockfilename);
  return(exit_status);
}
