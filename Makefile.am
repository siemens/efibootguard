#
# EFI Boot Guard
#
# Copyright (c) Siemens AG, 2017
#
# Authors:
#  Claudius Heine <ch@denx.de>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#
# SPDX-License-Identifier:	GPL-2.0
#
# Based on Makefile.am of gummiboot,
# Copyright (C) 2013 Karel Zak <kzak@redhat.com>
#

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
AM_MAKEFLAGS = --no-print-directory

efibootguarddir = $(libdir)/efibootguard

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir) -include config.h

AM_CFLAGS = \
	-Wno-unused-parameter \
	-Wmissing-prototypes \
	-fshort-wchar \
	-DHAVE_ENDIAN_H \
	-D_GNU_SOURCE \
	-D_FILE_OFFSET_BITS=64 \
	-Werror

AM_LDFLAGS = -L$(top_builddir)/

ARFLAGS = cr

EXTRA_DIST = autogen.sh README LICENSE
CLEANFILES =

define shtab
	$(AM_V_GEN) $(MKDIR_P) $(@D); \
	PYTHONPATH=${top_srcdir}/completion/shtab:${top_srcdir}/completion \
	@PYTHON@ -m shtab -u --shell=$1 $2 >$@
endef

define filechk
	$(AM_V_at)set -e;			\
	echo '  CHK      $@';			\
	mkdir -p $(dir $@);			\
	$(filechk_$(1)) < $< > $@.tmp;		\
	if [ -r $@ ] && cmp -s $@ $@.tmp; then	\
		rm -f $@.tmp;			\
	else					\
		echo '  UPD      $@';		\
		mv -f $@.tmp $@;		\
	fi
endef

#
# Version header
#
define filechk_version
	$(top_srcdir)/gen_version_h $(top_srcdir)/
endef

GEN_VERSION_H := $(top_builddir)/version.h

$(GEN_VERSION_H): $(top_srcdir)/Makefile.in FORCE
	$(call filechk,version)

CLEANFILES += $(GEN_VERSION_H)

#
# pkg-config
#
pkgconfig_DATA = libebgenv.pc

#
# Static libraries
#
lib_LIBRARIES = libebgenv.a

libebgenv_a_SOURCES = \
	env/@env_api_file@.c \
	env/env_api.c \
	env/env_config_file.c \
	env/env_config_partitions.c \
	env/env_disk_utils.c \
	env/uservars.c \
	tools/ebgpart.c

libebgenv_a_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir)/tools

libebgenv_a_CFLAGS = \
	$(AM_CFLAGS) \
	-fPIC

pkginclude_HEADERS = \
	include/ebgenv.h

#
# Shared libraries
#
LIBEBGENV_SO_VERSION = $(shell $(EGREP) -o '[0-9]+\.[0-9]+' $(top_srcdir)/VERSION)
lib_LTLIBRARIES = libebgenv.la
libebgenv_la_SOURCES = $(libebgenv_a_SOURCES)
libebgenv_la_LDFLAGS = -release $(LIBEBGENV_SO_VERSION).0

#
# bg_setenv binary
#
bin_PROGRAMS = bg_setenv

bg_setenv_SOURCES = \
	tools/bg_setenv.c \
	tools/bg_printenv.c \
	tools/bg_envtools.c \
	tools/main.c

bg_setenv_CFLAGS = \
	$(AM_CFLAGS) -static

bg_setenv_LDADD = \
	$(top_builddir)/libebgenv.a \
	-lz

install-exec-hook:
	$(AM_V_at)$(LN_S) -f bg_setenv$(EXEEXT) \
		$(DESTDIR)$(bindir)/bg_printenv$(EXEEXT)
	$(RM) $(DESTDIR)$(libdir)/$(lib_LTLIBRARIES)

#
# EFI compilation
#
# This part of the build system uses custom make rules and bypass regular
# automake to provide absolute control on compiler and linker flags.
#
efi_loadername = efibootguard$(MACHINE_TYPE_NAME).efi

if BOOTLOADER
if ARCH_AARCH64
efi_sources_watchdogs =
else
# NOTE: wdat.c is placed first so it is tried before any other drivers
# NOTE: ipc4x7e_wdt.c must be *before* itco.c
efi_sources_watchdogs = \
	drivers/watchdog/wdat.c \
	drivers/watchdog/amdfch_wdt.c \
	drivers/watchdog/i6300esb.c \
	drivers/watchdog/atom-quark.c \
	drivers/watchdog/ipc4x7e_wdt.c \
	drivers/watchdog/itco.c \
	drivers/watchdog/hpwdt.c
endif

efi_sources = \
	drivers/watchdog/init_array_start.S \
	$(efi_sources_watchdogs) \
	drivers/watchdog/init_array_end.S \
	env/syspart.c \
	env/fatvars.c \
	utils.c \
	bootguard.c \
	main.c

efi_cppflags = \
	-I$(top_builddir) -include config.h \
	-I$(top_srcdir)/include \
	-I$(GNUEFI_SYS_DIR)/usr/include \
	-I$(GNUEFI_INC_DIR) \
	-I$(GNUEFI_INC_DIR)/$(ARCH) \
	$(LIBPCI_CFLAGS)

