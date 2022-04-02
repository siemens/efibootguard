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

VOID __attribute__((noreturn)) error_exit(CHAR16 *message, EFI_STATUS status);

const VOID *get_fdt_compatible(VOID);
BOOLEAN match_fdt(const VOID *fdt, const CHAR8 *compatible);
VOID replace_fdt(const VOID *fdt);

VOID install_initrd_loader(VOID *initrd, UINTN initrd_size);
VOID uninstall_initrd_loader(VOID);
