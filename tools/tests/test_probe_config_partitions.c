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
#include <check.h>
#include <fff.h>
#include <env_api.h>
#include <env_config_file.h>
#include <env_config_partitions.h>
#include <fake_devices.h>

DEFINE_FFF_GLOBALS;

Suite *ebg_test_suite(void);

bool read_env_custom_fake(CONFIG_PART *cp, BG_ENVDATA *env);
bool read_env(CONFIG_PART *part, BG_ENVDATA *env);

bool read_env_custom_fake(CONFIG_PART *cp, BG_ENVDATA *env)
{
	if (!env) {
		return false;
	}
	memset(env, 0, sizeof(BG_ENVDATA));
	return true;
}

FAKE_VALUE_FUNC(bool, read_env, CONFIG_PART *, BG_ENVDATA *);
FAKE_VOID_FUNC(ped_device_probe_all, const char *);
FAKE_VALUE_FUNC(PedDevice *, ped_device_get_next, const PedDevice *);

START_TEST(env_api_fat_test_probe_config_partitions)
{
	bool result;
	/* In this unit test, contents of environment data are
	 * faked to be all zero
	 */

	/* Test if bgenv_init fails if no block devices are found
	 */
	RESET_FAKE(ped_device_probe_all);
	RESET_FAKE(ped_device_get_next);

	ped_device_get_next_fake.return_val = NULL;

	result = bgenv_init();

	ck_assert(ped_device_probe_all_fake.call_count == 1);
	ck_assert(result == false);

	/* Test if bgenv_init fails if a device with two partitions is found
	 * but now config file is there
	 */
	RESET_FAKE(ped_device_probe_all);
	RESET_FAKE(ped_device_get_next);

	allocate_fake_devices(1);

	for (int i = 0; i < ENV_NUM_CONFIG_PARTS; i++) {
		add_fake_partition(0);
	}

	ped_device_get_next_fake.custom_fake = ped_device_get_next_custom_fake;
	result = bgenv_init();

	free_fake_devices();

	ck_assert(ped_device_probe_all_fake.call_count == 1);
	ck_assert(ped_device_get_next_fake.call_count == 2);
	ck_assert(result == false);
}
END_TEST

Suite *ebg_test_suite(void)
{
	Suite *s;
	TCase *tc_core;

	s = suite_create("env_api_fat");

	tc_core = tcase_create("Core");
	tcase_add_test(tc_core, env_api_fat_test_probe_config_partitions);
	suite_add_tcase(s, tc_core);

	return s;
}
