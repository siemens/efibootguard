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

#ifndef __H_SYSPART__
#define __H_SYSPART__

#include <efi.h>
#include <efilib.h>
#include <efiprot.h>
#include <efipciio.h>
#include "bootguard.h"

#define open_cfg_file(root, file, mode)					      \
	uefi_call_wrapper((root)->Open, 5, (root), (file),		      \
			  ENV_FILE_NAME, (mode),			      \
			  EFI_FILE_ARCHIVE | EFI_FILE_HIDDEN | EFI_FILE_SYSTEM)

#define close_cfg_file(root, file)					      \
	uefi_call_wrapper((root)->Close, 1, (file))

#define read_cfg_file(file, len, buffer)				      \
	uefi_call_wrapper((file)->Read, 3, (file), (len), (buffer))

EFI_STATUS enumerate_cfg_parts(EFI_FILE_HANDLE *roots, UINTN *maxHandles);

#endif // __H_SYSPART__
