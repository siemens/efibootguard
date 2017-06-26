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

#define PCI_DEVICE_ID_INTEL_ESB_9	0x25ab

#define ESB_LOCK_REG			0x68
# define ESB_LOCK_WDT_LOCK		(1 << 0)
# define ESB_LOCK_WDT_ENABLE		(1 << 1)

#define ESB_TIMER1_REG			0x00
#define ESB_TIMER2_REG			0x04
#define ESB_RELOAD_REG			0x0c

static EFI_STATUS unlock_timer_regs(EFI_PCI_IO *pci_io)
{
	EFI_STATUS status;
	UINT32 value;

	value = 0x80;
	status = uefi_call_wrapper(pci_io->Mem.Write, 6, pci_io,
				   EfiPciIoWidthUint32, 0, ESB_RELOAD_REG,
				   1, &value);
	if (EFI_ERROR(status))
		return status;

	value = 0x86;
	return uefi_call_wrapper(pci_io->Mem.Write, 6, pci_io,
				   EfiPciIoWidthUint32, 0, ESB_RELOAD_REG,
				   1, &value);
}

static EFI_STATUS __attribute__((constructor))
init(EFI_PCI_IO *pci_io, UINT16 pci_vendor_id, UINT16 pci_device_id,
     UINTN timeout)
{
	EFI_STATUS status;
	UINT32 value;

	if (!pci_io || pci_vendor_id != PCI_VENDOR_ID_INTEL ||
	    pci_device_id != PCI_DEVICE_ID_INTEL_ESB_9)
		return EFI_UNSUPPORTED;

	Print(L"Detected i6300ESB watchdog\n");

	status = unlock_timer_regs(pci_io);
	if (EFI_ERROR(status))
		return status;

	value = ((timeout * 1000000000ULL) >> 15) / 30;
	status = uefi_call_wrapper(pci_io->Mem.Write, 6, pci_io,
				   EfiPciIoWidthUint32, 0, ESB_TIMER1_REG,
				   1, &value);
	if (EFI_ERROR(status))
		return status;

	status = unlock_timer_regs(pci_io);
	if (EFI_ERROR(status))
		return status;

	value = 0;
	status = uefi_call_wrapper(pci_io->Mem.Write, 6, pci_io,
				   EfiPciIoWidthUint32, 0, ESB_TIMER2_REG,
				   1, &value);
	if (EFI_ERROR(status))
		return status;

	value = ESB_LOCK_WDT_ENABLE | ESB_LOCK_WDT_LOCK;
	status = uefi_call_wrapper(pci_io->Pci.Write, 5, pci_io,
				   EfiPciIoWidthUint8, ESB_LOCK_REG,
				   1, &value);

	return status;
}
