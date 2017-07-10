/*
 * EFI Boot Guard
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

#define PCI_DEVICE_ID_INTEL_ITC		0x8186
#define PCI_DEVICE_ID_INTEL_CENTERTON	0x0c60
#define PCI_DEVICE_ID_INTEL_QUARK_X1000	0x095e

#define WDTBA_REG			0x84
# define WDTBA_ENABLED			(1 << 31)
# define WDTBA_ADDRMASK			0xffc0

#define TIMER1_REG			0x00
#define TIMER2_REG			0x04
#define RELOAD0_REG			0x0c
#define CONFIG_REG			0x10
# define CONFIG_RESET_ENABLE		(1 << 4)
#define LOCK_REG			0x18
# define LOCK_WDT_LOCK			(1 << 0)
# define LOCK_WDT_ENABLE		(1 << 1)

static EFI_STATUS unlock_timer_regs(EFI_PCI_IO *pci_io, UINT32 wdt_base)
{
	EFI_STATUS status;
	UINT32 value;

	value = 0x80;
	status = uefi_call_wrapper(pci_io->Io.Write, 6, pci_io,
				   EfiPciIoWidthUint8,
				   EFI_PCI_IO_PASS_THROUGH_BAR,
				   wdt_base + RELOAD0_REG, 1, &value);
	if (EFI_ERROR(status))
		return status;

	value = 0x86;
	return uefi_call_wrapper(pci_io->Io.Write, 6, pci_io,
				 EfiPciIoWidthUint8,
				 EFI_PCI_IO_PASS_THROUGH_BAR,
				 wdt_base + RELOAD0_REG, 1, &value);
}

static EFI_STATUS write_timer_regs(EFI_PCI_IO *pci_io, UINT32 wdt_base,
				   UINT32 timer, UINT32 value)
{
	for (unsigned int n = 0; n < 3; n++) {
		EFI_STATUS status;

		status = unlock_timer_regs(pci_io, wdt_base);
		if (EFI_ERROR(status))
			return status;

		status = uefi_call_wrapper(pci_io->Io.Write, 6, pci_io,
					   EfiPciIoWidthUint8,
					   EFI_PCI_IO_PASS_THROUGH_BAR,
					   wdt_base + timer, 1, &value);
		if (EFI_ERROR(status))
			return status;

		value >>= 8;
		timer++;
	}

	return EFI_SUCCESS;
}

static EFI_STATUS __attribute__((constructor))
init(EFI_PCI_IO *pci_io, UINT16 pci_vendor_id, UINT16 pci_device_id,
     UINTN timeout)
{
	UINT32 wdt_base, value;
	EFI_STATUS status;

	if (!pci_io || pci_vendor_id != PCI_VENDOR_ID_INTEL ||
	    (pci_device_id != PCI_DEVICE_ID_INTEL_ITC &&
	     pci_device_id != PCI_DEVICE_ID_INTEL_CENTERTON &&
	     pci_device_id != PCI_DEVICE_ID_INTEL_QUARK_X1000))
		return EFI_UNSUPPORTED;

	status = uefi_call_wrapper(pci_io->Pci.Read, 5, pci_io,
				   EfiPciIoWidthUint32, WDTBA_REG,
				   1, &wdt_base);
	if (EFI_ERROR(status))
		return status;

	if (!(wdt_base & WDTBA_ENABLED))
		return EFI_UNSUPPORTED;

	wdt_base &= WDTBA_ADDRMASK;

	Print(L"Detected Intel Atom/Quark watchdog\n");

	value = ((timeout * 1000000000ULL) >> 15) / 30;
	status = write_timer_regs(pci_io, wdt_base, TIMER1_REG, value);
	if (EFI_ERROR(status))
		return status;

	value = 0;
	status = write_timer_regs(pci_io, wdt_base, TIMER2_REG, value);
	if (EFI_ERROR(status))
		return status;

	value = CONFIG_RESET_ENABLE;
	status = uefi_call_wrapper(pci_io->Io.Write, 6, pci_io,
				   EfiPciIoWidthUint8,
				   EFI_PCI_IO_PASS_THROUGH_BAR,
				   wdt_base + CONFIG_REG, 1, &value);
	if (EFI_ERROR(status))
		return status;

	value = LOCK_WDT_ENABLE | LOCK_WDT_LOCK;
	status = uefi_call_wrapper(pci_io->Io.Write, 6, pci_io,
				   EfiPciIoWidthUint8,
				   EFI_PCI_IO_PASS_THROUGH_BAR,
				   wdt_base + LOCK_REG, 1, &value);

	return status;
}
