/*
 * EFI Boot Guard
 *
 * Copyright (c) Siemens AG, 2023
 *
 * Authors:
 *  Felix Moessbauer <felix.moessbauer@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * SPDX-License-Identifier:	GPL-2.0-only
 */

#pragma once

typedef struct _BG_INTERFACE_PARAMS {
	CHAR16 *loader_device_part_uuid;
} BG_INTERFACE_PARAMS;

// systemd bootloader interface vendor id
extern EFI_GUID vendor_guid;

EFI_STATUS set_bg_interface_vars(const BG_INTERFACE_PARAMS *params);
CHAR16 *disk_get_part_uuid(EFI_HANDLE *handle);
