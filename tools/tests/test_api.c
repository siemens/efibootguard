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
#include <error.h>
#include "env_api.h"
#include "ebgenv.h"

static BGENV env = {0};
static BGENV envupdate = {0};
static BG_ENVDATA data = {0};
static BG_ENVDATA dataupdate = {0};

#define DEFAULT_WATCHDOG_TIMEOUT_SEC 30
static int test_env_revision = 42;
static char *test_env_revision_str = "42";

bool bgenv_init(BGENVTYPE type)
{
	if (type != BGENVTYPE_FAT) {
		return false;
	}
	return true;
}

bool bgenv_write(BGENV *env)
{
	return mock_type(bool);
}

bool bgenv_close(BGENV *env_current)
{
	return true;
}

BGENV *bgenv_get_by_index(BGENVTYPE type, uint32_t index)
{
	return &envupdate;
}

BGENV *bgenv_get_latest(BGENVTYPE type)
{
	return mock_ptr_type(BGENV *);
}

BGENV *bgenv_get_oldest(BGENVTYPE type)
{
	return mock_ptr_type(BGENV *);
}

static void test_api_openclose(void **state)
{
	int ret;

	will_return(bgenv_get_latest, &env);
	ret = ebg_env_open_current();
	assert_int_equal(ret, 0);
	will_return(bgenv_write, true);
	ret = ebg_env_close();
	assert_int_equal(ret, 0);

	will_return(bgenv_get_latest, &env);
	ret = ebg_env_open_current();
	assert_int_equal(ret, 0);
	will_return(bgenv_write, false);
	ret = ebg_env_close();
	assert_int_equal(ret, EIO);

	will_return(bgenv_get_latest, NULL);
	ret = ebg_env_open_current();
	assert_int_equal(ret, EIO);
	ret = ebg_env_close();
	assert_int_equal(ret, EIO);

	(void)state;
}

static void test_api_accesscurrent(void **state)
{
	int ret;

	will_return(bgenv_get_latest, &env);
	ret = ebg_env_open_current();
	assert_int_equal(ret, 0);
	will_return(bgenv_write, true);
	ret = ebg_env_close();
	assert_int_equal(ret, 0);

	ret = ebg_env_set("NonsenseKey", "AnyValue");
	assert_int_equal(ret, EINVAL);

	ret = ebg_env_set("kernelfile", "vmlinuz");
	assert_int_equal(ret, EPERM);

	will_return(bgenv_get_latest, &env);
	ret = ebg_env_open_current();
	assert_int_equal(ret, 0);

	assert_int_equal(ebg_env_set("kernelfile", "vmlinuz"), 0);
	assert_int_equal(ebg_env_set("kernelparams", "root=/dev/sda"), 0);
	assert_int_equal(ebg_env_set("watchdog_timeout_sec", "abc"), EINVAL);
	assert_int_equal(ebg_env_set("watchdog_timeout_sec", "0013"), 0);
	assert_int_equal(ebg_env_set("ustate", "1"), 0);

	will_return(bgenv_write, true);
	ret = ebg_env_close();
	assert_int_equal(ret, 0);

	char *value;
	value = ebg_env_get("kernelfile");
	assert_null(value);
	assert_int_equal(errno, EPERM);

	will_return(bgenv_get_latest, &env);
	ret = ebg_env_open_current();
	assert_int_equal(ret, 0);

	assert_string_equal(ebg_env_get("kernelfile"), "vmlinuz");
	assert_string_equal(ebg_env_get("kernelparams"), "root=/dev/sda");
	assert_string_equal(ebg_env_get("watchdog_timeout_sec"), "13");
	assert_string_equal(ebg_env_get("ustate"), "1");
	assert_string_equal(ebg_env_get("revision"), test_env_revision_str);

	will_return(bgenv_write, true);
	ret = ebg_env_close();
	assert_int_equal(ret, 0);

	(void)state;
}

static void test_api_update(void **state)
{
	will_return(bgenv_get_latest, &env);
	will_return(bgenv_get_oldest, &envupdate);
	assert_int_equal(ebg_env_create_new(), 0);

	assert_int_equal(envupdate.data->revision, test_env_revision + 1);
	assert_int_equal(envupdate.data->watchdog_timeout_sec,
			 DEFAULT_WATCHDOG_TIMEOUT_SEC);
	assert_int_equal(envupdate.data->ustate, 1);

	assert_int_equal(ebg_env_set("ustate", "2"), 0);
	assert_int_equal(ebg_env_confirmupdate(), 0);

	assert_int_equal(ebg_env_set("revision", "0"), 0);
	assert_int_equal(ebg_env_set("ustate", "3"), 0);
	assert_false(ebg_env_isupdatesuccessful());

	assert_int_equal(ebg_env_set("revision", "0"), 0);
	assert_int_equal(ebg_env_set("ustate", "0"), 0);
	assert_true(ebg_env_isupdatesuccessful());

	assert_int_equal(ebg_env_set("revision", "0"), 0);
	assert_int_equal(ebg_env_set("ustate", "3"), 0);
	will_return(bgenv_write, true);
	assert_int_equal(ebg_env_clearerrorstate(), 0);
	assert_true(ebg_env_isupdatesuccessful());

	will_return(bgenv_write, true);
	assert_int_equal(ebg_env_close(), 0);

	(void)state;
}

int main(void)
{
	const struct CMUnitTest tests[] = {
	    cmocka_unit_test(test_api_openclose),
	    cmocka_unit_test(test_api_accesscurrent),
	    cmocka_unit_test(test_api_update)};

	env.data = &data;
	data.revision = test_env_revision;
	envupdate.data = &dataupdate;

	return cmocka_run_group_tests(tests, NULL, NULL);
}
