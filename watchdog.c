/*
 * EFI Boot Guard
 *
 * Copyright (c) Siemens AG, 2017-2025
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
#include <pci/header.h>

#include "bootguard.h"
#include "print.h"
#include "utils.h"
#include "watchdog.h"

extern const unsigned long wdfuncs_start[];
extern const unsigned long wdfuncs_end[];

#define PCI_GET_VENDOR_ID(id)	(UINT16)(id)
#define PCI_GET_PRODUCT_ID(id)	(UINT16)((id) >> 16)

static WATCHDOG_DRIVER *watchdog_drivers;
static WATCHDOG_DRIVER *last_watchdog_driver;

VOID register_watchdog(WATCHDOG_DRIVER *driver)
{
	if (last_watchdog_driver != NULL)
		last_watchdog_driver->next = driver;
	else
		watchdog_drivers = driver;
	last_watchdog_driver = driver;
}

EFI_STATUS probe_watchdogs(UINTN timeout)
{
#if GNU_EFI_VERSION < 3000016
	const unsigned long *entry = wdfuncs_start;
	for (entry++; entry < wdfuncs_end; entry++) {
		((void (*)(void))*entry)();
	}
#endif
	if (watchdog_drivers == NULL) {
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

		WATCHDOG_DRIVER *driver = watchdog_drivers;
		while (driver) {
			status = driver->probe(pci_io,
					       PCI_GET_VENDOR_ID(value),
					       PCI_GET_PRODUCT_ID(value),
						timeout);
			if (status == EFI_SUCCESS) {
				break;
			}
			driver = driver->next;
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
