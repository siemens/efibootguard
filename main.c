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

#include <efi.h>
#include <efilib.h>
#include <efiprot.h>
#include <efipciio.h>
#include <pci/header.h>
#include <bootguard.h>
#include <configuration.h>
#include "version.h"
#include "utils.h"

extern const unsigned long init_array_start[];
extern const unsigned long init_array_end[];
extern CHAR16 *boot_medium_path;

#define PCI_GET_VENDOR_ID(id)	(UINT16)(id)
#define PCI_GET_PRODUCT_ID(id)	(UINT16)((id) >> 16)

typedef EFI_STATUS (*WATCHDOG_PROBE)(EFI_PCI_IO *, UINT16, UINT16, UINTN);

static EFI_STATUS probe_watchdogs(EFI_LOADED_IMAGE *loaded_image, UINTN timeout)
{
	if (init_array_end - init_array_start == 0) {
		if (timeout > 0) {
			ERROR(L"No watchdog drivers registered, but timeout is non-zero.\n");
			return EFI_UNSUPPORTED;
		}
		return EFI_SUCCESS;
	}
	if (timeout == 0) {
		WARNING(L"Watchdog is disabled.\n");
		return EFI_SUCCESS;
	}

	UINTN handle_count = 0;
	EFI_HANDLE *handle_buffer = NULL;
	EFI_STATUS status = BS->LocateHandleBuffer(ByProtocol, &PciIoProtocol,
						   NULL, &handle_count,
						   &handle_buffer);
	if (EFI_ERROR(status) || (handle_count == 0)) {
		ERROR(L"No PCI I/O Protocol handles found.\n");
		if (handle_buffer) {
			FreePool(handle_buffer);
		}
		return EFI_UNSUPPORTED;
	}

	EFI_PCI_IO_PROTOCOL *pci_io;
	UINT32 value;
	for (UINTN index = 0; index < handle_count; index++) {
		status = BS->OpenProtocol(handle_buffer[index], &PciIoProtocol,
					  (VOID **)&pci_io, this_image, NULL,
					  EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
		if (EFI_ERROR(status)) {
			ERROR(L"Cannot not open PciIoProtocol: %r\n", status);
			FreePool(handle_buffer);
			return status;
		}

		status = pci_io->Pci.Read(pci_io, EfiPciIoWidthUint32,
					  PCI_VENDOR_ID, 1, &value);
		if (EFI_ERROR(status)) {
			WARNING(
			    L"Cannot not read from PCI device, skipping: %r\n",
			    status);
			(VOID) BS->CloseProtocol(handle_buffer[index],
						 &PciIoProtocol, this_image,
						 NULL);
			continue;
		}

		for (const unsigned long *entry = init_array_start;
		     entry < init_array_end; entry++) {
			WATCHDOG_PROBE probe = (WATCHDOG_PROBE)
				(UINT8 * ) loaded_image->ImageBase + *entry;
			if ((status = probe(pci_io, PCI_GET_VENDOR_ID(value),
					    PCI_GET_PRODUCT_ID(value),
					    timeout)) == EFI_SUCCESS) {
				break;
			}
		}

		(VOID) BS->CloseProtocol(handle_buffer[index], &PciIoProtocol,
					 this_image, NULL);

		if (status == EFI_SUCCESS) {
			break;
		}
	}
	FreePool(handle_buffer);

	return status;
}

EFI_STATUS efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table)
{
	EFI_DEVICE_PATH *payload_dev_path;
	EFI_LOADED_IMAGE *loaded_image;
	EFI_HANDLE payload_handle;
	EFI_STATUS status;
	BG_STATUS bg_status;
	BG_LOADER_PARAMS bg_loader_params;
	CHAR16 *tmp;

	ZeroMem(&bg_loader_params, sizeof(bg_loader_params));

	this_image = image_handle;
	InitializeLib(this_image, system_table);

	(VOID) ST->ConOut->ClearScreen(ST->ConOut);
	PrintC(EFI_CYAN, L"\nEFI Boot Guard %s\n", L"" EFIBOOTGUARD_VERSION);

	status = BS->OpenProtocol(this_image, &LoadedImageProtocol,
				  (VOID **)&loaded_image, this_image, NULL,
				  EFI_OPEN_PROTOCOL_GET_PROTOCOL);
	if (EFI_ERROR(status)) {
		error_exit(L"Cannot open LoadedImageProtocol to get image information",
			   status);
	}

	tmp = DevicePathToStr(DevicePathFromHandle(loaded_image->DeviceHandle));
	boot_medium_path = GetBootMediumPath(tmp);
	FreePool(tmp);
	INFO(L"Boot medium: %s\n", boot_medium_path);

	status = get_volumes(&volumes, &volume_count);
	if (EFI_ERROR(status)) {
		error_exit(L"Cannot get volumes installed on system", status);
	}

	INFO(L"Loading configuration...\n");

	bg_status = load_config(&bg_loader_params);
	if (BG_ERROR(bg_status)) {
		switch (bg_status) {
		case BG_CONFIG_ERROR:
			error_exit(L"Environment not set, cannot load config",
				   EFI_ABORTED);
			break;
		case BG_CONFIG_PARTIALLY_CORRUPTED:
			WARNING(L"Config is partially corrupted. Please check.\n"
			        L"EFI Boot Guard will try to boot.\n");
			break;
		default:
			error_exit(L"Unknown error occured while loading config",
				   EFI_ABORTED);
		}
	}

	payload_dev_path = FileDevicePathFromConfig(
	    loaded_image->DeviceHandle, bg_loader_params.payload_path);
	if (!payload_dev_path) {
		error_exit(L"Cannot convert payload file path to device path",
			   EFI_OUT_OF_RESOURCES);
	}

	status = close_volumes(volumes, volume_count);
	if (EFI_ERROR(status)) {
		WARNING(L"Cannot close volumes.\n", status);
	}

	status = probe_watchdogs(loaded_image, bg_loader_params.timeout);
	if (EFI_ERROR(status)) {
		error_exit(L"Cannot probe watchdog", status);
	}

	/* Load and start image */
	status = BS->LoadImage(TRUE, this_image, payload_dev_path, NULL, 0,
			       &payload_handle);
	if (EFI_ERROR(status)) {
		error_exit(L"Cannot load specified kernel image", status);
	}

	FreePool(payload_dev_path);
	FreePool(boot_medium_path);

	status = BS->OpenProtocol(payload_handle, &LoadedImageProtocol,
				  (VOID **)&loaded_image, this_image,
				  NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
	if (EFI_ERROR(status)) {
		error_exit(L"Cannot open LoadedImageProtocol to set kernel load options",
			   status);
	}

	loaded_image->LoadOptions = bg_loader_params.payload_options;
	loaded_image->LoadOptionsSize =
	    (StrLen(bg_loader_params.payload_options) + 1) * sizeof(CHAR16);

	INFO(L"Starting %s with watchdog set to %d seconds ...\n",
	     bg_loader_params.payload_path, bg_loader_params.timeout);

	return BS->StartImage(payload_handle, NULL, NULL);
}
