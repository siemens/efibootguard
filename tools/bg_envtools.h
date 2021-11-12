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

#ifndef __bg_envtools_h_
#define __bg_envtools_h_

#include "env_api.h"

#define OPT(name, key, arg, flags, doc)                                        \
	{                                                                      \
		name, key, arg, flags, doc                                     \
	}

/* if you change these, do not forget to update completion/common.py */
#define BG_CLI_OPTIONS_COMMON                                                  \
	OPT("filepath", 'f', "ENVFILE", 0,                                     \
	    "Environment to use. Expects a file name, "                        \
	    "usually called BGENV.DAT.")                                       \
	, OPT("part", 'p', "ENV_PART", 0,                                      \
	      "Set environment partition to update. If no partition is "       \
	      "specified, "                                                    \
	      "the one with the smallest revision value above zero is "        \
	      "updated.")                                                      \
	, OPT("verbose", 'v', 0, 0, "Be verbose")                              \
	, OPT("version", 'V', 0, 0, "Print version")

/* Common arguments used by both bg_setenv and bg_printenv. */
struct arguments_common {
	char *envfilepath;
	bool verbosity;
	/* which partition to operate on; a negative value means no partition
	 * was specified. */
	int which_part;
	bool part_specified;
};

int parse_int(char *arg);

char *ustate2str(uint8_t ustate);
uint8_t str2ustate(char *str);

error_t parse_common_opt(int key, char *arg, bool compat_mode,
			 struct arguments_common *arguments);

bool get_env(char *configfilepath, BG_ENVDATA *data);

#endif
