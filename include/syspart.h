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
 * SPDX-License-Identifier:	GPL-2.0-only
 */

#pragma once

#include <efi.h>
#include <efilib.h>
#include <efiprot.h>
#include <efipciio.h>
#include "bootguard.h"

#define open_cfg_file(root, file, mode)					\
	(root)->Open(							\
	    (root), (file), ENV_FILE_NAME, (mode),			\
	    EFI_FILE_ARCHIVE | EFI_FILE_HIDDEN | EFI_FILE_SYSTEM)

#define close_cfg_file(root, file)					\
	(root)->Close(file)

#define read_cfg_file(file, len, buffer)				\
	(file)->Read((file), (len), (buffer))

EFI_STATUS enumerate_cfg_parts(UINTN *config_volumes, UINTN *maxHandles);
