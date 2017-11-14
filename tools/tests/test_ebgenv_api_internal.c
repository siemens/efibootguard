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
#include <check.h>
#include <fff.h>
#include <env_api.h>
#include <env_config_file.h>
#include <env_config_partitions.h>

DEFINE_FFF_GLOBALS;

static char *devpath = "/dev/nobrain";

Suite *ebg_test_suite(void);

extern bool write_env(CONFIG_PART *part, BG_ENVDATA *env);
extern EBGENVKEY bgenv_str2enum(char *);
extern BGENV *bgenv_open_by_index(uint32_t index);

bool write_env_custom_fake(CONFIG_PART *part, BG_ENVDATA *env);

bool write_env_custom_fake(CONFIG_PART *part, BG_ENVDATA *env)
{
	return true;
}

FAKE_VALUE_FUNC(bool, write_env, CONFIG_PART *, BG_ENVDATA *);

CONFIG_PART config_parts[ENV_NUM_CONFIG_PARTS];
BG_ENVDATA envdata[ENV_NUM_CONFIG_PARTS];

START_TEST(ebgenv_api_internal_strXtoY)
{
	wchar_t *exp_res = L"This is a test";
	wchar_t bufferw[16];
	char buffer[16];
	char *input = "This is a test";
	wchar_t *resw;
	char *res;

	/* Test conversion from ASCII bits to 16 bit encoding
	 */
	resw = str8to16(bufferw, input);

	/* cannot use glibc for 16-bit wchar_t
	 * string compare since glibc has 32-bit wchar_t
	 */
	for (int i = 0; i < strlen(input); i++) {
		ck_assert(resw[i] == exp_res[i]);
	}

	/* Test conversion from 16 bit encoding to ASCII
	 */
	res = str16to8(buffer, exp_res);

	ck_assert(strcmp(res, input) == 0);
}
END_TEST

START_TEST(ebgenv_api_internal_bgenv_str2enum)
{
	EBGENVKEY e;

	/* Test bgenv_str2enum for correct key conversion
	 */
	e = bgenv_str2enum("kernelfile");
	ck_assert(e == EBGENV_KERNELFILE);

	e = bgenv_str2enum("kernelparams");
	ck_assert(e == EBGENV_KERNELPARAMS);

	e = bgenv_str2enum("watchdog_timeout_sec");
	ck_assert(e == EBGENV_WATCHDOG_TIMEOUT_SEC);

	e = bgenv_str2enum("revision");
	ck_assert(e == EBGENV_REVISION);

	e = bgenv_str2enum("ustate");
	ck_assert(e == EBGENV_USTATE);

	/* Test if bgenv_str2enum returns EBGENV_UNKNOWN for empty and invalid
	 * keys
	 */
	e = bgenv_str2enum("XZXOOZOOZIOFZOFZ");
	ck_assert(e == EBGENV_UNKNOWN);

	e = bgenv_str2enum("");
	ck_assert(e == EBGENV_UNKNOWN);
}
END_TEST

START_TEST(ebgenv_api_internal_bgenv_open_by_index)
{
	BGENV *handle;

	handle = bgenv_open_by_index(0);
	ck_assert(handle != NULL);
	ck_assert(handle->desc == &config_parts[0]);
	ck_assert(handle->data == &envdata[0]);
	free(handle);

	handle = bgenv_open_by_index(ENV_NUM_CONFIG_PARTS-1);
	ck_assert(handle != NULL);
	ck_assert(handle->desc == &config_parts[ENV_NUM_CONFIG_PARTS-1]);
	ck_assert(handle->data == &envdata[ENV_NUM_CONFIG_PARTS-1]);
	free(handle);

	/* Test if bgenv_open_by_index returns NULL if parameter is out of
	 * range
	 */
	handle = bgenv_open_by_index(ENV_NUM_CONFIG_PARTS);
	ck_assert(handle == NULL);
}
END_TEST

START_TEST(ebgenv_api_internal_bgenv_open_oldest)
{
	BGENV *handle;

	/* Test if bgenv_open_oldest returns a handle for the environment with
	 * the lowest revision
	 */
	for (int i = 0; i < ENV_NUM_CONFIG_PARTS; i++)
	{
		envdata[i].revision = ENV_NUM_CONFIG_PARTS - i;
	}
	handle = bgenv_open_oldest();
	ck_assert(handle != NULL);
	ck_assert(handle->desc == &config_parts[ENV_NUM_CONFIG_PARTS-1]);
	ck_assert(handle->data == &envdata[ENV_NUM_CONFIG_PARTS-1]);
	free(handle);
}
END_TEST

