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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <linux/types.h>
#include <linux/byteorder/little_endian.h>

#include "fat.h"
#include "linux_util.h"
#include "ebgpart.h"

#define fat_msg(sb, lvl, ...)                                                  \
	do {                                                                   \
		fprintf(stderr, __VA_ARGS__);                                  \
		fprintf(stderr, "\n");                                         \
	} while (0);
#define KERN_ERR "ERROR"

/******************************************************************************
 * The below code was adopted from the Linux kernel:
 *
 * https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/fs/fat/inode.c?id=5d0c230f1de8c7515b6567d9afba1f196fb4e2f4
 *
 * The modifications were kept to a minimum to make it easy to sync these files.
 */

/*
 * A deserialized copy of the on-disk structure laid out in struct
 * fat_boot_sector.
 */
struct fat_bios_param_block {
	u16	fat_sector_size;
	u8	fat_sec_per_clus;
	u16	fat_reserved;
	u8	fat_fats;
	u16	fat_dir_entries;
	u16	fat_sectors;
	u16	fat_fat_length;
	u32	fat_total_sect;

	u8	fat16_state;
	u32	fat16_vol_id;

	u32	fat32_length;
	u32	fat32_root_cluster;
	u16	fat32_info_sector;
	u8	fat32_state;
	u32	fat32_vol_id;
};

/* media of boot sector */
static inline int fat_valid_media(u8 media)
{
	return 0xf8 <= media || media == 0xf0;
}

static int fat_read_bpb(void __attribute__((unused)) *sb,
	const struct fat_boot_sector *b, int silent,
	struct fat_bios_param_block *bpb)
{
	int error = -EINVAL;

	/* Read in BPB ... */
	memset(bpb, 0, sizeof(*bpb));
	bpb->fat_sector_size = get_unaligned_le16(&b->sector_size);
	bpb->fat_sec_per_clus = b->sec_per_clus;
	bpb->fat_reserved = le16_to_cpu(b->reserved);
	bpb->fat_fats = b->fats;
	bpb->fat_dir_entries = get_unaligned_le16(&b->dir_entries);
	bpb->fat_sectors = get_unaligned_le16(&b->sectors);
	bpb->fat_fat_length = le16_to_cpu(b->fat_length);
	bpb->fat_total_sect = le32_to_cpu(b->total_sect);

	bpb->fat16_state = b->fat16.state;
	bpb->fat16_vol_id = get_unaligned_le32(b->fat16.vol_id);

	bpb->fat32_length = le32_to_cpu(b->fat32.length);
	bpb->fat32_root_cluster = le32_to_cpu(b->fat32.root_cluster);
	bpb->fat32_info_sector = le16_to_cpu(b->fat32.info_sector);
	bpb->fat32_state = b->fat32.state;
	bpb->fat32_vol_id = get_unaligned_le32(b->fat32.vol_id);

	/* Validate this looks like a FAT filesystem BPB */
	if (!bpb->fat_reserved) {
		if (!silent)
			fat_msg(sb, KERN_ERR,
				"bogus number of reserved sectors");
		goto out;
	}
	if (!bpb->fat_fats) {
		if (!silent)
			fat_msg(sb, KERN_ERR, "bogus number of FAT structure");
		goto out;
	}

	/*
	 * Earlier we checked here that b->secs_track and b->head are nonzero,
	 * but it turns out valid FAT filesystems can have zero there.
	 */

	if (!fat_valid_media(b->media)) {
		if (!silent)
			fat_msg(sb, KERN_ERR, "invalid media value (0x%02x)",
				(unsigned)b->media);
		goto out;
	}

	if (!is_power_of_2(bpb->fat_sector_size)
	    || (bpb->fat_sector_size < 512)
	    || (bpb->fat_sector_size > 4096)) {
		if (!silent)
			fat_msg(sb, KERN_ERR, "bogus logical sector size %u",
			       (unsigned)bpb->fat_sector_size);
		goto out;
	}

	if (!is_power_of_2(bpb->fat_sec_per_clus)) {
		if (!silent)
			fat_msg(sb, KERN_ERR, "bogus sectors per cluster %u",
				(unsigned)bpb->fat_sec_per_clus);
		goto out;
	}

	if (bpb->fat_fat_length == 0 && bpb->fat32_length == 0) {
		if (!silent)
			fat_msg(sb, KERN_ERR, "bogus number of FAT sectors");
		goto out;
	}

	error = 0;

out:
	return error;
}

 /* end of Linux kernel code */
 /*****************************************************************************/

int determine_FAT_bits(const struct fat_boot_sector *sector, bool verbosity)
{
	struct fat_bios_param_block bpb;
	if (fat_read_bpb(NULL, sector, !verbosity, &bpb)) {
		return 0;
	}
	/*
	 * at this point, the following assertions are true:
	 * bpb.fat_sec_per_clus > 0
	 * bpb.fat_sector_size > 0
	 */

	/* based on fat_fill_super() from the Linux kernel's fs/fat/inode.c */
	if (!bpb.fat_fat_length && bpb.fat32_length) {
		return 32;
	} else {
		unsigned short fat_start = bpb.fat_reserved;
		unsigned char fats = bpb.fat_fats;
		unsigned short fat_length = bpb.fat_fat_length;
		unsigned long dir_start = fat_start + fats * fat_length;
		unsigned short dir_entries = bpb.fat_dir_entries;
		unsigned long blocksize = bpb.fat_sector_size;
		u32 total_sectors = bpb.fat_sectors;
		if (total_sectors == 0) {
			total_sectors = bpb.fat_total_sect;
		}
		unsigned short sec_per_clus = bpb.fat_sec_per_clus;
		u32 rootdir_sectors = dir_entries *
				      sizeof(struct msdos_dir_entry) /
				      blocksize;
		unsigned long data_start = dir_start + rootdir_sectors;
		u32 total_clusters =
			(total_sectors - data_start) / sec_per_clus;
		return (total_clusters > MAX_FAT12) ? 16 : 12;
	}
}
