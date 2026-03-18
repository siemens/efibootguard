/*
 * EFI Boot Guard
 *
 * Copyright (c) Siemens AG, 2026
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * SPDX-License-Identifier:	GPL-2.0-only
 */

#pragma once

#include <efi.h>

typedef EFI_STATUS (*WATCHDOG_PROBE)(EFI_PCI_IO *, UINT16, UINT16, UINTN);

typedef struct _WATCHDOG_DRIVER {
	WATCHDOG_PROBE probe;
	struct _WATCHDOG_DRIVER *next;
} WATCHDOG_DRIVER;

VOID register_watchdog(WATCHDOG_DRIVER *driver);

#define WATCHDOG_REGISTER(_func)                                               \
	static WATCHDOG_DRIVER this_driver = {.probe = _func};                 \
	static void __attribute__((constructor)) register_driver(void)         \
	{                                                                      \
		register_watchdog(&this_driver);                               \
	}

EFI_STATUS probe_watchdogs(UINTN timeout);
