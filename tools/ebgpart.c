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

#include "ebgpart.h"

static PedDevice *first_device = NULL;
static PedDisk g_ped_dummy_disk;
static char buffer[37];

static bool verbosity = false;

void ebgpart_beverbose(bool v)
{
	verbosity = v;
}

void add_block_dev(PedDevice *dev)
{
	if (!first_device) {
		first_device = dev;
		return;
	}
	PedDevice *d = first_device;
	while (d->next) {
		d = d->next;
	}
	d->next = dev;
}

char *GUID_to_str(uint8_t *g)
{
	snprintf(buffer, 37, "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%"
			     "02X%02X%02X%02X%02X",
		 g[3], g[2], g[1], g[0], g[5], g[4], g[7], g[6], g[8], g[9],
		 g[10], g[11], g[12], g[13], g[14], g[15]);
	return buffer;
}

char *type_to_name(char t)
{
	switch (t) {
	case MBR_TYPE_FAT12:
		return "fat12";
		break;
	case MBR_TYPE_FAT16A:
	case MBR_TYPE_FAT16:
	case MBR_TYPE_FAT16_LBA:
		return "fat16";
		break;
	case MBR_TYPE_FAT32:
	case MBR_TYPE_FAT32_LBA:
		return "fat32";
		break;
	case MBR_TYPE_EXTENDED_LBA:
	case MBR_TYPE_EXTENDED:
		return "extended";
		break;
	default:
		return "not supported";
		break;
	}
}

bool check_GPT_FAT_entry(int fd, struct EFIpartitionentry *e,
			 PedFileSystemType *pfst, uint32_t i)
{
	if (strcmp(GPT_PARTITION_GUID_FAT_NTFS, GUID_to_str(e->type_GUID)) !=
		0 &&
	    strcmp(GPT_PARTITION_GUID_ESP, GUID_to_str(e->type_GUID)) != 0) {
		asprintf(&pfst->name, "not supported");
		return true;
	}
	VERBOSE(stdout, "GPT Partition #%u is FAT/NTFS.\n", i);
	/* Save current file offset */
	off64_t curr = lseek64(fd, 0, SEEK_CUR);
	if (curr == -1) {
		VERBOSE(stderr, "Error getting current seek position: %s\n",
			strerror(errno));
		return false;
	}
	/* Look if it is a FAT12 or FAT16 */
	off64_t dest = e->start_LBA * LB_SIZE + 0x36;
	if (lseek64(fd, dest, SEEK_SET) == -1) {
		VERBOSE(stderr, "Error seeking FAT12/16 Id String: %s\n",
			strerror(errno));
		return false;
	}
	char FAT_id[9];
	if (read(fd, FAT_id, 8) != 8) {
		VERBOSE(stderr, "Error reading FAT12/16 Id String: %s\n",
			strerror(errno));
		return false;
	};
	FAT_id[8] = 0;
	if (strcmp(FAT_id, "FAT12   ") != 0 &&
	    strcmp(FAT_id, "FAT16   ") != 0) {
		/* No FAT12/16 so read ID field for FAT32 */
		dest = e->start_LBA * LB_SIZE + 0x52;
		if (lseek64(fd, dest, SEEK_SET) == -1) {
			VERBOSE(stderr, "Error seeking FAT32 Id String: %s\n",
				strerror(errno));
			return false;
		}
		if (read(fd, FAT_id, 8) != 8) {
			VERBOSE(stderr, "Error reading FAT32 Id String: %s\n",
				strerror(errno));
			return false;
		}
	}
	if (strcmp(FAT_id, "FAT12   ") == 0) {
		asprintf(&pfst->name, "fat12");
	} else if (strcmp(FAT_id, "FAT16   ") == 0) {
		asprintf(&pfst->name, "fat16");
	} else {
		asprintf(&pfst->name, "fat32");
	}
	VERBOSE(stdout, "GPT Partition #%u is %s.\n", i, pfst->name);
	if (lseek64(fd, curr, SEEK_SET) == -1) {
		VERBOSE(stderr, "Error restoring seek position (%s)",
			strerror(errno));
		return false;
	}
	return true;
}

