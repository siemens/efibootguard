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
#include "loader_interface.h"

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
	UINT8 Ignore2[12];
	UINT32 SectionAlignment;
	UINT8 Ignore3[20];
	UINT32 SizeOfImage;
	UINT8 Ignore4[180];
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

EFI_PHYSICAL_ADDRESS align_addr(EFI_PHYSICAL_ADDRESS ptr,
				EFI_PHYSICAL_ADDRESS align)
{
	return (ptr + align - 1) & ~(align - 1);
}

VOID info(CHAR16 *message)
{
	Print(L"Unified kernel stub: %s\n", message);
}

VOID error(CHAR16 *message, EFI_STATUS status)
{
	Print(L"Unified kernel stub: %s (%r).\n", message, status);
	(VOID) BS->Stall(3 * 1000 * 1000);
}

VOID __attribute__((noreturn)) error_exit(CHAR16 *message, EFI_STATUS status)
{
	error(message, status);
	(VOID) BS->Exit(this_image, status, 0, NULL);
	__builtin_unreachable();
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
	const VOID *kernel_source;
	EFI_PHYSICAL_ADDRESS kernel_buffer;
	EFI_PHYSICAL_ADDRESS aligned_kernel_buffer;
	const CHAR8 *fdt_compatible;
	VOID *fdt, *alt_fdt = NULL;
	EFI_IMAGE_ENTRY_POINT kernel_entry;
	EFI_LOADED_IMAGE *stub_image;
	const PE_HEADER *pe_header;
	const SECTION *section;
	EFI_STATUS status, cleanup_status;
	UINTN n, kernel_pages;
	BG_INTERFACE_PARAMS bg_interface_params;

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

	/*
	 * Allocate new home for the kernel image. This is needed because
	 *  - its section is either not executable or not writable
	 *  - section alignment in virtual memory may not fit
	 *
	 * The new buffer size is based from SizeOfImage, aligned according to
	 * the kernels SectionAlignment. As SectionAlignment may be larger than
	 * the page size, over-allocate in order to adjust the base as needed.
	 */
	kernel_source = (UINT8 *) stub_image->ImageBase +
		kernel_section->VirtualAddress;

	pe_header = get_pe_header(kernel_source);

	kernel_pages = EFI_SIZE_TO_PAGES(pe_header->Opt.SizeOfImage +
					 pe_header->Opt.SectionAlignment);
	status = BS->AllocatePages(AllocateAnyPages, EfiLoaderData,
				   kernel_pages, &kernel_buffer);
	if (EFI_ERROR(status)) {
		error(L"Error allocating memory for kernel image", status);
		goto cleanup_initrd;
	}

	aligned_kernel_buffer =
		align_addr(kernel_buffer, pe_header->Opt.SectionAlignment);
	if ((uintptr_t) aligned_kernel_buffer != aligned_kernel_buffer) {
		error(L"Alignment overflow for kernel image", EFI_LOAD_ERROR);
		goto cleanup_buffer;
	}

	kernel_image.ImageBase = (VOID *) (uintptr_t) aligned_kernel_buffer;
	kernel_image.ImageSize = kernel_section->VirtualSize;

	CopyMem(kernel_image.ImageBase, kernel_source, kernel_image.ImageSize);
	/* Clear the rest so that .bss is definitely zero. */
	SetMem((UINT8 *) kernel_image.ImageBase + kernel_image.ImageSize,
	       pe_header->Opt.SizeOfImage - kernel_image.ImageSize, 0);

	status = BS->InstallMultipleProtocolInterfaces(
			&kernel_handle, &LoadedImageProtocol, &kernel_image,
			NULL);
	if (EFI_ERROR(status)) {
		error(L"Error registering kernel image", status);
		goto cleanup_buffer;
	}

	if (alt_fdt) {
		status = replace_fdt(alt_fdt);
		if (EFI_ERROR(status)) {
			goto cleanup_protocols;
		}
		info(L"Using matched embedded device tree");
	} else if (fdt_compatible) {
		if (has_dtbs) {
			info(L"WARNING: No embedded device tree matched firmware-provided one");
		}
		info(L"Using firmware-provided device tree");
	}

	UINT16 *boot_medium_uuidstr =
		disk_get_part_uuid(stub_image->DeviceHandle);
	bg_interface_params.loader_device_part_uuid = boot_medium_uuidstr;
	status = set_bg_interface_vars(&bg_interface_params);
	if (EFI_ERROR(status)) {
		error(L"could not set interface vars", status);
	}
	FreePool(boot_medium_uuidstr);

	kernel_entry = (EFI_IMAGE_ENTRY_POINT)
		((UINT8 *) kernel_image.ImageBase +
		 pe_header->Opt.AddressOfEntryPoint);

	status = kernel_entry(kernel_handle, system_table);

cleanup_protocols:
	cleanup_status = BS->UninstallMultipleProtocolInterfaces(
			kernel_handle, &LoadedImageProtocol, &kernel_image,
			NULL);
	if (EFI_ERROR(cleanup_status)) {
		error(L"Error unregistering kernel image", status);
		if (!EFI_ERROR(status)) {
			status = cleanup_status;
		}
	}
cleanup_buffer:
	BS->FreePages(kernel_buffer, kernel_pages);
cleanup_initrd:
	uninstall_initrd_loader();

	return status;
}
