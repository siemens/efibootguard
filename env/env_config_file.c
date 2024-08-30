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

#include <stdlib.h>
#include <stdio.h>
#include "env_api.h"
#include "env_disk_utils.h"
#include "env_config_file.h"

FILE *open_config_file(char *configfilepath, char *mode)
{
	VERBOSE(stdout, "Probing config file at %s.\n", configfilepath);
	return fopen(configfilepath, mode);
}

FILE *open_config_file_from_part(CONFIG_PART *cfgpart, char *mode)
{
	char *configfilepath;

	if (!cfgpart || !cfgpart->mountpoint) {
		return NULL;
	}
	configfilepath = (char *)malloc(strlen(FAT_ENV_FILENAME) +
					strlen(cfgpart->mountpoint) + 2);
	if (!configfilepath) {
		return NULL;
	}
	strcpy(configfilepath, cfgpart->mountpoint);
	strcat(configfilepath, "/");
	strcat(configfilepath, FAT_ENV_FILENAME);
	FILE *config = open_config_file(configfilepath, mode);
	free(configfilepath);
	return config;
}

bool probe_config_file(CONFIG_PART *cfgpart)
{
	bool do_unmount = false;
	if (!cfgpart) {
		return false;
	}
	printf_debug("Checking device: %s\n", cfgpart->devpath);
	if (!(cfgpart->mountpoint = get_mountpoint(cfgpart->devpath))) {
		/* partition is not mounted */
		cfgpart->not_mounted = true;
		VERBOSE(stdout, "Partition %s is not mounted.\n",
			cfgpart->devpath);
		if (!mount_partition(cfgpart)) {
			return false;
		}
		do_unmount = true;
	} else {
		cfgpart->not_mounted = false;
	}

	if (cfgpart->mountpoint) {
		/* partition is mounted to mountpoint, either before or by this
		 * program */
		VERBOSE(stdout, "Partition %s is mounted to %s.\n",
			cfgpart->devpath, cfgpart->mountpoint);
		bool result = false;
		FILE *config;
		if (!(config = open_config_file_from_part(cfgpart, "rb"))) {
			printf_debug(
			    "Could not open config file on partition %s.\n",
			    FAT_ENV_FILENAME);
		} else {
			result = true;
			if (fclose(config)) {
				VERBOSE(stderr, "Error closing config file on "
						"partition %s.\n",
						cfgpart->devpath);
			}
		}
		if (do_unmount) {
			unmount_partition(cfgpart);
		} else {
			free(cfgpart->mountpoint);
			cfgpart->mountpoint = NULL;
		}
		return result;
	}
	return false;
}
