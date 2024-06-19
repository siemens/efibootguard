/*
 * EFI Boot Guard
 *
 * Copyright (C) 2021 Mentor Graphics, A Siemens business
 *
 * Authors:
 *  Cedric Hombourger <Cedric_Hombourger@mentor.com>
 *
 * This driver includes WDAT structs from linux/include/acpi/actbl.h (GPL-2.0)
 * ACPI structs were created from https://uefi.org/specs/ACPI/6.4/index.html
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * SPDX-License-Identifier:     GPL-2.0
 */

#include <efi.h>
#include <efilib.h>
#include <mmio.h>
#include <sys/io.h>
#include "utils.h"

#define EFI_ACPI_TABLE_GUID \
    { 0xeb9d2d30, 0x2d88, 0x11d3, {0x9a, 0x16, 0x0, 0x90, 0x27, 0x3f, 0xc1, 0x4d }}
#define EFI_ACPI_20_TABLE_GUID \
    { 0x8868e871, 0xe4f1, 0x11d3, {0xbc, 0x22, 0x0, 0x80, 0xc7, 0x3c, 0x88, 0x81 }}

#define EFI_ACPI_ROOT_SDP_REVISION 0x02

#define ACPI_SIG_RSDP (CHAR8 *)"RSD PTR "
#define ACPI_SIG_RSDT (CHAR8 *)"RSDT"
#define ACPI_SIG_XSDT (CHAR8 *)"XSDT"
#define ACPI_SIG_WDAT (CHAR8 *)"WDAT"

#define ACPI_WDAT_ENABLED	1

#pragma pack(1)

/* --------------------------------------------------------------------------
 * ACPI structures
 * Reference: https://uefi.org/specs/ACPI/6.4/index.html
 * --------------------------------------------------------------------------
 */

/* Root System Description Pointer  (ACPI section 5.2.5.3) */
typedef struct {
    CHAR8   signature[8];
    UINT8   checksum;
    UINT8   oem_id[6];
    UINT8   revision;
    UINT32  rsdt_address;
    UINT32  length;
    UINT64  xsdt_address;
    UINT8   extended_checksum;
    UINT8   reserved[3];
} EFI_ACPI_ROOT_SDP_HEADER;

/* System Description Table  (ACPI section 5.2.6) */
typedef struct {
	CHAR8   signature[4];
	UINT32  length;
	UINT8   revision;
	UINT8   checksum;
	CHAR8   oem_id[6];
	CHAR8   oem_table_id[8];
	UINT32  oem_revision;
	UINT32  creator_id;
	UINT32  creator_revision;
} EFI_ACPI_SDT_HEADER;

/* Generic Address Structure (ACPI section 5.2.3.2) */
typedef struct {
	UINT8 space_id;            /* Address space where struct or register exists */
	UINT8 bit_width;           /* Size in bits of given register */
	UINT8 bit_offset;          /* Bit offset within the register */
	UINT8 access_width;        /* Minimum Access size (ACPI 3.0) */
	UINT64 address;            /* 64-bit address of struct or register */
} ACPI_ADDR;

/* Values for space_id field above */
enum acpi_spaces {
	ACPI_ADRR_SPACE_SYSTEM_MEMORY = 0,
	ACPI_ADDR_SPACE_SYSTEM_IO     = 1
};

/* --------------------------------------------------------------------------
 * WDAT structures
 * Reference: linux/include/acpi/actbl.h (GPL-2.0)
 * --------------------------------------------------------------------------
 */

/* WDAT Instruction Entries (actions) */
typedef struct {
	UINT8 action;
	UINT8 instruction;
	UINT16 reserved;
	ACPI_ADDR register_region;
	UINT32 value;              /* Value used with Read/Write register */
	UINT32 mask;               /* Bitmask required for this register instruction */
} ACPI_WDAT_ENTRY;

/* Values for action field above */
enum acpi_wdat_actions {
	ACPI_WDAT_RESET             = 1,
	ACPI_WDAT_SET_COUNTDOWN     = 6,
	ACPI_WDAT_SET_RUNNING_STATE = 9,
	ACPI_WDAT_SET_REBOOT        = 17,
	ACPI_WDAT_GET_STATUS        = 32
};

/* Values for instruction field above */
enum acpi_wdat_instructions {
	ACPI_WDAT_READ_VALUE        = 0,
	ACPI_WDAT_READ_COUNTDOWN    = 1,
	ACPI_WDAT_WRITE_VALUE       = 2,
	ACPI_WDAT_WRITE_COUNTDOWN   = 3,
	ACPI_WDAT_PRESERVE_REGISTER = 0x80
};

