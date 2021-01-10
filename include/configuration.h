/*
 * EFI Boot Guard
 *
 * Copyright (c) Siemens AG, 2017
 *
 * Authors:
 *  Andreas Reichel <andreas.reichel.ext@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#pragma once

#include <efi.h>
#include <efilib.h>
#include <efiprot.h>
#include <efipciio.h>
#include <bootguard.h>

BG_STATUS load_config(BG_LOADER_PARAMS *bg_loader_params);
BG_STATUS save_config(BG_LOADER_PARAMS *bg_loader_params);
