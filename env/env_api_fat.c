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

#include "env_api.h"
#include "env_disk_utils.h"
#include "env_config_partitions.h"
#include "env_config_file.h"
#include "uservars.h"
#include "test-interface.h"
#include "ebgpart.h"

extern ebgenv_opts_t ebgenv_opts;

EBGENVKEY bgenv_str2enum(const char *key)
{
	if (strcmp(key, "kernelfile") == 0) {
		return EBGENV_KERNELFILE;
	}
	if (strcmp(key, "kernelparams") == 0) {
		return EBGENV_KERNELPARAMS;
	}
	if (strcmp(key, "watchdog_timeout_sec") == 0) {
		return EBGENV_WATCHDOG_TIMEOUT_SEC;
	}
	if (strcmp(key, "revision") == 0) {
		return EBGENV_REVISION;
	}
	if (strcmp(key, "ustate") == 0) {
		return EBGENV_USTATE;
	}
	if (strcmp(key, "in_progress") == 0) {
		return EBGENV_IN_PROGRESS;
	}
	return EBGENV_UNKNOWN;
}

void bgenv_be_verbose(bool v)
{
	ebgpart_beverbose(v);
}

static void clear_envdata(BG_ENVDATA *data)
{
	memset(data, 0, sizeof(BG_ENVDATA));
	data->crc32 = bgenv_crc32(0, data,
				  sizeof(BG_ENVDATA) - sizeof(data->crc32));
}