void read_GPT_entries(int fd, uint64_t table_LBA, uint32_t num, PedDevice *dev)
{
	off64_t offset;
	struct EFIpartitionentry e;
	PedPartition *partition = NULL, *tmpp;
	PedFileSystemType *pfst = NULL;

	offset = LB_SIZE * table_LBA;
	if (lseek64(fd, offset, SEEK_SET) != offset) {
		VERBOSE(stderr, "Error seeking EFI partition table\n");
		return;
	}

	PedPartition **list_end = &dev->part_list;

	for (uint32_t i = 0; i < num; i++) {
		if (read(fd, &e, sizeof(e)) != sizeof(e)) {
			VERBOSE(stderr, "Error reading partition entry\n");
			VERBOSE(stderr, "(%s)\n", strerror(errno));
			return;
		}
		if ((*((uint64_t *)&e.type_GUID[0]) == 0) &&
		    (*((uint64_t *)&e.type_GUID[8]) == 0)) {
			return;
		}
		VERBOSE(stdout, "%u: %s\n", i, GUID_to_str(e.type_GUID));
		pfst = calloc(sizeof(PedFileSystemType), 1);
		if (!pfst) {
			VERBOSE(stderr, "Out of memory\n");
			return;
		}

		tmpp = calloc(sizeof(PedPartition), 1);
		if (!tmpp) {
			VERBOSE(stderr, "Out of memory\n");
			free(pfst);
			return;
		}
		tmpp->num = i + 1;
		tmpp->fs_type = pfst;

		if (!check_GPT_FAT_entry(fd, &e, pfst, i)) {
			if (pfst->name) free(pfst->name);
			free(pfst);
			free(tmpp);
			if (!partition) {
				dev->part_list = NULL;
			}
			continue;
		}

		*list_end = tmpp;
		list_end = &((*list_end)->next);
	}
}

void scanLogicalVolumes(int fd, off64_t extended_start_LBA,
			struct Masterbootrecord *ebr, int i,
			PedPartition *partition, int lognum)
{
	struct Masterbootrecord next_ebr;
	PedFileSystemType *pfst;

	off64_t offset = extended_start_LBA + ebr->parttable[i].start_LBA;
	if (extended_start_LBA == 0) {
		extended_start_LBA = offset;
	}
	VERBOSE(stdout, "Seeking to LBA %lld\n", offset);
	off64_t res = lseek64(fd, offset * LB_SIZE, SEEK_SET);
	if (res == -1) {
		VERBOSE(stderr, "(%s)\n", strerror(errno));
		return;
	}
	VERBOSE(stdout, "Seek returned %lld\n", res);
	if (read(fd, &next_ebr, sizeof(next_ebr)) != sizeof(next_ebr)) {
		VERBOSE(stderr, "Error reading next EBR (%s)\n",
			strerror(errno));
		return;
	}
	if (next_ebr.mbrsignature != 0xaa55) {
		VERBOSE(stderr, "Wrong signature of extended boot record.\n");
		return;
	}

	for (uint8_t j = 0; j < 4; j++) {
		uint8_t t = next_ebr.parttable[j].partition_type;
		if (t == 0) {
			return;
		}
		if (t == MBR_TYPE_EXTENDED || t == MBR_TYPE_EXTENDED_LBA) {
			VERBOSE(stdout, "Next EBR found.\n");
			scanLogicalVolumes(fd, extended_start_LBA, &next_ebr, j,
					   partition, lognum + 1);
			continue;
		}
		partition->next = calloc(sizeof(PedPartition), 1);
		if (!partition->next) {
			goto scl_out_of_mem;
		}
		pfst = calloc(sizeof(PedFileSystemType), 1);
		if (!pfst) {
			goto scl_out_of_mem;
		}
		if (asprintf(&pfst->name, type_to_name(t)) == -1) {
			goto scl_out_of_mem;
		};
		partition = partition->next;
		partition->num = lognum;
		partition->fs_type = pfst;
	}
	return;
scl_out_of_mem:
	VERBOSE(stderr, "Out of memory\n");
	if (pfst) free(pfst);
	if (partition->next) free(partition->next);
}

