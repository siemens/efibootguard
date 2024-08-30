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

#ifndef __bg_printenv_h_
#define __bg_printenv_h_

#include "env_api.h"

struct fields {
	unsigned int in_progress : 1;
	unsigned int revision : 1;
	unsigned int kernel : 1;
	unsigned int kernelargs : 1;
	unsigned int wdog_timeout : 1;
	unsigned int ustate : 1;
	unsigned int user : 1;
};

extern const struct fields ALL_FIELDS;

void dump_envs(const struct fields *output_fields, bool raw);
void dump_env(BG_ENVDATA *env, const struct fields *output_fields, bool raw);

error_t bg_printenv(int argc, char **argv);

#endif
