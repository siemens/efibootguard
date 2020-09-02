/*
 * EFI Boot Guard
 *
 * Copyright (C) 2020 Mentor Graphics, A Siemens business
 *
 * Authors:
 *  Arsalan H. Awan <Arsalan_Awan@mentor.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <efi.h>
#include <efilib.h>
#include <pci/header.h>

/* #define AMDFCH_WDT_DEBUG */

#ifdef AMDFCH_WDT_DEBUG
#define DebugPrint(fmt, ...) Print(fmt, ##__VA_ARGS__)
#else
#define DebugPrint(fmt, ...) {}
#endif

#define AMDFCH_WDT_MIN_TIMEOUT          0x0001	/* minimum timeout value */
#define AMDFCH_WDT_MAX_TIMEOUT          0xFFFF	/* maximum timeout value */

/* Watchdog register definitions */
#define AMD_ACPI_MMIO_BASE              0xFED80000
#define AMDFCH_WDT_MEM_MAP_OFFSET       0xB00
#define AMDFCH_WDT_MEM_MAP_SIZE         0x100

#define AMDFCH_WDT_CONTROL(base)        ((base) + 0x00) /* Watchdog Control */
 #define AMDFCH_WDT_START_STOP_BIT      (1 << 0)
 #define AMDFCH_WDT_FIRED_BIT           (1 << 1)
 #define AMDFCH_WDT_ACTION_RESET_BIT    (1 << 2)
 #define AMDFCH_WDT_DISABLE_BIT         (1 << 3)
 /* 6:4 bits Reserved */
 #define AMDFCH_WDT_TRIGGER_BIT         (1 << 7)
#define AMDFCH_WDT_COUNT(base)          ((base) + 0x04) /* Watchdog Count */
 #define AMDFCH_WDT_COUNT_MASK          0xFFFF

#define AMD_PM_WATCHDOG_EN_REG          0x00
 #define AMD_PM_WATCHDOG_TIMER_EN       (0x01 << 7)

#define AMD_PM_WATCHDOG_CONFIG_REG      0x03
 #define AMD_PM_WATCHDOG_32USEC_RES     0x0
 #define AMD_PM_WATCHDOG_10MSEC_RES     0x1
 #define AMD_PM_WATCHDOG_100MSEC_RES    0x2
 #define AMD_PM_WATCHDOG_1SEC_RES       0x3
#define AMD_PM_WATCHDOG_CONFIG_MASK     0x3

/* IO port address for indirect access using ACPI PM registers */
#define AMD_IO_PM_INDEX_REG             0xCD6
#define AMD_IO_PM_DATA_REG              0xCD7

#define PCI_REVISION_ID_REG             0x8

#define PCI_DEVICE_ID_AMD_CARRIZO_SMBUS 0x790B
#define PCI_VENDOR_ID_AMD               0x1022

static struct
{
	UINTN base;
	EFI_PCI_IO *pci_io;
} watchdog;

static EFI_STATUS writel(UINT32 val, UINT32 addr)
{
	DebugPrint(L"\n- writel(0x%X, 0x%X) ", val, addr);
	EFI_STATUS status;

	status = uefi_call_wrapper(watchdog.pci_io->Mem.Write, 6, watchdog.pci_io,
				   EfiPciIoWidthUint32,
				   EFI_PCI_IO_PASS_THROUGH_BAR,
				   addr, 1, &val);

	if (EFI_ERROR(status)) {
		Print(L"Error while writel(0x%X, 0x%X): %r", val, addr, status);
	}

	return status;
}

static UINT32 readl(UINT32 addr)
{
	DebugPrint(L"\n- readl(0x%X) ", addr);
	UINT32 val = 0;
	EFI_STATUS status;

	status = uefi_call_wrapper(watchdog.pci_io->Mem.Read, 6, watchdog.pci_io,
				   EfiPciIoWidthUint32,
				   EFI_PCI_IO_PASS_THROUGH_BAR,
				   addr, 1, &val);

	if (EFI_ERROR(status)) {
		Print(L"Error while readl(0x%X): %r", addr, status);
	}

	DebugPrint(L"= 0x%X ", val);
	return val;
}

static EFI_STATUS outb(UINT8 val, UINT16 reg)
{
	DebugPrint(L"\n- outb(0x%X, 0x%X) ", val, reg);
	EFI_STATUS status;

	status = uefi_call_wrapper(watchdog.pci_io->Io.Write, 6, watchdog.pci_io,
				   EfiPciIoWidthUint8,
				   EFI_PCI_IO_PASS_THROUGH_BAR,
				   reg, 1, &val);
	if (EFI_ERROR(status)) {
		Print(L"Error while outb(0x%X, 0x%X): %r", val, reg, status);
	}

	return status;
}

static UINT8 inb(UINT16 reg)
{
	DebugPrint(L"\n- inb(0x%X) ", reg);
	EFI_STATUS status;
	UINT8 val = 0;

	status = uefi_call_wrapper(watchdog.pci_io->Io.Read, 6, watchdog.pci_io,
				   EfiPciIoWidthUint8,
				   EFI_PCI_IO_PASS_THROUGH_BAR,
				   reg, 1, &val);
	if (EFI_ERROR(status)) {
		Print(L"Error while inb(0x%X): %r", reg, status);
	}

	DebugPrint(L"= 0x%X ", val);
	return val;
}

