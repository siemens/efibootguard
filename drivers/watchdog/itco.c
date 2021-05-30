/*
 * EFI Boot Guard, iTCO support (Version 2 and later)
 *
 * Copyright (c) Siemens AG, 2019-2021
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
#include <sys/io.h>

#define SMI_EN_REG		0x30
#define TCO_EN			(1 << 13)
#define GBL_SMI_EN		(1 << 0)

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
	ITCO_INTEL_LPC_NM10,
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
	UINT32 pm_base_reg;
	UINT32 pm_base_addr_mask;
	UINT32 pmc_base_reg;
	UINT32 pmc_reg;
	UINT32 pmc_no_reboot_mask;
	UINT32 pmc_base_addr_mask;
} iTCO_regs;

typedef struct {
	CHAR16 name[16];
	UINT32 pci_id;
	UINT32 itco_version;
} iTCO_info;

static const iTCO_regs iTCO_version_regs[] = {
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
	    .pm_base_reg = 0x40,
	    .pm_base_addr_mask = 0x0000ff80,
	},
    [ITCO_V3] =
	{
	    .pmc_base_reg = 0x44,
	    .pmc_reg = 0x08,
	    .pmc_no_reboot_mask = (1 << 4),
	    .pmc_base_addr_mask = 0xfffffe00,
	    .pm_base_reg = 0x40,
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

static const iTCO_info iTCO_chipset_info[] = {
    [ITCO_INTEL_APL] =
	{
	    .name = L"Apollo Lake SoC",
	    .pci_id = 0x5ae8,
	    .itco_version = ITCO_V5,
	},
    [ITCO_INTEL_BAYTRAIL] =
	{
	    .name = L"Bay Trail SoC",
	    .pci_id = 0x0f1c,
	    .itco_version = ITCO_V3,
	},
    [ITCO_INTEL_WPT_LP] =
	{
	    .name = L"Wildcat Point_LP",
	    .pci_id = 0x9cc3,
	    .itco_version = ITCO_V3,
	},
    [ITCO_INTEL_ICH9] =
	{
	    .name = L"ICH9", /* QEmu machine q35 */
	    .pci_id = 0x2918,
	    .itco_version = ITCO_V3,
	},
    [ITCO_INTEL_LPC_NM10] =
	{
	    .name = L"NM10",
	    .pci_id = 0x27bc,
	    .itco_version = ITCO_V2,
	},
    [ITCO_INTEL_LPC_LP] =
	{
	    .name = L"Lynx Point",
	    .pci_id = 0x8c4e,
	    .itco_version = ITCO_V2,
	},
    [ITCO_INTEL_WBG] =
	{
	    .name = L"Wellsburg",
	    .pci_id = 0x8d44,
	    .itco_version = ITCO_V2,
	},
    [ITCO_INTEL_EHL] =
	{
	    .name = L"Elkhart Lake",
	    .pci_id = 0x4b23,
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

static UINTN get_timeout_value(UINT32 iTCO_version, UINTN seconds)
{
	return iTCO_version == ITCO_V3 ? seconds : ((seconds * 10 ) / 6);
}

static UINT32 get_pm_base(EFI_PCI_IO *pci_io, const iTCO_info *itco)
{
	const iTCO_regs* regs = &iTCO_version_regs[itco->itco_version];
	EFI_STATUS status;
	UINT32 pm_base;

	if (!regs->pm_base_reg) {
		return 0;
	}

	status = uefi_call_wrapper(pci_io->Pci.Read, 5, pci_io,
				   EfiPciIoWidthUint32,
				   regs->pm_base_reg, 1, &pm_base);
	if (EFI_ERROR(status)) {
		Print(L"Error reading PM_BASE: %r\n", status);
		return 0;
	}
	return pm_base & regs->pm_base_addr_mask;
}

static UINT32 get_tco_base(UINT32 pm_base, const iTCO_info *itco)
{
	const iTCO_regs* regs = &iTCO_version_regs[itco->itco_version];

	if (regs->tco_base) {
		return regs->tco_base;
	} else if (pm_base) {
		return pm_base + 0x60;
	} else {
		return 0;
	}
}

static void update_no_reboot_flag_cnt(UINT32 tco_base)
{
	UINT32 value;

	value = inw(tco_base + TCO1_CNT_REG);
	value &= ~TCO1_CNT_NO_REBOOT;
	outw(value, tco_base + TCO1_CNT_REG);
}

static EFI_STATUS update_no_reboot_flag_mem(EFI_PCI_IO *pci_io,
					    const iTCO_info *itco)
{
	const iTCO_regs* regs = &iTCO_version_regs[itco->itco_version];
	EFI_STATUS status;
	UINT32 pmc_base;
	UINTN pmc_reg;

	status =
	    uefi_call_wrapper(pci_io->Pci.Read, 5, pci_io, EfiPciIoWidthUint32,
			      regs->pmc_base_reg, 1, &pmc_base);
	if (EFI_ERROR(status)) {
		return status;
	}
	pmc_base &= regs->pmc_base_addr_mask;

	pmc_reg = pmc_base + regs->pmc_reg;
	*(volatile UINT32 *)pmc_reg &= ~regs->pmc_no_reboot_mask;

	return status;
}

#define APL_MMCFG_BASE	0xE0000000

static UINTN apl_mmcfg_address(UINTN bus, UINTN device, UINTN function,
			       UINTN offset)
{
	return APL_MMCFG_BASE + (bus << 20) + (device << 15) +
		(function << 12) + offset;
}

static void update_no_reboot_flag_apl(const iTCO_info *itco)
{
	const iTCO_regs* regs = &iTCO_version_regs[itco->itco_version];

	/* Unhide the P2SB device if it's hidden. */
	BOOLEAN p2sb_hidden =
	    *(volatile UINT16 *)apl_mmcfg_address(0, 13, 0, 0) == 0xffff;
	if (p2sb_hidden) {
		*(volatile UINT8 *)apl_mmcfg_address(0, 13, 0, 0xE1) = 0;
	}

	/* Get PMC_BASE from PMC Controller Register. */
	volatile UINT8 *reg =
	    (volatile UINT8 *)apl_mmcfg_address(0, 13, 1, (UINTN)regs->pmc_reg);
	UINT8 value = *reg;
	value &= ~regs->pmc_no_reboot_mask;
	*reg = value;

	if (p2sb_hidden) {
		*(volatile UINT8 *)apl_mmcfg_address(0, 13, 0, 0xE1) = 1;
	}
}

static EFI_STATUS __attribute__((constructor))
init(EFI_PCI_IO *pci_io, UINT16 pci_vendor_id, UINT16 pci_device_id,
     UINTN timeout)
{
	UINT32 pm_base, tco_base, value;
	UINT8 itco_chip;
	const iTCO_info *itco;
	EFI_STATUS status;

	if (!pci_io || pci_vendor_id != PCI_VENDOR_ID_INTEL ||
	    !itco_supported(pci_device_id, &itco_chip)) {
		return EFI_UNSUPPORTED;
	}
	itco = &iTCO_chipset_info[itco_chip];

	Print(L"Detected Intel TCO %s watchdog\n", itco->name);

	pm_base = get_pm_base(pci_io, itco);
	if (pm_base) {
		/*
		 * If SMIs are not triggered, the reboot will only happen on
		 * the second timeout.
		 */
		value = inl(pm_base + SMI_EN_REG);
		if ((value & (TCO_EN | GBL_SMI_EN)) != (TCO_EN | GBL_SMI_EN))
			timeout /= 2;
	}

	tco_base = get_tco_base(pm_base, itco);
	if (!tco_base) {
		return EFI_UNSUPPORTED;
	}

	/* Set timer value */
	value = inw(tco_base + TCO_TMR_REG);
	value &= 0xfc00;
	value |= get_timeout_value(itco->itco_version, timeout) & 0x3ff;
	outw(value, tco_base + TCO_TMR_REG);

	/* Force reloading of timer value */
	outw(1, tco_base + TCO_RLD_REG);

	/* Clear NO_REBOOT flag */
	switch (itco->itco_version) {
	case ITCO_V6:
		update_no_reboot_flag_cnt(tco_base);
		break;
	case ITCO_V5:
		update_no_reboot_flag_apl(itco);
		break;
	case ITCO_V3:
	case ITCO_V2:
		status = update_no_reboot_flag_mem(pci_io, itco);
		if (EFI_ERROR(status)) {
			return status;
		}
		break;
	default:
		return EFI_UNSUPPORTED;
	}

	/* Clear HLT flag to start timer */
	value = inw(tco_base + TCO1_CNT_REG);
	value &= ~TCO_TMR_HLT_MASK;
	outw(value, tco_base + TCO1_CNT_REG);

	return EFI_SUCCESS;
}
