#
# EFI Boot Guard FAT Environment Tool
#
# Copyright (c) Siemens AG, 2017
#
# Authors:
#  Andreas Reichel <andreas.reichel.ext@siemens.com>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#

ARCH := $(shell uname -m)

CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld
AR = $(CROSS_COMPILE)ar
OBJCOPY = $(CROSS_COMPILE)objcopy

INCLUDE = /usr/include

CFLAGS = \
	-I$(shell pwd) \
	-I$(shell pwd)/../include \
    -I$(shell pwd)/../tools \
	-I$(INCLUDE) \
	-std=gnu99 \

DEFINES = \
	-DHAVE_ENDIAN_H \
	-D_GNU_SOURCE

ifneq ($(DEBUG),)
DEFINES += -DDEBUG
endif

CFLAGS += \
	-fshort-wchar

LIBS = -lparted \
       -lbg_utils

OBJS = ../tools/bg_utils.o

libebgenv.a: $(OBJS) ebgenv.o
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) $(DEFINES) -c $< -o $@

clean:
	@rm -rf *.o *.a

.PHONY: clean
