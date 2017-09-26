#
# EFI Boot Guard
#
# Copyright (c) Siemens AG, 2017
#
# Authors:
#  Jan Kiszka <jan.kiszka@siemens.com>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#

OBJCOPY ?= objcopy

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/swupdate-adapter \
	-I$(top_srcdir)/tools

AM_CFLAGS = \
	-Wno-unused-parameter \
	-Wmissing-prototypes \
	-fshort-wchar \
	-DHAVE_ENDIAN_H \
	-D_GNU_SOURCE \
	-g

CLEANFILES =

libebgenv-test.a: $(top_builddir)/libebgenv.a
	$(AM_V_GEN) $(OBJCOPY) --weaken $< $@

CLEANFILES += libebgenv-test.a

test_partitions_LDADD = \
	libebgenv-test.a \
	-lcmocka

test_environment_LDADD = \
	libebgenv-test.a \
	-lcmocka

test_api_LDADD = \
	libebgenv-test.a \
	-lcmocka

check_PROGRAMS = test_partitions test_environment test_api

TESTS = $(check_PROGRAMS)
