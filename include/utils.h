/*
 * EFI Boot Guard
 *
 * Copyright (c) Siemens AG, 2017
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *  Andreas Reichel <andreas.reichel.ext@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#ifndef __H_UTILS__
#define __H_UTILS__

#include "bootguard.h"

#define MAX_INFO_SIZE 1024

#define sleep(X)							      \
	uefi_call_wrapper(BS->Stall, 1, (X) * 1000 * 1000l)

typedef struct _VOLUME_DESC {
	EFI_DEVICE_PATH *devpath;
	CHAR16 *fslabel;
	CHAR16 *fscustomlabel;
	EFI_FILE_HANDLE root;
} VOLUME_DESC;

typedef enum { DOSFSLABEL, CUSTOMLABEL, NOLABEL } LABELMODE;

uint32_t calc_crc32(void *data, int32_t size);
void __noreturn error_exit(CHAR16 *message, EFI_STATUS status);
VOID *mmalloc(UINTN bytes);
EFI_STATUS mfree(VOID *p);
CHAR16 *get_volume_label(EFI_FILE_HANDLE fh);
EFI_STATUS get_volumes(VOLUME_DESC **volumes, UINTN *count);
EFI_STATUS close_volumes(VOLUME_DESC *volumes, UINTN count);
EFI_DEVICE_PATH *FileDevicePathFromConfig(EFI_HANDLE device,
					  CHAR16 *payloadpath);
CHAR16 *GetBootMediumPath(CHAR16 *input);
BOOLEAN IsOnBootMedium(EFI_DEVICE_PATH *dp);
VOID Color(EFI_SYSTEM_TABLE *system_table, char fgcolor, char bgcolor);

#endif // __H_UTILS__
