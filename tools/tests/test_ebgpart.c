/*
 * EFI Boot Guard
 *
 * Copyright (c) Siemens AG, 2026
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * SPDX-License-Identifier:	GPL-2.0-only
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <check.h>

#include <ebgpart.h>
#include <fat.h>

Suite *ebg_test_suite(void);

#define IMAGE_SECTORS	2048
#define GPT_ENTRIES_LBA	2

struct expected_part {
	uint16_t num;
	EbgFileSystemType fs_type;
};

/* 0FC63DAF-8483-4772-8E79-3D69D8477DE4 */
static const uint8_t linux_fs_GUID[16] = {
	0xaf, 0x3d, 0xc6, 0x0f, 0x83, 0x84, 0x72, 0x47,
	0x8e, 0x79, 0x3d, 0x69, 0xd8, 0x47, 0x7d, 0xe4,
};

/* C12A7328-F81F-11D2-BA4B-00A0C93EC93B */
static const uint8_t esp_GUID[16] = {
	0x28, 0x73, 0x2a, 0xc1, 0x1f, 0xf8, 0xd2, 0x11,
	0xba, 0x4b, 0x00, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b,
};

static uint8_t image[IMAGE_SECTORS * LB_SIZE] __attribute__((aligned(8)));

static struct Masterbootrecord *boot_record(uint32_t LBA)
{
	struct Masterbootrecord *br =
		(struct Masterbootrecord *)&image[LBA * LB_SIZE];

	br->mbrsignature = 0xaa55;
	return br;
}

static void set_mbr_entry(struct MBRentry *e, uint8_t type,
			  uint32_t start_LBA)
{
	e->partition_type = type;
	e->start_LBA = start_LBA;
	e->num_Sectors = 1;
}

static struct EFIHeader *setup_gpt(uint32_t num_entries)
{
	struct EFIHeader *hdr = (struct EFIHeader *)&image[LB_SIZE];

	memset(image, 0, sizeof(image));
	set_mbr_entry(&boot_record(0)->parttable[0], MBR_TYPE_GPT, 1);

	memcpy(hdr->signature, GPT_HEADER_SIGNATURE, sizeof(hdr->signature));
	hdr->partitiontable_LBA = GPT_ENTRIES_LBA;
	hdr->partitions = num_entries;
	hdr->partitionentrysize = sizeof(struct EFIpartitionentry);
	return hdr;
}

static void set_gpt_entry(unsigned int n, const uint8_t *type_GUID,
			  uint64_t start_LBA)
{
	struct EFIpartitionentry *e =
		(struct EFIpartitionentry *)&image[GPT_ENTRIES_LBA * LB_SIZE +
						   n * sizeof(*e)];

	memcpy(e->type_GUID, type_GUID, sizeof(e->type_GUID));
	e->start_LBA = start_LBA;
	e->end_LBA = start_LBA;
}

static void write_fat32_boot_sector(uint32_t LBA)
{
	struct fat_boot_sector *b =
		(struct fat_boot_sector *)&image[LBA * LB_SIZE];

	b->sector_size[0] = LB_SIZE & 0xff;
	b->sector_size[1] = LB_SIZE >> 8;
	b->sec_per_clus = 1;
	b->reserved = 32;
	b->fats = 2;
	b->media = 0xf8;
	b->fat32.length = 100;
}

static bool parse_image(PedDevice *dev)
{
	char path[] = "/tmp/ebgpart_XXXXXX";
	bool result;
	int fd;

	fd = mkstemp(path);
	ck_assert_int_ge(fd, 0);
	ck_assert_int_eq(write(fd, image, sizeof(image)), sizeof(image));
	close(fd);

	memset(dev, 0, sizeof(*dev));
	dev->path = path;
	result = check_partition_table(dev);
	dev->path = NULL;

	unlink(path);
	return result;
}

static void check_partitions(const PedDevice *dev,
			     const struct expected_part *expected,
			     unsigned int count)
{
	const PedPartition *p = dev->part_list;

	for (unsigned int n = 0; n < count; n++) {
		ck_assert_ptr_nonnull(p);
		ck_assert_uint_eq(p->num, expected[n].num);
		ck_assert_int_eq(p->fs_type, expected[n].fs_type);
		p = p->next;
	}
	ck_assert_ptr_null(p);
}

static void free_partitions(PedDevice *dev)
{
	PedPartition *p = dev->part_list;

	while (p) {
		PedPartition *next = p->next;

		free(p);
		p = next;
	}
	dev->part_list = NULL;
}

START_TEST(ebgpart_gpt_entry_limit)
{
	const PedPartition *p;
	unsigned int n = 0;
	PedDevice dev;

	setup_gpt(UINT32_MAX);
	for (unsigned int i = 0; i < 300; i++) {
		set_gpt_entry(i, linux_fs_GUID, 0);
	}

	ck_assert(parse_image(&dev));

	for (p = dev.part_list; p; p = p->next) {
		ck_assert_uint_eq(p->num, ++n);
	}
	ck_assert_uint_eq(n, 255);

	free_partitions(&dev);
}
END_TEST

