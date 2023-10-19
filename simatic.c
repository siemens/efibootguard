/*
 * EFI Boot Guard
 *
 * Copyright (c) Siemens AG, 2020
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

#include <efi.h>
#include <efilib.h>
#include "simatic.h"
#include "utils.h"

static UINT32 get_station_id(SMBIOS_STRUCTURE_POINTER oem_strct)
{
	SIMATIC_OEM_ENTRY *entry;
	UINTN n;

	entry = (SIMATIC_OEM_ENTRY *)(oem_strct.Raw + sizeof(*oem_strct.Hdr));

	/* Find 4th entry in OEM data. */
	for (n = 0; n < 3; n++) {
		if (entry->type != SIMATIC_OEM_ENTRY_TYPE_BINARY) {
			return 0;
		}
		entry = (SIMATIC_OEM_ENTRY *)((UINT8 *)entry + entry->length);
	}

	if (entry->type == SIMATIC_OEM_ENTRY_TYPE_BINARY &&
	    entry->length == sizeof(SIMATIC_OEM_ENTRY)) {
		return entry->station_id;
	}

	return 0;
}

UINT32 simatic_station_id(VOID)
{
	SMBIOS_STRUCTURE_TABLE *smbios_table;
	SMBIOS_STRUCTURE_POINTER smbios_struct;
	EFI_STATUS status;

	status = LibGetSystemConfigurationTable(&SMBIOSTableGuid,
						(VOID **)&smbios_table);
	if (status != EFI_SUCCESS) {
		return 0;
	}

	smbios_struct = smbios_find_struct(smbios_table, SMBIOS_TYPE_OEM_129);
	if (smbios_struct.Raw == NULL) {
		return 0;
	}

	return get_station_id(smbios_struct);
}
