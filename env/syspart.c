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

#include <syspart.h>
#include <utils.h>

#define MAX_INFO_SIZE 1024

EFI_STATUS enumerate_cfg_parts(EFI_FILE_HANDLE *roots, UINTN *numHandles)
{
	EFI_STATUS status;
	UINTN rootCount = 0;

	if (!roots || !numHandles) {
		Print(L"Invalid parameter in system partition enumeration.\n");
		return EFI_INVALID_PARAMETER;
	}
	for (UINTN index = 0; index < volume_count && rootCount < *numHandles;
	     index++) {
		EFI_FILE_HANDLE fh = NULL;
		if (!volumes[index].root) {
			continue;
		}
		status = uefi_call_wrapper(
		    volumes[index].root->Open, 5, volumes[index].root, &fh,
		    ENV_FILE_NAME, EFI_FILE_MODE_WRITE | EFI_FILE_MODE_READ,
		    EFI_FILE_ARCHIVE | EFI_FILE_HIDDEN | EFI_FILE_SYSTEM);
		if (status == EFI_SUCCESS) {
			Print(L"Config file found on volume %d.\n", index);
			roots[rootCount] = volumes[index].root;
			rootCount++;
			status = uefi_call_wrapper(volumes[index].root->Close,
						   1, fh);
			if (EFI_ERROR(status)) {
				Print(L"Could not close config file on "
				      L"partition %d.\n",
				      index);
			}
		}
	}
	*numHandles = rootCount;
	Print(L"%d config partitions detected.\n", rootCount);
	return EFI_SUCCESS;
}
