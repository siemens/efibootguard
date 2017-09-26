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

#include "env_api.h"
#include "ebgpart.h"
#include "test-interface.h"

const char *tmp_mnt_dir = "/tmp/mnt-XXXXXX";

static bool verbosity = false;

static EBGENVKEY bgenv_str2enum(char *key)
{
	if (strncmp(key, "kernelfile", strlen("kernelfile") + 1) == 0) {
		return EBGENV_KERNELFILE;
	}
	if (strncmp(key, "kernelparams", strlen("kernelparams") + 1) == 0) {
		return EBGENV_KERNELPARAMS;
	}
	if (strncmp(key, "watchdog_timeout_sec",
		    strlen("watchdog_timeout_sec") + 1) == 0) {
		return EBGENV_WATCHDOG_TIMEOUT_SEC;
	}
	if (strncmp(key, "revision", strlen("revision") + 1) == 0) {
		return EBGENV_REVISION;
	}
	if (strncmp(key, "ustate", strlen("ustate") + 1) == 0) {
		return EBGENV_USTATE;
	}
	return EBGENV_UNKNOWN;
}

void bgenv_be_verbose(bool v)
{
	verbosity = v;
	ebgpart_beverbose(v);
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

__attribute__((noinline))
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

static void unmount_partition(CONFIG_PART *cfgpart)
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
	FILE *config = fopen(configfilepath, mode);
	free(configfilepath);
	return config;
}

__attribute__((noinline))
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
			    (strcmp(part->fs_type->name, "fat12") != 0 &&
			     strcmp(part->fs_type->name, "fat16") != 0 &&
			     strcmp(part->fs_type->name, "fat32") != 0)) {
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
				if (count >= ENV_NUM_CONFIG_PARTS) {
					VERBOSE(stderr, "Error, there are "
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

static CONFIG_PART config_parts[ENV_NUM_CONFIG_PARTS];
static BG_ENVDATA envdata[ENV_NUM_CONFIG_PARTS];

bool bgenv_init()
{
	memset((void *)&config_parts, 0,
	       sizeof(CONFIG_PART) * ENV_NUM_CONFIG_PARTS);
	/* enumerate all config partitions */
	if (!probe_config_partitions(config_parts)) {
		VERBOSE(stderr, "Error finding config partitions.\n");
		return false;
	}
	for (int i = 0; i < ENV_NUM_CONFIG_PARTS; i++) {
		read_env(&config_parts[i], &envdata[i]);
		uint32_t sum = crc32(0, (Bytef *)&envdata[i],
		    sizeof(BG_ENVDATA) - sizeof(envdata[i].crc32));
		if (envdata[i].crc32 != sum) {
			VERBOSE(stderr, "Invalid CRC32!\n");
			/* clear invalid environment */
			memset(&envdata[i], 0, sizeof(BG_ENVDATA));
			envdata[i].crc32 = crc32(0, (Bytef *)&envdata[i],
			    sizeof(BG_ENVDATA) - sizeof(envdata[i].crc32));
		}
	}
	return true;
}

BGENV *bgenv_open_by_index(uint32_t index)
{
	BGENV *handle;

	/* get config partition by index and allocate handle */
	if (index >= ENV_NUM_CONFIG_PARTS) {
		return NULL;
	}
	if (!(handle = calloc(1, sizeof(BGENV)))) {
		return NULL;
	}
	handle->desc = (void *)&config_parts[index];
	handle->data = &envdata[index];
	return handle;
}

BGENV *bgenv_open_oldest()
{
	uint32_t minrev = 0xFFFFFFFF;
	uint32_t min_idx = 0;

	for (int i = 0; i < ENV_NUM_CONFIG_PARTS; i++) {
		if (envdata[i].revision < minrev) {
			minrev = envdata[i].revision;
			min_idx = i;
		}
	}
	return bgenv_open_by_index(min_idx);
}

BGENV *bgenv_open_latest()
{
	uint32_t maxrev = 0;
	uint32_t max_idx = 0;

	for (int i = 0; i < ENV_NUM_CONFIG_PARTS; i++) {
		if (envdata[i].revision > maxrev) {
			maxrev = envdata[i].revision;
			max_idx = i;
		}
	}
	return bgenv_open_by_index(max_idx);
}

bool bgenv_write(BGENV *env)
{
	CONFIG_PART *part;

	if (!env) {
		return false;
	}
	part = (CONFIG_PART *)env->desc;
	if (!part) {
		VERBOSE(
		    stderr,
		    "Invalid config partition to store environment.\n");
		return false;
	}
	if (!write_env(part, env->data)) {
		VERBOSE(stderr, "Could not write to %s\n",
			part->devpath);
		return false;
	}
	return true;
}

BG_ENVDATA *bgenv_read(BGENV *env)
{
	if (!env) {
		return NULL;
	}
	return env->data;
}

/* TODO: Refactored API has tests with static struct, that cannot be freed. If
 * gcc inlines this function within this translation unit, tests cannot
 * overload the function by weakening. Thus, define it as noinline until tests
 * are redesigned.
 */
__attribute((noinline))
bool bgenv_close(BGENV *env)
{
	if (env) {
		free(env);
		return true;
	}
	return false;
}

int bgenv_get(BGENV *env, char *key, char **type, void *data, size_t maxlen)
{
	EBGENVKEY e;

	if (!key || !data || maxlen == 0) {
		return EINVAL;
	}
	e = bgenv_str2enum(key);
	if (e == EBGENV_UNKNOWN) {
		return EINVAL;
	}
	if (!env) {
		return EPERM;
	}
	switch (e) {
	case EBGENV_KERNELFILE:
		str16to8(data, env->data->kernelfile);
		if (type) {
			sprintf(*type, "char*");
		}
		break;
	case EBGENV_KERNELPARAMS:
		str16to8(data, env->data->kernelparams);
		if (type) {
			sprintf(*type, "char*");
		}
		break;
	case EBGENV_WATCHDOG_TIMEOUT_SEC:
		sprintf(data, "%lu", env->data->watchdog_timeout_sec);
		if (type) {
			sprintf(*type, "uint16_t");
		}
		break;
	case EBGENV_REVISION:
		sprintf(data, "%lu", env->data->revision);
		if (type) {
			sprintf(*type, "uint32_t");
		}
		break;
	case EBGENV_USTATE:
		sprintf(data, "%u", env->data->ustate);
		if (type) {
			sprintf(*type, "uint16_t");
		}
		break;
	default:
		return EINVAL;
	}
	return 0;
}

int bgenv_set(BGENV *env, char *key, char *type, void *data, size_t datalen)
{
	EBGENVKEY e;
	int val;
	char *p;
	char *value = (char *)data;

	if (!key || !data || datalen == 0) {
		return EINVAL;
	}

	e = bgenv_str2enum(key);
	if (e == EBGENV_UNKNOWN) {
		return EINVAL;
	}
	if (!env) {
		return EPERM;
	}
	switch (e) {
	case EBGENV_REVISION:
		val = strtol(value, &p, 10);
		if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) ||
		    (errno != 0 && val == 0)) {
			return errno;
		}
		if (p == value) {
			return EINVAL;
		}
		env->data->revision = val;
		break;
	case EBGENV_KERNELFILE:
		str8to16(env->data->kernelfile, value);
		break;
	case EBGENV_KERNELPARAMS:
		str8to16(env->data->kernelparams, value);
		break;
	case EBGENV_WATCHDOG_TIMEOUT_SEC:
		val = strtol(value, &p, 10);
		if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) ||
		    (errno != 0 && val == 0)) {
			return errno;
		}
		if (p == value) {
			return EINVAL;
		}
		env->data->watchdog_timeout_sec = val;
		break;
	case EBGENV_USTATE:
		val = strtol(value, &p, 10);
		if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) ||
		    (errno != 0 && val == 0)) {
			return errno;
		}
		if (p == value) {
			return EINVAL;
		}
		env->data->ustate = val;
		break;
	default:
		return EINVAL;
	}
	return 0;
}

BGENV *bgenv_create_new()
{
	BGENV *env_latest;
	BGENV *env_new;

	env_latest = bgenv_open_latest();
	if (!env_latest)
		goto create_new_io_error;

	int new_rev = env_latest->data->revision + 1;

	if (!bgenv_close(env_latest))
		goto create_new_io_error;

	env_new = bgenv_open_oldest();
	if (!env_new)
		goto create_new_io_error;

	/* zero fields */
	memset(env_new->data, 0, sizeof(BG_ENVDATA));
	/* update revision field and testing mode */
	env_new->data->revision = new_rev;
	env_new->data->ustate = 1;
	/* set default watchdog timeout */
	env_new->data->watchdog_timeout_sec = 30;

	return env_new;

create_new_io_error:
	errno = EIO;
	return NULL;
}
