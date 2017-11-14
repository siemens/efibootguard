#
# EFI Boot Guard
#
# Copyright (c) Siemens AG, 2017
#
# Authors:
#  Claudius Heine <ch@denx.de>
#  Andreas Reichel <andreas.reichel.ext@siemens.com>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#
# Based on configure.ac of gummiboot,
# Copyright (C) 2013 Karel Zak <kzak@redhat.com>
#

AC_INIT([efibootguard],
	[0.1.0],
	[],
	[efibootguard],
	[])

AC_CONFIG_SRCDIR([bootguard.c])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_PREFIX_DEFAULT([/usr])

AM_INIT_AUTOMAKE([foreign 1.11 -Wall -Wno-portability silent-rules tar-pax no-dist-gzip dist-xz subdir-objects])
AM_SILENT_RULES([yes])

AC_CANONICAL_HOST
AC_SYS_LARGEFILE

AC_PROG_CC

dnl Dont try to use things like -std=c99 for efi compilation
GNUEFI_CC=$CC
AC_SUBST([GNUEFI_CC])

AC_PROG_CC_C99
AM_PROG_CC_C_O
AC_PROG_GCC_TRADITIONAL

AC_PROG_MKDIR_P
AC_PROG_RANLIB
AC_PROG_LN_S

AC_ARG_WITH([gnuefi-sys-dir],
	    AS_HELP_STRING([--with-gnuefi-sys-dir=DIRECTORY],
			   [specify the sys-root directory of gnuefi, defaults to ""]),
	    [GNUEFI_SYS_DIR="$withval"],
	    [GNUEFI_SYS_DIR=""])

AC_SUBST(GNUEFI_SYS_DIR)
AC_DEFINE_UNQUOTED(GNUEFI_SYS_DIR, ["$GNUEFI_SYS_DIR"], [The sys-root directory for gnuefi])

AC_ARG_WITH([gnuefi-include-dir],
	    AS_HELP_STRING([--with-gnuefi-include-dir=DIRECTORY],
			   [specify the include directory of gnuefi, defaults to "$GNUEFI_SYS_DIR/usr/include/efi"]),
	    [GNUEFI_INC_DIR="$withval"],
	    [GNUEFI_INC_DIR="$GNUEFI_SYS_DIR/usr/include/efi"])

AC_SUBST(GNUEFI_INC_DIR)
AC_DEFINE_UNQUOTED(GNUEFI_INC_DIR, ["$GNUEFI_INC_DIR"], [The include directory for gnuefi])

efiroot=$(echo $(cd $GNUEFI_SYS_DIR/usr/lib/$($CC -print-multi-os-directory); pwd))

AC_ARG_WITH([gnuefi-lib-dir],
	    AS_HELP_STRING([--with-gnuefi-lib-dir=DIRECTORY],
			   [specify the lib directory of gnuefi, defaults to "$efiroot"]),
	    [GNUEFI_LIB_DIR="$withval"],
	    [GNUEFI_LIB_DIR="$efiroot"])

AC_SUBST(GNUEFI_LIB_DIR)
AC_DEFINE_UNQUOTED(GNUEFI_LIB_DIR, ["$GNUEFI_LIB_DIR"], [The lib directory for gnuefi])

dnl Define ARCH_<NAME> conditionals
SET_ARCH(I586, i586*)
SET_ARCH(I686, i686*)
SET_ARCH(X86_64, x86_64*)
SET_ARCH(IA64, ia64*)

ARCH=$(echo $host | sed "s/\(-\).*$//")

AM_COND_IF(ARCH_I686, [
	   ARCH=ia32
	   MACHINE_TYPE_NAME=ia32])

AM_COND_IF(ARCH_I586, [
	   ARCH=ia32
	   MACHINE_TYPE_NAME=ia32])

AM_COND_IF(ARCH_X86_64, [
	   MACHINE_TYPE_NAME=x64])

AC_SUBST([ARCH])
AC_SUBST([MACHINE_TYPE_NAME])

