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
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <efi.h>
#include <efilib.h>
#include "loader_interface.h"
#include "utils.h"

EFI_GUID vendor_guid = {0x4a67b082,
			0x0a4c,
			0x41cf,
			{0xb6, 0xc7, 0x44, 0x0b, 0x29, 0xbb, 0x8c, 0x4f}};

EFI_STATUS set_bg_interface_vars(const BG_INTERFACE_PARAMS *params)
{
	EFI_STATUS status = EFI_SUCCESS;
	UINT32 attribs =
		EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS;

	// only set this if not set by a previous stage loader
	UINTN readsize = 0;
	if (RT->GetVariable(L"LoaderDevicePartUUID", &vendor_guid, NULL,
			    &readsize, NULL) == EFI_NOT_FOUND) {
		status = RT->SetVariable(
			L"LoaderDevicePartUUID", &vendor_guid, attribs,
			StrLen(params->loader_device_part_uuid) *
				sizeof(UINT16),
			params->loader_device_part_uuid);
	}
	return status;
}

CHAR16 *disk_get_part_uuid(EFI_HANDLE *handle)
{
	EFI_STATUS err;
	EFI_DEVICE_PATH *dp;
	err = BS->HandleProtocol(handle, &DevicePathProtocol, (void **)&dp);
	if (EFI_ERROR(err)) {
		return NULL;
	}

	for (; !IsDevicePathEnd(dp); dp = NextDevicePathNode(dp)) {
		if (dp->Type != MEDIA_DEVICE_PATH ||
		    dp->SubType != MEDIA_HARDDRIVE_DP) {
			continue;
		}

		HARDDRIVE_DEVICE_PATH *hd = (HARDDRIVE_DEVICE_PATH *)dp;
		if (hd->SignatureType != SIGNATURE_TYPE_GUID) {
			continue;
		}
		UINT16 *buffer = AllocatePool(sizeof(UINT16) * 37);
		GuidToString(buffer, (EFI_GUID *)hd->Signature);
		return buffer;
	}

	return NULL;
}
