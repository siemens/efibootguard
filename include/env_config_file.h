/*
 * EFI Boot Guard
 *
 * Copyright (c) Siemens AG, 2017
 *
 * Authors:
 *  Andreas Reichel <andreas.reichel.ext@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * SPDX-License-Identifier:	GPL-2.0-only
 */

#pragma once
#include <stdio.h>
#include "env_api.h"

FILE *open_config_file_from_part(const CONFIG_PART *cfgpart, const char *mode);
FILE *open_config_file(const char *configfilepath, const char *mode);
bool probe_config_file(CONFIG_PART *cfgpart);
