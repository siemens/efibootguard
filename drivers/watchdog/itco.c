/*
 * EFI Boot Guard, iTCO support (Version 3 and later)
 *
 * Copyright (c) Siemens AG, 2017
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <efi.h>
#include <efilib.h>
#include <pci/header.h>

#define PCI_DEVICE_ID_INTEL_BAYTRAIL	0x0f1c

#define PMBASE_REG			0x40
# define PMBASE_ADDRMASK		0xff00
#define PMCBASE_REG			0x44
# define PMCBASE_ADDRMASK		0xfffffe00

#define SMI_EN_REG			0x30
# define TCO_EN				(1 << 13)

#define TCO_RLD_REG			0x00
#define TCO1_CNT_REG			0x08
# define TCO_TMR_HLT			(1 << 11)
#define TCO_TMR_REG			0x12

#define PMC_REG				0x08
# define PMC_NO_REBOOT			(1 << 4)

static EFI_STATUS __attribute__((constructor))
init(EFI_PCI_IO *pci_io, UINT16 pci_vendor_id, UINT16 pci_device_id,
     UINTN timeout)
{
	UINT32 pmbase, tcobase, pmcbase, value;
	EFI_STATUS status;

	if (!pci_io || pci_vendor_id != PCI_VENDOR_ID_INTEL ||
	    (pci_device_id != PCI_DEVICE_ID_INTEL_BAYTRAIL))
		return EFI_UNSUPPORTED;

	Print(L"Detected Intel TCO watchdog\n");

	/* Get PMBASE and TCOBASE */
	status = uefi_call_wrapper(pci_io->Pci.Read, 5, pci_io,
				   EfiPciIoWidthUint32, PMBASE_REG,
				   1, &pmbase);
	if (EFI_ERROR(status))
		return status;
	pmbase &= PMBASE_ADDRMASK;
	tcobase = (pmbase & PMBASE_ADDRMASK) + 0x60;

	/* Get PMCBASE address */
	status = uefi_call_wrapper(pci_io->Pci.Read, 5, pci_io,
				   EfiPciIoWidthUint32, PMCBASE_REG,
				   1, &pmcbase);
	if (EFI_ERROR(status))
		return status;
	pmcbase &= PMCBASE_ADDRMASK;

	/* Enable TCO SMIs */
	status = uefi_call_wrapper(pci_io->Io.Read, 6, pci_io,
				   EfiPciIoWidthUint32,
				   EFI_PCI_IO_PASS_THROUGH_BAR,
				   pmbase + SMI_EN_REG, 1, &value);
	if (EFI_ERROR(status))
		return status;
	value |= TCO_EN;
	status = uefi_call_wrapper(pci_io->Io.Write, 6, pci_io,
				   EfiPciIoWidthUint32,
				   EFI_PCI_IO_PASS_THROUGH_BAR,
				   pmbase + SMI_EN_REG, 1, &value);
	if (EFI_ERROR(status))
		return status;

	/* Set timer value */
	status = uefi_call_wrapper(pci_io->Io.Read, 6, pci_io,
				   EfiPciIoWidthUint16,
				   EFI_PCI_IO_PASS_THROUGH_BAR,
				   tcobase + TCO_TMR_REG, 1, &value);
	if (EFI_ERROR(status))
		return status;
	value &= 0xfc00;
	value |= ((timeout * 10) / 6) & 0x3ff;
	status = uefi_call_wrapper(pci_io->Io.Write, 6, pci_io,
				   EfiPciIoWidthUint16,
				   EFI_PCI_IO_PASS_THROUGH_BAR,
				   tcobase + TCO_TMR_REG, 1, &value);
	if (EFI_ERROR(status))
		return status;

	/* Force reloading of timer value */
	value = 1;
	status = uefi_call_wrapper(pci_io->Io.Write, 6, pci_io,
				   EfiPciIoWidthUint16,
				   EFI_PCI_IO_PASS_THROUGH_BAR,
				   tcobase + TCO_RLD_REG, 1, &value);
	if (EFI_ERROR(status))
		return status;

	/* Clear NO_REBOOT flag */
	status = uefi_call_wrapper(pci_io->Mem.Read, 6, pci_io,
				   EfiPciIoWidthUint32,
				   EFI_PCI_IO_PASS_THROUGH_BAR,
				   pmcbase + PMC_REG, 1, &value);
	if (EFI_ERROR(status))
		return status;
	value &= ~PMC_NO_REBOOT;
	status = uefi_call_wrapper(pci_io->Mem.Write, 6, pci_io,
				   EfiPciIoWidthUint32,
				   EFI_PCI_IO_PASS_THROUGH_BAR,
				   pmcbase + PMC_REG, 1, &value);
	if (EFI_ERROR(status))
		return status;

	/* Clear HLT flag to start timer */
	status = uefi_call_wrapper(pci_io->Io.Read, 6, pci_io,
				   EfiPciIoWidthUint16,
				   EFI_PCI_IO_PASS_THROUGH_BAR,
				   tcobase + TCO1_CNT_REG, 1, &value);
	if (EFI_ERROR(status))
		return status;
	value &= ~TCO_TMR_HLT;
	status = uefi_call_wrapper(pci_io->Io.Write, 6, pci_io,
				   EfiPciIoWidthUint16,
				   EFI_PCI_IO_PASS_THROUGH_BAR,
				   tcobase + TCO1_CNT_REG, 1, &value);

	return status;
}
