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

START_TEST(test_determine_FAT_bits_12)
{
	/* generated using mkfs.vfat */
	const unsigned char sector[] = {
		0xeb, 0x3c, 0x90, 0x6d, 0x6b, 0x66, 0x73, 0x2e, 0x66, 0x61,
		0x74, 0x00, 0x02, 0x40, 0x40, 0x00, 0x02, 0x00, 0x04, 0x00,
		0x00, 0xf8, 0x40, 0x00, 0x20, 0x00, 0x08, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x20, 0x03, 0x00, 0x80, 0x00, 0x29, 0x80,
		0xae, 0xcd, 0x62, 0x4e, 0x4f, 0x20, 0x4e, 0x41, 0x4d, 0x45,
		0x20, 0x20, 0x20, 0x20, 0x46, 0x41, 0x54, 0x31, 0x32, 0x20,
		0x20, 0x20, 0x0e, 0x1f, 0xbe, 0x5b, 0x7c, 0xac, 0x22, 0xc0,
		0x74, 0x0b, 0x56, 0xb4, 0x0e, 0xbb, 0x07, 0x00, 0xcd, 0x10,
		0x5e, 0xeb, 0xf0, 0x32, 0xe4, 0xcd, 0x16, 0xcd, 0x19, 0xeb,
		0xfe, 0x54, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x6e,
		0x6f, 0x74, 0x20, 0x61, 0x20, 0x62, 0x6f, 0x6f, 0x74, 0x61,
		0x62, 0x6c, 0x65, 0x20, 0x64, 0x69, 0x73, 0x6b, 0x2e, 0x20,
		0x20, 0x50, 0x6c, 0x65, 0x61, 0x73, 0x65, 0x20, 0x69, 0x6e,
		0x73, 0x65, 0x72, 0x74, 0x20, 0x61, 0x20, 0x62, 0x6f, 0x6f,
		0x74, 0x61, 0x62, 0x6c, 0x65, 0x20, 0x66, 0x6c, 0x6f, 0x70,
		0x70, 0x79, 0x20, 0x61, 0x6e, 0x64, 0x0d, 0x0a, 0x70, 0x72,
		0x65, 0x73, 0x73, 0x20, 0x61, 0x6e, 0x79, 0x20, 0x6b, 0x65,
		0x79, 0x20, 0x74, 0x6f, 0x20, 0x74, 0x72, 0x79, 0x20, 0x61,
		0x67, 0x61, 0x69, 0x6e, 0x20, 0x2e, 0x2e, 0x2e, 0x20, 0x0d,
		0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x55, 0xaa,
	};
	int ret = determine_FAT_bits((const struct fat_boot_sector*) &sector);
	ck_assert_int_eq(ret, 12);
}
END_TEST

START_TEST(test_determine_FAT_bits_16)
{
	/* generated using mkfs.vfat */
	const unsigned char sector[] = {
		0xeb, 0x3c, 0x90, 0x6d, 0x6b, 0x66, 0x73, 0x2e, 0x66, 0x61,
		0x74, 0x00, 0x02, 0x04, 0x04, 0x00, 0x02, 0x00, 0x02, 0x00,
		0x00, 0xf8, 0xc8, 0x00, 0x20, 0x00, 0x08, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x20, 0x03, 0x00, 0x80, 0x00, 0x29, 0xe8,
		0x0b, 0x4a, 0x64, 0x4e, 0x4f, 0x20, 0x4e, 0x41, 0x4d, 0x45,
		0x20, 0x20, 0x20, 0x20, 0x46, 0x41, 0x54, 0x31, 0x36, 0x20,
		0x20, 0x20, 0x0e, 0x1f, 0xbe, 0x5b, 0x7c, 0xac, 0x22, 0xc0,
		0x74, 0x0b, 0x56, 0xb4, 0x0e, 0xbb, 0x07, 0x00, 0xcd, 0x10,
		0x5e, 0xeb, 0xf0, 0x32, 0xe4, 0xcd, 0x16, 0xcd, 0x19, 0xeb,
		0xfe, 0x54, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x6e,
		0x6f, 0x74, 0x20, 0x61, 0x20, 0x62, 0x6f, 0x6f, 0x74, 0x61,
		0x62, 0x6c, 0x65, 0x20, 0x64, 0x69, 0x73, 0x6b, 0x2e, 0x20,
		0x20, 0x50, 0x6c, 0x65, 0x61, 0x73, 0x65, 0x20, 0x69, 0x6e,
		0x73, 0x65, 0x72, 0x74, 0x20, 0x61, 0x20, 0x62, 0x6f, 0x6f,
		0x74, 0x61, 0x62, 0x6c, 0x65, 0x20, 0x66, 0x6c, 0x6f, 0x70,
		0x70, 0x79, 0x20, 0x61, 0x6e, 0x64, 0x0d, 0x0a, 0x70, 0x72,
		0x65, 0x73, 0x73, 0x20, 0x61, 0x6e, 0x79, 0x20, 0x6b, 0x65,
		0x79, 0x20, 0x74, 0x6f, 0x20, 0x74, 0x72, 0x79, 0x20, 0x61,
		0x67, 0x61, 0x69, 0x6e, 0x20, 0x2e, 0x2e, 0x2e, 0x20, 0x0d,
		0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x55, 0xaa,
	};
	int ret = determine_FAT_bits((const struct fat_boot_sector*) &sector);
	ck_assert_int_eq(ret, 16);
}
END_TEST

