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

#pragma once

#include <efi.h>

extern EFI_HANDLE this_image;

VOID __attribute__((noreturn)) error_exit(CHAR16 *message, EFI_STATUS status);

VOID PrintC(const UINT8 color, const CHAR16 *fmt, ...);

#define ERROR(fmt, ...)                                                        \
	do {                                                                   \
		PrintC(EFI_LIGHTRED, L"ERROR: ");                              \
		PrintC(EFI_LIGHTGRAY, fmt, ##__VA_ARGS__);                     \
	} while (0)

#define WARNING(fmt, ...)                                                      \
	do {                                                                   \
		PrintC(EFI_YELLOW, L"WARNING: ");                              \
		PrintC(EFI_LIGHTGRAY, fmt, ##__VA_ARGS__);                     \
	} while (0)

#if !defined(SILENT_BOOT)
#define INFO(fmt, ...)                                                         \
	PrintC(EFI_LIGHTGRAY, fmt, ##__VA_ARGS__)
#else
#define INFO(fmt, ...) do { } while (0)
#endif
