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
 */

#ifndef __FAKE_DEVICES_H__
#define __FAKE_DEVICES_H__

#include <ebgpart.h>

extern PedDevice *fake_devices;
extern int num_fake_devices;

void allocate_fake_devices(int n);
void add_fake_partition(int devnum);
void remove_fake_partitions(int n);
void free_fake_devices(void);

PedDevice *ped_device_get_next_custom_fake(const PedDevice *dev);

#endif // __FAKE_DEVICES_H__
