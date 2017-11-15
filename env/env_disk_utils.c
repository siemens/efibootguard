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
 */

#include <stdlib.h>
#include <mntent.h>
#include <string.h>
#include "env_api.h"
#include "env_disk_utils.h"

const char *tmp_mnt_dir = "/tmp/mnt-XXXXXX";

char *get_mountpoint(char *devpath)
{
	struct mntent *part;
	FILE *mtab;

	mtab = setmntent("/proc/mounts", "r");
	if (!mtab) {
		return NULL;
	}

	while ((part = getmntent(mtab)) != NULL) {
		if ((part->mnt_fsname != NULL) &&
		    (strcmp(part->mnt_fsname, devpath)) == 0) {
			char *mntpoint;

			mntpoint = malloc(strlen(part->mnt_dir) + 1);
			if (!mntpoint) {
				break;
			}
			strncpy(mntpoint, part->mnt_dir,
				strlen(part->mnt_dir) + 1);
			return mntpoint;
		}
	}
	endmntent(mtab);

	return NULL;
}

bool mount_partition(CONFIG_PART *cfgpart)
{
	char tmpdir_template[256];
	char *mountpoint;
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
	if (mount(cfgpart->devpath, mountpoint, "vfat", 0, "")) {
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
	strncpy(cfgpart->mountpoint, mountpoint, strlen(mountpoint) + 1);
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
