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
static ebgenv_t e;

bool bgenv_init()
{
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

BGENV *bgenv_open_by_index(uint32_t index)
{
	if (index == 1) {
		return &envupdate;
	}
	return &env;
}

BGENV *bgenv_open_latest()
{
	return mock_ptr_type(BGENV *);
}

BGENV *bgenv_open_oldest()
{
	return mock_ptr_type(BGENV *);
}

static void test_api_openclose(void **state)
{
	int ret;

	will_return(bgenv_open_latest, &env);
	ret = ebg_env_open_current(&e);
	assert_int_equal(ret, 0);
	will_return(bgenv_write, true);
	ret = ebg_env_close(&e);
	assert_int_equal(ret, 0);

	will_return(bgenv_open_latest, &env);
	ret = ebg_env_open_current(&e);
	assert_int_equal(ret, 0);
	will_return(bgenv_write, false);
	ret = ebg_env_close(&e);
	assert_int_equal(ret, EIO);

	will_return(bgenv_open_latest, NULL);
	ret = ebg_env_open_current(&e);
	assert_int_equal(ret, EIO);
	ret = ebg_env_close(&e);
	assert_int_equal(ret, EIO);

	(void)state;
}

static void test_api_accesscurrent(void **state)
{
	int ret;
	char buffer[4096];

	will_return(bgenv_open_latest, &env);
	ret = ebg_env_open_current(&e);
	assert_int_equal(ret, 0);
	will_return(bgenv_write, true);
	ret = ebg_env_close(&e);
	assert_int_equal(ret, 0);

	ret = ebg_env_set(&e, "kernelfile", "vmlinuz");
	assert_int_equal(ret, EPERM);

	will_return(bgenv_open_latest, &env);
	ret = ebg_env_open_current(&e);
	assert_int_equal(ret, 0);

	assert_int_equal(ebg_env_set(&e, "kernelfile", "vmlinuz"), 0);
	assert_int_equal(ebg_env_set(&e, "kernelparams", "root=/dev/sda"), 0);
	assert_int_equal(ebg_env_set(&e, "watchdog_timeout_sec", "abc"), EINVAL);
	assert_int_equal(ebg_env_set(&e, "watchdog_timeout_sec", "0013"), 0);
	assert_int_equal(ebg_env_set(&e, "ustate", "1"), 0);

	will_return(bgenv_write, true);
	ret = ebg_env_close(&e);
	assert_int_equal(ret, 0);

	ret = ebg_env_get(&e, "kernelfile", buffer);
	assert_int_equal(ret, EPERM);

	will_return(bgenv_open_latest, &env);
	ret = ebg_env_open_current(&e);
	assert_int_equal(ret, 0);

	assert_int_equal(ebg_env_get(&e, "kernelfile", buffer), 0);
	assert_string_equal(buffer, "vmlinuz");
	assert_int_equal(ebg_env_get(&e, "kernelparams", buffer), 0);
	assert_string_equal(buffer, "root=/dev/sda");
	assert_int_equal(ebg_env_get(&e, "watchdog_timeout_sec", buffer), 0);
	assert_string_equal(buffer, "13");
	assert_int_equal(ebg_env_get(&e, "ustate", buffer), 0);
	assert_string_equal(buffer, "1");
	assert_int_equal(ebg_env_get(&e, "revision", buffer), 0);
	assert_string_equal(buffer, test_env_revision_str);

	will_return(bgenv_write, true);
	ret = ebg_env_close(&e);
	assert_int_equal(ret, 0);

	(void)state;
}

static void test_api_update(void **state)
{
	will_return(bgenv_open_latest, &env);
	will_return(bgenv_open_oldest, &envupdate);
	will_return(bgenv_open_latest, &env);
	assert_int_equal(ebg_env_create_new(&e), 0);

	assert_int_equal(envupdate.data->revision, test_env_revision + 1);
	assert_int_equal(envupdate.data->ustate, 1);

	assert_int_equal(ebg_env_set(&e, "ustate", "2"), 0);
	for (int i = 0; i < ENV_NUM_CONFIG_PARTS; i++) {
		will_return(bgenv_write, true);
	}
	assert_int_equal(ebg_env_setglobalstate(&e, 0), 0);

	if (ENV_NUM_CONFIG_PARTS == 1) {
		will_return(bgenv_open_latest, &envupdate);
	}
	assert_int_equal(ebg_env_set(&e, "revision", "0"), 0);
	assert_int_equal(ebg_env_set(&e, "ustate", "3"), 0);
	assert_int_equal(ebg_env_getglobalstate(&e), 3);

	will_return(bgenv_open_latest, &env);
	for (int i = 0; i < ENV_NUM_CONFIG_PARTS; i++) {
		will_return(bgenv_write, true);
	}
	assert_int_equal(ebg_env_setglobalstate(&e, 0), 0);
	assert_int_equal(ebg_env_getglobalstate(&e), 0);

	will_return(bgenv_write, true);
	assert_int_equal(ebg_env_close(&e), 0);

	(void)state;
}

static void test_api_uservars(void **state)
{
	int ret;
	char *test_key = "NonsenseKey";
	char *test_key2 = "TestKey2";

	char *test_val = "AnyValue";
	char *test_val2 = "BnyVbluf";
	char *test_val3 = "TESTTESTTESTTEST";
	char *test_val4 = "abc";
	char buffer[ENV_MEM_USERVARS];
	uint32_t space_left;

	will_return(bgenv_open_latest, &env);
	ret = ebg_env_open_current(&e);
	assert_int_equal(ret, 0);

	assert_int_equal(ebg_env_user_free(&e), ENV_MEM_USERVARS);

	ret = ebg_env_set(&e, test_key, test_val);
	assert_int_equal(ret, 0);

	space_left = ENV_MEM_USERVARS - strlen(test_key) - 1
			- strlen(test_val) - 1 - sizeof(uint32_t)
			- strlen(USERVAR_TYPE_DEFAULT) - 1;

	assert_int_equal(ebg_env_user_free(&e), space_left);

	ret = ebg_env_get(&e, test_key, buffer);
	assert_int_equal(ret, 0);
	assert_string_equal(buffer, test_val);

	// replace value with same length value
	ret = ebg_env_set(&e, test_key, test_val2);
	assert_int_equal(ret, 0);
	assert_int_equal(ebg_env_user_free(&e), space_left);

	ret = ebg_env_get(&e, test_key, buffer);
	assert_int_equal(ret, 0);
	assert_string_equal(buffer, test_val2);

	// replace value with larger value
	ret = ebg_env_set(&e, test_key, test_val3);
	assert_int_equal(ret, 0);

	space_left = ENV_MEM_USERVARS - strlen(test_key) - 1
			- strlen(test_val3) - 1 - sizeof(uint32_t)
			- strlen(USERVAR_TYPE_DEFAULT) - 1;

	assert_int_equal(ebg_env_user_free(&e), space_left);

	// replace value with smaller value
	ret = ebg_env_set(&e, test_key, test_val4);
	assert_int_equal(ret, 0);

	space_left = ENV_MEM_USERVARS - strlen(test_key) - 1
			- strlen(test_val4) - 1 - sizeof(uint32_t)
			- strlen(USERVAR_TYPE_DEFAULT) - 1;

	assert_int_equal(ebg_env_user_free(&e), space_left);

	// add 2nd variable
	ret = ebg_env_set(&e, test_key2, test_val2);
	assert_int_equal(ret, 0);

	space_left = space_left - strlen(test_key2) - 1
			- strlen(test_val2) - 1 - sizeof(uint32_t)
			- strlen(USERVAR_TYPE_DEFAULT) - 1;

	assert_int_equal(ebg_env_user_free(&e), space_left);

	// retrieve both variables
	ret = ebg_env_get(&e, test_key2, buffer);
	assert_int_equal(ret, 0);
	assert_string_equal(buffer, test_val2);
	ret = ebg_env_get(&e, test_key, buffer);
	assert_int_equal(ret, 0);
	assert_string_equal(buffer, test_val4);

	// overwrite first variable
	ret = ebg_env_set(&e, test_key, test_val3);
	assert_int_equal(ret, 0);

	space_left = space_left + strlen(test_val4)
				- strlen(test_val3);
	assert_int_equal(ebg_env_user_free(&e), space_left);

	// retrieve both variables
	ret = ebg_env_get(&e, test_key2, buffer);
	assert_int_equal(ret, 0);
	assert_string_equal(buffer, test_val2);
	ret = ebg_env_get(&e, test_key, buffer);
	assert_int_equal(ret, 0);
	assert_string_equal(buffer, test_val3);

	void *dummymem = malloc(space_left);

	// test out of memory
	ret = ebg_env_set_ex(&e, "A", "B", dummymem, space_left);
	free(dummymem);

	assert_int_equal(ret, ENOMEM);

	// test user data type
	ret = ebg_env_set_ex(&e, "A", "B", "C", 2);
	assert_int_equal(ret, 0);

	char type[2];
	char data[2];

	ret = ebg_env_get_ex(&e, "A", type, data, sizeof(data));
	assert_int_equal(ret, 0);
	assert_string_equal("B", type);
	assert_string_equal("C", data);

	(void)state;
}

int main(void)
{
	const struct CMUnitTest tests[] = {
	    cmocka_unit_test(test_api_openclose),
	    cmocka_unit_test(test_api_accesscurrent),
	    cmocka_unit_test(test_api_update),
	    cmocka_unit_test(test_api_uservars)};

	env.data = &data;
	data.revision = test_env_revision;
	envupdate.data = &dataupdate;

	return cmocka_run_group_tests(tests, NULL, NULL);
}