START_TEST(ebgpart_gpt_unused_entries)
{
	const struct expected_part expected[] = {
		{ 2, FS_TYPE_UNKNOWN },
		{ 4, FS_TYPE_UNKNOWN },
	};
	PedDevice dev;

	setup_gpt(128);
	set_gpt_entry(1, linux_fs_GUID, 0);
	set_gpt_entry(3, linux_fs_GUID, 0);

	ck_assert(parse_image(&dev));
	check_partitions(&dev, expected, 2);

	free_partitions(&dev);
}
END_TEST

START_TEST(ebgpart_gpt_uninspectable_entries)
{
	const struct expected_part expected[] = {
		{ 1, FS_TYPE_UNKNOWN },
		{ 4, FS_TYPE_FAT32 },
		{ 5, FS_TYPE_UNKNOWN },
		{ 6, FS_TYPE_UNKNOWN },
	};
	PedDevice dev;

	setup_gpt(128);
	set_gpt_entry(0, linux_fs_GUID, 0);
	/* beyond the end of the device */
	set_gpt_entry(1, esp_GUID, 1000000);
	/* overflowing the file offset */
	set_gpt_entry(2, esp_GUID, UINT64_MAX);
	set_gpt_entry(3, esp_GUID, 1024);
	write_fat32_boot_sector(1024);
	/* no FAT file system */
	set_gpt_entry(4, esp_GUID, 1536);
	set_gpt_entry(5, linux_fs_GUID, 0);

	ck_assert(parse_image(&dev));
	check_partitions(&dev, expected, 4);

	free_partitions(&dev);
}
END_TEST

START_TEST(ebgpart_gpt_invalid_header)
{
	struct EFIHeader *hdr;
	PedDevice dev;

	hdr = setup_gpt(128);
	set_gpt_entry(0, linux_fs_GUID, 0);
	hdr->signature[0] = 'X';

	ck_assert(parse_image(&dev));
	ck_assert_ptr_null(dev.part_list);

	hdr = setup_gpt(128);
	set_gpt_entry(0, linux_fs_GUID, 0);
	hdr->partitionentrysize = 2 * sizeof(struct EFIpartitionentry);

	ck_assert(parse_image(&dev));
	ck_assert_ptr_null(dev.part_list);
}
END_TEST

START_TEST(ebgpart_mbr_logical_chain)
{
	const struct expected_part expected[] = {
		{ 1, FS_TYPE_EXTENDED },
		{ 5, FS_TYPE_UNKNOWN },
		{ 6, FS_TYPE_FAT32 },
		{ 7, FS_TYPE_FAT32 },
	};
	struct Masterbootrecord *ebr;
	PedDevice dev;

	memset(image, 0, sizeof(image));
	set_mbr_entry(&boot_record(0)->parttable[0], MBR_TYPE_EXTENDED, 64);

	/* EBR links are relative to the start of the extended partition */
	ebr = boot_record(64);
	set_mbr_entry(&ebr->parttable[0], 0x83, 1);
	set_mbr_entry(&ebr->parttable[1], MBR_TYPE_EXTENDED, 128);
	ebr = boot_record(64 + 128);
	set_mbr_entry(&ebr->parttable[0], MBR_TYPE_FAT32_LBA, 1);
	set_mbr_entry(&ebr->parttable[1], MBR_TYPE_EXTENDED, 256);
	ebr = boot_record(64 + 256);
	set_mbr_entry(&ebr->parttable[0], MBR_TYPE_FAT32, 1);

	ck_assert(parse_image(&dev));
	check_partitions(&dev, expected, 4);

	free_partitions(&dev);
}
END_TEST

START_TEST(ebgpart_mbr_primary_after_extended)
{
	const struct expected_part expected[] = {
		{ 1, FS_TYPE_EXTENDED },
		{ 5, FS_TYPE_FAT32 },
		{ 6, FS_TYPE_FAT32 },
		{ 2, FS_TYPE_FAT16 },
	};
	struct Masterbootrecord *mbr, *ebr;
	PedDevice dev;

	memset(image, 0, sizeof(image));
	mbr = boot_record(0);
	set_mbr_entry(&mbr->parttable[0], MBR_TYPE_EXTENDED, 64);
	set_mbr_entry(&mbr->parttable[1], MBR_TYPE_FAT16, 1024);

	ebr = boot_record(64);
	set_mbr_entry(&ebr->parttable[0], MBR_TYPE_FAT32_LBA, 1);
	set_mbr_entry(&ebr->parttable[1], MBR_TYPE_EXTENDED, 128);
	ebr = boot_record(64 + 128);
	set_mbr_entry(&ebr->parttable[0], MBR_TYPE_FAT32, 1);

	ck_assert(parse_image(&dev));
	check_partitions(&dev, expected, 4);

	free_partitions(&dev);
}
END_TEST

