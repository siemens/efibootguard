/*
 * EFI Boot Guard
 *
 * Copyright (c) Siemens AG, 2017
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef __TEST_INTERFACE_H__
#define __TEST_INTERFACE_H__

#include "bg_utils.h"

bool read_env(CONFIG_PART *part, BG_ENVDATA *env);
bool write_env(CONFIG_PART *part, BG_ENVDATA *env);

bool probe_config_file(CONFIG_PART *cfgpart);
bool probe_config_partitions(CONFIG_PART *cfgparts);
bool mount_partition(CONFIG_PART *cfgpart);

#endif // __TEST_INTERFACE_H__
