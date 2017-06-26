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
 */

#ifndef __H_SYSPART__
#define __H_SYSPART__

#include <efi.h>
#include <efilib.h>
#include <efiprot.h>
#include <efipciio.h>
#include "bootguard.h"

EFI_STATUS enumerate_cfg_parts(EFI_FILE_HANDLE *roots, UINTN *maxHandles);

#endif // __H_SYSPART__
