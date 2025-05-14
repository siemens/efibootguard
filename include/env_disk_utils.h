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

#include <stdbool.h>
#include "env_api.h"

char *get_mountpoint(const char *devpath);
bool mount_partition(CONFIG_PART *cfgpart);
void unmount_partition(CONFIG_PART *cfgpart);
