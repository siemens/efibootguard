/*
 * EFI Boot Guard
 *
 * Copyright (c) Siemens AG, 2020-2023
 *
 * Authors:
 *  Dr. Johann Pfefferl <johann.pfefferl@siemens.com>
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <smbios.h>

SMBIOS_STRUCTURE_POINTER smbios_find_struct(SMBIOS_STRUCTURE_TABLE *table,
					    UINT16 type)
{
	SMBIOS_STRUCTURE_POINTER strct;
	UINT8 *str;
	UINTN n;

	strct.Raw = (UINT8 *)(uintptr_t)table->TableAddress;

	for (n = 0; n < table->NumberOfSmbiosStructures; n++) {
		if (strct.Hdr->Type == type) {
			return strct;
		}
		/* Read over any appended strings. */
		str = strct.Raw + strct.Hdr->Length;
		while (str[0] != 0 || str[1] != 0) {
			str++;
		}
		strct.Raw = str + 2;
	}

	strct.Raw = NULL;
	return strct;
}
