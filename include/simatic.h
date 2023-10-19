/*
 * EFI Boot Guard
 *
 * Copyright (c) Siemens AG, 2023
 *
 * Authors:
 *  Dr. Johann Pfefferl <johann.pfefferl@siemens.com>
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *  Cedric Hombourger <cedric.hombourger@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#pragma once

#include <efi.h>
#include <efilib.h>

#define SMBIOS_TYPE_OEM_129			129

#define SIMATIC_OEM_ENTRY_TYPE_BINARY		0xff

#define SIMATIC_IPC427E				0x0a01
#define SIMATIC_IPC477E				0x0a02
#define SIMATIC_IPCBX_59A			0x1202

typedef struct {
	UINT8	type;
	UINT8	length;
	UINT8	reserved[3];
	UINT32	station_id;
} __attribute__((packed)) SIMATIC_OEM_ENTRY;

UINT32 simatic_station_id(void);