/* Watchdog Action Table */
typedef struct {
	EFI_ACPI_SDT_HEADER header; /* Common ACPI table header */
	UINT32 header_length;       /* Watchdog Header Length */
	UINT16 pci_segment;         /* PCI Segment number */
	UINT8 pci_bus;              /* PCI Bus number */
	UINT8 pci_device;           /* PCI Device number */
	UINT8 pci_function;         /* PCI Function number */
	UINT8 reserved[3];
	UINT32 timer_period;        /* Period of one timer count (msec) */
	UINT32 max_count;           /* Maximum counter value supported */
	UINT32 min_count;           /* Minimum counter value */
	UINT8 flags;
	UINT8 reserved2[3];
	UINT32 entries;             /* Number of watchdog entries that follow */
} ACPI_TABLE_WDAT;

#pragma pack()

static BOOLEAN probed_before;

/* --------------------------------------------------------------------------
 * Parsing of ACPI/WDAT structures for efibootguard
 * --------------------------------------------------------------------------
 */

static EFI_STATUS
parse_rsdt(EFI_ACPI_SDT_HEADER *rsdt, ACPI_TABLE_WDAT **wdat_table_ptr)
{
	UINT32 *entry_ptr;
	UINT32 n, count;

	if (strncmpa(ACPI_SIG_RSDT, (CHAR8 *)(VOID *)(rsdt->signature), 4)) {
		return EFI_INCOMPATIBLE_VERSION;
	}

	entry_ptr = (UINT32 *)(rsdt + 1);
	count = (rsdt->length - sizeof (EFI_ACPI_SDT_HEADER)) / sizeof(UINT32);
	for (n = 0; n < count; n++, entry_ptr++) {
		EFI_ACPI_SDT_HEADER *entry =
			(EFI_ACPI_SDT_HEADER *)((UINTN)(*entry_ptr));
		if (!strncmpa(ACPI_SIG_WDAT, entry->signature, 4)) {
			*wdat_table_ptr = (ACPI_TABLE_WDAT *)entry;
			return EFI_SUCCESS;
		}
	}
	return EFI_UNSUPPORTED;
}

static EFI_STATUS
parse_xsdt(EFI_ACPI_SDT_HEADER *xsdt, ACPI_TABLE_WDAT **wdat_table_ptr)
{
	UINT64 *entry_ptr;
	UINT64 n, count;

	if (strncmpa(ACPI_SIG_XSDT, (CHAR8 *)(VOID *)(xsdt->signature), 4)) {
		return EFI_INCOMPATIBLE_VERSION;
	}

	entry_ptr = (UINT64 *)(xsdt + 1);
	count = (xsdt->length - sizeof (EFI_ACPI_SDT_HEADER)) / sizeof(UINT64);
	for (n = 0; n < count; n++, entry_ptr++) {
		EFI_ACPI_SDT_HEADER *entry =
			(EFI_ACPI_SDT_HEADER *)((UINTN)(*entry_ptr));
		if (!strncmpa(ACPI_SIG_WDAT, entry->signature, 4)) {
			*wdat_table_ptr = (ACPI_TABLE_WDAT *)entry;
			return EFI_SUCCESS;
		}
	}
	return EFI_UNSUPPORTED;
}

static EFI_STATUS
parse_rsdp(EFI_ACPI_ROOT_SDP_HEADER *rsdp, ACPI_TABLE_WDAT **wdat_table_ptr)
{
	EFI_ACPI_SDT_HEADER *sdt;

	*wdat_table_ptr = NULL;

	if (rsdp->revision > EFI_ACPI_ROOT_SDP_REVISION) {
		ERROR(L"SDP revision not supported (%d)\n", rsdp->revision);
		return EFI_INCOMPATIBLE_VERSION;
	}

	if (rsdp->revision == EFI_ACPI_ROOT_SDP_REVISION) {
		sdt = (EFI_ACPI_SDT_HEADER *)(UINTN)(rsdp->xsdt_address);
		return parse_xsdt(sdt, wdat_table_ptr);
	}
	else {
		sdt = (EFI_ACPI_SDT_HEADER *)(UINTN)(rsdp->rsdt_address);
		return parse_rsdt(sdt, wdat_table_ptr);
	}
}

