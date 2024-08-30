/*
 * EFI Boot Guard
 *
 * Copyright (c) Siemens AG, 2024
 *
 * Authors:
 *  Christian Storm <christian.storm@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * SPDX-License-Identifier:	GPL-2.0-only
 */

#include <efi.h>
#include "utils.h"

/* Section .wdfunc's end address for watchdog probing function pointers
 * preceding this marker, if any. */
WATCHDOG_PROBE wdfuncs_end __attribute__((used, section(".wdfuncs"))) =
	(WATCHDOG_PROBE)0x4353;