bool validate_envdata(BG_ENVDATA *data)
{
	uint32_t sum = bgenv_crc32(0, data,
				   sizeof(BG_ENVDATA) - sizeof(data->crc32));

	if (data->crc32 != sum) {
		VERBOSE(stderr, "Invalid CRC32!\n");
		/* clear invalid environment */
		clear_envdata(data);
		return false;
	}
	if (!bgenv_validate_uservars(data->userdata)) {
		VERBOSE(stderr, "Corrupt uservars!\n");
		/* clear invalid environment */
		clear_envdata(data);
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
	config = open_config_file_from_part(part, "rb");
	if (!config) {
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
	if (result == false) {
		clear_envdata(env);
		return false;
	}

	/* enforce NULL-termination of strings */
	env->kernelfile[ENV_STRING_LENGTH - 1] = 0;
	env->kernelparams[ENV_STRING_LENGTH - 1] = 0;

	return validate_envdata(env);
}

bool write_env(CONFIG_PART *part, const BG_ENVDATA *env)
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
	config = open_config_file_from_part(part, "wb");
	if (!config) {
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

/* Weaken the symbols in order to permit overloading in the test cases. */
CONFIG_PART __attribute__((weak)) config_parts[ENV_NUM_CONFIG_PARTS];
BG_ENVDATA __attribute__((weak)) envdata[ENV_NUM_CONFIG_PARTS];

static bool initialized;

bool bgenv_init(void)
{
	if (initialized) {
		return true;
	}
	/* enumerate all config partitions */
	if (!probe_config_partitions(config_parts,
				     ebgenv_opts.search_all_devices)) {
		VERBOSE(stderr, "Error finding config partitions.\n");
		return false;
	}
	for (int i = 0; i < ENV_NUM_CONFIG_PARTS; i++) {
		read_env(&config_parts[i], &envdata[i]);
	}
	initialized = true;
	return true;
}

void bgenv_finalize(void)
{
	if (!initialized) {
		return;
	}
	for (int i = 0; i < ENV_NUM_CONFIG_PARTS; i++) {
		free(config_parts[i].devpath);
		config_parts[i].devpath = NULL;
		free(config_parts[i].mountpoint);
		config_parts[i].mountpoint = NULL;
	}
	initialized = false;
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

BGENV *bgenv_open_oldest(void)
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

BGENV *bgenv_open_latest(void)
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

BG_ENVDATA *bgenv_read(const BGENV *env)
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
void bgenv_close(BGENV *env)
{
	free(env);
}

static int bgenv_get_uint(char *buffer, uint64_t *type, void *data,
			  unsigned int src, uint64_t t)
{
	int res;

	res = sprintf(buffer, "%u", src);
	if (!data) {
		return res+1;
	}
	memcpy(data, buffer, res+1);
	if (type) {
		*type = t;
	}
	return 0;
}

static int bgenv_get_string(char *buffer, uint64_t *type, void *data,
			    const char16_t *srcstr)
{
	str16to8(buffer, srcstr);
	if (!data) {
		return strlen(buffer)+1;
	}
	strcpy(data, buffer);
	if (type) {
		*type = USERVAR_TYPE_STRING_ASCII;
	}
	return 0;
}

int bgenv_get(BGENV *env, const char *key, uint64_t *type, void *data,
	      uint32_t maxlen)
{
	EBGENVKEY e;
	char buffer[ENV_STRING_LENGTH];

	if (!key || maxlen == 0) {
		return -EINVAL;
	}
	e = bgenv_str2enum(key);
	if (!env) {
		return -EPERM;
	}
	if (e == EBGENV_UNKNOWN) {
		if (!data) {
			uint8_t *u;
			uint32_t size;
			u = bgenv_find_uservar(env->data->userdata, key);
			if (!u) {
				return -ENOENT;
			}
			bgenv_map_uservar(u, NULL, NULL, NULL, NULL, &size);
			return size;
		}
		return bgenv_get_uservar(env->data->userdata, key, type, data,
					 maxlen);
	}
	switch (e) {
	case EBGENV_KERNELFILE:
		return bgenv_get_string(buffer, type, data,
					env->data->kernelfile);
	case EBGENV_KERNELPARAMS:
		return bgenv_get_string(buffer, type, data,
					env->data->kernelparams);
	case EBGENV_WATCHDOG_TIMEOUT_SEC:
		return bgenv_get_uint(buffer, type, data,
				      env->data->watchdog_timeout_sec,
				      USERVAR_TYPE_UINT16);
	case EBGENV_REVISION:
		return bgenv_get_uint(buffer, type, data,
				      env->data->revision,
				      USERVAR_TYPE_UINT16);
	case EBGENV_USTATE:
		return bgenv_get_uint(buffer, type, data,
				      env->data->ustate,
				      USERVAR_TYPE_UINT8);
	case EBGENV_IN_PROGRESS:
		return bgenv_get_uint(buffer, type, data,
				      env->data->in_progress,
				      USERVAR_TYPE_UINT8);
	default:
		if (!data) {
			return 0;
		}
		return -EINVAL;
	}
}

static long bgenv_convert_to_long(const char *value)
{
	long val;
	char *p;

	errno = 0;
	val = strtol(value, &p, 10);
	if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) ||
	    (errno != 0 && val == 0)) {
		return -errno;
	}
	if (p == value) {
		return -EINVAL;
	}
	return val;
}

int bgenv_set(BGENV *env, const char *key, uint64_t type, const void *data,
	      uint32_t datalen)
{
	EBGENVKEY e;
	int val;
	const char *value = (const char *)data;

	if (!key || !data || datalen == 0) {
		return -EINVAL;
	}

	e = bgenv_str2enum(key);
	if (!env) {
		return -EPERM;
	}
	if (e == EBGENV_UNKNOWN) {
		return bgenv_set_uservar(env->data->userdata, key, type, data,
					 datalen);
	}
	switch (e) {
	case EBGENV_REVISION:
		val = bgenv_convert_to_long(value);
		if (val < 0) {
			return -EINVAL;
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
		val = bgenv_convert_to_long(value);
		if (val < 0) {
			return -EINVAL;
		}
		env->data->watchdog_timeout_sec = val;
		break;
	case EBGENV_USTATE:
		val = bgenv_convert_to_long(value);
		if (val < 0) {
			return -EINVAL;
		}
		env->data->ustate = val;
		break;
	case EBGENV_IN_PROGRESS:
		val = bgenv_convert_to_long(value);
		if (val < 0) {
			return -EINVAL;
		}
		env->data->in_progress = val;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

BGENV *bgenv_create_new(void)
{
	BGENV *env_latest;
	BGENV *env_new;

	env_latest = bgenv_open_latest();
	if (!env_latest) {
		goto create_new_io_error;
	}

	int new_rev = env_latest->data->revision + 1;

	env_new = bgenv_open_oldest();
	if (!env_new) {
		bgenv_close(env_latest);
		goto create_new_io_error;
	}

	if (env_latest->data != env_new->data) {
		/* zero fields */
		memset(env_new->data, 0, sizeof(BG_ENVDATA));
		/* set default watchdog timeout */
		env_new->data->watchdog_timeout_sec = DEFAULT_TIMEOUT_SEC;
	}
	bgenv_close(env_latest);
	/* update revision field and testing mode */
	env_new->data->revision = new_rev;
	env_new->data->in_progress = 1;

	return env_new;

create_new_io_error:
	errno = EIO;
	return NULL;
}
