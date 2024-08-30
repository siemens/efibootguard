/*
 * EFI Boot Guard, unified kernel stub
 *
 * Copyright (c) Siemens AG, 2022
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * SPDX-License-Identifier:	GPL-2.0-only
 */

#include <efi.h>
#include <efilib.h>

#include "kernel-stub.h"

typedef struct {
	VENDOR_DEVICE_PATH vendor;
	EFI_DEVICE_PATH end;
} __attribute__((packed)) INITRD_DEVICE_PATH;

#define LINUX_INITRD_MEDIA_GUID \
	{0x5568e427, 0x68fc, 0x4f3d, \
	 {0xac, 0x74, 0xca, 0x55, 0x52, 0x31, 0xcc, 0x68}}

typedef struct {
	EFI_LOAD_FILE_PROTOCOL protocol;
	const void *addr;
	UINTN size;
} INITRD_LOADER;

#ifndef EfiLoadFile2Protocol
static const EFI_GUID gEfiLoadFile2Protocol = {
	0x4006c0c1, 0xfcb3, 0x403e,
	{0x99, 0x6d, 0x4a, 0x6c, 0x87, 0x24, 0xe0, 0x6d}
};
#define EfiLoadFile2Protocol gEfiLoadFile2Protocol
#endif

static const INITRD_DEVICE_PATH initrd_device_path = {
	.vendor = {
		.Header.Type = MEDIA_DEVICE_PATH,
		.Header.SubType = MEDIA_VENDOR_DP,
		.Header.Length = {
			sizeof(initrd_device_path.vendor),
			0,
		},
		.Guid = LINUX_INITRD_MEDIA_GUID,
	},
	.end.Type = END_DEVICE_PATH_TYPE,
	.end.SubType = END_ENTIRE_DEVICE_PATH_SUBTYPE,
        .end.Length = {
		sizeof(initrd_device_path.end),
		0,
	},
};
static INITRD_LOADER initrd_loader;

static EFI_HANDLE initrd_handle;

static EFIAPI EFI_STATUS initrd_load_file(EFI_LOAD_FILE_PROTOCOL *this,
					  EFI_DEVICE_PATH *file_path,
					  BOOLEAN boot_policy,
					  UINTN *buffer_size,
					  VOID *buffer)
{
	INITRD_LOADER *loader = (INITRD_LOADER *) this;

	if (!loader || !file_path || !buffer_size) {
		return EFI_INVALID_PARAMETER;
	}
	if (boot_policy) {
		return EFI_UNSUPPORTED;
	}
	if (!buffer || *buffer_size < loader->size) {
		*buffer_size = loader->size;
		return EFI_BUFFER_TOO_SMALL;
	}

	CopyMem(buffer, (VOID*)loader->addr, loader->size);
	*buffer_size = loader->size;

	return EFI_SUCCESS;
}

VOID install_initrd_loader(VOID *initrd, UINTN initrd_size)
{
	EFI_STATUS status;

	initrd_loader.protocol.LoadFile = initrd_load_file;
	initrd_loader.addr = initrd;
	initrd_loader.size = initrd_size;

	status = BS->InstallMultipleProtocolInterfaces(
			&initrd_handle, &DevicePathProtocol,
			&initrd_device_path, &EfiLoadFile2Protocol,
			&initrd_loader, NULL);
	if (EFI_ERROR(status)) {
		error_exit(L"Error registering initrd loader", status);
	}
}

VOID uninstall_initrd_loader(VOID)
{
	EFI_STATUS status;

	if (!initrd_handle) {
		return;
	}

	status = BS->UninstallMultipleProtocolInterfaces(
			initrd_handle, &DevicePathProtocol,
			&initrd_device_path, &EfiLoadFile2Protocol,
			&initrd_loader, NULL);
	if (EFI_ERROR(status)) {
		error_exit(L"Error unregistering initrd loader", status);
	}
}
