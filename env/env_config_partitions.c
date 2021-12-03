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

#include "env_api.h"
#include "ebgpart.h"
#include "env_config_partitions.h"
#include "env_config_file.h"

bool probe_config_partitions(CONFIG_PART *cfgpart)
{
	PedDevice *dev = NULL;
	char devpath[4096];
	int count = 0;

	if (!cfgpart) {
		return false;
	}

	ped_device_probe_all();

	while ((dev = ped_device_get_next(dev))) {
		printf_debug("Device: %s\n", dev->model);
		PedDisk *pd = ped_disk_new(dev);
		if (!pd) {
			continue;
		}
		PedPartition *part = pd->part_list;
		while (part) {
			if (!part->fs_type || !part->fs_type->name ||
			    (strcmp(part->fs_type->name, "fat12") != 0 &&
			     strcmp(part->fs_type->name, "fat16") != 0 &&
			     strcmp(part->fs_type->name, "fat32") != 0)) {
				part = ped_disk_next_partition(pd, part);
				continue;
			}
			if (strncmp("/dev/mmcblk", dev->path, 11) == 0 ||
			    strncmp("/dev/loop", dev->path, 9) == 0 ||
			    strncmp("/dev/nvme", dev->path, 9) == 0) {
				(void)snprintf(devpath, 4096, "%sp%u",
					       dev->path, part->num);
			} else {
				(void)snprintf(devpath, 4096, "%s%u",
					       dev->path, part->num);
			}

			CONFIG_PART tmp = {.devpath = strdup(devpath)};
			if (!tmp.devpath) {
				VERBOSE(stderr, "Out of memory.");
				return false;
			}
			if (probe_config_file(&tmp)) {
				printf_debug("%s", "Environment file found.\n");
				if (count < ENV_NUM_CONFIG_PARTS) {
					cfgpart[count] = tmp;
				} else {
					free(tmp.devpath);
					VERBOSE(stderr,
						"Error, there are "
						"more than %d config "
						"partitions.\n",
						ENV_NUM_CONFIG_PARTS);
					return false;
				}
				count++;
			}
			part = ped_disk_next_partition(pd, part);
		}
	}
	if (count < ENV_NUM_CONFIG_PARTS) {
		VERBOSE(stderr,
			"Error, less than %d config partitions exist.\n",
			ENV_NUM_CONFIG_PARTS);
		return false;
	}
	return true;
}
