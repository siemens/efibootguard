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

#include <stdlib.h>
#include <errno.h>
#include <sys/queue.h>
#include <check.h>
#include <fff.h>
#include <env_api.h>
#include <env_config_file.h>
#include <env_config_partitions.h>
#include <ebgpart.h>
#include <stdio.h>
#include <env_disk_utils.h>
#include <fake_devices.h>

DEFINE_FFF_GLOBALS;

Suite *ebg_test_suite(void);

char *get_mountpoint_custom_fake(char *devpath);
void delete_temp_files(void);

bool __wrap_probe_config_file(CONFIG_PART *);
bool __real_probe_config_file(CONFIG_PART *);
int probe_config_file_call_count;

char *fake_mountpoint = "/tmp/tmp.XXXXXX";

struct stailhead *headp;
struct fake_env_file_path {
	char *path;
	STAILQ_ENTRY(fake_env_file_path) fake_env_file_paths;
};
STAILQ_HEAD(stailhead, fake_env_file_path) head = STAILQ_HEAD_INITIALIZER(head);

char *get_mountpoint_custom_fake(char *devpath)
{
	char *buff = NULL;
	char *tmpdir = NULL;
	char *tmpfile = NULL;

	if (asprintf(&tmpdir, "%s", fake_mountpoint) == -1) {
		tmpdir = NULL;
		goto fake_mountpoint_error;
	}

	tmpdir = mkdtemp(tmpdir);
	if (!tmpdir) {
		goto fake_mountpoint_error;
	}

	if (asprintf(&buff, "%s", tmpdir) == -1) {
		buff = NULL;
		goto fake_mountpoint_error;
	}

	if (asprintf(&tmpfile, "%s/%s", tmpdir, FAT_ENV_FILENAME) == -1) {
		tmpfile = NULL;
		goto fake_mountpoint_error;
	}

	/* create a fake environment file
	 */
	FILE *temp_env_file = fopen(tmpfile, "w");

	BG_ENVDATA env_data;
	memset(&env_data, 0, sizeof(BG_ENVDATA));
	fwrite(&env_data, sizeof(BG_ENVDATA), 1, temp_env_file);
	fclose(temp_env_file);

	free(tmpfile);
	free(tmpdir);

	struct fake_env_file_path *fefp;
	fefp = malloc(sizeof(struct fake_env_file_path));
	if (!fefp) {
		goto fake_mountpoint_error;
	}

	char *buffer_copy;
	if (asprintf(&buffer_copy, "%s", buff) == -1) {
		goto fake_mountpoint_error;
	}

	if (fefp && buffer_copy) {
		fefp->path = buffer_copy;
		STAILQ_INSERT_TAIL(&head, fefp, fake_env_file_paths);
	}
	return buff;

fake_mountpoint_error:
	free(fefp);
	free(buff);
	free(tmpdir);
	return NULL;
}

bool __wrap_probe_config_file(CONFIG_PART *cp)
{
	probe_config_file_call_count++;

	return  __real_probe_config_file(cp);
}

void delete_temp_files(void)
{
	char *buffer;
	while (!STAILQ_EMPTY(&head)) {
		struct  fake_env_file_path *fefp = STAILQ_FIRST(&head);

		if (asprintf(&buffer, "%s/BGENV.DAT", fefp->path) != -1) {
			remove(buffer);
			free(buffer);
		}
		rmdir(fefp->path);
		free(fefp->path);

		STAILQ_REMOVE_HEAD(&head, fake_env_file_paths);
		free(fefp);
	}
}

FAKE_VOID_FUNC(ped_device_probe_all);
FAKE_VALUE_FUNC(PedDevice *, ped_device_get_next, const PedDevice *);
FAKE_VALUE_FUNC(char *, get_mountpoint, char *);

START_TEST(env_api_fat_test_probe_config_file)
{
	bool result;

	RESET_FAKE(ped_device_probe_all);
	RESET_FAKE(ped_device_get_next);
	RESET_FAKE(get_mountpoint);

	allocate_fake_devices(1);

	for (int i = 0; i < ENV_NUM_CONFIG_PARTS; i++) {
		add_fake_partition(0);
	}

	ped_device_get_next_fake.custom_fake = ped_device_get_next_custom_fake;
	get_mountpoint_fake.custom_fake = get_mountpoint_custom_fake;
	probe_config_file_call_count = 0;

	STAILQ_INIT(&head);

	result = bgenv_init();

	delete_temp_files();

	free_fake_devices();

	ck_assert_int_eq(ped_device_probe_all_fake.call_count, 1);
	ck_assert_int_eq(ped_device_get_next_fake.call_count, 2);
	ck_assert_int_eq(probe_config_file_call_count, ENV_NUM_CONFIG_PARTS);
	ck_assert_int_eq(get_mountpoint_fake.call_count, ENV_NUM_CONFIG_PARTS);
	ck_assert(result == true);

	bgenv_finalize();
}
END_TEST

Suite *ebg_test_suite(void)
{
	Suite *s;
	TCase *tc_core;

	s = suite_create("env_api_fat");

	tc_core = tcase_create("Core");
	tcase_add_test(tc_core, env_api_fat_test_probe_config_file);
	suite_add_tcase(s, tc_core);

	return s;
}
