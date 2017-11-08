#
# EFI Boot Guard
#
# Copyright (c) Siemens AG, 2017
#
# Authors:
#  Andreas Reichel <andreas.reichel.ext@siemens.com>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#

OBJCOPY ?= objcopy

AM_CFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/swupdate-adapter \
	-I$(top_srcdir)/tools \
	-I$(top_srcdir)/tools/tests \
	-I$(top_srcdir)/tests/fff \
	-I$(top_srcdir) \
	-include config.h \
	-Wno-unused-parameter \
	-Wmissing-prototypes \
	-fshort-wchar \
	-DHAVE_ENDIAN_H \
	-D_GNU_SOURCE \
	-g

libtest_env_api_fat_a_SRC = \
	../../env/env_api.c \
	../../env/env_api_fat.c \
	../../tools/ebgpart.c \
	../../env/env_config_file.c \
	../../env/env_config_partitions.c \
	../../env/env_disk_utils.c \
	../../env/uservars.c

CLEANFILES =

check_LIBRARIES = libtest_env_api_fat.a
libtest_env_api_fat_a_SOURCES = $(libtest_env_api_fat_a_SRC)

libenvapi_testlib_fat.a: libtest_env_api_fat.a
	$(OBJCOPY) --weaken $^ $@

check_PROGRAMS = test_bgenv_init_retval \
		 test_probe_config_partitions \
		 test_probe_config_file \
		 test_ebgenv_api_internal \
		 test_ebgenv_api

FAT_TESTLIB=libenvapi_testlib_fat.a

SRC_TEST_COMMON=test_main.c

test_bgenv_init_retval_CFLAGS = $(AM_CFLAGS)
test_bgenv_init_retval_SOURCES = test_bgenv_init_retval.c $(SRC_TEST_COMMON)
test_bgenv_init_retval_LDADD = $(FAT_TESTLIB) $(LIBCHECK_LIBS)

test_probe_config_partitions_CFLAGS = $(AM_CFLAGS)
test_probe_config_partitions_SOURCES = test_probe_config_partitions.c \
				       fake_devices.c \
				       $(SRC_TEST_COMMON)
test_probe_config_partitions_LDADD = $(FAT_TESTLIB) $(LIBCHECK_LIBS)

test_probe_config_file_CFLAGS = $(AM_CFLAGS) -Wl,--wrap=probe_config_file
test_probe_config_file_SOURCES = test_probe_config_file.c fake_devices.c \
				 $(SRC_TEST_COMMON)
test_probe_config_file_LDADD = $(FAT_TESTLIB) $(LIBCHECK_LIBS)

test_ebgenv_api_internal_CFLAGS = $(AM_CFLAGS)
test_ebgenv_api_internal_SOURCES = test_ebgenv_api_internal.c $(SRC_TEST_COMMON)
test_ebgenv_api_internal_LDADD = $(FAT_TESTLIB) $(LIBCHECK_LIBS)

test_ebgenv_api_CFLAGS = $(AM_CFLAGS) -Wl,--wrap=bgenv_set -Wl,--wrap=bgenv_get
test_ebgenv_api_SOURCES = test_ebgenv_api.c $(SRC_TEST_COMMON)
test_ebgenv_api_LDADD = $(FAT_TESTLIB) $(LIBCHECK_LIBS)

TESTS = $(check_PROGRAMS)
