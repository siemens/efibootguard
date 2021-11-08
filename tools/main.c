/*
 * EFI Boot Guard
 *
 * Copyright (c) Siemens AG, 2017-2021
 *
 * Authors:
 *  Andreas Reichel <andreas.reichel.ext@siemens.com>
 *  Michael Adler <michael.adler@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include "bg_printenv.h"
#include "bg_setenv.h"

int main(int argc, char **argv)
{
	if (strstr(argv[0], "bg_setenv")) {
		return bg_setenv(argc, argv);
	} else {
		return bg_printenv(argc, argv);
	}
}
