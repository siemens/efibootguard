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

static EFI_STATUS probe_watchdog(EFI_LOADED_IMAGE *loaded_image,
				 EFI_PCI_IO *pci_io, UINT16 pci_vendor_id,
				 UINT16 pci_device_id, UINTN timeout)
{
	const unsigned long *entry;

	for (entry = init_array_start; entry < init_array_end; entry++) {
		EFI_STATUS (*probe)(EFI_PCI_IO *, UINT16, UINT16, UINTN);

		probe = loaded_image->ImageBase + *entry;
		if (probe(pci_io, pci_vendor_id, pci_device_id, timeout) ==
		    EFI_SUCCESS) {
			return EFI_SUCCESS;
		}
	}

	return EFI_UNSUPPORTED;
}

static EFI_STATUS scan_devices(EFI_LOADED_IMAGE *loaded_image, UINTN timeout)
{
	EFI_HANDLE devices[1000];
	UINTN count, size = sizeof(devices);
	EFI_PCI_IO *pci_io;
	EFI_STATUS status;
	UINT32 value;

	status = uefi_call_wrapper(BS->LocateHandle, 5, ByProtocol,
				   &PciIoProtocol, NULL, &size, devices);
	if (EFI_ERROR(status)) {
		return status;
	}

	count = size / sizeof(EFI_HANDLE);
	if (count == 0) {
		return probe_watchdog(loaded_image, NULL, 0, 0, timeout);
	}

	do {
		EFI_HANDLE device = devices[count - 1];

		count--;

		status = uefi_call_wrapper(BS->OpenProtocol, 6, device,
					   &PciIoProtocol, (VOID **)&pci_io,
					   this_image, NULL,
					   EFI_OPEN_PROTOCOL_GET_PROTOCOL);
		if (EFI_ERROR(status)) {
			error_exit(L"Could not open PciIoProtocol while "
				   L"probing watchdogs.",
				   status);
		}

		status = uefi_call_wrapper(pci_io->Pci.Read, 5, pci_io,
					   EfiPciIoWidthUint32, PCI_VENDOR_ID,
					   1, &value);
		if (EFI_ERROR(status)) {
			error_exit(L"Could not read from PCI device while "
				   L"probing watchdogs.",
				   status);
		}

		status = probe_watchdog(loaded_image, pci_io, (UINT16)value,
					value >> 16, timeout);

		uefi_call_wrapper(BS->CloseProtocol, 4, device, &PciIoProtocol,
				  this_image, NULL);
	} while (status != EFI_SUCCESS && count > 0);

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

	Color(system_table, 3, 0);
	Print(L"\nEFI Boot Guard %s\n", L""EFIBOOTGUARD_VERSION);
	Color(system_table, 7, 0);

	status =
	    uefi_call_wrapper(BS->OpenProtocol, 6, this_image,
			      &LoadedImageProtocol, (VOID **)&loaded_image,
			      this_image, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
	if (EFI_ERROR(status)) {
		error_exit(L"Could not open LoadedImageProtocol to get image "
			   L"information.",
			   status);
	}

	tmp = DevicePathToStr(DevicePathFromHandle(loaded_image->DeviceHandle));
	boot_medium_path = GetBootMediumPath(tmp);
	mfree(tmp);
	Print(L"Boot medium: %s\n", boot_medium_path);

	status = get_volumes(&volumes, &volume_count);
	if (EFI_ERROR(status)) {
		error_exit(L"Could not get volumes installed on system.\n",
			   status);
	}

	Print(L"Loading configuration...\n");

	bg_status = load_config(&bg_loader_params);
	if (BG_ERROR(bg_status)) {
		switch (bg_status) {
		case BG_CONFIG_ERROR:
			error_exit(
			    L"Fatal error: Environment not set, could not "
			    L"load config.",
			    EFI_ABORTED);
			break;
		case BG_CONFIG_PARTIALLY_CORRUPTED:
			Print(L"Config is partially corrupted. Please check.\n"
			      L"efibootguard will try to boot.\n");
			break;
		default:
			error_exit(L"Fatal error: Unknown error occured while "
				   L"loading config.",
				   EFI_ABORTED);
		}
	}

	payload_dev_path = FileDevicePathFromConfig(
	    loaded_image->DeviceHandle, bg_loader_params.payload_path);
	if (!payload_dev_path) {
		error_exit(
		    L"Could not convert payload file path to device path.",
		    EFI_OUT_OF_RESOURCES);
	}

	if (bg_loader_params.timeout == 0) {
		Print(L"Watchdog is disabled.\n");
	} else {
		status = scan_devices(loaded_image, bg_loader_params.timeout);
		if (EFI_ERROR(status)) {
			error_exit(L"Could not probe watchdog.", status);
		}
	}

	/* Load and start image */
	status = uefi_call_wrapper(BS->LoadImage, 6, TRUE, this_image,
				   payload_dev_path, NULL, 0, &payload_handle);
	if (EFI_ERROR(status)) {
		error_exit(L"Could not load specified kernel image.", status);
	}

	mfree(payload_dev_path);
	mfree(boot_medium_path);

	status =
	    uefi_call_wrapper(BS->OpenProtocol, 6, payload_handle,
			      &LoadedImageProtocol, (VOID **)&loaded_image,
			      this_image, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
	if (EFI_ERROR(status)) {
		error_exit(L"Could not open LoadedImageProtocol to set kernel "
			   L"load options.",
			   status);
	}

	loaded_image->LoadOptions = bg_loader_params.payload_options;
	loaded_image->LoadOptionsSize =
	    (StrLen(bg_loader_params.payload_options) + 1) * sizeof(CHAR16);

	Print(L"Starting %s with watchdog set to %d seconds\n",
	      bg_loader_params.payload_path, bg_loader_params.timeout);

	return uefi_call_wrapper(BS->StartImage, 3, payload_handle, 0, 0);
}
