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
#include <string.h>
#include "bg_utils.h"
#include "test-interface.h"

/* Mock functions from libparted */

CONFIG_PART config_parts[CONFIG_PARTITION_COUNT];
BG_ENVDATA oldenvs[CONFIG_PARTITION_COUNT];

FILE test_file;

bool mount_partition(CONFIG_PART *cfgpart)
{
	return true;
}

int feof(FILE *f)
{
	return 0;
}

FILE *fopen(const char *filename, const char *mode)
{
	if (strcmp(filename, "/nobrain/BGENV.DAT") == 0) {
		return &test_file;
	}
	return NULL;
}

int fclose(FILE *handle)
{
	return mock_type(int);
}

size_t fread(void *ptr, size_t size, size_t count, FILE *stream)
{
	return mock_type(size_t);
}

size_t fwrite(const void *ptr, size_t size, size_t count, FILE *stream)
{
	return mock_type(size_t);
}

static void test_configfile_read(void **state)
{
	CONFIG_PART part;
	BG_ENVDATA env;

	part.devpath = "/dev/nobrain42";
	part.mountpoint = "/nobrain";
	part.not_mounted = false;

	will_return(fread, 0);
	will_return(fclose, 0);
	bool ret = read_env(&part, &env);
	assert_false(ret);

	will_return(fread, 1);
	will_return(fclose, 0);
	ret = read_env(&part, &env);
	assert_true(ret);

	will_return(fread, 1);
	will_return(fclose, -1);
	ret = read_env(&part, &env);
	assert_true(ret);

	(void)state;
}

static void test_configfile_write(void **state)
{
	CONFIG_PART part;
	BG_ENVDATA env;

	part.devpath = "/dev/nobrain42";
	part.mountpoint = "/nobrain";
	part.not_mounted = false;

	will_return(fwrite, 1);
	will_return(fclose, 0);
	bool ret = write_env(&part, &env);
	assert_true(ret);

	will_return(fwrite, 0);
	will_return(fclose, 1);
	ret = write_env(&part, &env);
	assert_false(ret);

	(void)state;
}

int main(void)
{
	const struct CMUnitTest tests[] = {
	    cmocka_unit_test(test_configfile_read),
	    cmocka_unit_test(test_configfile_write)};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
