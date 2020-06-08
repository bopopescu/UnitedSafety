# Configure template for GNU shar utilities.
# Copyright (C) 1994, 1995, 1996, 2002, 2005 Free Software Foundation, Inc.
# Process this file with autoconf to produce a configure script.
dnl
dnl FIXME: AC_CHECK_HEADERS([sys/time.h])
dnl FIXME: AC_CHECK_FUNCS([gethostname getwd])
dnl
m4_include([version.m4])
AC_INIT([sharutils], sharutils_version, sharutils_eaddr)
AC_CONFIG_SRCDIR([src/shar.c])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([1.9 gnits dist-bzip2])
AC_SUBST(DIST_ALPHA)

dnl AC_ARG_PROGRAM

AC_SUBST(ADD_SCRIPT, ["mail-files mailshar"])

AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_RANLIB

dnl FIXME: Check that it is GNU diff indeed.
# Carefully avoid gdiff for X as found on SGI systems.
if (DISPLAY= gdiff /dev/null /dev/null) 2> /dev/null; then
  AC_PATH_PROGS(DIFF, gnudiff diffgnu gdiff diff, no)
else
  AC_PATH_PROGS(DIFF, gnudiff diffgnu diff, no)
fi

dnl FIXME: Check that mailer accepts -s.
PATH_save="$PATH"; PATH="$PATH:/usr/sbin:/usr/ucb:/usr/bin:/bin"
AC_PATH_PROGS(MAILER, Mail mailx elm mush mail, no)
PATH="$PATH_save"

dnl FIXME: Check that perl was found.
AC_PATH_PROG(PERL, perl, no)

AC_PATH_PROGS(SH, bash sh, no)

dnl FIXME: Check that it is GNU tar indeed.
AC_PATH_PROGS(TAR, gnutar targnu gtar tar, tar)

if test $ac_cv_path_PERL != no; then
  ADD_SCRIPT="$ADD_SCRIPT remsync"
  AC_CONFIG_FILES([src/remsync])
fi

dnl in m4/sharutils.m4
ke_CHECK_COMPRESS_AND_LINK

AC_AIX
AC_ISC_POSIX
AC_MINIX

AC_SYS_LARGEFILE
if test $cross_compiling = no; then
  AC_C_CHAR_UNSIGNED
fi
AC_C_CONST
AC_C_INLINE
AC_C_BIGENDIAN

AC_CHECK_HEADERS(limits.h locale.h memory.h string.h sys/wait.h unistd.h)
AC_HEADER_DIRENT
[test X$ac_header_dirent = Xno && {]
  AC_MSG_ERROR([a dirent header is required.])
[}
DIRENT_HEADER=\<${ac_header_dirent}\>]
AC_DEFINE_UNQUOTED(DIRENT_HEADER, [${DIRENT_HEADER}],
         [define to directory entry header file])
AC_SUBST(DIRENT_HEADER)
AC_HEADER_STAT
AC_HEADER_STDC
AC_STRUCT_TIMEZONE
AC_TYPE_SIZE_T

jm_PREREQ_ERROR
jm_FUNC_MALLOC
jm_FUNC_REALLOC
AC_CHECK_FUNCS(fchmod getcwd isascii memcpy strchr strerror uname)
AC_FUNC_ALLOCA
AC_FUNC_CLOSEDIR_VOID
AC_FUNC_FSEEKO
AC_FUNC_STRFTIME
AC_FUNC_VPRINTF
AC_REPLACE_FUNCS(memset mktime stpcpy strftime)
jm_AC_PREREQ_XSTRTOIMAX

AC_MSG_CHECKING(for /etc/systemid)
if test -f /etc/systemid; then
  AC_MSG_RESULT(yes)
  AC_DEFINE([HAVE_ETC_SYSTEMID], 1,
            [Define if the current hostname may be found in /etc/systemid.])
else
  AC_MSG_RESULT(no)
fi

AC_ARG_ENABLE(uucode,
  [AS_HELP_STRING([--disable-uucode],
	[disable installation of uuencode and uudecode])])
AM_CONDITIONAL([UUCODE], [test "x$enable_uucode" != xno])

AM_GNU_GETTEXT
AM_GNU_GETTEXT_VERSION([0.14.5])
AM_WITH_DMALLOC

# sharutils is somewhat special about internationalization in that it
# always requires the GNU .mo files installed.
if test "x$CATOBJEXT" = "x.mo" || test "x$CATOBJEXT" = "x.cat"; then
  new_CATALOGS=""
  for cat in $CATALOGS; do
    lang=`echo $cat | sed "s/\(.*\)\..*/\1/"`
    new_CATALOGS="$new_CATALOGS $cat $lang.gmo"
  done
  CATALOGS="$new_CATALOGS"
fi

AC_DEFINE(_GNU_SOURCE, 1, [Make sure we see all GNU extensions.])

AC_CONFIG_FILES([
  contrib/Makefile
  doc/Makefile
  intl/Makefile
  lib/Makefile
  po/Makefile.in
  src/Makefile
  src/mail-files
  src/mailshar
  tests/Makefile
  Makefile
])
AC_OUTPUT
dnl Local Variables:
dnl mode: autoconf-mode
dnl tab-width: 8
dnl indent-tabs-mode: nil
dnl sh-indentation: 2
dnl sh-basic-offset: 2
dnl End:
dnl
dnl end of configure.ac
