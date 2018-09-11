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
 *
 *
 * This code implements functions to scan for FAT partitions in DOS/GPT
 * partition tables.
 */

#include "ebgpart.h"
#include <sys/sysmacros.h>

static PedDevice *first_device = NULL;
static PedDisk g_ped_dummy_disk;
static char buffer[37];

static bool verbosity = false;

void ebgpart_beverbose(bool v)
{
	verbosity = v;
}

static void add_block_dev(PedDevice *dev)
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

static char *GUID_to_str(uint8_t *g)
{
	(void)snprintf(buffer, 37,
		       "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-"
		       "%02X%02X%02X%02X%02X%02X",
		 g[3], g[2], g[1], g[0], g[5], g[4], g[7], g[6], g[8], g[9],
		 g[10], g[11], g[12], g[13], g[14], g[15]);
	return buffer;
}

static char *type_to_name(char t)
{
	switch (t) {
	case MBR_TYPE_FAT12:
		return "fat12";
	case MBR_TYPE_FAT16A:
	case MBR_TYPE_FAT16:
	case MBR_TYPE_FAT16_LBA:
		return "fat16";
	case MBR_TYPE_FAT32:
	case MBR_TYPE_FAT32_LBA:
		return "fat32";
	case MBR_TYPE_EXTENDED_LBA:
	case MBR_TYPE_EXTENDED:
		return "extended";
	}
	return "not supported";
}

static bool check_GPT_FAT_entry(int fd, struct EFIpartitionentry *e,
				PedFileSystemType *pfst, uint32_t i)
{
	if (strcmp(GPT_PARTITION_GUID_FAT_NTFS, GUID_to_str(e->type_GUID)) !=
		0 &&
	    strcmp(GPT_PARTITION_GUID_ESP, GUID_to_str(e->type_GUID)) != 0) {
		if (asprintf(&pfst->name, "%s", "not supported") == -1) {
			goto error_asprintf;
		}
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
	off64_t dest = (off64_t)e->start_LBA * LB_SIZE + 0x36;
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
		dest = (off64_t)e->start_LBA * LB_SIZE + 0x52;
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
		if (asprintf(&pfst->name, "%s", "fat12") == -1) {
			goto error_asprintf;
		}
	} else if (strcmp(FAT_id, "FAT16   ") == 0) {
		if (asprintf(&pfst->name, "%s", "fat16") == -1) {
			goto error_asprintf;
		}
	} else {
		if (asprintf(&pfst->name, "%s", "fat32") == -1) {
			goto error_asprintf;
		}
	}
	VERBOSE(stdout, "GPT Partition #%u is %s.\n", i, pfst->name);
	if (lseek64(fd, curr, SEEK_SET) == -1) {
		VERBOSE(stderr, "Error restoring seek position (%s)",
			strerror(errno));
		return false;
	}
	return true;

error_asprintf:
	VERBOSE(stderr, "Error in asprintf - possibly out of memory.\n");
	return false;
}

static void read_GPT_entries(int fd, uint64_t table_LBA, uint32_t num,
			     PedDevice *dev)
{
	off64_t offset;
	struct EFIpartitionentry e;
	PedPartition *tmpp;
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
			free(pfst->name);
			free(pfst);
			free(tmpp);
			dev->part_list = NULL;
			continue;
		}

		*list_end = tmpp;
		list_end = &((*list_end)->next);
	}
}

static void scanLogicalVolumes(int fd, off64_t extended_start_LBA,
			       struct Masterbootrecord *ebr, int i,
			       PedPartition *partition, int lognum)
{
	struct Masterbootrecord next_ebr;
	PedFileSystemType *pfst = NULL;

	off64_t offset = extended_start_LBA + ebr->parttable[i].start_LBA;
	if (extended_start_LBA == 0) {
		extended_start_LBA = offset;
	}
	VERBOSE(stdout, "Seeking to LBA %llu\n", (unsigned long long)offset);
	off64_t res = lseek64(fd, offset * LB_SIZE, SEEK_SET);
	if (res == -1) {
		VERBOSE(stderr, "(%s)\n", strerror(errno));
		return;
	}
	VERBOSE(stdout, "Seek returned %lld\n", (signed long long)res);
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
		if (asprintf(&pfst->name, "%s", type_to_name(t)) == -1) {
			goto scl_out_of_mem;
		};
		partition = partition->next;
		partition->num = lognum;
		partition->fs_type = pfst;
	}
	return;