START_TEST(ebgpart_mbr_empty_extended)
{
	const struct expected_part expected[] = {
		{ 1, FS_TYPE_EXTENDED },
		{ 2, FS_TYPE_FAT32 },
	};
	struct Masterbootrecord *mbr;
	PedDevice dev;

	/* no EBR in the extended partition */
	memset(image, 0, sizeof(image));
	mbr = boot_record(0);
	set_mbr_entry(&mbr->parttable[0], MBR_TYPE_EXTENDED, 64);
	set_mbr_entry(&mbr->parttable[1], MBR_TYPE_FAT32, 1024);

	ck_assert(parse_image(&dev));
	check_partitions(&dev, expected, 2);

	free_partitions(&dev);
}
END_TEST

START_TEST(ebgpart_mbr_logical_unused_entries)
{
	const struct expected_part expected[] = {
		{ 1, FS_TYPE_EXTENDED },
		{ 5, FS_TYPE_FAT32 },
		{ 6, FS_TYPE_FAT32 },
	};
	struct Masterbootrecord *ebr;
	PedDevice dev;

	memset(image, 0, sizeof(image));
	set_mbr_entry(&boot_record(0)->parttable[0], MBR_TYPE_EXTENDED, 64);

	/* no logical partition, only a link */
	ebr = boot_record(64);
	set_mbr_entry(&ebr->parttable[1], MBR_TYPE_EXTENDED, 128);
	/* garbage in the unused entries */
	ebr = boot_record(64 + 128);
	set_mbr_entry(&ebr->parttable[0], MBR_TYPE_FAT32_LBA, 1);
	set_mbr_entry(&ebr->parttable[1], MBR_TYPE_EXTENDED, 256);
	set_mbr_entry(&ebr->parttable[2], MBR_TYPE_FAT16, 1);
	set_mbr_entry(&ebr->parttable[3], MBR_TYPE_EXTENDED, 0);
	ebr = boot_record(64 + 256);
	set_mbr_entry(&ebr->parttable[0], MBR_TYPE_FAT32, 1);

	ck_assert(parse_image(&dev));
	check_partitions(&dev, expected, 3);

	free_partitions(&dev);
}
END_TEST

START_TEST(ebgpart_mbr_logical_loop)
{
	const PedPartition *p;
	struct Masterbootrecord *ebr;
	unsigned int n = 0;
	PedDevice dev;

	memset(image, 0, sizeof(image));
	set_mbr_entry(&boot_record(0)->parttable[0], MBR_TYPE_EXTENDED, 64);

	/* logical partition and link back to the same EBR */
	ebr = boot_record(64);
	set_mbr_entry(&ebr->parttable[0], MBR_TYPE_FAT32_LBA, 1);
	set_mbr_entry(&ebr->parttable[1], MBR_TYPE_EXTENDED, 0);

	ck_assert(parse_image(&dev));

	p = dev.part_list;
	ck_assert_ptr_nonnull(p);
	ck_assert_uint_eq(p->num, 1);
	ck_assert_int_eq(p->fs_type, FS_TYPE_EXTENDED);
	for (p = p->next; p; p = p->next) {
		ck_assert_uint_eq(p->num, 5 + n++);
		ck_assert_int_eq(p->fs_type, FS_TYPE_FAT32);
	}
	ck_assert_uint_eq(n, 251);

	free_partitions(&dev);
}
END_TEST

START_TEST(ebgpart_mbr_link_loop)
{
	const struct expected_part expected[] = {
		{ 1, FS_TYPE_EXTENDED },
	};
	struct Masterbootrecord *ebr;
	PedDevice dev;

	memset(image, 0, sizeof(image));
	set_mbr_entry(&boot_record(0)->parttable[0], MBR_TYPE_EXTENDED, 64);

	/* only a link back to the same EBR */
	ebr = boot_record(64);
	set_mbr_entry(&ebr->parttable[1], MBR_TYPE_EXTENDED, 0);

	ck_assert(parse_image(&dev));
	check_partitions(&dev, expected, 1);

	free_partitions(&dev);
}
END_TEST

Suite *ebg_test_suite(void)
{
	Suite *s;
	TCase *tc_core;

	s = suite_create("ebgpart");

	tc_core = tcase_create("Core");

	tcase_add_test(tc_core, ebgpart_gpt_entry_limit);
	tcase_add_test(tc_core, ebgpart_gpt_unused_entries);
	tcase_add_test(tc_core, ebgpart_gpt_uninspectable_entries);
	tcase_add_test(tc_core, ebgpart_gpt_invalid_header);
	tcase_add_test(tc_core, ebgpart_mbr_logical_chain);
	tcase_add_test(tc_core, ebgpart_mbr_primary_after_extended);
	tcase_add_test(tc_core, ebgpart_mbr_empty_extended);
	tcase_add_test(tc_core, ebgpart_mbr_logical_unused_entries);
	tcase_add_test(tc_core, ebgpart_mbr_logical_loop);
	tcase_add_test(tc_core, ebgpart_mbr_link_loop);

	suite_add_tcase(s, tc_core);

	return s;
}
