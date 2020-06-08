## Makefile for program src directory in GNU Shar utilities package.
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

bin_PROGRAMS = shar unshar
if UUCODE
  bin_PROGRAMS += uuencode uudecode
endif
bin_SCRIPTS = $(ADD_SCRIPT)
EXTRA_SCRIPTS = compress-dummy mail-files mailshar remsync
EXTRA_DIST = scripts.def scripts.tpl scripts.x

DISTCLEANFILES = $(bin_SCRIPTS)

localedir = $(datadir)/locale

AM_CPPFLAGS = -I$(srcdir) -I.. -I$(top_srcdir)/lib -I../intl
DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

# stolen from fileutils-4.1.9:
# Sometimes, the expansion of $(LIBINTL) includes -lc which may
# include modules defining variables like `optind', so libfetish.a
# must precede $(LIBINTL) in order to ensure we use GNU getopt.
# But libshar.a must also follow $(LIBINTL), since libintl uses
# replacement functions defined in libshar.a.
LDADD = ../lib/libshar.a $(LIBINTL) ../lib/libshar.a

# Source dependecies.
shar_SOURCES = shar.c encode.c
unshar_SOURCES = unshar.c encode.c
uudecode_SOURCES = uudecode.c
uuencode_SOURCES = uuencode.c

all: exe-bits
installcheck: exe-bits

$(bin_PROGRAMS): ../lib/libshar.a

install-exec-hook: $(INSTALL_COMPRESS_LINK)

compress-link:
	test -f $(DESTDIR)$(bindir)/compress-dummy \
	  && ln -sf compress-dummy $(DESTDIR)$(bindir)/compress

exe-bits : $(bin_SCRIPTS)
	chmod ugo+x $(bin_SCRIPTS)

## src/Makefile.am ends here
