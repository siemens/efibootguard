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

#define TCO_RLD_REG		0x00
#define TCO1_CNT_NO_REBOOT	(1 << 0)
#define TCO1_CNT_REG		0x08
#define TCO_TMR_HLT_MASK	(1 << 11)
#define TCO_TMR_REG		0x12

enum iTCO_chipsets {
	ITCO_INTEL_APL = 0,
	ITCO_INTEL_BAYTRAIL,
	ITCO_INTEL_WPT_LP,
	ITCO_INTEL_ICH9,
	ITCO_INTEL_LPC_LP,
	ITCO_INTEL_WBG,
	ITCO_INTEL_EHL,
};

enum iTCO_versions {
	ITCO_V1 = 1,
	ITCO_V2,
	ITCO_V3,
	ITCO_V4,
	ITCO_V5,
	ITCO_V6,
};

typedef struct {
	UINT32 tco_base;
	UINT32 pm_base_addr_mask;
	UINT32 pmc_base_reg;
	UINT32 pmc_reg;
	UINT32 pmc_no_reboot_mask;
	UINT32 pmc_base_addr_mask;
} iTCO_regs;

typedef struct {
	CHAR16 name[16];
	UINT32 pci_id;
	iTCO_regs* regs;
	UINT32 itco_version;
} iTCO_info;

static iTCO_regs iTCO_version_regs[] = {
    [ITCO_V1] =
	{
	    /* Not implemented yet */
	},
    [ITCO_V2] =
	{
	    .pmc_base_reg = 0xf0,		/* RCBABASE_REG */
	    .pmc_reg = 0x3410,			/* GCS_REG */
	    .pmc_no_reboot_mask = (1 << 5),	/* GCS_NO_REBOOT */
	    .pmc_base_addr_mask = 0xffffc000,	/* RCBABASE_ADDRMASK */
	    .pm_base_addr_mask = 0x0000ff80,
	},
    [ITCO_V3] =
	{
	    .pmc_base_reg = 0x44,
	    .pmc_reg = 0x08,
	    .pmc_no_reboot_mask = (1 << 4),
	    .pmc_base_addr_mask = 0xfffffe00,
	    .pm_base_addr_mask = 0x0000ff80,
	},
    [ITCO_V4] =
	{
	    /* Not implemented yet */
	},
    [ITCO_V5] =
	{
	    .pmc_base_reg = 0x10,
	    .pmc_reg = 0x1008,
	    .tco_base = 0x460,
	    .pmc_no_reboot_mask = (1 << 4),
	    .pmc_base_addr_mask = 0xfffffe00,
	},
    [ITCO_V6] =
	{
	    .tco_base = 0x400,
	},
};