scl_out_of_mem:
	VERBOSE(stderr, "Out of memory\n");
	free(pfst);
	free(partition->next);
}

static bool check_partition_table(PedDevice *dev)
{
	int fd;
	struct Masterbootrecord mbr;

	VERBOSE(stdout, "Checking %s\n", dev->path);
	fd = open(dev->path, O_RDONLY);
	if (fd < 0) {
		VERBOSE(stderr, "Cannot open block device, skipping...\n");
		return false;
	}
	if (read(fd, &mbr, sizeof(mbr)) != sizeof(mbr)) {
		VERBOSE(stderr, "Cannot read MBR on %s, skipping...\n",
			dev->path);
		close(fd);
		return false;
	};
	if (mbr.mbrsignature != 0xaa55) {
		VERBOSE(stderr, "No valid MBR signature found, skipping...\n");
		close(fd);
		return false;
	}
	int numpartitions = 0;
	PedPartition **list_end = &dev->part_list;
	PedPartition *tmp = NULL;
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
			off64_t offset = LB_SIZE *
			    (off64_t)mbr.parttable[i].start_LBA;
			if (lseek64(fd, offset, SEEK_SET) != offset) {
				VERBOSE(stderr, "Error seeking EFI Header\n.");
				VERBOSE(stderr, "(%s)", strerror(errno));
				close(fd);
				return false;
			}
			struct EFIHeader efihdr;
			if (read(fd, &efihdr, sizeof(efihdr)) !=
			    sizeof(efihdr)) {
				close(fd);
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
				(unsigned long long)efihdr.partitiontable_LBA);
			read_GPT_entries(fd, efihdr.partitiontable_LBA,
					 efihdr.partitions, dev);
			break;
		}
		PedFileSystemType *pfst = calloc(sizeof(PedFileSystemType), 1);
		if (!pfst) {
			goto cpt_out_of_mem;
		}

		tmp = calloc(sizeof(PedPartition), 1);
		if (!tmp) {
			goto cpt_out_of_mem;
		}

		tmp->num = i + 1;
		tmp->fs_type = pfst;

		*list_end = tmp;
		list_end = &((*list_end)->next);

		if (t == MBR_TYPE_EXTENDED || t == MBR_TYPE_EXTENDED_LBA) {
			if (asprintf(&pfst->name, "%s", "extended") == -1) {
				goto cpt_out_of_mem;
			}
			scanLogicalVolumes(fd, 0, &mbr, i, tmp, 5);
			/* Could be we still have MBR entries after
			 * logical volumes */
			while ((*list_end)->next) {
				list_end = &((*list_end)->next);
			}
		} else {
			if (asprintf(&pfst->name, "%s", type_to_name(t)) == -1) {
				goto cpt_out_of_mem;
			}
		}
		continue;
	cpt_out_of_mem:
		close(fd);
		VERBOSE(stderr, "Out of mem while checking partition table\n.");
		free(pfst);
		free(tmp);
		return false;
	}
	close(fd);
	if (numpartitions == 0) {
		return false;
	}
	return true;
}