# Checks from autoscan:
AC_CHECK_FUNCS([getmntent])
AC_CHECK_FUNCS([memset])
AC_CHECK_FUNCS([rmdir])
AC_CHECK_FUNCS([strstr])
AC_CHECK_FUNCS([strtol])
AC_CHECK_HEADERS([fcntl.h])
AC_CHECK_HEADERS([mntent.h])
AC_CHECK_HEADERS([stddef.h])
AC_CHECK_HEADERS([stdint.h])
AC_CHECK_HEADERS([stdlib.h])
AC_CHECK_HEADERS([string.h])
AC_CHECK_HEADERS([sys/file.h])
AC_CHECK_HEADERS([sys/mount.h])
AC_CHECK_HEADERS([unistd.h])
AC_CHECK_HEADERS([wchar.h])
AC_CHECK_HEADER_STDBOOL
AC_CHECK_HEADERS([zlib.h])
AC_CHECK_LIB([z], [crc32], [], [AC_MSG_ERROR([need crc32 implementation from libz])])
AC_FUNC_GETMNTENT
AC_FUNC_MALLOC
AC_PROG_CXX
AC_TYPE_INT32_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

AC_ARG_WITH([env-backend],
	    AS_HELP_STRING([--with-env-backend=STRING],
			   [define the environment backend, defaults to "env_api_fat"]),
	    [ ENV_API_FILE="$withval" ],
	    [ ENV_API_FILE="env_api_fat" ])

AC_SUBST([env_api_file], [${ENV_API_FILE}])

AC_ARG_WITH([num-config-parts],
	    AS_HELP_STRING([--with-num-config-parts=INT],
			   [specify the number of used config partitions, defaults to 2]),
	    [
		ENV_NUM_CONFIG_PARTS=${withval:-0}
		AS_IF([test "${ENV_NUM_CONFIG_PARTS}" -lt "1"],
		      [
			AC_MSG_ERROR([Invalid number of config partitions.])
		      ])
	    ],
	    [ ENV_NUM_CONFIG_PARTS=2 ])

AC_DEFINE_UNQUOTED([ENV_NUM_CONFIG_PARTS], [${ENV_NUM_CONFIG_PARTS}], [Number of config partitions])

AC_ARG_WITH([mem-uservars],
	    AS_HELP_STRING([--with-mem-uservars=INT],
			   [specify amount of memory reserved for user variables in bytes]),
	    [
		ENV_MEM_USERVARS=${withval:-0}
		AS_IF([test "${ENV_MEM_USERVARS}" -lt "96"],
		      [
			AC_MSG_ERROR([Minimum space allowed for uservars is 96 bytes])
		      ])
	    ],
	    [
		ENV_MEM_USERVARS=131072
	    ])

AC_DEFINE_UNQUOTED([ENV_MEM_USERVARS], [${ENV_MEM_USERVARS}], [Reserved memory for user variables])

dnl pkg-config
AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
if test "x$PKG_CONFIG" = "xno"; then
	AC_MSG_ERROR([You need to install pkg-config])
fi
PKG_PROG_PKG_CONFIG()
PKG_CHECK_MODULES(LIBCHECK, check)
# ------------------------------------------------------------------------------
AC_CONFIG_FILES([
	Makefile
	tools/tests/Makefile
])

AC_OUTPUT
AC_MSG_RESULT([
	$PACKAGE_NAME $VERSION

	arch:                    $ARCH
	machine type:            $MACHINE_TYPE_NAME

	prefix:                  ${prefix}
	exec_prefix:             ${exec_prefix}
	libexecdir:              ${libexecdir}
	libdir:                  ${libdir}

	efi libs:                ${GNUEFI_LIB_DIR}

	environment backend:     ${ENV_API_FILE}.c
	number of config parts:  ${ENV_NUM_CONFIG_PARTS}
	reserved for uservars:   ${ENV_MEM_USERVARS} bytes
])
