#
# EFI Boot Guard
#
# Copyright (c) Siemens AG, 2017
#
# Authors:
#  Jan Kiszka <jan.kiszka@siemens.com>
#  Andreas Reichel <andreas.reichel.ext@siemens.com>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#

OBJS := syspart.o fatvars.o

env_backends: $(OBJS)

%.o: %.c
	@$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ -c $<

clean:
	@rm -rf *.o

.PHONY: env_backends clean