START_TEST(test_determine_FAT_bits_32)
{
	/* generated using mkfs.vfat */
	const unsigned char sector[] = {
		0xeb, 0x58, 0x90, 0x6d, 0x6b, 0x66, 0x73, 0x2e, 0x66, 0x61,
		0x74, 0x00, 0x02, 0x01, 0x20, 0x00, 0x02, 0x00, 0x00, 0x00,
		0x00, 0xf8, 0x00, 0x00, 0x20, 0x00, 0x08, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x20, 0x03, 0x00, 0x28, 0x06, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00,
		0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x29, 0x2d, 0x2e, 0xb5,
		0x64, 0x4e, 0x4f, 0x20, 0x4e, 0x41, 0x4d, 0x45, 0x20, 0x20,
		0x20, 0x20, 0x46, 0x41, 0x54, 0x33, 0x32, 0x20, 0x20, 0x20,
		0x0e, 0x1f, 0xbe, 0x77, 0x7c, 0xac, 0x22, 0xc0, 0x74, 0x0b,
		0x56, 0xb4, 0x0e, 0xbb, 0x07, 0x00, 0xcd, 0x10, 0x5e, 0xeb,
		0xf0, 0x32, 0xe4, 0xcd, 0x16, 0xcd, 0x19, 0xeb, 0xfe, 0x54,
		0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x6e, 0x6f, 0x74,
		0x20, 0x61, 0x20, 0x62, 0x6f, 0x6f, 0x74, 0x61, 0x62, 0x6c,
		0x65, 0x20, 0x64, 0x69, 0x73, 0x6b, 0x2e, 0x20, 0x20, 0x50,
		0x6c, 0x65, 0x61, 0x73, 0x65, 0x20, 0x69, 0x6e, 0x73, 0x65,
		0x72, 0x74, 0x20, 0x61, 0x20, 0x62, 0x6f, 0x6f, 0x74, 0x61,
		0x62, 0x6c, 0x65, 0x20, 0x66, 0x6c, 0x6f, 0x70, 0x70, 0x79,
		0x20, 0x61, 0x6e, 0x64, 0x0d, 0x0a, 0x70, 0x72, 0x65, 0x73,
		0x73, 0x20, 0x61, 0x6e, 0x79, 0x20, 0x6b, 0x65, 0x79, 0x20,
		0x74, 0x6f, 0x20, 0x74, 0x72, 0x79, 0x20, 0x61, 0x67, 0x61,
		0x69, 0x6e, 0x20, 0x2e, 0x2e, 0x2e, 0x20, 0x0d, 0x0a, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x55, 0xaa,
	};
	int ret = determine_FAT_bits((const struct fat_boot_sector*) &sector);
	ck_assert_int_eq(ret, 32);
}
END_TEST

