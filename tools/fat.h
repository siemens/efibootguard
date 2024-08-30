/*
 * EFI Boot Guard
 *
 * Copyright (c) Siemens AG, 2023
 *
 * Author: Michael Adler <michael.adler@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * SPDX-License-Identifier:	GPL-2.0-only
 */
#pragma once

#include <linux/msdos_fs.h>
#include "ebgpart.h"

/**
 * Determines the number of FAT bits (12, 16, or 32) based on the provided fat_boot_sector.
 *
 * The function performs the necessary checks and validations on the provided boot sector
 * to ensure it is a valid FAT boot sector. If the provided boot sector is not valid or an error
 * occurs during the determination process, the function returns a value less than or equal to 0.
 */
int determine_FAT_bits(const struct fat_boot_sector *sector, bool verbosity);
