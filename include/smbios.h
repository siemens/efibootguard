/*
 * EFI Boot Guard
 *
 * Copyright (c) Siemens AG, 2020-2023
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#pragma once

#include <efi.h>
#include <efilib.h>

SMBIOS_STRUCTURE_POINTER smbios_find_struct(SMBIOS_STRUCTURE_TABLE *table,
					    UINT16 type);
