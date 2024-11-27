/*
 * EFI Boot Guard
 *
 * Copyright (c) Siemens AG, 2024
 *
 * Ported by Cedric Hombourger <cedric.hombourger@siemens.com>
 * From unpublished code created by XingTong Wu <xingtong.wu@siemens.com>
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
#include "simatic.h"
#include "utils.h"

#define WDT_CTRL_REG_BX_21A			0x1854
#define TIMEOUT_MIN_BX_21A			(1)
#define TIMEOUT_DEF_BX_21A			(60)
#define TIMEOUT_MAX_BX_21A			(1024)
/* Over-Clocking WDT Timeout Value */
#define WDT_CTRL_REG_TOV_MASK_BX_21A			(0x3FF)
/* Over-Clocking WDT ICC Survivability Impact */
#define WDT_CTRL_REG_ICCSURV_BIT_BX_21A			BIT(13)
/* Over-Clocking WDT Enable */
#define WDT_CTRL_REG_EN_BIT_BX_21A			BIT(14)
/* Over-Clocking WDT Force All */
#define WDT_CTRL_REG_FORCE_ALL_BIT_BX_21A		BIT(15)
/* Over-Clocking WDT Non-ICC Survivability Mode Timeout Status */
#define WDT_CTRL_REG_NO_ICCSURV_STS_BIT_BX_21A		BIT(24)
/* Over-Clocking WDT ICC Survivability Mode Timeout Status */
#define WDT_CTRL_REG_ICCSURV_STS_BIT_BX_21A		BIT(25)

static EFI_STATUS init(EFI_PCI_IO __attribute__((unused)) * pci_io,
		       UINT16 __attribute__((unused)) pci_vendor_id,
		       UINT16 __attribute__((unused)) pci_device_id,
		       UINTN timeout)
{
	UINT32 regval;

	if (simatic_station_id() != SIMATIC_IPCBX_21A)
		return EFI_UNSUPPORTED;

	INFO(L"Detected SIMATIC BX-21A watchdog\n");

	if (timeout < TIMEOUT_MIN_BX_21A || timeout > TIMEOUT_MAX_BX_21A) {
		WARNING(L"Invalid timeout value (%d), default (%ds) is used.\n",
			timeout, TIMEOUT_DEF_BX_21A);
		timeout = TIMEOUT_DEF_BX_21A;
	}

	regval = inl(WDT_CTRL_REG_BX_21A);
	/* setup timeout value */
	regval &= (~WDT_CTRL_REG_TOV_MASK_BX_21A);
	regval |= (timeout - 1);
	/* get and clear status */
	regval |= WDT_CTRL_REG_NO_ICCSURV_STS_BIT_BX_21A;
	regval |= WDT_CTRL_REG_ICCSURV_STS_BIT_BX_21A;
	outl(regval, WDT_CTRL_REG_BX_21A);

	/* start watchdog */
	regval = inl(WDT_CTRL_REG_BX_21A);
	regval |= (WDT_CTRL_REG_EN_BIT_BX_21A |
			WDT_CTRL_REG_ICCSURV_BIT_BX_21A |
			WDT_CTRL_REG_FORCE_ALL_BIT_BX_21A);
	outl(regval, WDT_CTRL_REG_BX_21A);

	return EFI_SUCCESS;
}

WATCHDOG_REGISTER(init);
