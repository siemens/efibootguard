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

#include <efi.h>
#include <efilib.h>
#include <efiapi.h>
#include <bootguard.h>
#include <utils.h>
#include <syspart.h>
#include <envdata.h>

static int current_partition = 0;
static BG_ENVDATA *env;

static BG_STATUS save_current_config(const UINTN *config_volumes, UINTN numHandles)
{
	EFI_STATUS efistatus;

	if (numHandles != ENV_NUM_CONFIG_PARTS) {
		ERROR(L"Unexpected number of config partitions: found %d, but expected %d.\n",
		      numHandles, ENV_NUM_CONFIG_PARTS);
		/* In case of saving, this must be treated as error, to not
		 * overwrite another partition's config file. */
		return BG_CONFIG_ERROR;
	}

	VOLUME_DESC *v = &volumes[config_volumes[current_partition]];
	EFI_FILE_HANDLE fh = NULL;
	efistatus = open_cfg_file(v->root, &fh, EFI_FILE_MODE_WRITE |
				  EFI_FILE_MODE_READ);
	if (EFI_ERROR(efistatus)) {
		ERROR(L"Could not open environment file on system partition %d: %r\n",
		      current_partition, efistatus);
		return BG_CONFIG_ERROR;
	}

	UINTN writelen = sizeof(BG_ENVDATA);

	uint32_t crc32;
	(VOID) BS->CalculateCrc32(
	    &env[current_partition],
	    sizeof(BG_ENVDATA) - sizeof(env[current_partition].crc32), &crc32);
	env[current_partition].crc32 = crc32;
	efistatus = fh->Write(fh, &writelen, (VOID *)&env[current_partition]);
	if (EFI_ERROR(efistatus)) {
		ERROR(L"Cannot write environment to file: %r\n", efistatus);
		(VOID) close_cfg_file(v->root, fh);
		return BG_CONFIG_ERROR;
	}

	if (EFI_ERROR(close_cfg_file(v->root, fh))) {
		ERROR(L"Could not close environment config file.\n");
		return BG_CONFIG_ERROR;
	}

	return BG_SUCCESS;
}

