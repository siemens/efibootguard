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
 */

#include <stdlib.h>
#include <env_api.h>
#include <env_config_file.h>
#include <env_config_partitions.h>
#include <fake_devices.h>

PedDevice *fake_devices;
int num_fake_devices;

void allocate_fake_devices(int n)
{
	fake_devices = (PedDevice *)malloc(n * sizeof(PedDevice));
	for (char i = 0; i < n; i++) {
		asprintf(&fake_devices[i].model, "%s", "Fake Device");
		asprintf(&fake_devices[i].path, "/dev/nobrain_%c", 'a' + i);
		fake_devices[i].part_list = NULL;
		fake_devices[i].next = NULL;
	}
	num_fake_devices = n;
	for (char i = n - 1; i > 0; i--) {
		fake_devices[i-1].next = &fake_devices[i];
	}
}

void add_fake_partition(int devnum)
{
	PedPartition **pp = &fake_devices[devnum].part_list;

	int16_t num = 0;
	while (*pp) {
		pp = &(*pp)->next;
		num++;
	}
	*pp = (PedPartition *)malloc(sizeof(PedPartition));
	(*pp)->num = num;
	(*pp)->fs_type = (PedFileSystemType *)malloc(sizeof(PedFileSystemType));
	asprintf(&(*pp)->fs_type->name, "%s", "fat16");
	(*pp)->next = NULL;
}

void remove_fake_partitions(int n)
{
	PedPartition *pp = fake_devices[n].part_list;
	PedPartition *next;
	while(pp) {
		next = pp->next;
		if (!pp->fs_type)
			goto skip_fstype;
		if (pp->fs_type->name)
			free(pp->fs_type->name);
		free(pp->fs_type);
skip_fstype:
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
		if (fake_devices[i].model)
			free(fake_devices[i].model);
		if (fake_devices[i].path)
			free(fake_devices[i].path);
		if (fake_devices[i].part_list)
			remove_fake_partitions(i);
	}

	free(fake_devices);
}

PedDevice *ped_device_get_next_custom_fake(const PedDevice *dev)
{
	if (!dev)
		return fake_devices;

	return dev->next;
}
