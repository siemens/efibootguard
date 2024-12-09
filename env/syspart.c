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

#include <syspart.h>
#include <utils.h>
#include <envdata.h>

#define MAX_INFO_SIZE 1024

EFI_STATUS enumerate_cfg_parts(UINTN *config_volumes, UINTN *numHandles)
{
	EFI_STATUS status;
	UINTN rootCount = 0;

	if (!config_volumes || !numHandles) {
		ERROR(L"Invalid parameter in system partition enumeration.\n");
		return EFI_INVALID_PARAMETER;
	}
	for (UINTN index = 0; index < volume_count && rootCount < *numHandles;
	     index++) {
		EFI_FILE_HANDLE fh = NULL;
		if (!volumes[index].root) {
			continue;
		}
		status = open_cfg_file(volumes[index].root, &fh,
				       EFI_FILE_MODE_READ);
		if (status == EFI_SUCCESS) {
			INFO(L"Config file found on volume %d.\n", index);
			config_volumes[rootCount] = index;
			rootCount++;
			status = close_cfg_file(volumes[index].root, fh);
			if (EFI_ERROR(status)) {
				ERROR(L"Could not close config file on partition %d.\n",
				      index);
			}
		}
	}
	*numHandles = rootCount;
	INFO(L"%d config partitions detected.\n", rootCount);
	return EFI_SUCCESS;
}

static VOID swap_uintn(UINTN *a, UINTN *b)
{
	UINTN tmp;
	tmp = *a;
	*a = *b;
	*b = tmp;
}

UINTN filter_cfg_parts(UINTN *config_volumes, UINTN numHandles)
{
	BOOLEAN use_envs_on_bootmedium_only = FALSE;

	INFO(L"Config filter: \n");
	for (UINTN index = 0; index < numHandles; index++) {
		VOLUME_DESC *v = &volumes[config_volumes[index]];

		if (v->onbootmedium) {
			use_envs_on_bootmedium_only = TRUE;
		};
	}

	if (!use_envs_on_bootmedium_only) {
		// nothing to do
		return numHandles;
	}

	INFO(L"Booting with environments from boot medium only.\n");
	UINTN num_sorted = 0;
	for (UINTN j = 0; j < numHandles; j++) {
		UINTN cvi = config_volumes[j];
		VOLUME_DESC *v = &volumes[cvi];

		if (v->onbootmedium) {
			swap_uintn(&config_volumes[j],
				   &config_volumes[num_sorted++]);
		} else {
			WARNING(L"Ignoring config on volume #%d\n", cvi);
		}
	}

	return num_sorted;
}
