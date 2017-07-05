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
 *
 * This code implements functions to scan for FAT partitions in DOS/GPT
 * partition tables.
 */

#ifndef __EBGPART_H__
#define __EBGPART_H__

#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef VERBOSE
#define VERBOSE(o, ...)                                                        \
	if (verbosity) fprintf(o, __VA_ARGS__)
#endif

#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>

#define DEVDIRNAME "/sys/block"

#define LB_SIZE 512

#define MBR_TYPE_GPT 0xEE
#define MBR_TYPE_FAT12 0x01
#define MBR_TYPE_FAT16A 0x04
#define MBR_TYPE_FAT16 0x06
#define MBR_TYPE_EXTENDED 0x05
#define MBR_TYPE_FAT32 0x0B
#define MBR_TYPE_FAT32_LBA 0x0C
#define MBR_TYPE_FAT16_LBA 0x0E
#define MBR_TYPE_EXTENDED_LBA 0x0F

#define GPT_PARTITION_GUID_FAT_NTFS "EBD0A0A2-B9E5-4433-87C0-68B6B72699C7"
#define GPT_PARTITION_GUID_ESP "C12A7328-F81F-11D2-BA4B-00A0C93EC93B"

#pragma pack(push)
#pragma pack(1)
struct MBRentry {
	uint8_t boot_flag;
	uint8_t first_sector_chs[3];
	uint8_t partition_type;
	uint8_t last_sector_chs[3];
	uint32_t start_LBA;
	uint32_t num_Sectors;
};
struct Masterbootrecord {
	char bootloader[0x1B8];
	char devsignature[4];
	char mbr_padding[2];
	struct MBRentry parttable[4];
	uint16_t mbrsignature;
};
struct EFIHeader {
	char signature[8];
	uint32_t revision;
	uint32_t header_size;
	uint32_t header_crc32;
	uint32_t reserved;
	uint64_t this_LBA;
	uint64_t backup_LBA;
	uint64_t firstentry_LBA;
	uint64_t lastentry_LBA;
	uint8_t GUID[16];
	uint64_t partitiontable_LBA;
	uint32_t partitions;
	uint32_t partitionentrysize;
	uint32_t partitiontable_CRC32;
	uint32_t reserved2[420];
};
struct EFIpartitionentry {
	uint8_t type_GUID[16];
	uint8_t partition_GUID[16];
	uint64_t start_LBA;
	uint64_t end_LBA;
	uint64_t attribute;
	uint16_t name[36];
};
#pragma pack(pop)

/* Implementing a minimalistic API replacing used libparted functions */
typedef struct _PedFileSystemType {
	char *name;
} PedFileSystemType;

typedef struct _PedPartition {
	PedFileSystemType *fs_type;
	uint16_t num;
	struct _PedPartition *next;
} PedPartition;

typedef struct _PedDevice {
	char *model;
	char *path;
	PedPartition *part_list;
	struct _PedDevice *next;
} PedDevice;

typedef struct _PedDisk {
	PedPartition *part_list;
} PedDisk;

void ped_device_probe_all();
PedDevice *ped_device_get_next(const PedDevice *dev);
PedDisk *ped_disk_new(const PedDevice *dev);
PedPartition *ped_disk_next_partition(const PedDisk *pd,
				      const PedPartition *part);

void ebgpart_beverbose(bool v);

#endif // __EBGPART_H__