bool check_partition_table(PedDevice *dev)
{
	int fd;
	struct Masterbootrecord mbr;

	VERBOSE(stdout, "Checking %s\n", dev->path);
	fd = open(dev->path, O_RDONLY);
	if (fd == 0) {
		VERBOSE(stderr, "Error opening block device.\n");
		return false;
	}
	if (read(fd, &mbr, sizeof(mbr)) != sizeof(mbr)) {
		VERBOSE(stderr, "Error reading mbr on %s.\n", dev->path);
		close(fd);
		return false;
	};
	if (mbr.mbrsignature != 0xaa55) {
		VERBOSE(stderr, "Error, MBR has wrong signature.\n");
		close(fd);
		return false;
	}
	int numpartitions = 0;
	PedPartition **list_end = &dev->part_list;
	for (int i = 0; i < 4; i++) {
		if (mbr.parttable[i].partition_type == 0) {
			continue;
		}
		numpartitions++;
		VERBOSE(stdout, "Partition %d: Type %X\n", i,
			mbr.parttable[i].partition_type);
		uint8_t t = mbr.parttable[i].partition_type;
		if (t == MBR_TYPE_GPT) {
			VERBOSE(stdout, "GPT header at %X\n",
				mbr.parttable[i].start_LBA);
			off64_t offset = LB_SIZE * mbr.parttable[i].start_LBA;
			if (lseek64(fd, offset, SEEK_SET) != offset) {
				VERBOSE(stderr, "Error seeking EFI Header\n.");
				VERBOSE(stderr, "(%s)", strerror(errno));
				return false;
			}
			struct EFIHeader efihdr;
			if (read(fd, &efihdr, sizeof(efihdr)) !=
			    sizeof(efihdr)) {
				VERBOSE(stderr, "Error reading EFI Header\n.");
				VERBOSE(stderr, "(%s)", strerror(errno));
				return false;
			}
			VERBOSE(stdout, "EFI Header: %X %X %X %X %X %X %X %X\n",
				efihdr.signature[0], efihdr.signature[1],
				efihdr.signature[2], efihdr.signature[3],
				efihdr.signature[4], efihdr.signature[5],
				efihdr.signature[6], efihdr.signature[7]);
			VERBOSE(stdout, "Number of partition entries: %u\n",
				efihdr.partitions);
			VERBOSE(stdout, "Partition Table @ LBA %llu\n",
				efihdr.partitiontable_LBA);
			read_GPT_entries(fd, efihdr.partitiontable_LBA,
					 efihdr.partitions, dev);
			break;
		}
		PedFileSystemType *pfst = calloc(sizeof(PedFileSystemType), 1);
		if (!pfst) {
			goto cpt_out_of_mem;
		}

		PedPartition *tmp = calloc(sizeof(PedPartition), 1);
		if (!tmp) {
			goto cpt_out_of_mem;
		}

		tmp->num = i + 1;
		tmp->fs_type = pfst;

		*list_end = tmp;
		list_end = &((*list_end)->next);

		if (t == MBR_TYPE_EXTENDED || t == MBR_TYPE_EXTENDED_LBA) {
			asprintf(&pfst->name, "extended");
			scanLogicalVolumes(fd, 0, &mbr, i, tmp, 5);
			/* Could be we still have MBR entries after
			 * logical volumes */
			while ((*list_end)->next) {
				list_end = &((*list_end)->next);
			}
		} else {
			asprintf(&pfst->name, type_to_name(t));
		}
		continue;
	cpt_out_of_mem:
		if (pfst) free(pfst);
		if (tmp) free(tmp);
		return false;
	}
	close(fd);
	if (numpartitions == 0) {
		return false;
	}
	return true;
}

void ped_device_probe_all()
{
	struct dirent *devfile;
	char fullname[256];

	DIR *devdir = opendir(DEVDIRNAME);
	if (!devdir) {
		VERBOSE(stderr, "Could not open %s\n", DEVDIRNAME);
		return;
	}

	/* get all files from devdir */
	do {
		devfile = readdir(devdir);
		if (!devfile)
			break;
		if (strcmp(devfile->d_name, ".") == 0 ||
		    strcmp(devfile->d_name, "..") == 0)
			continue;

		snprintf(fullname, 255, "/dev/%s", devfile->d_name);
		/* This is a block device, so add it to the list*/
		PedDevice *dev = calloc(sizeof(PedDevice), 1);
		asprintf(&dev->model, "N/A");
		asprintf(&dev->path, "%s", fullname);
		if (check_partition_table(dev)) {
			add_block_dev(dev);
		} else {
			free(dev->model);
			free(dev->path);
			free(dev);
		}
	} while (devfile);

	closedir(devdir);
}

void ped_partition_destroy(PedPartition *p)
{
	if (!p) return;
	if (!p->fs_type) goto fs_type_Null;
	if (p->fs_type->name) free(p->fs_type->name);
	free(p->fs_type);
fs_type_Null:
	free(p);
}

void ped_device_destroy(PedDevice *d)
{
	if (!d) return;
	if (d->model) free(d->model);
	if (d->path) free(d->path);
	PedPartition *p = d->part_list;
	PedPartition *tmpp;
	while (p) {
		tmpp = p;
		p = p->next;
		ped_partition_destroy(tmpp);
	}
	free(d);
}

PedDevice *ped_device_get_next(const PedDevice *dev)
{
	if (!dev) {
		return first_device;
	}
	if (dev->next != NULL) {
		return dev->next;
	}
	/* free all memory */
	PedDevice *d = first_device;
	PedDevice *tmpd;

	while (d) {
		tmpd = d;
		d = d->next;
		ped_device_destroy(tmpd);
	}
	first_device = NULL;
	return NULL;
}

PedDisk *ped_disk_new(const PedDevice *dev)
{
	g_ped_dummy_disk.part_list = dev->part_list;
	return &g_ped_dummy_disk;
}

PedPartition *ped_disk_next_partition(const PedDisk *pd,
				      const PedPartition *part)
{
	return part->next;
}
