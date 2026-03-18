/*
 * EFI Boot Guard
 *
 * Copyright (c) Siemens AG, 2017-2026
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *  Andreas Reichel <andreas.reichel.ext@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * SPDX-License-Identifier:	GPL-2.0-only
 */

#include <efi.h>
#include <efilib.h>
#include <efiprot.h>
#include <efipciio.h>
#include <pci/header.h>

#include "bootguard.h"
#include "configuration.h"
#include "loader_interface.h"
#include "print.h"
#include "utils.h"
#include "version.h"
#include "watchdog.h"

EFI_STATUS efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table)
{
	EFI_DEVICE_PATH *payload_dev_path;
	EFI_LOADED_IMAGE *loaded_image;
	EFI_HANDLE payload_handle;
	EFI_STATUS status;
	BG_STATUS bg_status;
	BG_LOADER_PARAMS bg_loader_params;
	BG_INTERFACE_PARAMS bg_interface_params;
	CHAR16 *tmp;

	ZeroMem(&bg_loader_params, sizeof(bg_loader_params));

	this_image = image_handle;
	InitializeLib(this_image, system_table);

#if !defined(SILENT_BOOT)
	(VOID) ST->ConOut->ClearScreen(ST->ConOut);
	PrintC(EFI_CYAN, L"EFI Boot Guard %s\n", L"" EFIBOOTGUARD_VERSION);
#endif

	status = BS->OpenProtocol(this_image, &LoadedImageProtocol,
				  (VOID **)&loaded_image, this_image, NULL,
				  EFI_OPEN_PROTOCOL_GET_PROTOCOL);
	if (EFI_ERROR(status)) {
		error_exit(L"Cannot open LoadedImageProtocol to get image information",
			   status);
	}

	tmp = DevicePathToStr(DevicePathFromHandle(loaded_image->DeviceHandle));
	boot_medium_path = GetMediumPath(tmp);
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

	status = probe_watchdogs(bg_loader_params.timeout);
	if (EFI_ERROR(status)) {
		error_exit(L"Cannot probe watchdog", status);
	}

	/* Load and start image */
	status = BS->LoadImage(FALSE, this_image, payload_dev_path, NULL, 0,
			       &payload_handle);
	if (EFI_ERROR(status)) {
		if (bg_loader_params.ustate == USTATE_TESTING) {
			/*
			 * `ustate` was just switched from `1` (INSTALLED)
			 * to `2` (TESTING), but the kernel image to be
			 * tested is not present. Reboot to trigger a
			 * fallback into the original boot path.
			 */
			ERROR(L"Failed to load kernel image %s (%r).\n",
			      bg_loader_params.payload_path, status);
			ERROR(L"Triggering Rollback as ustate==2 (TESTING).\n");
			(VOID) BS->Stall(3 * 1000 * 1000);
			ST->RuntimeServices->ResetSystem(EfiResetCold,
							 EFI_SUCCESS, 0, NULL);
		}
		error_exit(L"Cannot load specified kernel image", status);
	}

	UINT16 *boot_medium_uuidstr =
		disk_get_part_uuid(loaded_image->DeviceHandle);
	if (!boot_medium_uuidstr) {
		WARNING(L"Cannot get boot partition UUID\n");
	} else {
		bg_interface_params.loader_device_part_uuid = boot_medium_uuidstr;
		status = set_bg_interface_vars(&bg_interface_params);
		if (EFI_ERROR(status)) {
			WARNING(L"Cannot set bootloader interface variables (%r)\n",
				status);
		}
		INFO(L"LoaderDevicePartUUID=%s\n", boot_medium_uuidstr);
		FreePool(boot_medium_uuidstr);
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

	BS->Stall(1000 * 1000 * ENV_BOOT_DELAY);

	return BS->StartImage(payload_handle, NULL, NULL);
}
