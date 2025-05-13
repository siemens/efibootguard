/*
 * EFI Boot Guard
 *
 * Modified version of the w83627hf_wdt.c driver found in the Linux kernel
 *
 *	(c) Copyright 2013 Guenter Roeck
 *		converted to watchdog infrastructure
 *
 *	(c) Copyright 2007 Vlad Drukker <vlad@storewiz.com>
 *		added support for W83627THF.
 *
 *	(c) Copyright 2003,2007 Pádraig Brady <P@draigBrady.com>
 *
 *	Based on advantechwdt.c which is based on wdt.c.
 *	Original copyright messages:
 *
 *	(c) Copyright 2000-2001 Marek Michalkiewicz <marekm@linux.org.pl>
 *
 *	(c) Copyright 1996 Alan Cox <alan@lxorguk.ukuu.org.uk>,
 *						All Rights Reserved.
 *
 *	Neither Alan Cox nor CymruNet Ltd. admit liability nor provide
 *	warranty for any of this software. This material is provided
 *	"AS-IS" and at no charge.
 *
 *	(c) Copyright 1995    Alan Cox <alan@lxorguk.ukuu.org.uk>
 *
 * Changes and EFI Boot Guard specific code:
 *
 * 	by Cedric Hombourger <cedric.hombourger@siemens.com>
 * 	(c) Copyright (c) Siemens AG, 2023
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * SPDX-License-Identifier:	GPL-2.0-only
 */

#include <efi.h>
#include <efilib.h>
#include <pci/header.h>
#include <sys/io.h>
#include <mmio.h>
#include "simatic.h"
#include "utils.h"

#define WDT_EFER (wdt_io+0)   /* Extended Function Enable Registers */
#define WDT_EFIR (wdt_io+0)   /* Extended Function Index Register
							(same as EFER) */
#define WDT_EFDR (WDT_EFIR+1) /* Extended Function Data Register */

#define W83627HF_LD_WDT		0x08

#define NCT6116_ID		0xd2

#define NCT6102D_WDT_TIMEOUT	0xf1
#define NCT6102D_WDT_CONTROL	0xf0
#define NCT6102D_WDT_CSR	0xf2

#define WDT_CSR_STATUS		0x10
#define WDT_CSR_KBD		0x40
#define WDT_CSR_MOUSE		0x80

enum chips { nct6116 };

static int wdt_io;
static int cr_wdt_timeout;	/* WDT timeout register */
static int cr_wdt_control;	/* WDT control register */
static int cr_wdt_csr;		/* WDT control & status register */
static int wdt_cfg_enter = 0x87;/* key to unlock configuration space */
static int wdt_cfg_leave = 0xAA;/* key to lock configuration space */

static void superio_outb(int reg, int val)
{
	outb(reg, WDT_EFER);
	outb(val, WDT_EFDR);
}

static inline int superio_inb(int reg)
{
	outb(reg, WDT_EFER);
	return inb(WDT_EFDR);
}

static void superio_enter(void)
{
	outb_p(wdt_cfg_enter, WDT_EFER); /* Enter extended function mode */
	outb_p(wdt_cfg_enter, WDT_EFER); /* Again according to manual */
}

static void superio_select(int ld)
{
	superio_outb(0x07, ld);
}

static void superio_exit(void)
{
	outb_p(wdt_cfg_leave, WDT_EFER); /* Leave extended function mode */
}

static int wdt_find(int addr)
{
	UINT8 val;
	int ret;

	wdt_io = addr;
	superio_enter();
	superio_select(W83627HF_LD_WDT);
	val = superio_inb(0x20);
	switch(val) {
	case NCT6116_ID:
		ret = nct6116;
		cr_wdt_timeout = NCT6102D_WDT_TIMEOUT;
		cr_wdt_control = NCT6102D_WDT_CONTROL;
		cr_wdt_csr = NCT6102D_WDT_CSR;
		break;
	default:
		ret = -1;
		break;
	}
	superio_exit();
	return ret;
}

static void w83627hf_init(enum chips chip)
{
	unsigned char t;

	superio_enter();

	superio_select(W83627HF_LD_WDT);

	/* set CR30 bit 0 to activate GPIO2 */
	t = superio_inb(0x30);
	if (!(t & 0x01))
		superio_outb(0x30, t | 0x01);

	switch (chip) {
	case nct6116:
		/*
		 * These chips have a fixed WDTO# output pin (W83627UHG),
		 * or support more than one WDTO# output pin.
		 * Don't touch its configuration, and hope the BIOS
		 * does the right thing.
		 */
		t = superio_inb(cr_wdt_control);
		t |= 0x02;	/* enable the WDTO# output low pulse
				 * to the KBRST# pin */
		superio_outb(cr_wdt_control, t);
		break;
	default:
		break;
	}

	t = superio_inb(cr_wdt_timeout);
	if (t != 0) {
		WARNING(L"Watchdog already running.\n");
	}

	/* set second mode & disable keyboard turning off watchdog */
	t = superio_inb(cr_wdt_control) & ~0x0C;
	superio_outb(cr_wdt_control, t);

	/* reset status, disable keyboard & mouse turning off watchdog */
	t = superio_inb(cr_wdt_csr);
	t &= ~(WDT_CSR_STATUS | WDT_CSR_KBD | WDT_CSR_MOUSE);
	superio_outb(cr_wdt_csr, t);

	superio_exit();
}

static void wdt_set_time(unsigned int timeout)
{
	superio_enter();
	superio_select(W83627HF_LD_WDT);
	superio_outb(cr_wdt_timeout, timeout);
	superio_exit();
}

static EFI_STATUS init(EFI_PCI_IO *pci_io, UINT16 pci_vendor_id,
		       UINT16 __attribute__((unused)) pci_device_id,
		       UINTN timeout)
{
	int chip;

	if (!pci_io || pci_vendor_id != PCI_VENDOR_ID_INTEL) {
		return EFI_UNSUPPORTED;
	}

	switch (simatic_station_id()) {
	case SIMATIC_IPCBX_56A:
	case SIMATIC_IPCBX_59A:
		chip = wdt_find(0x2e);
		if (chip < 0)
			return EFI_UNSUPPORTED;
		INFO(L"Detected SIMATIC BX5xA watchdog\n");
		w83627hf_init(chip);
		wdt_set_time(timeout);
		return EFI_SUCCESS;
	}
	return EFI_UNSUPPORTED;
}

WATCHDOG_REGISTER(init);
