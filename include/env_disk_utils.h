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
 * SPDX-License-Identifier:	GPL-2.0
 */

#ifndef __ENV_DISK_UTILS_H__
#define __ENV_DISK_UTILS_H__

char *get_mountpoint(char *devpath);
bool mount_partition(CONFIG_PART *cfgpart);
void unmount_partition(CONFIG_PART *cfgpart);

#endif // __ENV_DISK_UTILS_H__
