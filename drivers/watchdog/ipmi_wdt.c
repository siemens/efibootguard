/*
 * EFI Boot Guard
 *
 * Copyright (c) Siemens AG, 2023
 *
 * Authors:
 *  Henning Schild <henning.schild@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <efi.h>
#include <pci/header.h>
#include <sys/io.h>
#include "smbios.h"
#include "utils.h"

#define SMBIOS_TYPE_IPMI_KCS		38
#define IPMI_KCS_DEFAULT_IOBASE		0xca2

#define IPMI_KCS_STS_OBF        	0x1
#define IPMI_KCS_STS_IBF        	0x2

#define IPMI_KCS_CMD_ABORT	        0x60
#define IPMI_KCS_CMD_WRITE_START        0x61
#define IPMI_KCS_CMD_WRITE_END          0x62

#define IPMI_KCS_NETFS_LUN_WDT  	0x18

#define IPMI_WDT_CMD_RESET      	0x22
#define IPMI_WDT_CMD_SET        	0x24
#define  IPMI_WDT_SET_USE_OSLOAD        0x3
#define  IPMI_WDT_SET_ACTION_HARD_RESET 0x1

#define kcs_sts_is_error(sts) (((sts >> 6 ) & 0x3) == 0x3)

static UINT8
set_wdt_data[] = {IPMI_WDT_SET_USE_OSLOAD, IPMI_WDT_SET_ACTION_HARD_RESET,
		  0x00, 0x00,0x00, 0x00};

static EFI_EVENT cmdtimer;

static BOOLEAN probed_before;

static EFI_STATUS
kcs_wait_iobf(UINT16 io_base, UINTN iobf)
{
	EFI_STATUS timerstatus = EFI_NOT_READY;

	while (timerstatus == EFI_NOT_READY) {
		UINT8 sts = inb(io_base + 1);

		if (kcs_sts_is_error(sts))
			return EFI_DEVICE_ERROR;
		if (iobf == IPMI_KCS_STS_IBF) {
			/* IBF we wait for clear */
			if (!(sts & IPMI_KCS_STS_IBF))
				return EFI_SUCCESS;
		} else {
			/* OBF we wait for set */
			if (sts & IPMI_KCS_STS_OBF)
				return EFI_SUCCESS;
		}
		BS->Stall(1000 * 1000 / 10);
		timerstatus = BS->CheckEvent(cmdtimer);
	}

	return EFI_DEVICE_ERROR;
}

static EFI_STATUS
kcs_outb(UINT8 value, UINT16 io_base, UINT16 port)
{
	EFI_STATUS status;

	status = kcs_wait_iobf(io_base, IPMI_KCS_STS_IBF);
	if (status)
		return status;

	outb(value, io_base + port);
	/* dummy read, as mentioned in spec */
	inb(io_base);

	return EFI_SUCCESS;
}

static EFI_STATUS
_send_ipmi_cmd(UINT16 io_base, UINT8 cmd, UINT8 *data, UINTN datalen)
{
	UINT8 lastbyte = cmd;
	UINTN err = 0;

	err += kcs_outb(IPMI_KCS_CMD_WRITE_START, io_base, 1);
	err += kcs_outb(IPMI_KCS_NETFS_LUN_WDT, io_base, 0);

	if (datalen) {
		lastbyte = data[datalen - 1];
		err += kcs_outb(cmd, io_base, 0);
		for (UINTN n = 0; n < datalen - 1; n++)
			err += kcs_outb(data[n], io_base, 0);
	}

	err += kcs_outb(IPMI_KCS_CMD_WRITE_END, io_base,  1);
	err += kcs_outb(lastbyte, io_base, 0);

	if (err)
		return EFI_DEVICE_ERROR;

	return kcs_wait_iobf(io_base, IPMI_KCS_STS_OBF);
}

static VOID
handle_ipmi_error(UINT16 io_base)
{
	WARNING(L"Handling Error Status 0x%x\n", inb(io_base + 1));

	outb(IPMI_KCS_CMD_ABORT, io_base + 1);

	if (kcs_wait_iobf(io_base, IPMI_KCS_STS_IBF))
		return;

	if (inb((io_base + 1) & IPMI_KCS_STS_OBF))
		inb(io_base);
	outb(0x0, io_base);

	if (kcs_wait_iobf(io_base, IPMI_KCS_STS_IBF))
		return;
}

static EFI_STATUS
send_ipmi_cmd(UINT16 io_base, UINT8 cmd, UINT8 *data, UINTN datalen)
{
	EFI_STATUS timerstatus;
	EFI_STATUS status;

	/*
	 * Guard every command with a 5s timeout where we retry and try to
	 * recover.
	 */
	BS->SetTimer(cmdtimer, TimerRelative, 50000000);

	do {
		status = _send_ipmi_cmd(io_base, cmd, data, datalen);
		if (status == EFI_SUCCESS)
			return status;
		handle_ipmi_error(io_base);
		timerstatus = BS->CheckEvent(cmdtimer);
	} while (timerstatus == EFI_NOT_READY);

	return status;
}

static EFI_STATUS __attribute__((constructor))
init(__attribute__((unused)) EFI_PCI_IO *pci_io,
     __attribute__((unused)) UINT16 pci_vendor_id,
     __attribute__((unused)) UINT16 pci_device_id,
     UINTN timeout)
{
	SMBIOS_STRUCTURE_TABLE *smbios_table;
	SMBIOS_STRUCTURE_POINTER smbios_struct;
	EFI_STATUS status;
	UINT64 io_base;
	UINT16 *timeout_value;

	/* We do not use PCI, and machines with IPMI have many PCI devices */
	if (probed_before)
		return EFI_UNSUPPORTED;
	probed_before = TRUE;

	status = LibGetSystemConfigurationTable(&SMBIOSTableGuid,
						(VOID **)&smbios_table);

	if (status != EFI_SUCCESS)
		return EFI_UNSUPPORTED;

	smbios_struct = smbios_find_struct(smbios_table, SMBIOS_TYPE_IPMI_KCS);

	if (smbios_struct.Raw == NULL)
		return EFI_UNSUPPORTED;

	io_base = *((UINT64 *)(smbios_struct.Raw + 8));
	if (io_base == 0) {
		io_base = IPMI_KCS_DEFAULT_IOBASE;
	} else {
		if (!(io_base & 1))
			/* MMIO not implemented */
			return EFI_UNSUPPORTED;

		io_base &= ~1;
	}

	INFO(L"Detected IPMI watchdog at I/O 0x%x\n", io_base);
	timeout_value = (UINT16 *)(set_wdt_data + 4);
	*timeout_value = timeout * 10;

	status = BS->CreateEvent(EVT_TIMER, 0, NULL, NULL, &cmdtimer);
	if (status != EFI_SUCCESS)
		return status;

	status = send_ipmi_cmd(io_base, IPMI_WDT_CMD_SET, set_wdt_data,
			       sizeof(set_wdt_data));

	if (status == EFI_SUCCESS)
		status = send_ipmi_cmd(io_base, IPMI_WDT_CMD_RESET, NULL, 0);

	if (status != EFI_SUCCESS)
		ERROR(L"Watchdog device repeatedly reported errors.\n");

	BS->CloseEvent(cmdtimer);
	return status;
}
