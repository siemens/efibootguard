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
#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>
#include <setjmp.h>
#include <cmocka.h>
#include "bg_utils.h"

static PedDevice ped_devices[32] = {0};
static int num_simulated_devices = 2;
static int curr_ped_device = 0;
static PedPartition ped_parts[32] = {0};
static int num_simulated_partitions_per_disk = 2;
static PedFileSystemType ped_fstypes[32] = {0};

static const char *const fsname = "fat16";

static char *fakemodel = "Mocked Disk Drive";
static char *fakedevice = "/dev/nobrain";

extern bool probe_config_partitions(CONFIG_PART *cfgparts);

/* Mock functions from libparted */
void ped_device_probe_all()
{
	/* Setup the test data structure */
	for (int i = 0; i < 32; i++) {
		ped_devices[i].model = fakemodel;
		ped_devices[i].path = fakedevice;
		ped_devices[i].part_list = &ped_parts[0];
	}

	for (int i = 0; i < 32; i++) {
		ped_parts[i].fs_type = &ped_fstypes[i];
		ped_parts[i].num = i + 1;

		/* Unfortunately, the struct member to set is
		 * 'const char * const'
		 */
		long ptr = (long)fsname;
		memcpy((void *)&ped_fstypes[i].name, &ptr,
		       sizeof(ped_fstypes[i].name));
	}
}

PedDevice *ped_device_get_next(const PedDevice *dev)
{
	if (dev == NULL) {
		return &ped_devices[0];
	}
	for (int i = 0; i < num_simulated_devices - 1; i++) {
		if (dev == &ped_devices[i]) {
			return &ped_devices[i + 1];
		}
	}
	return NULL;
}

PedPartition *ped_disk_next_partition(const PedDisk *disk,
				      const PedPartition *part)
{
	if (disk == NULL) {
		return NULL;
	}
	if (part == NULL) {
		return &ped_parts[0];
	}
	for (int i = 0; i < num_simulated_partitions_per_disk - 1; i++) {
		if (part == &ped_parts[i]) {
			return &ped_parts[i + 1];
		}
	}
	return NULL;
}

bool probe_config_file(CONFIG_PART *cfgpart)
{
	return mock_type(bool);
}

static void test_partition_count(void **state)
{
	CONFIG_PART cfgparts[256];
	bool ret;

	num_simulated_devices = 1;
	num_simulated_partitions_per_disk = CONFIG_PARTITION_COUNT;
	for (int i = 0; i < CONFIG_PARTITION_COUNT; i++) {
		will_return(probe_config_file, true);
	}
	ret = probe_config_partitions(cfgparts);
	assert_true(ret);

	num_simulated_devices = CONFIG_PARTITION_COUNT;
	num_simulated_partitions_per_disk = 1;
	for (int i = 0; i < CONFIG_PARTITION_COUNT; i++) {
		will_return(probe_config_file, true);
	}
	ret = probe_config_partitions(cfgparts);
	assert_true(ret);

	num_simulated_devices = 1;
	num_simulated_partitions_per_disk = CONFIG_PARTITION_COUNT - 1;
	for (int i = 0; i < CONFIG_PARTITION_COUNT - 1; i++) {
		will_return(probe_config_file, true);
	}
	ret = probe_config_partitions(cfgparts);
	assert_false(ret);

	num_simulated_devices = 1;
	num_simulated_partitions_per_disk = CONFIG_PARTITION_COUNT + 1;
	for (int i = 0; i < CONFIG_PARTITION_COUNT + 1; i++) {
		will_return(probe_config_file, true);
	}
	ret = probe_config_partitions(cfgparts);
	assert_false(ret);

	(void)state;
}

static void test_config_file_existence(void **state)
{
	CONFIG_PART cfgparts[256];
	bool ret;

	num_simulated_devices = 1;
	num_simulated_partitions_per_disk = CONFIG_PARTITION_COUNT;
	for (int i = 0; i < CONFIG_PARTITION_COUNT; i++) {
		will_return(probe_config_file, false);
	}
	ret = probe_config_partitions(cfgparts);
	assert_false(ret);

	if (CONFIG_PARTITION_COUNT > 1) {
		for (int i = 0; i < CONFIG_PARTITION_COUNT - 1; i++) {
			will_return(probe_config_file, true);
		}
		will_return(probe_config_file, false);
	}
	ret = probe_config_partitions(cfgparts);
	assert_false(ret);

	(void)state;
}

int main(void)
{
	const struct CMUnitTest tests[] = {
	    cmocka_unit_test(test_partition_count),
	    cmocka_unit_test(test_config_file_existence)};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
