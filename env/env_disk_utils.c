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
#include <mntent.h>
#include <string.h>
#include "env_api.h"
#include "env_disk_utils.h"

const char *tmp_mnt_dir = "/tmp/mnt-XXXXXX";

char *get_mountpoint(const char *devpath)
{
	const struct mntent *part;
	char *mntpoint = NULL;
	FILE *mtab;

	mtab = setmntent("/proc/mounts", "r");
	if (!mtab) {
		return NULL;
	}

	while ((part = getmntent(mtab)) != NULL) {
		if ((part->mnt_fsname != NULL) &&
		    (strcmp(part->mnt_fsname, devpath)) == 0) {
			mntpoint = strdup(part->mnt_dir);
			break;
		}
	}
	endmntent(mtab);

	return mntpoint;
}

bool mount_partition(CONFIG_PART *cfgpart)
{
	char tmpdir_template[256];
	const char *mountpoint;
	(void)snprintf(tmpdir_template, 256, "%s", tmp_mnt_dir);
	if (!cfgpart) {
		return false;
	}
	if (!cfgpart->devpath) {
		return false;
	}
	if (!(mountpoint = mkdtemp(tmpdir_template))) {
		VERBOSE(stderr, "Error creating temporary mount point.\n");
		return false;
	}
	if (mount(cfgpart->devpath, mountpoint, "vfat", MS_SYNCHRONOUS, NULL)) {
		VERBOSE(stderr, "Error mounting to temporary mount point.\n");
		if (rmdir(tmpdir_template)) {
			VERBOSE(stderr,
				"Error deleting temporary directory.\n");
		}
		return false;
	}
	cfgpart->mountpoint = (char *)malloc(strlen(mountpoint) + 1);
	if (!cfgpart->mountpoint) {
		VERBOSE(stderr, "Error, out of memory.\n");
		return false;
	}
	strcpy(cfgpart->mountpoint, mountpoint);
	return true;
}

void unmount_partition(CONFIG_PART *cfgpart)
{
	if (!cfgpart) {
		return;
	}
	if (!cfgpart->mountpoint) {
		return;
	}
	if (umount(cfgpart->mountpoint)) {
		VERBOSE(stderr, "Error unmounting temporary mountpoint %s.\n",
			cfgpart->mountpoint);
	}
	if (rmdir(cfgpart->mountpoint)) {
		VERBOSE(stderr, "Error deleting temporary directory %s.\n",
			cfgpart->mountpoint);
	}
	free(cfgpart->mountpoint);
	cfgpart->mountpoint = NULL;
}
