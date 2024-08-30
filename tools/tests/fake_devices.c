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
 * SPDX-License-Identifier:	GPL-2.0-only
 */

#include <stdlib.h>
#include <env_api.h>
#include <env_config_file.h>
#include <env_config_partitions.h>
#include "fake_devices.h"

PedDevice *fake_devices;
int num_fake_devices;

void allocate_fake_devices(int n)
{
	fake_devices = (PedDevice *)calloc(n, sizeof(PedDevice));
	if (!fake_devices) {
		exit(1);
	}
	num_fake_devices = n;
	for (char i = 0; i < n; i++) {
		if (asprintf(&fake_devices[i].model, "%s", "Fake Device")
		    == -1) {
			fake_devices[i].model = NULL;
			goto allocate_fake_devs_error;
		}
		if (asprintf(&fake_devices[i].path, "/dev/nobrain_%c", 'a' + i)
		    == -1) {
			fake_devices[i].path = NULL;
			goto allocate_fake_devs_error;
		}
	}
	for (char i = n - 1; i > 0; i--) {
		fake_devices[i-1].next = &fake_devices[i];
	}
	return;

allocate_fake_devs_error:
	free_fake_devices();
	exit(1);
}

void add_fake_partition(int devnum)
{
	PedPartition **pp = &fake_devices[devnum].part_list;

	int16_t num = 0;
	while (*pp) {
		pp = &(*pp)->next;
		num++;
	}
	*pp = (PedPartition *)calloc(1, sizeof(PedPartition));
	if (!*pp) {
		goto allocate_fake_part_error;
	}
	(*pp)->num = num;
	(*pp)->fs_type = FS_TYPE_FAT16;
	return;

allocate_fake_part_error:
	free_fake_devices();
	exit(1);
}

void remove_fake_partitions(int n)
{
	PedPartition *pp = fake_devices[n].part_list;
	PedPartition *next;
	while(pp) {
		next = pp->next;
		free(pp);
		pp = next;
	}
}

void free_fake_devices()
{
	if (!fake_devices) {
		return;
	}

	for (int i = 0; i < num_fake_devices; i++) {
		free(fake_devices[i].model);
		free(fake_devices[i].path);
		remove_fake_partitions(i);
	}

	free(fake_devices);
}

PedDevice *ped_device_get_next_custom_fake(const PedDevice *dev)
{
	if (!dev) {
		return fake_devices;
	}

	return dev->next;
}
