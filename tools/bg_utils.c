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

#include "bg_utils.h"

const char *tmp_mnt_dir = "/tmp/mnt-XXXXXX";

static bool verbosity = false;

void be_verbose(bool v)
{
	verbosity = v;
	ebgpart_beverbose(v);
}

/* UEFI uses 16-bit wide unicode strings.
 * However, wchar_t support functions are fixed to 32-bit wide
 * characters in glibc. This code is compiled with
 *  -fshort-wchar
 * which enables 16-bit wide wchar_t support. However,
 * glibc functions do not work with 16-bit wchar_t input, except
 * it was specifically compiled for that, which is unusual.
 * Thus, the needed conversion by truncation function is
 * reimplemented here.
 */
char *str16to8(char *buffer, wchar_t *src)
{
	if (!src || !buffer) {
		return NULL;
	}
	char *tmp = buffer;
	while (*src) {
		*buffer = (char)*src;
		src++;
		buffer++;
	}
	*buffer = 0;
	return tmp;
}

wchar_t *str8to16(wchar_t *buffer, char *src)
{
	if (!src || !buffer) {
		return NULL;
	}
	wchar_t *tmp = buffer;
	while (*src) {
		*buffer = (wchar_t)*src;
		src++;
		buffer++;
	}
	*buffer = 0;
	return tmp;
}