static EFI_STATUS
locate_and_parse_rsdp(ACPI_TABLE_WDAT **wdat_table_ptr)
{
	EFI_CONFIGURATION_TABLE *ect = ST->ConfigurationTable;
	EFI_ACPI_ROOT_SDP_HEADER *rsdp;
	EFI_GUID acpi_table_guid = ACPI_TABLE_GUID;
	EFI_GUID acpi2_table_guid = ACPI_20_TABLE_GUID;
	UINTN n;

	for (n = 0; n < ST->NumberOfTableEntries; n++) {
		if ((CompareGuid(&ect->VendorGuid, &acpi_table_guid) ||
		     CompareGuid(&ect->VendorGuid, &acpi2_table_guid)) &&
		    !strncmpa(ACPI_SIG_RSDP, (CHAR8 *)(ect->VendorTable), 8)) {
			rsdp = (EFI_ACPI_ROOT_SDP_HEADER *)ect->VendorTable;
			return parse_rsdp(rsdp, wdat_table_ptr);
		}
		ect++;
	}
	return EFI_UNSUPPORTED;
}

static EFI_STATUS
read_reg(ACPI_ADDR *addr, UINT32 *value_ptr)
{
	UINT32 value;

	if (addr->access_width < 1 || addr->access_width > 3) {
		ERROR(L"invalid width for WDAT read operation!\n");
		return EFI_UNSUPPORTED;
	}

	if (addr->space_id == ACPI_ADDR_SPACE_SYSTEM_IO) {
		switch (addr->access_width) {
		case 1:
			value = inb(addr->address);
			break;
		case 2:
			value = inw(addr->address);
			break;
		case 3:
			value = inl(addr->address);
			break;
		}
	}
	else {
		switch (addr->access_width) {
		case 1:
			value = readb(addr->address);
			break;
		case 2:
			value = readw(addr->address);
			break;
		case 3:
			value = readl(addr->address);
			break;
		}
	}

	if (value_ptr) {
		*value_ptr = value;
	}
	return EFI_SUCCESS;
}

static EFI_STATUS
write_reg(ACPI_ADDR *addr, UINT32 value)
{
	if ((addr->access_width < 1) || (addr->access_width > 3)) {
		ERROR(L"invalid width for WDAT write operation!\n");
		return EFI_UNSUPPORTED;
	}

	if (addr->space_id == ACPI_ADDR_SPACE_SYSTEM_IO) {
		switch (addr->access_width) {
		case 1:
			outb(value, addr->address);
			break;
		case 2:
			outw(value, addr->address);
			break;
		case 3:
			outl(value, addr->address);
			break;
		}
	}
	else {
		switch (addr->access_width) {
		case 1:
			writeb(value, addr->address);
			break;
		case 2:
			writew(value, addr->address);
			break;
		case 3:
			writel(value, addr->address);
			break;
		}
	}
	return EFI_SUCCESS;
}

static EFI_STATUS
read_value(ACPI_ADDR *addr, UINT32 value, UINT32 mask, UINT32 *retval)
{
	EFI_STATUS status;
	UINT32 x;

	status = read_reg(addr, &x);
	if (EFI_ERROR(status)) {
		return status;
	}
	x >>= addr->bit_offset;
	x &= mask;
	if (retval) {
		*retval = (x == value);
	}
	return status;
}

static EFI_STATUS
read_countdown(ACPI_ADDR *addr, UINT32 mask, UINT32 *retval)
{
	EFI_STATUS status;
	UINT32 x;

	status = read_reg(addr, &x);
	if (EFI_ERROR(status)) {
		return status;
	}
	x >>= addr->bit_offset;
	x &= mask;
	if (retval) {
		*retval = x;
	}
	return status;
}

static EFI_STATUS
write_value(ACPI_ADDR *addr, UINT32 value, UINT32 mask, BOOLEAN preserve)
{
	UINT32 x, y;

	x = value & mask;
	x <<= addr->bit_offset;
	if (preserve) {
		EFI_STATUS status = read_reg(addr, &y);
		if (EFI_ERROR(status)) {
			return status;
		}
		y = y & ~(mask << addr->bit_offset);
		x |= y;
	}
	return write_reg(addr, x);
}

