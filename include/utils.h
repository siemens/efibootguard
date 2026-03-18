/*
 * EFI Boot Guard
 *
 * Copyright (c) Siemens AG, 2017-2025
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *  Andreas Reichel <andreas.reichel.ext@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * SPDX-License-Identifier:	GPL-2.0-only
 */

#pragma once

#include <efi.h>
#include <efilib.h>

#define MAX_INFO_SIZE 1024

typedef struct _VOLUME_DESC {
	EFI_DEVICE_PATH *devpath;
	BOOLEAN onbootmedium;
	CHAR16 *fslabel;
	CHAR16 *fscustomlabel;
	EFI_FILE_HANDLE root;
} VOLUME_DESC;

extern VOLUME_DESC *volumes;
extern UINTN volume_count;
extern CHAR16 *boot_medium_path;

typedef enum { DOSFSLABEL, CUSTOMLABEL, NOLABEL } LABELMODE;

EFI_STATUS get_volumes(VOLUME_DESC **volumes, UINTN *count);
EFI_STATUS close_volumes(VOLUME_DESC *volumes, UINTN count);
EFI_DEVICE_PATH *FileDevicePathFromConfig(EFI_HANDLE device,
					  CHAR16 *payloadpath);
CHAR16 *GetMediumPath(const CHAR16 *input);

#define BIT(x) (1UL << (x))