static int scan_devdir(unsigned int fmajor, unsigned int fminor, char *fullname,
		       unsigned int maxlen)
{
	int result = -1;

	DIR *devdir = opendir(DEVDIR);
	if (!devdir) {
		VERBOSE(stderr, "Failed to open %s\n", DEVDIR);
		return result;
	}
	struct dirent *devfile;
	do {
		devfile = readdir(devdir);
		if (!devfile) {
			break;
		}
		(void)snprintf(fullname, maxlen, "%s/%s", DEVDIR,
			       devfile->d_name);
		struct stat fstat;
		if (stat(fullname, &fstat) == -1) {
			VERBOSE(stderr, "stat failed on %s\n", fullname);
			break;
		}
		if (major(fstat.st_rdev) == fmajor &&
		    minor(fstat.st_rdev) == fminor) {
			VERBOSE(stdout, "Node found: %s\n", fullname);
			result = 0;
			break;
		}
	} while (devfile);
	closedir(devdir);

	return result;
}

static int get_major_minor(char *filename, unsigned int *major, unsigned int *minor)
{
	FILE *fh = fopen(filename, "r");
	if (fh == 0) {
		VERBOSE(stderr, "Error opening %s for read", filename);
		return -1;
	}
	int res = fscanf(fh, "%u:%u", major, minor);
	(void)fclose(fh);
	if (res < 2) {
		VERBOSE(stderr,
			"Error reading major/minor of device entry. (%s)\n",
			strerror(errno));
		return -1;
	};
	return 0;
}

void ped_device_probe_all(void)
{
	struct dirent *sysblockfile;
	char fullname[DEV_FILENAME_LEN+16];

	DIR *sysblockdir = opendir(SYSBLOCKDIR);
	if (!sysblockdir) {
		VERBOSE(stderr, "Could not open %s\n", SYSBLOCKDIR);
		return;
	}

	/* get all files from sysblockdir */
	do {
		sysblockfile = readdir(sysblockdir);
		if (!sysblockfile) {
			break;
		}
		if (strcmp(sysblockfile->d_name, ".") == 0 ||
		    strcmp(sysblockfile->d_name, "..") == 0) {
			continue;
		}
		(void)snprintf(fullname, sizeof(fullname), "/sys/block/%s/dev",
			 sysblockfile->d_name);
		/* Get major and minor revision from /sys/block/sdX/dev */
		unsigned int fmajor, fminor;
		if (get_major_minor(fullname, &fmajor, &fminor) < 0) {
			continue;
		}
		VERBOSE(stdout,
			"Trying device with: Major = %u, Minor = %u, (%s)\n",
			fmajor, fminor, fullname);
		/* Check if this file is really in the dev directory */
		(void)snprintf(fullname, sizeof(fullname), "%s/%s", DEVDIR,
			 sysblockfile->d_name);
		struct stat fstat;
		if (stat(fullname, &fstat) == -1) {
			/* Node with same name not found in /dev, thus search
			* for node with identical Major and Minor revision */
			if (scan_devdir(fmajor, fminor, fullname,
					sizeof(fullname)) != 0) {
				continue;
			}
		}
		/* This is a block device, so add it to the list*/
		PedDevice *dev = calloc(sizeof(PedDevice), 1);
		if (!dev) {
			continue;
		}
		if (asprintf(&dev->model, "%s", "N/A") == -1) {
			dev->model = NULL;
			goto pedprobe_error;
		}
		if (asprintf(&dev->path, "%s", fullname) == -1) {
			dev->path = NULL;
			goto pedprobe_error;
		}
		if (check_partition_table(dev)) {
			add_block_dev(dev);
			continue;
		}
pedprobe_error:
		free(dev->model);
		free(dev->path);
		free(dev);
	} while (sysblockfile);

	closedir(sysblockdir);
}

static void ped_partition_destroy(PedPartition *p)
{
	if (!p) {
		return;
	}
	if (p->fs_type) {
		free(p->fs_type->name);
		free(p->fs_type);
	}
	free(p);
}

static void ped_device_destroy(PedDevice *d)
{
	if (!d) {
		return;
	}
	free(d->model);
	free(d->path);
	PedPartition *p = d->part_list;
	while (p) {
		PedPartition *tmpp = p;

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

	while (d) {
		PedDevice *tmpd = d;

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

PedPartition *ped_disk_next_partition(const PedDisk *__unused pd,
				      const PedPartition *part)
{
	return part->next;
}