static EFI_STATUS
run_action(ACPI_TABLE_WDAT *wdat_table, UINT8 action, UINT32 param, UINT32 *retval)
{
	ACPI_WDAT_ENTRY *wdat_entry;
	ACPI_ADDR *addr;
	EFI_STATUS status = EFI_UNSUPPORTED;
	BOOLEAN preserve;
	UINT32 flags, value, mask;
	UINTN n;

	/* ACPI_TABLE_WDAT is immediately followed by multiple ACPI_WDAT_ENTRY tables,
	 * the former tells us how many (via ->entries). */
	wdat_entry = (ACPI_WDAT_ENTRY *)(wdat_table + 1);
	for (n = 0; n < wdat_table->entries; n++, wdat_entry++) {
		/* Check if this is the action we have requested */
		if (wdat_entry->action != action) {
			continue;
		}

		/* Decode the action */
		preserve = (wdat_entry->instruction & ACPI_WDAT_PRESERVE_REGISTER) != 0;
		flags = wdat_entry->instruction & ~ACPI_WDAT_PRESERVE_REGISTER;
		value = wdat_entry->value;
		mask = wdat_entry->mask;
		addr = &wdat_entry->register_region;

		/* Operation */
		switch (flags) {
		case ACPI_WDAT_READ_VALUE:
			status = read_value(addr, value, mask, retval);
			break;
		case ACPI_WDAT_READ_COUNTDOWN:
			status = read_countdown(addr, mask, retval);
			break;
		case ACPI_WDAT_WRITE_VALUE:
			status = write_value(addr, value, mask, preserve);
			break;
		case ACPI_WDAT_WRITE_COUNTDOWN:
			status = write_value(addr, param, mask, preserve);
			break;
		default:
			ERROR(L"Unsupported WDAT instruction %x!\n", flags);
			return EFI_UNSUPPORTED;
		}
		/* Stop on first error */
		if (EFI_ERROR(status)) {
			return status;
		}
	}
	return status;
}

static EFI_STATUS init(EFI_PCI_IO __attribute__((unused)) * pci_io,
		       UINT16 __attribute__((unused)) pci_vendor_id,
		       UINT16 __attribute__((unused)) pci_device_id,
		       UINTN timeout)
{
	ACPI_TABLE_WDAT *wdat_table;
	EFI_STATUS status;
	UINT32 boot_status;
	UINTN n;

	/* We do not use PCI, and machines may have many PCI devices */
	if (probed_before)
		return EFI_UNSUPPORTED;
	probed_before = TRUE;

	/* Locate WDAT in ACPI tables */
	status = locate_and_parse_rsdp(&wdat_table);
	if (EFI_ERROR(status)) {
		return status;
	}
	if (!(wdat_table->flags & ACPI_WDAT_ENABLED)) {
		return EFI_UNSUPPORTED;
	}
	INFO(L"Detected WDAT watchdog\n");

	/* Check if the boot was caused by the watchdog */
	status = run_action(wdat_table, ACPI_WDAT_GET_STATUS, 0, &boot_status);
	if ((status == EFI_SUCCESS) && (boot_status != 0)) {
		INFO(L"Boot caused by watchdog\n");
	}

	/* Enable reboot */
	status = run_action(wdat_table, ACPI_WDAT_SET_REBOOT, 0, NULL);
	if (EFI_ERROR(status) && (status != EFI_UNSUPPORTED)) {
		ERROR(L"Could not enable REBOOT for WDAT!\n");
		return status;
	}

	/* Check period */
	if (wdat_table->timer_period == 0) {
		ERROR(L"Invalid WDAT period in ACPI tables!\n");
		return EFI_INVALID_PARAMETER;
	}

	/* Compute timeout in periods */
	n = (timeout * 1000) / wdat_table->timer_period;

	/* Program countdown */
	status = run_action(wdat_table, ACPI_WDAT_SET_COUNTDOWN, n, NULL);
	if (EFI_ERROR(status)) {
		ERROR(L"Could not change WDAT timeout!\n");
		return status;
	}

	/* Initial ping with specified timeout */
	status = run_action(wdat_table, ACPI_WDAT_RESET, n, NULL);
	if (EFI_ERROR(status)) {
		ERROR(L"Could not reset WDAT!\n");
		return status;
	}

	/* Enable watchdog */
	status = run_action(wdat_table, ACPI_WDAT_SET_RUNNING_STATE, 0, NULL);
	if (EFI_ERROR(status)) {
		ERROR(L"Could not change WDAT to RUNNING state!\n");
		return status;
	}

	return EFI_SUCCESS;
}

WATCHDOG_REGISTER(init);