static iTCO_info iTCO_chipset_info[] = {
    [ITCO_INTEL_APL] =
	{
	    .name = L"Apollo Lake",
	    .pci_id = 0x5ae8,
	    .regs = &iTCO_version_regs[ITCO_V5],
	    .itco_version = ITCO_V5,
	},
    [ITCO_INTEL_BAYTRAIL] =
	{
	    .name = L"Baytrail",
	    .pci_id = 0x0f1c,
	    .regs = &iTCO_version_regs[ITCO_V3],
	    .itco_version = ITCO_V3,
	},
    [ITCO_INTEL_WPT_LP] =
	{
	    .name = L"Wildcat",
	    .pci_id = 0x9cc3,
	    .regs = &iTCO_version_regs[ITCO_V3],
	    .itco_version = ITCO_V3,
	},
    [ITCO_INTEL_ICH9] =
	{
	    .name = L"ICH9", /* QEmu machine q35 */
	    .pci_id = 0x2918,
	    .regs = &iTCO_version_regs[ITCO_V3],
	    .itco_version = ITCO_V3,
	},
    [ITCO_INTEL_LPC_LP] =
	{
	    .name = L"Lynx Point",
	    .pci_id = 0x8c4e,
	    .regs = &iTCO_version_regs[ITCO_V2],
	    .itco_version = ITCO_V2,
	},
    [ITCO_INTEL_WBG] =
	{
	    .name = L"Wellsburg",
	    .pci_id = 0x8d44,
	    .regs = &iTCO_version_regs[ITCO_V2],
	    .itco_version = ITCO_V2,
	},
    [ITCO_INTEL_EHL] =
	{
	    .name = L"Elkhart Lake",
	    .pci_id = 0x4b23,
	    .regs = &iTCO_version_regs[ITCO_V6],
	    .itco_version = ITCO_V6,
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

static UINTN get_timeout_value(UINT32 iTCO_version, UINTN seconds){
	return iTCO_version == ITCO_V3 ? seconds : ((seconds * 10 ) / 6);
}

static UINT32 get_tco_base(EFI_PCI_IO *pci_io, iTCO_info *itco)
{
	UINT32 pm_base;
	EFI_STATUS status;

	if (itco->regs->tco_base) {
		return itco->regs->tco_base;
	}

	status = uefi_call_wrapper(pci_io->Pci.Read, 5, pci_io,
				   EfiPciIoWidthUint32, 0x40, 1, &pm_base);
	if (EFI_ERROR(status)) {
		Print(L"Error reading PM_BASE: %r\n", status);
		return 0;
	}
	return (pm_base & itco->regs->pm_base_addr_mask) + 0x60;
}

static EFI_STATUS update_no_reboot_flag_cnt(EFI_PCI_IO *pci_io,
					    UINT32 tco_base)
{
	EFI_STATUS status;
	UINT32 value;

	status = uefi_call_wrapper(pci_io->Io.Read, 6, pci_io,
				   EfiPciIoWidthUint16,
				   EFI_PCI_IO_PASS_THROUGH_BAR,
				   tco_base + TCO1_CNT_REG, 1, &value);
	if (EFI_ERROR(status)) {
		return status;
	}
	value &= ~TCO1_CNT_NO_REBOOT;
	status = uefi_call_wrapper(pci_io->Io.Write, 6, pci_io,
				   EfiPciIoWidthUint16,
				   EFI_PCI_IO_PASS_THROUGH_BAR,
				   tco_base + TCO1_CNT_REG, 1, &value);
	if (EFI_ERROR(status)) {
		return status;
	}
	return status;
}

static EFI_STATUS update_no_reboot_flag_mem(EFI_PCI_IO *pci_io,
					    iTCO_info *itco)
{
	EFI_STATUS status;
	UINT32 pmc_base, value;

	status =
	    uefi_call_wrapper(pci_io->Pci.Read, 5, pci_io, EfiPciIoWidthUint32,
			      itco->regs->pmc_base_reg, 1, &pmc_base);
	if (EFI_ERROR(status)) {
		return status;
	}
	pmc_base &= itco->regs->pmc_base_addr_mask;

	status = uefi_call_wrapper(pci_io->Mem.Read, 6, pci_io,
				   EfiPciIoWidthUint32,
				   EFI_PCI_IO_PASS_THROUGH_BAR,
				   pmc_base + itco->regs->pmc_reg, 1, &value);
	if (EFI_ERROR(status)) {
		return status;
	}
	value &= ~itco->regs->pmc_no_reboot_mask;
	status = uefi_call_wrapper(pci_io->Mem.Write, 6, pci_io,
				   EfiPciIoWidthUint32,
				   EFI_PCI_IO_PASS_THROUGH_BAR,
				   pmc_base + itco->regs->pmc_reg, 1, &value);
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
	BOOLEAN p2sb_hidden =
	    *(volatile UINT16 *)apl_mmcfg_address(0, 13, 0, 0) == 0xffff;
	if (p2sb_hidden) {
		*(volatile UINT8 *)apl_mmcfg_address(0, 13, 0, 0xE1) = 0;
	}

	/* Get PMC_BASE from PMC Controller Register. */
	volatile UINT8 *reg =
	    (volatile UINT8 *)apl_mmcfg_address(0, 13, 1, (UINTN)itco->regs->pmc_reg);
	UINT8 value = *reg;
	value &= ~itco->regs->pmc_no_reboot_mask;
	*reg = value;

	if (p2sb_hidden) {
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
	UINT32 tco_base, value;
	EFI_STATUS status;

	if (!pci_io || pci_vendor_id != PCI_VENDOR_ID_INTEL ||
	    !itco_supported(pci_device_id, &itco_chip)) {
		return EFI_UNSUPPORTED;
	}
	itco = &iTCO_chipset_info[itco_chip];

	Print(L"Detected Intel TCO %s watchdog\n", itco->name);

	/* Get TCOBASE */
	tco_base = get_tco_base(pci_io, itco);
	if (!tco_base) {
		return EFI_UNSUPPORTED;
	}

	/* Set timer value */
	status = uefi_call_wrapper(pci_io->Io.Read, 6, pci_io,
				   EfiPciIoWidthUint16,
				   EFI_PCI_IO_PASS_THROUGH_BAR,
				   tco_base + TCO_TMR_REG, 1, &value);
	if (EFI_ERROR(status)) {
		return status;
	}
	value &= 0xfc00;
	value |= get_timeout_value(itco->itco_version, timeout) & 0x3ff;
	status = uefi_call_wrapper(pci_io->Io.Write, 6, pci_io,
				   EfiPciIoWidthUint16,
				   EFI_PCI_IO_PASS_THROUGH_BAR,
				   tco_base + TCO_TMR_REG, 1, &value);
	if (EFI_ERROR(status)) {
		return status;
	}

	/* Force reloading of timer value */
	value = 1;
	status = uefi_call_wrapper(pci_io->Io.Write, 6, pci_io,
				   EfiPciIoWidthUint16,
				   EFI_PCI_IO_PASS_THROUGH_BAR,
				   tco_base + TCO_RLD_REG, 1, &value);
	if (EFI_ERROR(status)) {
		return status;
	}

	/* Clear NO_REBOOT flag */
	switch (itco->itco_version) {
	case ITCO_V6:
		status = update_no_reboot_flag_cnt(pci_io, tco_base);
		break;
	case ITCO_V5:
		status = update_no_reboot_flag_apl(pci_io, itco);
		break;
	case ITCO_V3:
	case ITCO_V2:
		status = update_no_reboot_flag_mem(pci_io, itco);
		break;
	}
	if (EFI_ERROR(status)) {
		return status;
	}

	/* Clear HLT flag to start timer */
	status = uefi_call_wrapper(pci_io->Io.Read, 6, pci_io,
				   EfiPciIoWidthUint16,
				   EFI_PCI_IO_PASS_THROUGH_BAR,
				   tco_base + TCO1_CNT_REG, 1, &value);
	if (EFI_ERROR(status)) {
		return status;
	}
	value &= ~TCO_TMR_HLT_MASK;
	status = uefi_call_wrapper(pci_io->Io.Write, 6, pci_io,
				   EfiPciIoWidthUint16,
				   EFI_PCI_IO_PASS_THROUGH_BAR,
				   tco_base + TCO1_CNT_REG, 1, &value);

	return status;
}