START_TEST(ebgenv_api_internal_bgenv_open_latest)
{
	BGENV *handle;

	/* Test if bgenv_open_latest returns a handle for the environment with
	 * the highest revision
	 */
	for (int i = 0; i < ENV_NUM_CONFIG_PARTS; i++)
	{
		envdata[i].revision = ENV_NUM_CONFIG_PARTS - i;
	}
	handle = bgenv_open_latest();
	ck_assert(handle != NULL);
	ck_assert(handle->desc == &config_parts[0]);
	ck_assert(handle->data == &envdata[0]);
	free(handle);
}
END_TEST

START_TEST(ebgenv_api_internal_bgenv_write)
{
	bool res;
	BGENV *dummy_env;

	dummy_env = calloc(1, sizeof(BGENV));
	if (!dummy_env)
		goto bgew_error;

	RESET_FAKE(write_env);
	write_env_fake.custom_fake = write_env_custom_fake;

	/* Test if writing with a NULL-handle fails
	 */
	res = bgenv_write(NULL);
	ck_assert(write_env_fake.call_count == 0);
	ck_assert(res == false);

	/* Test if writing with a handle describing no partition
	 * and no environment data fails
	 */
	res = bgenv_write(dummy_env);
	ck_assert(write_env_fake.call_count == 0);
	ck_assert(res == false);

	/* Test if writing with a handle describing both partition
	 * and envrionment data succeeds
	 */
	dummy_env->desc = calloc(1, sizeof(CONFIG_PART));
	if (!dummy_env->desc)
		goto bgew_error;

	dummy_env->data = calloc(1, sizeof(BG_ENVDATA));
	if (!dummy_env->data)
		goto bgew_error;

	res = bgenv_write(dummy_env);
	ck_assert(write_env_fake.call_count == 1);
	ck_assert(res == true);

	return;

bgew_error:
	free(dummy_env->data);
	free(dummy_env->desc);
	free(dummy_env);
	exit(errno);
}
END_TEST

START_TEST(ebgenv_api_internal_bgenv_read)
{
	BGENV env;
	BG_ENVDATA data;

	env.data = &data;

	/* Test if bgenv_read returns a pointer to the environment data
	 */
	BG_ENVDATA *res = bgenv_read(&env);
	ck_assert(res == env.data);
}
END_TEST

START_TEST(ebgenv_api_internal_bgenv_create_new)
{
	BGENV *handle;
	int max = ENV_NUM_CONFIG_PARTS;

	for (int i = 0; i < max; i++)
	{
		envdata[i].revision = max - i;
	}

	/* Test if bgenv_create_new updates the oldest environment with default
	 * values and sets its revision to revision(latest)+1
	 */
	handle = bgenv_create_new();

	ck_assert(handle != NULL);
	ck_assert(handle->data == &envdata[max-1]);
	ck_assert(envdata[max-1].revision == max+1);
	ck_assert(envdata[max-1].watchdog_timeout_sec == 30);

	free(handle);
}
END_TEST

START_TEST(ebgenv_api_internal_bgenv_get)
{
	BGENV *handle = bgenv_open_latest();
	ck_assert(handle != NULL);

	wchar_t buffer[ENV_STRING_LENGTH];
	char *test_strings[] = {
		"kernelfile_test123",
		"kernelparams_test123",
	};
	void *dests[] = {
		&handle->data->kernelfile,
		&handle->data->kernelparams,
	};

	for (int i = 0; i < sizeof(test_strings)/sizeof(void*); i++)
	{
		memcpy(dests[i], str8to16(buffer, test_strings[i]),
		       strlen(test_strings[i]) * 2 + 2);
	}
	handle->data->watchdog_timeout_sec = 44;
	handle->data->revision = 10000;
	handle->data->ustate = USTATE_INSTALLED;

	char *data = NULL;
	char buffera[22];
	int res;

	/* Test if bgenv_get fails if maxlen is set to 0
	 */
	res = bgenv_get(handle, "kernelfile", NULL, data, 0);
	ck_assert_int_eq(res, -EINVAL);

	/* Test if bgenv_get fails if key is NULL
	 */
	res = bgenv_get(handle, NULL, NULL, data, 1000);
	ck_assert_int_eq(res, -EINVAL);

	/* Test if bgenv_get fails if no environment is provided
	 */
	res = bgenv_get(NULL, "kernelfile", NULL, NULL, 1000);
	ck_assert_int_eq(res, -EPERM);

	/* Test if bgenv_get returns the correct size of the needed
	 * buffer if provided with a NULL buffer
	 */
	res = bgenv_get(handle, "kernelfile", NULL, NULL, 1000);
	ck_assert_int_eq(res, strlen(test_strings[0]) + 1);

	/* Test if bgenv_get returns the correct value
	 */
	res = bgenv_get(handle, "kernelfile", NULL, buffera, res);
	ck_assert_int_eq(strcmp(buffera, test_strings[0]), 0);

	res = bgenv_get(handle, "kernelparams", NULL, NULL, 1000);
	res = bgenv_get(handle, "kernelparams", NULL, buffera, res);
	ck_assert_int_eq(strcmp(buffera, test_strings[1]), 0);

	free(handle);
}
END_TEST

