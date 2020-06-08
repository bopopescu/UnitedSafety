## Makefile for the support lib subdirectory of the GNU Shar utilities.
## Copyright (C) 1995, 1996, 2002 Free Software Foundation, Inc.
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2, or (at your option)
## any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

## Process this file with automake to produce Makefile.in.

noinst_LIBRARIES = libshar.a

EXTRA_DIST = gen-uio alloca.c inttostr.c md5.c memset.c mktime.c \
stpcpy.c strftime.c strtol.c

BUILT_SOURCES = unlocked-io.h
CLEANFILES = unlocked-io.h

libshar_a_SOURCES = basename.c error.c getopt.c getopt1.c md5.c offtostr.c \
whoami.c xgetcwd.c xmalloc.c xstrdup.c

libshar_a_LIBADD = $(ALLOCA) $(LIBOBJS)

noinst_HEADERS = basename.h error.h exit.h getopt.h gettext.h inttostr.h \
liballoca.h md5.h pathmax.h stpcpy.h system.h xalloc.h xgetcwd.h

AM_CPPFLAGS = -I.. -I$(srcdir) -I../intl
DEFS = -DLIBDIR=\"$(libdir)\" @DEFS@

error$U.o: unlocked-io.h

unlocked-io.h: $(srcdir)/gen-uio Makefile.am
	srcdir=$(srcdir) $(SHELL) $(srcdir)/gen-uio $(io_functions) > $@t
	mv $@t $@