BG_STATUS load_config(BG_LOADER_PARAMS *bglp)
{
	BG_STATUS result = BG_CONFIG_ERROR;
	UINTN numHandles = volume_count;
	UINTN *config_volumes;
	UINTN i;
	int env_invalid[ENV_NUM_CONFIG_PARTS] = {0};

	env = (BG_ENVDATA *)AllocateZeroPool(sizeof(BG_ENVDATA) *
					 ENV_NUM_CONFIG_PARTS);
	if (!env) {
		ERROR(L"Could not allocate memory for config data.\n");
		return result;
	}

	config_volumes = (UINTN *)AllocatePool(sizeof(UINTN) * volume_count);
	if (!config_volumes) {
		ERROR(L"Could not allocate memory for config partition mapping.\n");
		goto env_cleanup;
	}

	if (EFI_ERROR(enumerate_cfg_parts(config_volumes, &numHandles))) {
		ERROR(L"Could not enumerate config partitions.\n");
		goto lc_cleanup;
	}

	numHandles = filter_cfg_parts(config_volumes, numHandles);

	if (numHandles > ENV_NUM_CONFIG_PARTS) {
		ERROR(L"Too many config partitions found. Aborting.\n");
		goto lc_cleanup;
	}

	result = BG_SUCCESS;

	if (numHandles < ENV_NUM_CONFIG_PARTS) {
		WARNING(L"Too few config partitions: found: %d, but expected %d.\n",
			numHandles, ENV_NUM_CONFIG_PARTS);
		/* Don't treat this as error because we may still be able to
		 * find a valid config */
		result = BG_CONFIG_PARTIALLY_CORRUPTED;
	}

	/* Load all config data */
	for (i = 0; i < numHandles; i++) {
		EFI_FILE_HANDLE fh = NULL;
		VOLUME_DESC *v = &volumes[config_volumes[i]];
		if (EFI_ERROR(open_cfg_file(v->root, &fh,
					    EFI_FILE_MODE_READ))) {
			WARNING(L"Could not open environment file on config partition %d\n",
				i);
			result = BG_CONFIG_PARTIALLY_CORRUPTED;
			continue;
		}
		UINTN readlen = sizeof(BG_ENVDATA);
		if (EFI_ERROR(read_cfg_file(fh, &readlen, (VOID *)&env[i])) ||
		    readlen < sizeof(BG_ENVDATA)) {
			ERROR(L"Cannot read environment from config partition %d.\n", i);
			env_invalid[i] = 1;
			if (EFI_ERROR(close_cfg_file(v->root, fh))) {
				ERROR(L"Could not close environment config file.\n");
			}
			result = BG_CONFIG_PARTIALLY_CORRUPTED;
			continue;
		}

		uint32_t crc32;
		(VOID) BS->CalculateCrc32(
		    &env[i], sizeof(BG_ENVDATA) - sizeof(env[i].crc32), &crc32);

		if (crc32 != env[i].crc32) {
			ERROR(L"CRC32 error in environment data on config partition %d.\n",
			      i);
			INFO(L"calculated: %lx\n", crc32);
			INFO(L"stored: %lx\n", env[i].crc32);
			/* Don't treat this as fatal error because we may still
			 * have
			 * valid environments */
			env_invalid[i] = 1;
			result = BG_CONFIG_PARTIALLY_CORRUPTED;
		}

		if (EFI_ERROR(close_cfg_file(v->root, fh))) {
			ERROR(L"Could not close environment config file.\n");
			/* Don't abort, so we may still be able to boot a
			 * config */
			result = BG_CONFIG_PARTIALLY_CORRUPTED;
		}

		/* enforce NULL-termination of strings */
		env[i].kernelfile[ENV_STRING_LENGTH - 1] = 0;
		env[i].kernelparams[ENV_STRING_LENGTH - 1] = 0;
	}

	/* Find environment with latest revision and check if there is a test
	 * configuration. */
	UINTN latest_rev = 0, latest_idx = 0;
	UINTN pre_latest_rev = 0, pre_latest_idx = 0;
	for (i = 0; i < ENV_NUM_CONFIG_PARTS; i++) {
		if (!env_invalid[i]) {
			if (env[i].revision > latest_rev) {
				pre_latest_rev = latest_rev;
				latest_rev = env[i].revision;
				pre_latest_idx = latest_idx;
				latest_idx = i;
			} else if (env[i].revision > pre_latest_rev) {
				/* we always need a 2nd iteration if
				 * revisions are decreasing with growing i
				 * so that pre_* gets set */
				pre_latest_rev = env[i].revision;
				pre_latest_idx = i;
			}
		}
	}

	/* Assume we boot with the latest configuration */
	current_partition = latest_idx;

	/* Test if this environment is currently 'in_progress'. If yes,
	 * do not boot from it, instead ignore it */
	if (env[latest_idx].in_progress == 1) {
		current_partition = pre_latest_idx;
	} else if (env[latest_idx].ustate == USTATE_TESTING) {
		/* If it has already been booted, this indicates a failed
		 * update. In this case, mark it as failed by giving a
		 * zero-revision */
		env[latest_idx].ustate = USTATE_FAILED;
		env[latest_idx].revision = REVISION_FAILED;
		save_current_config(config_volumes, numHandles);
		/* We must boot with the configuration that was active before
		 */
		current_partition = pre_latest_idx;
	} else if (env[latest_idx].ustate == USTATE_INSTALLED) {
		/* If this configuration has never been booted with, set ustate
		 * to indicate that this configuration is now being tested */
		env[latest_idx].ustate = USTATE_TESTING;
		save_current_config(config_volumes, numHandles);
	}

	bglp->payload_path = StrDuplicate(env[current_partition].kernelfile);
	bglp->payload_options =
	    StrDuplicate(env[current_partition].kernelparams);
	bglp->timeout = env[current_partition].watchdog_timeout_sec;

	INFO(L"Config Revision: %d:\n", latest_rev);
	INFO(L" ustate: %d\n", env[current_partition].ustate);
	INFO(L" kernel: %s\n", bglp->payload_path);
	INFO(L" args: %s\n", bglp->payload_options);
	INFO(L" timeout: %d seconds\n", bglp->timeout);

lc_cleanup:
	FreePool(config_volumes);
env_cleanup:
	FreePool(env);
	return result;
}

BG_STATUS save_config(BG_LOADER_PARAMS *bglp)
{
	(VOID)bglp;
	return BG_NOT_IMPLEMENTED;
}