START_TEST(ebgenv_api_internal_bgenv_set)
{
	int res;

	BGENV *handle = bgenv_open_latest();
	ck_assert(handle != NULL);
	ck_assert(handle->data != NULL);

	/* Test if bgenv_set returns -EINVAL if the handle is invalid
	 */
	res = bgenv_set(NULL, "kernelfile", 0, NULL, 0);
	ck_assert_int_eq(res, -EINVAL);

	/* Test if bgenv_set returns -EINVAL if the key is invalid
	 */
	res = bgenv_set(handle, "AOFIJAOEGIHA", 0, NULL, 0);
	ck_assert_int_eq(res, -EINVAL);

	/* Test if bgenv_set works correctly for valid parameters
	 */
	res = bgenv_set(handle, "kernelfile", 0, "vmlinuz", 8);
	ck_assert_int_eq(res, 0);

	char buffer[8];
	char *kfile = str16to8(buffer, handle->data->kernelfile);

	ck_assert(strcmp(kfile, "vmlinuz") == 0);

	res = bgenv_set(handle, "watchdog_timeout_sec", 0, "-0", 2);
	ck_assert_int_eq(res, 0);
	ck_assert_int_eq(handle->data->watchdog_timeout_sec, 0);

	res = bgenv_set(handle, "watchdog_timeout_sec", 0, "311", 4);
	ck_assert_int_eq(res, 0);
	ck_assert_int_eq(handle->data->watchdog_timeout_sec, 311);

	res = bgenv_set(handle, "kernelparams", 0, "root=", 6);
	ck_assert_int_eq(res, 0);

	char *kparm = str16to8(buffer, handle->data->kernelparams);

	ck_assert(strcmp(kparm, "root=") == 0);

	res = bgenv_set(handle, "ustate", 0, "2", 2);
	ck_assert_int_eq(res, 0);

	ck_assert_int_eq(handle->data->ustate, 2);

	res = bgenv_set(handle, "revision", 0, "0", 2);
	ck_assert_int_eq(res, 0);
	ck_assert_int_eq(handle->data->revision, 0);

	res = bgenv_set(handle, "revision", 0, "10301", 6);
	ck_assert_int_eq(res, 0);
	ck_assert_int_eq(handle->data->revision, 10301);

	free(handle);
}
END_TEST

Suite *ebg_test_suite(void)
{
	Suite *s;
	TCase *tc_core;

	s = suite_create("ebgenv_api");

	TFun tfuncs[] = {
		ebgenv_api_internal_strXtoY,
		ebgenv_api_internal_bgenv_str2enum,
		ebgenv_api_internal_bgenv_open_by_index,
		ebgenv_api_internal_bgenv_open_oldest,
		ebgenv_api_internal_bgenv_open_latest,
		ebgenv_api_internal_bgenv_write,
		ebgenv_api_internal_bgenv_read,
		ebgenv_api_internal_bgenv_create_new,
		ebgenv_api_internal_bgenv_get,
		ebgenv_api_internal_bgenv_set
	};

	tc_core = tcase_create("Core");

	for (int i = 0; i < sizeof(tfuncs)/sizeof(void *); i++) {
		tcase_add_test(tc_core, tfuncs[i]);
	}

	suite_add_tcase(s, tc_core);

	return s;
}
