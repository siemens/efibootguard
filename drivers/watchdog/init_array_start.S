/*
 * EFI Boot Guard
 *
 * Copyright (c) Siemens AG, 2017
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

.section .init_array
/* Dummy value to avoid zero-size .init_array section linker error on
 * architectures having no watchdog drivers */
.word 0x5343
/* align to 4K so that signing tools will not stumble over this section */
/* Note: Explicitly use .b(yte-)align as .align takes power of 2 on ARM */
.balign 0x1000
.global init_array_start
init_array_start:
