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

test_partitions_LDADD = \
	libebgenv-test_partitions.a \
	-lcmocka

libebgenv-test_partitions.a: $(top_builddir)/libebgenv.a
	$(AM_V_GEN) $(OBJCOPY) -W probe_config_file -W ped_device_probe_all \
	  -W ped_device_get_next -W ped_disk_next_partition $< $@

CLEANFILES += libebgenv-test_partitions.a

test_environment_LDADD = \
	libebgenv-test_environment.a \
	-lcmocka

libebgenv-test_environment.a: $(top_builddir)/libebgenv.a
	$(AM_V_GEN) $(OBJCOPY) -W oldenvs -W configparts -W fopen -W fclose \
	  -W fread -W fwrite -W feof $< $@

CLEANFILES += libebgenv-test_environment.a

test_api_LDADD = \
	libebgenv-test_api.a \
	-lcmocka

libebgenv-test_api.a: $(top_builddir)/libebgenv.a
	$(AM_V_GEN) $(OBJCOPY) -W bgenv_init -W bgenv_write -W bgenv_close \
	  -W bgenv_get_latest -W bgenv_get_by_index -W bgenv_get_oldest $< $@

CLEANFILES += libebgenv-test_api.a

check_PROGRAMS = test_partitions test_environment test_api

TESTS = $(check_PROGRAMS)
