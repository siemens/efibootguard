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

#include "kernel-stub.h"
#include "version.h"

typedef struct {
	UINT8 Ignore[60];
	UINT32 PEOffset;
} __attribute__((packed)) DOS_HEADER;

typedef struct {
	UINT8 Ignore1[2];
	UINT16 NumberOfSections;
	UINT8 Ignore2[12];
	UINT16 SizeOfOptionalHeader;
	UINT8 Ignore3[2];
} __attribute__((packed)) COFF_HEADER;

typedef struct {
	UINT8 Ignore1[16];
	UINT32 AddressOfEntryPoint;
	UINT8 Ignore2[220];
} __attribute__((packed)) OPT_HEADER;

typedef struct {
	UINT32 Signature;
	COFF_HEADER Coff;
	OPT_HEADER Opt;
} __attribute__((packed)) PE_HEADER;

typedef struct {
	CHAR8 Name[8];
	UINT32 VirtualSize;
	UINT32 VirtualAddress;
	UINT8 Ignore[24];
} __attribute__((packed)) SECTION;

static EFI_HANDLE this_image;
static EFI_LOADED_IMAGE kernel_image;

VOID __attribute__((noreturn)) error_exit(CHAR16 *message, EFI_STATUS status)
{
	Print(L"Unified kernel stub: %s (%r).\n", message, status);
	(VOID) BS->Stall(3 * 1000 * 1000);
	(VOID) BS->Exit(this_image, status, 0, NULL);
	__builtin_unreachable();
}

static VOID info(CHAR16 *message)
{
	Print(L"Unified kernel stub: %s\n", message);
}

static const PE_HEADER *get_pe_header(const VOID *image)
{
	const DOS_HEADER *dos_header = image;

	return (const PE_HEADER *) ((const UINT8 *) image +
				    dos_header->PEOffset);
}

static const SECTION *get_sections(const PE_HEADER *pe_header)
{
	return (const SECTION *) ((const UINT8 *) &pe_header->Opt +
				  pe_header->Coff.SizeOfOptionalHeader);
}

EFI_STATUS efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table)
{
	const SECTION *cmdline_section = NULL;
	const SECTION *kernel_section = NULL;
	const SECTION *initrd_section = NULL;
	EFI_HANDLE kernel_handle = NULL;
	BOOLEAN has_dtbs = FALSE;
	const CHAR8 *fdt_compatible;
	VOID *fdt, *alt_fdt = NULL;
	EFI_IMAGE_ENTRY_POINT kernel_entry;
	EFI_LOADED_IMAGE *stub_image;
	const PE_HEADER *pe_header;
	const SECTION *section;
	EFI_STATUS status, kernel_status;
	UINTN n;

	this_image = image_handle;
	InitializeLib(image_handle, system_table);

	Print(L"Unified kernel stub (EFI Boot Guard %s)\n",
	      L"" EFIBOOTGUARD_VERSION);

	fdt_compatible = get_fdt_compatible();

	status = BS->OpenProtocol(image_handle, &LoadedImageProtocol,
				  (void **)&stub_image, image_handle,
				  NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
        if (EFI_ERROR(status)) {
		error_exit(L"Error getting LoadedImageProtocol", status);
	}

	/* consider zero-termination for string length */
	if (stub_image->LoadOptionsSize > sizeof(CHAR16)) {
		info(L"WARNING: Passed command line options ignored, only built-in used");
	}

	pe_header = get_pe_header(stub_image->ImageBase);
	for (n = 0, section = get_sections(pe_header);
	     n < pe_header->Coff.NumberOfSections;
	     n++, section++) {
		if (CompareMem(section->Name, ".cmdline", 8) == 0) {
			cmdline_section = section;
		} else if (CompareMem(section->Name, ".kernel", 8) == 0) {
			kernel_section = section;
		} else if (CompareMem(section->Name, ".initrd", 8) == 0) {
			initrd_section = section;
		} else if (CompareMem(section->Name, ".dtb-", 5) == 0) {
			has_dtbs = TRUE;
			fdt = (UINT8 *) stub_image->ImageBase +
				section->VirtualAddress;
			if (match_fdt(fdt, fdt_compatible)) {
				alt_fdt = fdt;
			}
		}
	}

	if (!kernel_section) {
		error_exit(L"Missing .kernel section", EFI_NOT_FOUND);
	}

	kernel_image.ImageBase = (UINT8 *) stub_image->ImageBase +
		kernel_section->VirtualAddress;
	kernel_image.ImageSize = kernel_section->VirtualSize;

	if (cmdline_section) {
		kernel_image.LoadOptions = (UINT8 *) stub_image->ImageBase +
			cmdline_section->VirtualAddress;
		kernel_image.LoadOptionsSize = cmdline_section->VirtualSize;
	}

	if (initrd_section) {
		install_initrd_loader(
			(UINT8 *) stub_image->ImageBase +
			initrd_section->VirtualAddress,
			initrd_section->VirtualSize);
	}

	status = BS->InstallMultipleProtocolInterfaces(
			&kernel_handle, &LoadedImageProtocol, &kernel_image,
			NULL);
	if (EFI_ERROR(status)) {
		uninstall_initrd_loader();
		error_exit(L"Error registering kernel image", status);
	}

	if (alt_fdt) {
		replace_fdt(alt_fdt);
		info(L"Using matched embedded device tree");
	} else if (fdt_compatible) {
		if (has_dtbs) {
			info(L"WARNING: No embedded device tree matched firmware-provided one");
		}
		info(L"Using firmware-provided device tree");
	}

	pe_header = get_pe_header(kernel_image.ImageBase);
	kernel_entry = (EFI_IMAGE_ENTRY_POINT)
		((UINT8 *) kernel_image.ImageBase +
		 pe_header->Opt.AddressOfEntryPoint);

	kernel_status = kernel_entry(kernel_handle, system_table);

	status = BS->UninstallMultipleProtocolInterfaces(
			kernel_handle, &LoadedImageProtocol, &kernel_image,
			NULL);
	if (EFI_ERROR(status)) {
		error_exit(L"Error unregistering kernel image", status);
	}
	uninstall_initrd_loader();

	return kernel_status;
}