START_TEST(test_determine_FAT_bits_fat16_swupdate)
{
	/* generated using SWUpdate's fat_mkfs; this is an interesting test case
	 * because the BS_FilSysType field is "FAT     " and does NOT contain
	 * the FAT bit size (which mkfs.vfat does). */
	const unsigned char sector[] = {
		0xeb, 0xfe, 0x90, 0x4d, 0x53, 0x44, 0x4f, 0x53, 0x35, 0x2e,
		0x30, 0x00, 0x02, 0x04, 0x01, 0x00, 0x01, 0x00, 0x02, 0x00,
		0x50, 0xf8, 0x15, 0x00, 0x3f, 0x00, 0xff, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x29, 0x54,
		0x55, 0x11, 0x57, 0x4e, 0x4f, 0x20, 0x4e, 0x41, 0x4d, 0x45,
		0x20, 0x20, 0x20, 0x20, 0x46, 0x41, 0x54, 0x20, 0x20, 0x20,
		0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x55, 0xaa,
	};
	int ret = determine_FAT_bits((const struct fat_boot_sector*) &sector);
	ck_assert_int_eq(ret, 16);
}
END_TEST

START_TEST(test_determine_FAT_bits_squashfs)
{
	const unsigned char sector[] = {
		0x68, 0x73, 0x71, 0x73, 0x1a, 0x2f, 0x00, 0x00, 0x86, 0xaa,
		0xc3, 0x64, 0x00, 0x00, 0x02, 0x00, 0xe3, 0x02, 0x00, 0x00,
		0x01, 0x00, 0x11, 0x00, 0xc0, 0x00, 0x09, 0x00, 0x04, 0x00,
		0x00, 0x00, 0xa4, 0x1d, 0x34, 0xd8, 0x01, 0x00, 0x00, 0x00,
		0x2c, 0xd7, 0xf1, 0x06, 0x00, 0x00, 0x00, 0x00, 0xd6, 0xd6,
		0xf1, 0x06, 0x00, 0x00, 0x00, 0x00, 0x14, 0xd7, 0xf1, 0x06,
		0x00, 0x00, 0x00, 0x00, 0x85, 0xd0, 0xed, 0x06, 0x00, 0x00,
		0x00, 0x00, 0x0e, 0xb2, 0xef, 0x06, 0x00, 0x00, 0x00, 0x00,
		0x0d, 0x87, 0xf1, 0x06, 0x00, 0x00, 0x00, 0x00, 0x53, 0xd6,
		0xf1, 0x06, 0x00, 0x00, 0x00, 0x00, 0x78, 0xda, 0x94, 0xbd,
		0xdb, 0x8e, 0xec, 0x3a, 0x8e, 0x36, 0x78, 0x3f, 0x4f, 0x51,
		0x4f, 0xd0, 0x08, 0x9f, 0xed, 0xcb, 0x42, 0xff, 0x73, 0x51,
		0xf8, 0x07, 0x98, 0x06, 0xba, 0x30, 0xff, 0xa5, 0x21, 0xcb,
		0x72, 0xa6, 0x2b, 0x23, 0xc2, 0xb1, 0xed, 0x88, 0x3c, 0xd4,
		0xd3, 0x8f, 0x48, 0xca, 0x07, 0x51, 0x94, 0x73, 0xf5, 0x02,
		0xf6, 0x5a, 0xb9, 0x53, 0x9f, 0xa9, 0x13, 0x25, 0x51, 0x14,
		0x0f, 0x97, 0x8b, 0xff, 0xe7, 0x6f, 0xff, 0xeb, 0x6f, 0x6d,
		0xfb, 0x30, 0x73, 0xab, 0x1f, 0xaf, 0x76, 0x79, 0xaa, 0xf9,
		0xf9, 0x7f, 0x5d, 0x42, 0xc8, 0x30, 0x7e, 0x9b, 0x1e, 0x60,
		0x80, 0xea, 0xd5, 0x53, 0xf9, 0xa0, 0xa4, 0x37, 0x7f, 0xfb,
		0xfb, 0xdf, 0x3e, 0xcc, 0xb7, 0xd1, 0xad, 0x9e, 0xee, 0xcf,
		0x79, 0xba, 0xda, 0x7f, 0x7b, 0xd3, 0x2e, 0xe3, 0xbf, 0x8d,
		0x07, 0x4d, 0x88, 0x1e, 0x92, 0x31, 0xdd, 0xeb, 0xcd, 0x56,
		0x39, 0xcd, 0x3e, 0x24, 0x25, 0xc8, 0x38, 0xff, 0x05, 0xed,
		0xd1, 0x1f, 0x6d, 0x67, 0xff, 0x1a, 0xef, 0x12, 0xb4, 0xdc,
		0xa9, 0x3d, 0x97, 0xa5, 0x9d, 0xbf, 0xbc, 0xd2, 0x8e, 0x4a,
		0xdf, 0xfa, 0x67, 0xfb, 0x50, 0x6f, 0xfe, 0x97, 0x1a, 0xca,
		0xfa, 0xbf, 0x99, 0x6f, 0x6d, 0x1e, 0xcf, 0x71, 0xba, 0x53,
		0x55, 0xcb, 0x11, 0x93, 0xe4, 0x0e, 0x63, 0xfb, 0xf3, 0xe3,
		0x9a, 0x02, 0x4d, 0x60, 0xa4, 0x92, 0x82, 0xaa, 0x31, 0xcb,
		0xc3, 0x8e, 0x52, 0xfb, 0xa5, 0xfa, 0x7e, 0xe6, 0xe5, 0xf5,
		0x5e, 0x8e, 0x74, 0x58, 0x79, 0xb2, 0x76, 0xe2, 0x7a, 0xd5,
		0xed, 0xd8, 0xb3, 0xd2, 0x14, 0x1a, 0x71, 0xd3, 0xc6, 0x8e,
		0xc3, 0xfd, 0x63, 0x69, 0xd5, 0x3c, 0xab, 0x1f, 0x1f, 0x92,
		0xa7, 0x40, 0x00, 0x20, 0xf7, 0xd7, 0x8d, 0x60, 0x0c, 0x90,
		0x1f, 0x6b, 0x58, 0xde, 0xd5, 0x6c, 0x67, 0xf3, 0xa6, 0x1e,
		0x1c, 0x55, 0xaf, 0x53, 0x33, 0x1a, 0xa1, 0xb8, 0x58, 0x89,
		0x68, 0x3b, 0x0f, 0x52, 0xf9, 0xfa, 0xf9, 0x32, 0x76, 0x57,
		0x98, 0xb0, 0x10, 0x52, 0xae, 0x24, 0xc6, 0xfb, 0x30, 0xf9,
		0x65, 0xc5, 0x56, 0x66, 0x3b, 0xd1, 0x99, 0x99, 0x97, 0x02,
		0xf1, 0xe7, 0xfb, 0xb8, 0x20, 0xa3, 0x4e, 0xc3, 0xc0, 0xca,
		0x2b, 0xf8, 0xfa, 0xbb, 0x2e, 0xdb, 0x6e, 0x9c, 0x08, 0xa3,
		0x1e, 0xa3, 0xe6, 0x83, 0x59, 0x54, 0xb9, 0x83, 0x21, 0xcf,
		0x4c, 0xad, 0xd2, 0x8f, 0x31, 0x04, 0xd5, 0x0c, 0x24, 0x52,
		0xd2, 0x16, 0xb4, 0xe8, 0x77, 0x3b, 0x92, 0x38, 0x1e, 0x8f,
		0x79, 0x9c, 0xe6, 0xf1, 0xc9, 0xa6, 0xa6, 0xa8, 0x61, 0xf6,
		0x70, 0xe4, 0xa7, 0xe9, 0xb1, 0xe0, 0x4a, 0xfb, 0xd7, 0x38,
		0x0c, 0x1c, 0xa6, 0x00, 0xf6, 0xb8, 0x69, 0x4b, 0xc6, 0x7c,
		0xb6, 0x57, 0x33, 0x3c, 0x7d, 0x40, 0xa5, 0xd6, 0xd1, 0x79,
		0xff, 0x6a, 0xcd, 0xa7, 0xe5, 0x49, 0x7f, 0x8a, 0x4b, 0xd5,
		0xd7, 0x40, 0xc1, 0xcc, 0x43, 0x7b, 0xbf, 0x8d, 0x76, 0x39,
		0x3c, 0xd5,
	};
	int ret = determine_FAT_bits((const struct fat_boot_sector *)&sector);
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
	tcase_add_test(tc_core, test_determine_FAT_bits_12);
	tcase_add_test(tc_core, test_determine_FAT_bits_16);
	tcase_add_test(tc_core, test_determine_FAT_bits_32);
	tcase_add_test(tc_core, test_determine_FAT_bits_fat16_swupdate);
	tcase_add_test(tc_core, test_determine_FAT_bits_squashfs);

	suite_add_tcase(s, tc_core);

	return s;
}
