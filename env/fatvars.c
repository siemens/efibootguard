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

#include <bootguard.h>
#include <syspart.h>
#include <envdata.h>

static int current_partition = 0;
static BG_ENVDATA env[ENV_NUM_CONFIG_PARTS];

BG_STATUS save_current_config(void)
{
	BG_STATUS result = BG_SUCCESS;
	EFI_STATUS efistatus;
	UINTN numHandles = volume_count;
	EFI_FILE_HANDLE *roots;

	roots = (EFI_FILE_HANDLE *)mmalloc(sizeof(EFI_FILE_HANDLE) *
					   volume_count);
	if (!roots) {
		Print(L"Error, could not allocate memory for config partition "
		      L"handles.\n");
		return BG_CONFIG_ERROR;
	}

	if (EFI_ERROR(enumerate_cfg_parts(roots, &numHandles))) {
		Print(L"Error, could not enumerate config partitions.");
		mfree(roots);
		return BG_CONFIG_ERROR;
	}

	if (numHandles != ENV_NUM_CONFIG_PARTS) {
		Print(L"Error, unexpected number of config partitions: found "
		      L"%d, but expected %d.\n",
		      numHandles, ENV_NUM_CONFIG_PARTS);
		/* In case of saving, this must be treated as error, to not
		 * overwrite another partition's config file. */
		mfree(roots);
		return BG_CONFIG_ERROR;
	}

	EFI_FILE_HANDLE fh = NULL;
	efistatus = open_cfg_file(roots[current_partition], &fh,
				  EFI_FILE_MODE_WRITE | EFI_FILE_MODE_READ);
	if (EFI_ERROR(efistatus)) {
		Print(L"Error, could not open environment file on system "
		      L"partition %d: %r\n",
		      current_partition, efistatus);
		mfree(roots);
		return BG_CONFIG_ERROR;
	}

	UINTN writelen = sizeof(BG_ENVDATA);
	uint32_t crc32 = calc_crc32(&env[current_partition],
				    sizeof(BG_ENVDATA) -
					sizeof(env[current_partition].crc32));
	env[current_partition].crc32 = crc32;
	efistatus = uefi_call_wrapper(fh->Write, 3, fh, &writelen,
				      (VOID *)&env[current_partition]);
	if (EFI_ERROR(efistatus)) {
		Print(L"Error writing environment to file: %r\n", efistatus);
		result = BG_CONFIG_ERROR;
	}

	if (EFI_ERROR(close_cfg_file(roots[current_partition], fh))) {
		Print(L"Error, could not close environment config file.\n");
		result = BG_CONFIG_ERROR;
	}
	mfree(roots);
	return result;
}

BG_STATUS load_config(BG_LOADER_PARAMS *bglp)
{
	BG_STATUS result = BG_SUCCESS;
	UINTN numHandles = volume_count;
	EFI_FILE_HANDLE *roots;
	UINTN i;
	int env_invalid[ENV_NUM_CONFIG_PARTS] = {0};

	roots = (EFI_FILE_HANDLE *)mmalloc(sizeof(EFI_FILE_HANDLE) *
					   volume_count);
	if (!roots) {
		Print(L"Error, could not allocate memory for config partition "
		      L"handles.\n");
		return BG_CONFIG_ERROR;
	}

	if (EFI_ERROR(enumerate_cfg_parts(roots, &numHandles))) {
		Print(L"Error, could not enumerate config partitions.");
		mfree(roots);
		return BG_CONFIG_ERROR;
	}

	if (numHandles > ENV_NUM_CONFIG_PARTS) {
		Print(L"Error, too many config partitions found. Aborting.\n");
		mfree(roots);
		return BG_CONFIG_ERROR;
	}

	if (numHandles < ENV_NUM_CONFIG_PARTS) {
		Print(L"Warning, too few config partitions: found: "
		      L"%d, but expected %d.\n",
		      numHandles, ENV_NUM_CONFIG_PARTS);
		/* Don't treat this as error because we may still be able to
		 * find a valid config */
		result = BG_CONFIG_PARTIALLY_CORRUPTED;
	}

	/* Load all config data */
	for (i = 0; i < numHandles; i++) {
		EFI_FILE_HANDLE fh = NULL;
		if (EFI_ERROR(open_cfg_file(roots[i], &fh,
			                    EFI_FILE_MODE_READ))) {
			Print(L"Warning, could not open environment file on "
			      L"config partition %d\n",
			      i);
			result = BG_CONFIG_PARTIALLY_CORRUPTED;
			continue;
		}
		UINTN readlen = sizeof(BG_ENVDATA);
		if (EFI_ERROR(read_cfg_file(fh, &readlen, (VOID *)&env[i]))) {
			Print(L"Error reading environment from config "
			      L"partition %d.\n",
			      i);
			env_invalid[i] = 1;
			if (EFI_ERROR(close_cfg_file(roots[i], fh))) {
				Print(L"Error, could not close environment "
				      L"config file.\n");
			}
			result = BG_CONFIG_PARTIALLY_CORRUPTED;
			continue;
		}

		uint32_t crc32 = calc_crc32(&env[i], sizeof(BG_ENVDATA) -
							 sizeof(env[i].crc32));
		if (crc32 != env[i].crc32) {
			Print(L"CRC32 error in environment data on config "
			      L"partition %d.\n",
			      i);
			Print(L"calculated: %lx\n", crc32);
			Print(L"stored: %lx\n", env[i].crc32);
			/* Don't treat this as fatal error because we may still
			 * have
			 * valid environments */
			env_invalid[i] = 1;
			result = BG_CONFIG_PARTIALLY_CORRUPTED;
		}

		if (EFI_ERROR(uefi_call_wrapper(fh->Close, 1, fh))) {
			Print(L"Error, could not close environment config "
			      L"file.\n");
			/* Don't abort, so we may still be able to boot a
			 * config */
			result = BG_CONFIG_PARTIALLY_CORRUPTED;
		}
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
		save_current_config();
		/* We must boot with the configuration that was active before
		 */
		current_partition = pre_latest_idx;
	} else if (env[latest_idx].ustate == USTATE_INSTALLED) {
		/* If this configuration has never been booted with, set ustate
		 * to indicate that this configuration is now being tested */
		env[latest_idx].ustate = USTATE_TESTING;
		save_current_config();
	}

	bglp->payload_path = StrDuplicate(env[current_partition].kernelfile);
	bglp->payload_options =
	    StrDuplicate(env[current_partition].kernelparams);
	bglp->timeout = env[current_partition].watchdog_timeout_sec;

	Print(L"Config Revision: %d:\n", latest_rev);
	Print(L" ustate: %d\n",
	      env[current_partition].ustate);
	Print(L" kernel: %s\n", bglp->payload_path);
	Print(L" args: %s\n", bglp->payload_options);
	Print(L" timeout: %d seconds\n", bglp->timeout);
	mfree(roots);
	return result;
}

BG_STATUS save_config(BG_LOADER_PARAMS *bglp)
{
	(void)bglp;
	return BG_NOT_IMPLEMENTED;
}
