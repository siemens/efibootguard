/*
 * EFI Boot Guard
 *
 * Copyright (c) Siemens AG, 2017-2023
 *
 * Authors:
 *  Andreas Reichel <andreas.reichel.ext@siemens.com>
 *  Felix Moessbauer <felix.moessbauer@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include "env_api.h"
#include "ebgpart.h"
#include "env_config_partitions.h"
#include "env_config_file.h"

#define LOADER_PROT_VENDOR_GUID "4a67b082-0a4c-41cf-b6c7-440b29bb8c4f"
#define GUID_LEN_CHARS		36
#define EFI_ATTR_LEN_IN_WCHAR	2
#define ARRAY_SIZE(arr)		(sizeof(arr) / sizeof((arr)[0]))

/**
 * Read the ESP UUID from the efivars. This only works if the bootloader
 * implements the LoaderDevicePartUUID from the systemd bootloader interface
 * spec. Returns a device name (e.g. sda) or NULL. The returned string needs
 * to be freed by the caller.
 */
static char *get_rootdev_from_efi(void)
{
	const char *vendor_guid = LOADER_PROT_VENDOR_GUID;
	const char *basepath = "/sys/firmware/efi/efivars/";
	char part_uuid[GUID_LEN_CHARS + 1];
	FILE *f = 0;
	union {
		char aschar[512];
		char16_t aswchar[256];
	} buffer;

	// read LoaderDevicePartUUID efi variable
	snprintf(buffer.aschar, sizeof(buffer.aschar),
		 "%s/LoaderDevicePartUUID-%s", basepath, vendor_guid);
	if (!(f = fopen(buffer.aschar, "r"))) {
		VERBOSE(stderr, "Error, cannot access efi var at %s.\n",
			buffer.aschar);
		return NULL;
	}
	const size_t readnb = fread(buffer.aswchar, sizeof(*buffer.aswchar),
				    ARRAY_SIZE(buffer.aswchar), f);
	if (readnb != GUID_LEN_CHARS + EFI_ATTR_LEN_IN_WCHAR) {
		VERBOSE(stderr, "Data in LoaderDevicePartUUID not valid\n");
		fclose(f);
		return NULL;
	}
	fclose(f);

	// convert char16_t to char and lowercase uuid, skip attributes
	for (int i = 0; i < GUID_LEN_CHARS; i++) {
		part_uuid[i] = tolower(
			(char)buffer.aswchar[i + EFI_ATTR_LEN_IN_WCHAR]);
	}
	part_uuid[GUID_LEN_CHARS] = '\0';

	// resolve device based on partition uuid
	snprintf(buffer.aschar, sizeof(buffer.aschar),
		 "/dev/disk/by-partuuid/%s", part_uuid);
	char *devpath = realpath(buffer.aschar, NULL);
	if (!devpath) {
		VERBOSE(stderr, "Error, no disk in %s\n", buffer.aschar);
		return NULL;
	}
	VERBOSE(stdout, "resolved ESP to %s\n", devpath);
	// get disk name from path
	char *partition = strrchr(devpath, '/') + 1;

	// resolve parent device. As the ESP must be a primary partition, the
	// parent is the block device.
	snprintf(buffer.aschar, sizeof(buffer.aschar), "/sys/class/block/%s/..",
		 partition);
	free(devpath);

	// resolve to e.g. /sys/devices/pci0000:00/0000:00:1f.2/<...>/block/sda
	char *blockpath = realpath(buffer.aschar, NULL);
	char *_blockdev = strrchr(blockpath, '/') + 1;
	char *blockdev = strdup(_blockdev);
	free(blockpath);
	return blockdev;
}

bool probe_config_partitions(CONFIG_PART *cfgpart, bool search_all_devices)
{
	PedDevice *dev = NULL;
	char devpath[4096];
	char *rootdev = NULL;
	int count = 0;

	if (!cfgpart) {
		return false;
	}

	if (!search_all_devices) {
		if (!(rootdev = get_rootdev_from_efi())) {
			VERBOSE(stderr, "Warning, could not determine root "
					"dev. Search on all devices\n");
		} else {
			VERBOSE(stdout, "Limit probing to disk %s\n", rootdev);
		}
	}

	ped_device_probe_all(rootdev);
	free(rootdev);

	while ((dev = ped_device_get_next(dev))) {
		printf_debug("Device: %s\n", dev->model);
		PedDisk *pd = ped_disk_new(dev);
		if (!pd) {
			continue;
		}
		PedPartition *part = pd->part_list;
		while (part) {
			if (!part->fs_type || !part->fs_type->name ||
			    (strcmp(part->fs_type->name, "fat12") != 0 &&
			     strcmp(part->fs_type->name, "fat16") != 0 &&
			     strcmp(part->fs_type->name, "fat32") != 0)) {
				part = ped_disk_next_partition(pd, part);
				continue;
			}
			if (strncmp("/dev/mmcblk", dev->path, 11) == 0 ||
			    strncmp("/dev/loop", dev->path, 9) == 0 ||
			    strncmp("/dev/nvme", dev->path, 9) == 0) {
				(void)snprintf(devpath, 4096, "%sp%u",
					       dev->path, part->num);
			} else {
				(void)snprintf(devpath, 4096, "%s%u",
					       dev->path, part->num);
			}

			CONFIG_PART tmp = {.devpath = strdup(devpath)};
			if (!tmp.devpath) {
				VERBOSE(stderr, "Out of memory.");
				return false;
			}
			if (probe_config_file(&tmp)) {
				printf_debug("%s", "Environment file found.\n");
				if (count < ENV_NUM_CONFIG_PARTS) {
					cfgpart[count] = tmp;
				} else {
					free(tmp.devpath);
					VERBOSE(stderr,
						"Error, there are "
						"more than %d config "
						"partitions.\n",
						ENV_NUM_CONFIG_PARTS);
					return false;
				}
				count++;
			} else {
				free(tmp.devpath);
			}
			part = ped_disk_next_partition(pd, part);
		}
	}
	if (count < ENV_NUM_CONFIG_PARTS) {
		VERBOSE(stderr,
			"Error, less than %d config partitions exist.\n",
			ENV_NUM_CONFIG_PARTS);
		return false;
	}
	return true;
}