static EFI_STATUS amdfch_wdt_enable(VOID)
{
	DebugPrint(L"\n-- amdfch_wdt_enable() ");
	UINT8 val;
	/* Enable watchdog timer */
	outb(AMD_PM_WATCHDOG_EN_REG, AMD_IO_PM_INDEX_REG);
	val = inb(AMD_IO_PM_DATA_REG);
	val |= AMD_PM_WATCHDOG_TIMER_EN;
	return outb(val, AMD_IO_PM_DATA_REG);
}

static EFI_STATUS amdfch_wdt_set_resolution(UINT8 freq)
{
	DebugPrint(L"\n-- amdfch_wdt_set_resolution(%d) ", freq);
	UINT8 val;
	/* Set the watchdog timer resolution */
	outb(AMD_PM_WATCHDOG_CONFIG_REG, AMD_IO_PM_INDEX_REG);
	val = inb(AMD_IO_PM_DATA_REG);
	/* Clear the previous frequency setting, if any */
	val &= ~AMD_PM_WATCHDOG_CONFIG_MASK;
	/* Set the new frequency value */
	val |= freq;
	return outb(val, AMD_IO_PM_DATA_REG);
}

static EFI_STATUS amdfch_wdt_set_timeout_action_reboot(VOID)
{
	DebugPrint(L"\n-- amdfch_wdt_set_timeout_action_reboot() ");
	UINT32 val;
	/* Set the watchdog timeout action to reboot */
	val = readl(AMDFCH_WDT_CONTROL(watchdog.base));
	val &= ~AMDFCH_WDT_ACTION_RESET_BIT;
	return writel(val, AMDFCH_WDT_CONTROL(watchdog.base));
}

static EFI_STATUS amdfch_wdt_set_time(UINT32 t)
{
	DebugPrint(L"\n-- amdfch_wdt_set_time(%d) ", t);
	if (t < AMDFCH_WDT_MIN_TIMEOUT)
		t = AMDFCH_WDT_MIN_TIMEOUT;
	else if (t > AMDFCH_WDT_MAX_TIMEOUT)
		t = AMDFCH_WDT_MAX_TIMEOUT;

	/* Write new timeout value to watchdog COUNT register */
	return writel(t, AMDFCH_WDT_COUNT(watchdog.base));
}

static EFI_STATUS amdfch_wdt_start(VOID)
{
	DebugPrint(L"\n-- amdfch_wdt_start() ");
	UINT32 val;
	/* Start the watchdog timer */
	val = readl(AMDFCH_WDT_CONTROL(watchdog.base));
	val |= AMDFCH_WDT_START_STOP_BIT;
	return writel(val, AMDFCH_WDT_CONTROL(watchdog.base));
}

static EFI_STATUS amdfch_wdt_ping(VOID)
{
	DebugPrint(L"\n-- amdfch_wdt_ping() ");
	UINT32 val;
	/* Trigger/Ping watchdog timer */
	val = readl(AMDFCH_WDT_CONTROL(watchdog.base));
	val |= AMDFCH_WDT_TRIGGER_BIT;
	return writel(val, AMDFCH_WDT_CONTROL(watchdog.base));
}

static EFI_STATUS __attribute__((constructor))
init(EFI_PCI_IO *pci_io, UINT16 pci_vendor_id, UINT16 pci_device_id,
     UINTN timeout)
{
	EFI_STATUS status;
	UINT8 pci_revision_id = 0;

	if (!pci_io || pci_vendor_id != PCI_VENDOR_ID_AMD ||
	    pci_device_id != PCI_DEVICE_ID_AMD_CARRIZO_SMBUS) {
		return EFI_UNSUPPORTED;
	}

	status = uefi_call_wrapper(pci_io->Pci.Read, 5, pci_io,
				   EfiPciIoWidthUint8, PCI_REVISION_ID_REG,
				   1, &pci_revision_id);
	if (EFI_ERROR(status)) {
		return EFI_UNSUPPORTED;
	}

	switch (pci_revision_id) {
	case 0x59:
	case 0x61:
		Print(L"\nDetected AMD FCH Watchdog Timer (rev %X)\n", pci_revision_id);
		break;
	default:
		Print(L"\nError: Detected Unknown AMD FCH Watchdog Timer\n");
		return EFI_UNSUPPORTED;
	}

	watchdog.base = AMD_ACPI_MMIO_BASE + AMDFCH_WDT_MEM_MAP_OFFSET;
	watchdog.pci_io = pci_io;

	DebugPrint(L"\nwatchdog.base = 0x%X\n", watchdog.base);

	status = amdfch_wdt_enable();
	if (EFI_ERROR(status)) {
		Print(L"Error: amdfch_wdt_enable() failed.");
		return status;
	}

	status = amdfch_wdt_set_resolution(AMD_PM_WATCHDOG_1SEC_RES);
	if (EFI_ERROR(status)) {
		Print(L"Error: amdfch_wdt_set_resolution(%d) failed.", AMD_PM_WATCHDOG_1SEC_RES);
		return status;
	}

	status = amdfch_wdt_set_timeout_action_reboot();
	if (EFI_ERROR(status)) {
		Print(L"Error: amdfch_wdt_set_timeout_action_reboot() failed.");
		return status;
	}

	status = amdfch_wdt_set_time(timeout);
	if (EFI_ERROR(status)) {
		Print(L"Error: amdfch_wdt_set_time(%d) failed.", timeout);
		return status;
	}

	status = amdfch_wdt_start();
	if (EFI_ERROR(status)) {
		Print(L"Error: amdfch_wdt_start() failed.");
		return status;
	}

	status = amdfch_wdt_ping();
	if (EFI_ERROR(status)) {
		Print(L"Error: amdfch_wdt_ping() failed.");
		return status;
	}

	return status;
}
