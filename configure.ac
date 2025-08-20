#
# EFI Boot Guard
#
# Copyright (c) Siemens AG, 2017-2025
#
# Authors:
#  Claudius Heine <ch@denx.de>
#  Andreas Reichel <andreas.reichel.ext@siemens.com>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#
# SPDX-License-Identifier:	GPL-2.0-only
#
# Based on configure.ac of gummiboot,
# Copyright (C) 2013 Karel Zak <kzak@redhat.com>
#

AC_INIT([efibootguard],
	[m4_esyscmd_s([cat VERSION])],
	[],
	[efibootguard],
	[])

AC_CONFIG_SRCDIR([bootguard.c])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_PREFIX_DEFAULT([/usr])

LT_INIT

AM_INIT_AUTOMAKE([foreign 1.11 -Wall -Wno-portability silent-rules tar-pax no-dist-gzip dist-xz subdir-objects])
AM_SILENT_RULES([yes])

AC_CANONICAL_HOST
AC_SYS_LARGEFILE

AC_PROG_CC
AC_CHECK_TOOL(LD, ld)
AC_CHECK_TOOL(OBJCOPY, objcopy)
AC_CHECK_TOOL(GREP, grep)
AC_CHECK_TOOL(TR, tr)

dnl Dont try to use things like -std=c99 for efi compilation
GNUEFI_CC=$CC
AC_SUBST([GNUEFI_CC])

AC_PROG_CC_C99
AM_PROG_CC_C_O
AC_PROG_GCC_TRADITIONAL

AC_PROG_MKDIR_P
AC_PROG_LN_S
AC_PROG_EGREP

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
SET_ARCH(I586, i586*|i686*)
SET_ARCH(X86_64, x86_64*)
SET_ARCH(IA64, ia64*)
SET_ARCH(AARCH64, aarch64*)
SET_ARCH(ARM, arm*)
SET_ARCH(RISCV64, riscv64*)

ARCH=$(echo $host | sed "s/\(-\).*$//")

AM_COND_IF(ARCH_I586, [
	   ARCH=ia32
	   MACHINE_TYPE_NAME=ia32])

AM_COND_IF(ARCH_X86_64, [
	   MACHINE_TYPE_NAME=x64])

AM_COND_IF(ARCH_AARCH64, [
	   MACHINE_TYPE_NAME=aa64])

AM_COND_IF(ARCH_ARM, [
	   MACHINE_TYPE_NAME=arm])

AM_COND_IF(ARCH_RISCV64, [
	   MACHINE_TYPE_NAME=riscv64])

AC_SUBST([ARCH])
AC_SUBST([MACHINE_TYPE_NAME])
AM_CONDITIONAL([ARCH_IS_X86], [test "$ARCH" = "ia32" -o "$ARCH" = "x86_64"])

# -mgeneral-regs-only was introduced with gcc-9 to ARM, and RISCV64 does not support it until now
AX_CHECK_COMPILE_FLAG([-mgeneral-regs-only],
	[CFLAGS_MGENERAL_REGS_ONLY=-mgeneral-regs-only],
	[CFLAGS_MGENERAL_REGS_ONLY=])
AC_SUBST([CFLAGS_MGENERAL_REGS_ONLY])

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
AC_FUNC_GETMNTENT
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

AC_ARG_WITH([env-file-name],
	    AS_HELP_STRING([--with-env-file-name=STRING],
                           [override the boot environment file name]),
	    [
		ENV_FILE_NAME="$withval"
	    ],
	    [ ENV_FILE_NAME="BGENV.DAT" ])

AC_DEFINE_UNQUOTED(
	[ENV_FILE_NAME], [L"${ENV_FILE_NAME}"], [Boot environment file name])
AC_DEFINE_UNQUOTED(
	[FAT_ENV_FILENAME], ["${ENV_FILE_NAME}"], [Boot environment FAT file name])

AC_ARG_WITH([boot-delay],
	    AS_HELP_STRING([--with-boot-delay=INT],
			   [specify the additional boot delay in seconds, defaults to 3]),
	    [
		# Limit the boot delay to 1 hour to avoid overflowing
		# BS->Stall() on 32 bit hosts.
		ENV_BOOT_DELAY=${withval:-3}
		AS_IF([test "${ENV_BOOT_DELAY}" -lt "0" -o "${ENV_BOOT_DELAY}" -gt "3600"],
		      [
			AC_MSG_ERROR([Invalid boot delay.])
		      ])
	    ],
	    [ ENV_BOOT_DELAY=0 ])

AC_DEFINE_UNQUOTED([ENV_BOOT_DELAY], [${ENV_BOOT_DELAY}], [Additional boot delay])

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

AC_ARG_ENABLE([bootloader],
    AS_HELP_STRING([--disable-bootloader], [Compile the bootloader disabled, only make the tools]),
	, [enable_bootloader="yes"])

AC_ARG_ENABLE([silent-boot],
    AS_HELP_STRING([--enable-silent-boot], [Do not output informational messages while booting]),
	[silent_boot="yes"], [silent_boot="no"]
)

if test "x$silent_boot" != "xno"; then
    AC_DEFINE([SILENT_BOOT], [] , [Silent Boot])
fi

dnl pkg-config
PKG_PROG_PKG_CONFIG()
if test "x$PKG_CONFIG" = "xno"; then
	AC_MSG_ERROR([You need to install pkg-config])
