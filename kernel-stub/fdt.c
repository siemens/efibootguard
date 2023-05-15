/*
 * EFI Boot Guard, unified kernel stub
 *
 * Copyright (c) Siemens AG, 2022
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <efi.h>
#include <efilib.h>
#include <byteswap.h>
#include <endian.h>

#include "kernel-stub.h"

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define BE32_TO_HOST(val)	bswap_32(val)
#else
#define BE32_TO_HOST(val)	(val)
#endif

#define SIZE_IN_PAGES(size) ((size + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE)

#define FDT_BEGIN_NODE	0x1
#define FDT_END_NODE	0x2
#define FDT_PROP	0x3
#define FDT_NOP		0x4

typedef struct {
	UINT32 Magic;
	UINT32 TotalSize;
	UINT32 OffDtStruct;
	UINT32 OffDtStrings;
	UINT32 OffMemRsvmap;
	UINT32 Version;
	UINT32 LastCompVersion;
	UINT32 BootCpuidPhys;
	UINT32 SizeDtStrings;
	UINT32 SizeDtStruct;
} FDT_HEADER;

#ifndef EfiDtbTableGuid
static EFI_GUID gEfiDtbTableGuid = {
	0xb1b621d5, 0xf19c, 0x41a5,
	{0x83, 0x0b, 0xd9, 0x15, 0x2c, 0x69, 0xaa, 0xe0}
};
#define EfiDtbTableGuid gEfiDtbTableGuid
#endif

/*
 * FDT fixup protocol, provided only by U-Boot so far, but other firmware may
 * follow. See also
 *  - https://github.com/U-Boot-EFI/EFI_DT_FIXUP_PROTOCOL
 *  - https://github.com/ARM-software/ebbr/issues/68
 */
#ifndef EfiDtFixupProtocol
static EFI_GUID gEfiDtFixupProtocol = {
        0xe617d64c, 0xfe08, 0x46da,
	{0xf4, 0xdc, 0xbb, 0xd5, 0x87, 0x0c, 0x73, 0x00}
};
#define EfiDtFixupProtocol gEfiDtFixupProtocol

#define EFI_DT_APPLY_FIXUPS	0x00000001
#define EFI_DT_RESERVE_MEMORY	0x00000002
#define EFI_DT_INSTALL_TABLE	0x00000004

typedef struct _EFI_DT_FIXUP_PROTOCOL EFI_DT_FIXUP_PROTOCOL;

typedef EFI_STATUS (EFIAPI *EFI_DT_FIXUP)(EFI_DT_FIXUP_PROTOCOL *This,
					  VOID *Fdt, UINTN *BufferSize,
					  UINT32 Flags);

struct _EFI_DT_FIXUP_PROTOCOL {
	UINT64 Revision;
	EFI_DT_FIXUP Fixup;
};
#endif

static const VOID *get_compatible_from_fdt(const VOID *fdt)
{
	const FDT_HEADER *header = fdt;
	const CHAR8 *strings;
	const UINT32 *pos;
	UINT32 len;

	if (BE32_TO_HOST(header->Magic) != 0xd00dfeed) {
		return NULL;
	}

	pos = (const UINT32 *) ((const UINT8 *) fdt +
				BE32_TO_HOST(header->OffDtStruct));
	if (BE32_TO_HOST(*pos++) != FDT_BEGIN_NODE || *pos++ != 0) {
		return NULL;
	}

	strings = (const CHAR8 *) fdt + BE32_TO_HOST(header->OffDtStrings);

	while (1) {
		switch (BE32_TO_HOST(*pos++)) {
		case FDT_PROP:
			len = BE32_TO_HOST(*pos++);
			if (strcmpa(strings + BE32_TO_HOST(*pos++),
				    (const CHAR8 *) "compatible") == 0) {
				return pos;
			}
			pos += (len + 3) / 4;
			break;
		case FDT_NOP:
			break;
		default:
			return NULL;
		}
	}
}

const VOID *get_fdt_compatible(VOID)
{
	const CHAR8 *compatible = NULL;
	EFI_STATUS status;
	VOID *fdt;

	status = LibGetSystemConfigurationTable(&EfiDtbTableGuid, &fdt);
	if (status == EFI_SUCCESS) {
		compatible = get_compatible_from_fdt(fdt);
		if (!compatible) {
			error_exit(L"Invalid firmware FDT",
				   EFI_INVALID_PARAMETER);
		}
	}

	return compatible;
}

BOOLEAN match_fdt(const VOID *fdt, const CHAR8 *compatible)
{
	const CHAR8 *alt_compatible;

	if (!compatible) {
		error_exit(L"Found .dtb section but no firmware DTB",
			   EFI_NOT_FOUND);
	}

	alt_compatible = get_compatible_from_fdt(fdt);
	if (!alt_compatible) {
		error_exit(L"Invalid .dtb section", EFI_INVALID_PARAMETER);
	}

	return strcmpa(compatible, alt_compatible) == 0;
}

static EFI_STATUS clone_fdt(const VOID *fdt, UINTN size,
			    EFI_PHYSICAL_ADDRESS *fdt_buffer)
{
	const FDT_HEADER *header = fdt;
	EFI_STATUS status;

	status = BS->AllocatePages(AllocateAnyPages, EfiACPIReclaimMemory,
				   SIZE_IN_PAGES(size), fdt_buffer);
	if (EFI_ERROR(status)) {
		error(L"Error allocating device tree buffer", status);
		return status;
	}
	CopyMem((VOID *)*fdt_buffer, fdt, BE32_TO_HOST(header->TotalSize));
	return EFI_SUCCESS;
}

EFI_STATUS replace_fdt(const VOID *fdt)
{
	EFI_DT_FIXUP_PROTOCOL *protocol;
	EFI_PHYSICAL_ADDRESS fdt_buffer;
	EFI_STATUS status;
	UINTN size;

	status = LibLocateProtocol(&EfiDtFixupProtocol, (VOID **)&protocol);
	if (EFI_ERROR(status)) {
		const FDT_HEADER *header = fdt;

		info(L"Firmware does not provide device tree fixup protocol");

		size = BE32_TO_HOST(header->TotalSize);
		status = clone_fdt(fdt, size, &fdt_buffer);
		if (EFI_ERROR(status)) {
			return status;
		}
	} else {
		/* Find out which size we need */
		size = 0;
		status = protocol->Fixup(protocol, (VOID *) fdt, &size,
					 EFI_DT_APPLY_FIXUPS);
		if (status != EFI_BUFFER_TOO_SMALL) {
			error(L"Device tree fixup: unexpected error", status);
			return status;
		}

		status = clone_fdt(fdt, size, &fdt_buffer);
		if (EFI_ERROR(status)) {
			return status;
		}

		status = protocol->Fixup(protocol, (VOID *)fdt_buffer, &size,
					 EFI_DT_APPLY_FIXUPS |
					 EFI_DT_RESERVE_MEMORY);
		if (EFI_ERROR(status)) {
			(VOID) BS->FreePages(fdt_buffer, SIZE_IN_PAGES(size));
			error(L"Device tree fixup failed", status);
			return status;
		}
	}

	status = BS->InstallConfigurationTable(&EfiDtbTableGuid,
					       (VOID *)fdt_buffer);
	if (EFI_ERROR(status)) {
		(VOID) BS->FreePages(fdt_buffer, SIZE_IN_PAGES(size));
		error(L"Failed to install alternative device tree", status);
	}

	return status;
}
