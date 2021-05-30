/*
 * EFI Boot Guard
 *
 * Copyright (c) Siemens AG, 2021
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

static inline UINT8 readb(UINTN addr)
{
	return *(volatile UINT8 *)addr;
}

static inline UINT16 readw(UINTN addr)
{
	return *(volatile UINT16 *)addr;
}

static inline UINT32 readl(UINTN addr)
{
	return *(volatile UINT32 *)addr;
}

static inline void writeb(UINT8 value, UINTN addr)
{
	*(volatile UINT8 *)addr = value;
}

static inline void writew(UINT16 value, UINTN addr)
{
	*(volatile UINT16 *)addr = value;
}

static inline void writel(UINT32 value, UINTN addr)
{
	*(volatile UINT32 *)addr = value;
}
