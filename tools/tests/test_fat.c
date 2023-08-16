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
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <stdlib.h>
#include <check.h>
#include <fff.h>

#include <env_api.h>
#include <uservars.h>
#include <bg_envtools.h>
#include <fat.h>
#include <linux_util.h>

DEFINE_FFF_GLOBALS;

Suite *ebg_test_suite(void);

static inline void u16_to_le(u16 value, __u8 out[2]) {
    out[0] = (value >> 0) & 0xFF;
    out[1] = (value >> 8) & 0xFF;
}

START_TEST(test_determine_FAT_bits_empty)
{
	struct fat_boot_sector sector;
	memset(&sector, 0, sizeof(sector));
	int ret = determine_FAT_bits(&sector);
	ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_determine_FAT_bits_sec_per_clus_zero)
{
	struct fat_boot_sector sector = {
		.sec_per_clus = 0, /* test we do not divide by this value */
		.reserved = 42,
		.fats = 16,
		.media = 0xf8,
	};
	u16_to_le(512, sector.sector_size);
	int ret = determine_FAT_bits(&sector);
	ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_determine_FAT_bits_fat_sector_size_zero)
{
	struct fat_boot_sector sector = {
		.sector_size = 0, /* test we do not divide by this value */
		.sec_per_clus = 32,
		.reserved = 42,
		.fats = 16,
		.media = 0xf8,
	};
	int ret = determine_FAT_bits(&sector);
	ck_assert_int_eq(ret, 0);
}
END_TEST

Suite *ebg_test_suite(void)
{
	Suite *s;
	TCase *tc_core;

	s = suite_create("fat");

	tc_core = tcase_create("Core");

	tcase_add_test(tc_core, test_determine_FAT_bits_empty);
	tcase_add_test(tc_core, test_determine_FAT_bits_sec_per_clus_zero);
	tcase_add_test(tc_core, test_determine_FAT_bits_fat_sector_size_zero);

	suite_add_tcase(s, tc_core);

	return s;
}
