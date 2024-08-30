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
 * SPDX-License-Identifier:	GPL-2.0-only
 */

#ifndef __bg_setenv_h_
#define __bg_setenv_h_

#include <errno.h>

error_t bg_setenv(int argc, char **argv);

#endif
