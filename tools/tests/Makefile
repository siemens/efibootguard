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

PROJECTDIR := $(dir $(realpath $(firstword $(MAKEFILE_LIST))))

CC ?= $(CROSS_COMPILE)gcc
LD ?= $(CROSS_COMPILE)ld
AR ?= $(CROSS_COMPILE)ar
OBJCOPY ?= $(CROSS_COMPILE)objcopy

INCLUDE ?= /usr/include

CFLAGS = \
	-I$(PROJECTDIR) \
	-I$(PROJECTDIR)/.. \
	-I$(PROJECTDIR)/../../include \
	-I$(PROJECTDIR)/../../swupdate-adapter \
	-std=gnu99 \
	-g

DEFINES = \
	-DHAVE_ENDIAN_H \
	-D_GNU_SOURCE

ifneq ($(DEBUG),)
DEFINES += -DDEBUG
endif

CFLAGS += \
	-fshort-wchar

LIBS = -L../.. \
	   -L../../swupdate-adapter \
	   -lcmocka \
	   -lebgenv \
	   -lz

# Test recipes shall run everytime independent of already built files.
# A simple way to achieve this is to depend on files that don't exist
# by changing their extension with Makefile's string functions.
# All capital O's get replaced by lower-case o's within the target
# dependency recipes.
# All targets' '.target' extensions get removed within the target recipes.
#
OBJS_test_partitions = test_partitions.O bg_utils.O ebgpart.O
OBJS_test_environment = test_environment.O bg_utils.O ebgpart.O
OBJS_test_api = test_api.O bg_utils.O ebgenv.O

MOCKOBJS_test_partitions = bg_utils ebgpart
MOCKOBJS_test_environment = bg_utils
MOCKOBJS_test_api = bg_utils

# Define symbols to be stripped dependent on target and object file name
# MOCKOBJS_SYMBOLS_objectname-targetname = symbolname1 symbolname2 ...

MOCKOBJS_SYMBOLS_bg_utils-test_partitions = probe_config_file
MOCKOBJS_SYMBOLS_bg_utils-test_environment = oldenvs configparts fopen fclose fread fwrite feof mount_partition
MOCKOBJS_SYMBOLS_bg_utils-test_api = bgenv_init bgenv_write bgenv_close bgenv_get_latest bgenv_get_by_index bgenv_get_oldest
MOCKOBJS_SYMBOLS_ebgpart-test_partitions = ped_device_probe_all ped_device_get_next ped_disk_next_partition

TEST_TARGETS = test_partitions.target test_environment.target test_api.target

define WEAKEN_SYMBOL =
	objcopy --weaken-symbol $(1) $(2)
	
endef

define WEAKEN_SYMBOLS =
$(foreach symbol,$(MOCKOBJS_SYMBOLS_$(1)-$(2)),$(call WEAKEN_SYMBOL,$(symbol),$(1).o))
endef

define TEST_TARGET_TEMPLATE =
$(1): $$(OBJS_$(1:.target=))
	$(foreach mockobj,$(MOCKOBJS_$(1:.target=)),$(call WEAKEN_SYMBOLS,$(mockobj),$(1:.target=)))
	$(CC) $$(OBJS_$(1:.target=):O=o) -o $(1:.target=) $(LIBS)
	./$(1:.target=)
endef

.PHONY: clean all $(TEST_TARGETS)

all: $(TEST_TARGETS)

$(foreach test,$(TEST_TARGETS),$(eval $(call TEST_TARGET_TEMPLATE,$(test))))

# Search for source files in current and parent directory
%.O: %.c
	$(CC) $(CFLAGS) $(DEFINES) -c $< -o $(@:O=o)

%.O: ../%.c
	$(CC) $(CFLAGS) $(DEFINES) -c $< -o $(@:O=o)

%.O: ../../swupdate-adapter/%.c
	$(CC) $(CFLAGS) $(DEFINES) -c $< -o $(@:O=o)

clean:
	@rm -rf *.o $(TEST_TARGETS:.target=)