efi_cflags = \
	-Wall \
	-Wextra \
	-std=gnu99 \
	-nostdinc \
	-ggdb -O0 \
	-fpic \
	-fshort-wchar \
	-ffreestanding \
	-fno-strict-aliasing \
	-fno-stack-protector \
	-Wsign-compare \
	-DGNU_EFI_USE_MS_ABI \
	$(CFLAGS)

if ARCH_X86_64
efi_cflags += \
	-mno-sse \
	-mno-mmx \
	-mno-red-zone
endif

efi_ldflags = \
	-T $(GNUEFI_LIB_DIR)/elf_$(ARCH)_efi.lds \
	-shared \
	-Bsymbolic \
	-nostdlib \
	-znocombreloc \
	--warn-common \
	--no-undefined \
	--fatal-warnings \
	-L $(GNUEFI_LIB_DIR) \
	$(GNUEFI_LIB_DIR)/crt0-efi-$(ARCH).o

if ARCH_AARCH64
# aarch64's objcopy doesn't understand --target efi-[app|bsdrv|rtdrv],
# hence set subsystem 0xa (EFI application) and binary format.
objcopy_format = -O binary
efi_ldflags += --defsym=EFI_SUBSYSTEM=0xa
else
objcopy_format = --target=efi-app-$(ARCH)
endif

efi_objects_pre1 = $(efi_sources:.c=.o)
efi_objects_pre2 = $(efi_objects_pre1:.S=.o)
efi_objects = $(addprefix $(top_builddir)/,$(efi_objects_pre2))
efi_solib = $(top_builddir)/efibootguard$(MACHINE_TYPE_NAME).so

# automake stuff
efibootguard_DATA = $(efi_loadername)
CLEANFILES += $(efi_objects) $(efi_solib) $(efi_loadername)
EXTRA_DIST += $(efi_sources)

define gnuefi_compile
	$(AM_V_CC) $(MKDIR_P) $(shell dirname $@)/; \
	$(GNUEFI_CC) $(efi_cppflags) $(efi_cflags) -c $< -o $@
endef

$(top_builddir)/%.o: $(top_srcdir)/%.c
	$(call gnuefi_compile)

$(top_builddir)/env/%.o: $(top_srcdir)/env/%.c
	$(call gnuefi_compile)

$(top_builddir)/drivers/watchdog/%.o: $(top_srcdir)/drivers/watchdog/%.c
	$(call gnuefi_compile)

$(top_builddir)/drivers/watchdog/%.o: $(top_srcdir)/drivers/watchdog/%.S
	$(call gnuefi_compile)

$(top_builddir)/main.o: $(GEN_VERSION_H)

$(efi_solib): $(efi_objects)
	$(AM_V_CCLD)$(LD) $(efi_ldflags) $(efi_objects) \
		-o $@ -lefi -lgnuefi $(shell $(CC) $(CFLAGS) -print-libgcc-file-name); \
	nm -D -u $@ | grep ' U ' && exit 1 || :

$(efi_loadername): $(efi_solib)
	$(AM_V_GEN) $(OBJCOPY) -j .text -j .sdata -j .data -j .dynamic \
	  -j .dynsym -j .rel -j .rela -j .reloc -j .init_array \
	  -j .rela.got -j .rela.data $(objcopy_format) $< $@
endif

$(top_builddir)/tools/bg_setenv-bg_setenv.o: $(GEN_VERSION_H)

bg_printenvdir = $(top_srcdir)

bg_printenv: $(bg_setenv)
	$(AM_V_at)$(LN_S) -f bg_setenv bg_printenv

BASH_COMPLETION_FILES := $(top_builddir)/completion/bash/bg_setenv.bash $(top_builddir)/completion/bash/bg_printenv.bash
ZSH_COMPLETION_FILES := $(top_builddir)/completion/zsh/_bg_setenv $(top_builddir)/completion/zsh/_bg_printenv

bashcompletiondir = ${datarootdir}/efibootguard/completion/bash
bashcompletion_DATA = $(BASH_COMPLETION_FILES)

zshcompletiondir = ${datarootdir}/efibootguard/completion/zsh
zshcompletion_DATA = $(ZSH_COMPLETION_FILES)

.PHONY: bash-completion
bash-completion: $(BASH_COMPLETION_FILES)

.PHONY: zsh-completion
zsh-completion: $(ZSH_COMPLETION_FILES)

$(top_builddir)/completion/bash/bg_setenv.bash: ${top_srcdir}/completion/bg_setenv/cli.py
	$(call shtab,bash,bg_setenv.cli.bg_setenv)

$(top_builddir)/completion/zsh/_bg_setenv: ${top_srcdir}/completion/bg_setenv/cli.py
	$(call shtab,zsh,bg_setenv.cli.bg_setenv)

$(top_builddir)/completion/bash/bg_printenv.bash: ${top_srcdir}/completion/bg_printenv/cli.py
	$(call shtab,bash,bg_printenv.cli.bg_printenv)

$(top_builddir)/completion/zsh/_bg_printenv: ${top_srcdir}/completion/bg_printenv/cli.py
	$(call shtab,zsh,bg_printenv.cli.bg_printenv)

all-local: bg_printenv bash-completion zsh-completion

CLEANFILES += bg_printenv $(BASH_COMPLETION_FILES) $(ZSH_COMPLETION_FILES)

# Tests depend on libraries being built - start with "."
SUBDIRS = . tools/tests

FORCE:

.PHONY: FORCE