fi

# Note: gnu-efi introduced pkg-config with version 3.0.16
# GNU_EFI_VERSION resolves to gnu-efi's version without dots, e.g., GNU_EFI_VERSION=3016
# gnu-efi versions < 3.0.16 resolve to GNU_EFI_VERSION=0
AC_SUBST([GNU_EFI_VERSION], [$(($PKG_CONFIG --modversion "gnu-efi" 2>/dev/null || echo 0) | $TR -d '.' )])

AC_SUBST([OBJCOPY_HAS_EFI_APP_TARGET], [$($OBJCOPY --info | $GREP -q pei- 2>/dev/null && echo "true")])

# Use objcopy -O binary on riscv64, arm, and aarch64 with gnu-efi 3.0.15
AM_CONDITIONAL([OBJCOPY_USE_BINARY_COPY],
	       [test "$ARCH" = "arm" || test "$ARCH" = "riscv64" || test "$ARCH" = "aarch64" -a $GNU_EFI_VERSION -eq 0])

# Ensure objcopy supports target=efi-app for x86 across the board(s) and aarch64 with gnu-efi > 3.0.15
if test -z "$ARCH_IS_X86_TRUE" -a -z "$OBJCOPY_HAS_EFI_APP_TARGET" || test "$ARCH" = "aarch64" -a -z "$OBJCOPY_HAS_EFI_APP_TARGET" -a $GNU_EFI_VERSION -gt 0; then
	AC_MSG_ERROR([objcopy doesn't support --target=efi-app but that is needed by gnu-efi])
fi

# Use --no-warn-rwx-segments for gnu-efi version 3.0.15
# on non-x86 architectures.
AS_IF([test -z "$ARCH_IS_X86_FALSE" -a $GNU_EFI_VERSION -eq 0],
	[AX_CHECK_LINK_FLAG([-Wl,--no-warn-rwx-segments],
		[LDFLAGS_NO_WARN_RWX_SEGMENTS=--no-warn-rwx-segments],
		[LDFLAGS_NO_WARN_RWX_SEGMENTS=])
	 AC_SUBST([LDFLAGS_NO_WARN_RWX_SEGMENTS])])

# Keep .dynsym section for gnu-efi 3.0.15 or older
AS_IF([test $GNU_EFI_VERSION -eq 0],
	EXTRA_EFI_SECTIONS="-j .dynsym"
	AC_SUBST([EXTRA_EFI_SECTIONS]))

AS_IF([test "x$enable_bootloader" != "xno"],
    [
	PKG_CHECK_MODULES(LIBPCI, libpci)
    ])
AM_CONDITIONAL([BOOTLOADER], [test "x$enable_bootloader" != "xno"])

AC_ARG_ENABLE([tests],
    [AS_HELP_STRING([--disable-tests], [Disable building tests])],
    [], [enable_tests=yes])

AM_CONDITIONAL([BUILD_TESTS], [test "x$enable_tests" != "xno"])

# Only include libcheck if tests are enabled
if test "x$enable_tests" != "xno"; then
	PKG_CHECK_MODULES(LIBCHECK, check)
fi

PKG_INSTALLDIR
AC_SUBST(LIBEBGENV_VERSION, $(echo $VERSION | cut -dv -f2))

dnl shell completion
AC_ARG_ENABLE([completion],
    AS_HELP_STRING([--disable-completion], [do not generate shell completion files]),
	, [enable_completion="yes"])

AS_IF([test "x$enable_completion" != "xno"],
      [
      dnl Python
      AC_PATH_PROGS(PYTHON, [python3 python3.9 python3.8 python3.7 python3.6 python2.7 python2], no)
      if test "x$PYTHON" = "xno"; then
          AC_MSG_ERROR([You need to install python])
      fi
      ])
AM_CONDITIONAL([COMPLETION], [test "x$enable_completion" != "xno"])

AX_VALGRIND_DFLT(memcheck, on)
AX_VALGRIND_DFLT(helgrind, off)
AX_VALGRIND_DFLT(drd, off)
AX_VALGRIND_DFLT(sgcheck, off)
AX_VALGRIND_CHECK

# ------------------------------------------------------------------------------
AC_CONFIG_FILES([
	Makefile
	libebgenv.pc
])

# Only configure tools/tests/Makefile if tests are enabled
if test "x$enable_tests" != "xno"; then
    AC_CONFIG_FILES([tools/tests/Makefile])
fi

AC_OUTPUT
AC_MSG_RESULT([
	$PACKAGE_NAME $VERSION

	arch:                    $ARCH
	machine type:            $MACHINE_TYPE_NAME

	build efi bootloader:    ${enable_bootloader}
	enable shell completion: ${enable_completion}

	prefix:                  ${prefix}
	exec_prefix:             ${exec_prefix}
	libexecdir:              ${libexecdir}
	libdir:                  ${libdir}

	efi libs:                ${GNUEFI_LIB_DIR}

	environment backend:     ${ENV_API_FILE}.c
	environment file name:   ${ENV_FILE_NAME}
	number of config parts:  ${ENV_NUM_CONFIG_PARTS}
	reserved for uservars:   ${ENV_MEM_USERVARS} bytes
	silent boot:             ${silent_boot}
	boot delay:              ${ENV_BOOT_DELAY} seconds
])
