/*
 * EFI Boot Guard, iTCO support (Version 3 and later)
 *
 * Copyright (c) Siemens AG, 2019
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *  Christian Storm <christian.storm@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <efi.h>
#include <efilib.h>
#include <pci/header.h>

#define PMBASE_ADDRMASK		0x0000ff80
#define PMCBASE_ADDRMASK	0xfffffe00

#define SMI_TCO_MASK		(1 << 13)

#define TCO_RLD_REG		0x00
#define TCO1_CNT_REG		0x08
#define TCO_TMR_HLT_MASK	(1 << 11)
#define TCO_TMR_REG		0x12

#define PMC_NO_REBOOT_MASK	(1 << 4)

enum iTCO_chipsets {
	ITCO_INTEL_APL = 0,
	ITCO_INTEL_BAYTRAIL,
	ITCO_INTEL_WPT_LP,
	ITCO_INTEL_ICH9,
};

typedef struct {
	CHAR16 name[16];
	UINT32 pci_id;
	UINT32 pmbase;
	UINT32 pmcbasereg;
	UINT32 pmcreg;
	UINT32 smireg;
} iTCO_info;

static iTCO_info iTCO_chipset_info[] = {
    [ITCO_INTEL_APL] =
	{
	    .name = L"Apollo Lake",
	    .pci_id = 0x5ae8,
	    .pmcbasereg = 0x10,
	    .pmcreg = 0x1008,
	    .smireg = 0x40,
	    .pmbase = 0x400,
	},
    [ITCO_INTEL_BAYTRAIL] =
	{
	    .name = L"Baytrail",
	    .pci_id = 0x0f1c,
	    .pmcbasereg = 0x44,
	    .pmcreg = 0x08,
	    .smireg = 0x30,
	},
    [ITCO_INTEL_WPT_LP] =
	{
	    .name = L"Wildcat",
	    .pci_id = 0x9cc3,
	    .pmcbasereg = 0x44,
	    .pmcreg = 0x08,
	    .smireg = 0x30,
	},
    [ITCO_INTEL_ICH9] =
	{
	    .name = L"ICH9", /* QEmu machine q35 */
	    .pci_id = 0x2918,
	    .pmcbasereg = 0x44,
	    .pmcreg = 0x08,
	    .smireg = 0x30,
	},
};

static BOOLEAN itco_supported(UINT16 pci_device_id, UINT8 *index)
{
	for (UINT8 i = 0;
	     i < (sizeof(iTCO_chipset_info) / sizeof(iTCO_chipset_info[0]));
	     i++) {
		if (pci_device_id == iTCO_chipset_info[i].pci_id) {
			*index = i;
			return TRUE;
		}
	}
	return FALSE;
}

static UINT32 get_pmbase(EFI_PCI_IO *pci_io, iTCO_info *itco)
{
	UINT32 pmbase;
	EFI_STATUS status;

	if (itco->pmbase) {
		return itco->pmbase & PMBASE_ADDRMASK;
	}

	status = uefi_call_wrapper(pci_io->Pci.Read, 5, pci_io,
				   EfiPciIoWidthUint32, 0x40, 1, &pmbase);
	if (EFI_ERROR(status)) {
		Print(L"Error reading PMBASE: %r\n", status);
		return 0;
	}
	return pmbase & PMBASE_ADDRMASK;
}

static EFI_STATUS update_no_reboot_flag(EFI_PCI_IO *pci_io, iTCO_info *itco)
{
	EFI_STATUS status;
	UINT32 pmcbase, value;

	status =
	    uefi_call_wrapper(pci_io->Pci.Read, 5, pci_io, EfiPciIoWidthUint32,
			      itco->pmcbasereg, 1, &pmcbase);
	if (EFI_ERROR(status)) {
		return status;
	}
	pmcbase &= PMCBASE_ADDRMASK;

	status = uefi_call_wrapper(pci_io->Mem.Read, 6, pci_io,
				   EfiPciIoWidthUint32,
				   EFI_PCI_IO_PASS_THROUGH_BAR,
				   pmcbase + itco->pmcreg, 1, &value);
	if (EFI_ERROR(status)) {
		return status;
	}
	value &= ~PMC_NO_REBOOT_MASK;
	status = uefi_call_wrapper(pci_io->Mem.Write, 6, pci_io,
				   EfiPciIoWidthUint32,
				   EFI_PCI_IO_PASS_THROUGH_BAR,
				   pmcbase + itco->pmcreg, 1, &value);
	if (EFI_ERROR(status)) {
		return status;
	}
	return status;
}

#define APL_MMCFG_BASE	0xE0000000

static UINTN apl_mmcfg_address(UINTN bus, UINTN device, UINTN function,
			       UINTN offset)
{
	return APL_MMCFG_BASE + (bus << 20) + (device << 15) +
		(function << 12) + offset;
}

