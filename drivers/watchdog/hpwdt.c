/*
 * EFI Boot Guard
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
#include <pci/header.h>
#include "utils.h"

#define PCI_VENDOR_ID_HP		0x103c
#define PCI_VENDOR_ID_HP_3PAR		0x1590
#define PCI_DEVICE_ID_ILO3		0x3306
#define PCI_DEVICE_ID_AUX_ILO		0x1979
#define PCI_DEVICE_ID_CL		0x0289
#define PCI_DEVICE_ID_PCTRL		0x0389

#define HPWDT_TIMER_REG			0x70
#define HPWDT_TIMER_CON			0x72

#define SECS_TO_TICKS(secs)		((secs) * 1000 / 128)

#define PCI_GET_SUBSYS_VENDOR_ID(id)	(UINT16)(id)
#define PCI_GET_SUBSYS_PRODUCT_ID(id)	(UINT16)((id) >> 16)

static EFI_STATUS init(EFI_PCI_IO *pci_io, UINT16 pci_vendor_id,
		       UINT16 pci_device_id, UINTN timeout)
{
	EFI_STATUS status;
	UINT16 reload;
	UINT8 control;

	if (!pci_io || 
	    !((pci_vendor_id == PCI_VENDOR_ID_HP &&
	       pci_device_id == PCI_DEVICE_ID_ILO3) ||
	      (pci_vendor_id == PCI_VENDOR_ID_HP_3PAR &&
	       pci_device_id == PCI_DEVICE_ID_PCTRL))) {
		return EFI_UNSUPPORTED;
	}

	if (pci_device_id == PCI_DEVICE_ID_ILO3) {
		UINT16 vendor, product;
		UINT32 value;

		status = pci_io->Pci.Read(
		    pci_io, EfiPciIoWidthUint32, PCI_SUBSYSTEM_VENDOR_ID, 1,
		    &value);
		if (EFI_ERROR(status)) {
			return status;
		}

		vendor = PCI_GET_SUBSYS_VENDOR_ID(value);
		product = PCI_GET_SUBSYS_PRODUCT_ID(value);
		if ((vendor == PCI_VENDOR_ID_HP &&
		     product == PCI_DEVICE_ID_AUX_ILO) ||
		    (vendor == PCI_VENDOR_ID_HP_3PAR &&
		     product == PCI_DEVICE_ID_CL)) {
			return EFI_UNSUPPORTED;
		}
	}

	INFO(L"Detected HPE ProLiant watchdog\n");

	reload = SECS_TO_TICKS(timeout);
	status = pci_io->Mem.Write(
	    pci_io, EfiPciIoWidthUint16, 1, HPWDT_TIMER_REG, 1, &reload);
	if (EFI_ERROR(status)) {
		return status;
	}

	control = 0x81;
	status = pci_io->Mem.Write(
	    pci_io, EfiPciIoWidthUint8, 1, HPWDT_TIMER_CON, 1, &control);
	if (EFI_ERROR(status)) {
		return status;
	}

	return EFI_SUCCESS;
}

WATCHDOG_REGISTER(init);