static char *get_mountpoint(char *devpath)
{
	struct mntent *part = NULL;
	FILE *mtab = NULL;

	if ((mtab = setmntent("/proc/mounts", "r")) == NULL)
		return NULL;

	while ((part = getmntent(mtab)) != NULL) {
		if ((part->mnt_fsname != NULL) &&
		    (strcmp(part->mnt_fsname, devpath)) == 0) {
			char *mntpoint;

			if (!(mntpoint =
				  malloc(strlen(part->mnt_dir) + 1))) {
				break;
			};
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
	snprintf(tmpdir_template, 256, "%s", tmp_mnt_dir);
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

static FILE *open_config_file(CONFIG_PART *cfgpart, char *mode)
{
	char *configfilepath;
	configfilepath = (char *)malloc(strlen(FAT_ENV_FILENAME) +
					strlen(cfgpart->mountpoint) + 2);
	if (!configfilepath) {
		return NULL;
	}
	strncpy(configfilepath, cfgpart->mountpoint,
		strlen(cfgpart->mountpoint) + 1);
	strncat(configfilepath, "/", 1);
	strncat(configfilepath, FAT_ENV_FILENAME, strlen(FAT_ENV_FILENAME));
	VERBOSE(stdout, "Probing config file at %s.\n", configfilepath);
	FILE* config = fopen(configfilepath, mode);
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
		if (!(config = open_config_file(cfgpart, "rb"))) {
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
		}
		return result;
	}
	return false;
}

bool probe_config_partitions(CONFIG_PART *cfgpart)
{
	PedDevice *dev = NULL;
	char devpath[4096];
	int count = 0;

	if (!cfgpart) {
		return false;
	}

	ped_device_probe_all();

	while (dev = ped_device_get_next(dev)) {
		printf_debug("Device: %s\n", dev->model);
		PedDisk *pd = ped_disk_new(dev);
		if (!pd) {
			continue;
		}
		PedPartition *part = pd->part_list;
		while (part) {
			if (!part->fs_type || !part->fs_type->name ||
			    strcmp(part->fs_type->name, "fat16") != 0) {
				part = ped_disk_next_partition(pd, part);
				continue;
			}
			if (strncmp("/dev/mmcblk", dev->path, 11) == 0) {
				snprintf(devpath, 4096, "%sp%u", dev->path,
					 part->num);
			} else {
				snprintf(devpath, 4096, "%s%u", dev->path,
					 part->num);
			}
			if (!cfgpart[count].devpath) {
				cfgpart[count].devpath =
				    malloc(strlen(devpath) + 1);
				if (!cfgpart[count].devpath) {
					VERBOSE(stderr, "Out of memory.");
					return false;
				}
			}
			strncpy(cfgpart[count].devpath, devpath,
				strlen(devpath) + 1);
			if (probe_config_file(&cfgpart[count])) {
				printf_debug("%s", "Environment file found.\n");
				if (count >= CONFIG_PARTITION_COUNT) {
					VERBOSE(stderr, "Error, there are "
							"more than %d config "
							"partitions.\n",
						CONFIG_PARTITION_COUNT);
					return false;
				}
				count++;
			}
			part = ped_disk_next_partition(pd, part);
		}
	}
	if (count < CONFIG_PARTITION_COUNT) {
		VERBOSE(stderr,
			"Error, less than %d config partitions exist.\n",
			CONFIG_PARTITION_COUNT);
		return false;
	}
	return true;
}

bool read_env(CONFIG_PART *part, BG_ENVDATA *env)
{
	if (!part) {
		return false;
	}
	if (part->not_mounted) {
		/* mount partition before reading config file */
		if (!mount_partition(part)) {
			return false;
		}
	} else {
		VERBOSE(stdout, "Read config file: mounted to %s\n",
			part->mountpoint);
	}
	FILE *config;
	if (!(config = open_config_file(part, "rb"))) {
		return false;
	}
	bool result = true;
	if (!(fread(env, sizeof(BG_ENVDATA), 1, config) == 1)) {
		VERBOSE(stderr, "Error reading environment data from %s\n",
			part->devpath);
		if (feof(config)) {
			VERBOSE(stderr, "End of file encountered.\n");
		}
		result = false;
	}
	if (fclose(config)) {
		VERBOSE(stderr,
			"Error closing environment file after reading.\n");
	};
	if (part->not_mounted) {
		unmount_partition(part);
	}
	return result;
}

bool write_env(CONFIG_PART *part, BG_ENVDATA *env)
{
	if (!part) {
		return false;
	}
	if (part->not_mounted) {
		/* mount partition before reading config file */
		if (!mount_partition(part)) {
			return false;
		}
	} else {
		VERBOSE(stdout, "Read config file: mounted to %s\n",
			part->mountpoint);
	}
	FILE *config;
	if (!(config = open_config_file(part, "wb"))) {
		VERBOSE(stderr, "Could not open config file for writing.\n");
		return false;
	}
	bool result = true;
	if (!(fwrite(env, sizeof(BG_ENVDATA), 1, config) == 1)) {
		VERBOSE(stderr, "Error saving environment data to %s\n",
			part->devpath);
		result = false;
	}
	if (fclose(config)) {
		VERBOSE(stderr,
			"Error closing environment file after writing.\n");
		result = false;
	};
	if (part->not_mounted) {
		unmount_partition(part);
	}
	return result;
}

CONFIG_PART config_parts[CONFIG_PARTITION_COUNT];
BG_ENVDATA oldenvs[CONFIG_PARTITION_COUNT];

bool bgenv_init(BGENVTYPE type)
{
	switch (type) {
	case BGENVTYPE_FAT:
		memset((void *)&config_parts, 0,
		       sizeof(CONFIG_PART) * CONFIG_PARTITION_COUNT);
		/* enumerate all config partitions */
		if (!probe_config_partitions(config_parts)) {
			VERBOSE(stderr, "Error finding config partitions.\n");
			return false;
		}
		for (int i = 0; i < CONFIG_PARTITION_COUNT; i++) {
			read_env(&config_parts[i], &oldenvs[i]);
			uint32_t sum = crc32(0, (Bytef *)&oldenvs[i],
			    sizeof(BG_ENVDATA) - sizeof(oldenvs[i].crc32));
			if (oldenvs[i].crc32 != sum) {
				VERBOSE(stderr, "Invalid CRC32!\n");
				continue;
			}
		}
		return true;
	}
	return false;
}

BGENV *bgenv_get_by_index(BGENVTYPE type, uint32_t index)
{
	BGENV *handle;

	switch (type) {
	case BGENVTYPE_FAT:
		/* get config partition by index and allocate handle */
		if (index >= CONFIG_PARTITION_COUNT) {
			return NULL;
		}
		if (!(handle = calloc(1, sizeof(BGENV)))) {
			return NULL;
		}
		handle->desc = (void *)&config_parts[index];
		handle->data = &oldenvs[index];
		handle->type = type;
		return handle;
	}
	return NULL;
}

BGENV *bgenv_get_oldest(BGENVTYPE type)
{
	uint32_t minrev = 0xFFFFFFFF;
	uint32_t min_idx = 0;

	switch (type) {
	case BGENVTYPE_FAT:
		for (int i = 0; i < CONFIG_PARTITION_COUNT; i++) {
			if (oldenvs[i].revision < minrev) {
				minrev = oldenvs[i].revision;
				min_idx = i;
			}
		}
		return bgenv_get_by_index(type, min_idx);
	}
	return NULL;
}

BGENV *bgenv_get_latest(BGENVTYPE type)
{
	uint32_t maxrev = 0;
	uint32_t max_idx = 0;

	switch (type) {
	case BGENVTYPE_FAT:
		for (int i = 0; i < CONFIG_PARTITION_COUNT; i++) {
			if (oldenvs[i].revision > maxrev) {
				maxrev = oldenvs[i].revision;
				max_idx = i;
			}
		}
		return bgenv_get_by_index(type, max_idx);
	}
	return NULL;
}

bool bgenv_write(BGENV *env)
{
	CONFIG_PART *part;

	if (!env) {
		return false;
	}
	switch (env->type) {
	case BGENVTYPE_FAT:
		part = (CONFIG_PART *)env->desc;
		if (!part) {
			VERBOSE(stderr, "Invalid config partition to store environment.\n");
			return false;
		}
		if (!write_env(part, env->data)) {
			VERBOSE(stderr, "Could not write to %s\n", part->devpath);
			return false;
		}
		return true;
	}
	return false;
}

BG_ENVDATA *bgenv_read(BGENV *env)
{
	if (!env) {
		return NULL;
	}
	return env->data;
}

bool bgenv_close(BGENV *env)
{
	if (env) {
		free(env);
		return true;
	}
	return false;
}