static EFI_STATUS update_no_reboot_flag_apl(__attribute__((unused))
					    EFI_PCI_IO *pci_io,
					    iTCO_info *itco)
{
	/* Unhide the P2SB device if it's hidden. */
	BOOLEAN p2sbvisible =
	    *(volatile UINT16 *)apl_mmcfg_address(0, 13, 0, 0) != 0xFFFF;
	if (!p2sbvisible) {
		*(volatile UINT8 *)apl_mmcfg_address(0, 13, 0, 0xE1) = 0;
	}

	/* Get PMC_BASE from PMC Controller Register. */
	volatile UINT8 *reg =
	    (volatile UINT8 *)apl_mmcfg_address(0, 13, 1, (UINTN)itco->pmcreg);
	UINT8 value = *reg;
	value &= ~PMC_NO_REBOOT_MASK;
	*reg = value;

	if (p2sbvisible) {
		*(volatile UINT8 *)apl_mmcfg_address(0, 13, 0, 0xE1) = 1;
	}

	return EFI_SUCCESS;
}

static EFI_STATUS __attribute__((constructor))
init(EFI_PCI_IO *pci_io, UINT16 pci_vendor_id, UINT16 pci_device_id,
     UINTN timeout)
{
	UINT8 itco_chip;
	iTCO_info *itco;
	UINT32 pmbase, tcobase, value;
	EFI_STATUS status;

	if (!pci_io || pci_vendor_id != PCI_VENDOR_ID_INTEL ||
	    !itco_supported(pci_device_id, &itco_chip)) {
		return EFI_UNSUPPORTED;
	}
	itco = &iTCO_chipset_info[itco_chip];

	Print(L"Detected Intel TCO %s watchdog\n", itco->name);

	/* Get PMBASE and TCOBASE */
	if ((pmbase = get_pmbase(pci_io, itco)) == 0) {
		return EFI_UNSUPPORTED;
	}
	tcobase = pmbase + 0x60;

	/* Enable TCO SMIs */
	status = uefi_call_wrapper(pci_io->Io.Read, 6, pci_io,
				   EfiPciIoWidthUint32,
				   EFI_PCI_IO_PASS_THROUGH_BAR,
				   pmbase + itco->smireg, 1, &value);
	if (EFI_ERROR(status)) {
		return status;
	}
	value |= SMI_TCO_MASK;
	status = uefi_call_wrapper(pci_io->Io.Write, 6, pci_io,
				   EfiPciIoWidthUint32,
				   EFI_PCI_IO_PASS_THROUGH_BAR,
				   pmbase + itco->smireg, 1, &value);
	if (EFI_ERROR(status)) {
		return status;
	}

	/* Set timer value */
	status = uefi_call_wrapper(pci_io->Io.Read, 6, pci_io,
				   EfiPciIoWidthUint16,
				   EFI_PCI_IO_PASS_THROUGH_BAR,
				   tcobase + TCO_TMR_REG, 1, &value);
	if (EFI_ERROR(status)) {
		return status;
	}
	value &= 0xfc00;
	value |= timeout & 0x3ff;
	status = uefi_call_wrapper(pci_io->Io.Write, 6, pci_io,
				   EfiPciIoWidthUint16,
				   EFI_PCI_IO_PASS_THROUGH_BAR,
				   tcobase + TCO_TMR_REG, 1, &value);
	if (EFI_ERROR(status)) {
		return status;
	}

	/* Force reloading of timer value */
	value = 1;
	status = uefi_call_wrapper(pci_io->Io.Write, 6, pci_io,
				   EfiPciIoWidthUint16,
				   EFI_PCI_IO_PASS_THROUGH_BAR,
				   tcobase + TCO_RLD_REG, 1, &value);
	if (EFI_ERROR(status)) {
		return status;
	}

	/* Clear NO_REBOOT flag */
	switch (itco_chip) {
	case ITCO_INTEL_APL:
		status = update_no_reboot_flag_apl(pci_io, itco);
		break;
	case ITCO_INTEL_BAYTRAIL:
	case ITCO_INTEL_WPT_LP:
		status = update_no_reboot_flag(pci_io, itco);
		break;
	}
	if (EFI_ERROR(status)) {
		return status;
	}

	/* Clear HLT flag to start timer */
	status = uefi_call_wrapper(pci_io->Io.Read, 6, pci_io,
				   EfiPciIoWidthUint16,
				   EFI_PCI_IO_PASS_THROUGH_BAR,
				   tcobase + TCO1_CNT_REG, 1, &value);
	if (EFI_ERROR(status)) {
		return status;
	}
	value &= ~TCO_TMR_HLT_MASK;
	status = uefi_call_wrapper(pci_io->Io.Write, 6, pci_io,
				   EfiPciIoWidthUint16,
				   EFI_PCI_IO_PASS_THROUGH_BAR,
				   tcobase + TCO1_CNT_REG, 1, &value);

	return status;
}
