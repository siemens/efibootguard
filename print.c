/*
 * EFI Boot Guard
 *
 * Copyright (c) Siemens AG, 2017-2026
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *  Andreas Reichel <andreas.reichel.ext@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * SPDX-License-Identifier:	GPL-2.0-only
 */

#include <efi.h>
#include <efilib.h>

#include "print.h"

EFI_HANDLE this_image;

VOID PrintC(const UINT8 color, const CHAR16 *fmt, ...)
{
	INT32 attr = ST->ConOut->Mode->Attribute;
	(VOID) ST->ConOut->SetAttribute(ST->ConOut, color);

	va_list args;
	va_start(args, fmt);
	(VOID)VPrint(fmt, args);
	va_end(args);

	(VOID) ST->ConOut->SetAttribute(ST->ConOut, attr);
}

VOID __attribute__((noreturn)) error_exit(CHAR16 *message, EFI_STATUS status)
{
	ERROR(L"%s (%r).\n", message, status);
	(VOID) BS->Stall(3 * 1000 * 1000);
	(VOID) BS->Exit(this_image, status, 0, NULL);
	__builtin_unreachable();
}
