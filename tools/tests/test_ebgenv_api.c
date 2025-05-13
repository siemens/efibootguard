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
#include <ebgenv.h>
#include <env_config_file.h>
#include <env_config_partitions.h>

DEFINE_FFF_GLOBALS;

Suite *ebg_test_suite(void);

extern bool write_env(CONFIG_PART *part, BG_ENVDATA *env);
extern bool bgenv_write(BGENV *);
extern bool bgenv_init(void);
extern void bgenv_close(BGENV *);
extern BGENV *bgenv_create_new(void);

FAKE_VALUE_FUNC(bool, bgenv_init);
FAKE_VALUE_FUNC(bool, bgenv_write, BGENV *);

int __real_bgenv_set(BGENV *, char *, uint64_t, void *, uint32_t);
int __wrap_bgenv_set(BGENV *, char *, uint64_t, void *, uint32_t);
int __real_bgenv_get(BGENV *, char *, uint64_t *, void *, uint32_t);
int __wrap_bgenv_get(BGENV *, char *, uint64_t *, void *, uint32_t);

static struct {
	BGENV *getset_arg0;
	char *getset_arg1;
	uint64_t *get_arg2;
	uint64_t set_arg2;
	void *getset_arg3;
	uint32_t getset_arg4;
	int get_call_count;
	int set_call_count;
} bgenv_, bgenv;

/* FFF does not provide calls to the original function, so in this case
 * we need to use the linker wrapping method and reimplement some of FFFs
 * functionality.
 */
int __wrap_bgenv_get(BGENV *env, char *key, uint64_t *type, void *buffer,
		     uint32_t len)
{
	bgenv.get_call_count++;
	bgenv.getset_arg0 = env;
	bgenv.getset_arg1 = key;
	bgenv.get_arg2 = type;
	bgenv.getset_arg3 = buffer;
	bgenv.getset_arg4 = len;
	return __real_bgenv_get(env, key, type, buffer, len);
}

int __wrap_bgenv_set(BGENV *env, char *key, uint64_t type, void *buffer,
		     uint32_t len)
{
	bgenv.set_call_count++;
	bgenv.getset_arg0 = env;
	bgenv.getset_arg1 = key;
	bgenv.set_arg2 = type;
	bgenv.getset_arg3 = buffer;
	bgenv.getset_arg4 = len;
	return __real_bgenv_set(env, key, type, buffer, len);
}

/* These variables substitute weakened symbols in the ebgenv library code
 * so that all environment functions use these as data sources
 */
CONFIG_PART config_parts[ENV_NUM_CONFIG_PARTS];
BG_ENVDATA envdata[ENV_NUM_CONFIG_PARTS];

static void
init_test()
{
	bgenv = bgenv_;
	memset(config_parts, 0, sizeof(config_parts));
	memset(envdata, 0, sizeof(envdata));
}

START_TEST(ebgenv_api_ebg_env_options)
{
	init_test();

	int status;
	status = ebg_set_opt_bool(EBG_OPT_VERBOSE, true);
	ck_assert_int_eq(status, 0);
	bool verbose;
	status = ebg_get_opt_bool(EBG_OPT_VERBOSE, &verbose);
	ck_assert_int_eq(status, 0);
	ck_assert_int_eq(verbose, 1);

	// try invalid option
	status = ebg_set_opt_bool(0xffff, false);
	ck_assert_int_ne(status, 0);
	status = ebg_get_opt_bool(0xffff, &verbose);
	ck_assert_int_ne(status, 0);
}
END_TEST

START_TEST(ebgenv_api_ebg_env_create_new)
{
	ebgenv_t e = { };
	int ret;
	char buffer[10];
	const char *kernelfile = "kernel123";
	const char *kernelparams = "param456";
	int watchdogtimeout = 44;

	init_test();

	memset(envdata, 0, sizeof(envdata));

	for (int i = 0; i < ENV_NUM_CONFIG_PARTS; i++) {
		envdata[i].revision = i + 1;
	}

	/* Test if ebg_env_create_new returns EIO if bgenv_init
	 * returns false
	 */
	bgenv_init_fake.return_val = false;
	ret = ebg_env_create_new(&e);
	ck_assert_int_eq(ret, EIO);

	/* Check if values of the latest environment are copied if a new
	 * environment is created. The new environment must overwrite the
	 * oldest environment and revision and ustate must be set correctly.
	 */
	if (ENV_NUM_CONFIG_PARTS > 1) {
		char16_t bufferw[10];

		envdata[ENV_NUM_CONFIG_PARTS-1].watchdog_timeout_sec =
			watchdogtimeout;
		(void)str8to16(bufferw, kernelfile);
		memcpy(envdata[ENV_NUM_CONFIG_PARTS-1].kernelfile, bufferw,
		       strlen(kernelfile) * 2 + 2);
		(void)str8to16(bufferw, kernelparams);
		memcpy(envdata[ENV_NUM_CONFIG_PARTS-1].kernelparams, bufferw,
		       strlen(kernelparams) * 2 + 2);
	} else {
		kernelfile = "";
		kernelparams = "";
		watchdogtimeout = 0;
	}
	errno = 0;

	bgenv_init_fake.return_val = true;
	ret = ebg_env_create_new(&e);

	ck_assert_int_eq(errno, 0);
	ck_assert_int_eq(ret, 0);

	ck_assert(((BGENV *)e.bgenv)->data == &envdata[0]);

	ck_assert_int_eq(((BGENV *)e.bgenv)->data->in_progress, 1);
	ck_assert_int_eq(
		((BGENV *)e.bgenv)->data->revision, ENV_NUM_CONFIG_PARTS+1);

	ck_assert_int_eq(((BGENV *)e.bgenv)->data->ustate, USTATE_OK);
	ck_assert_int_eq(((BGENV *)e.bgenv)->data->watchdog_timeout_sec,
			 watchdogtimeout);
	(void)str16to8(buffer, ((BGENV *)e.bgenv)->data->kernelfile);
	ck_assert_int_eq(strcmp(buffer, kernelfile), 0);
	(void)str16to8(buffer, ((BGENV *)e.bgenv)->data->kernelparams);
	ck_assert_int_eq(strcmp(buffer, kernelparams), 0);

	(void)ebg_env_close(&e);

	/* Test that a new creation of environment does keep the current
	 * values if an update is already in progress
	 */
	ret = ebg_env_create_new(&e);

	ck_assert_int_eq(ret, 0);
	ck_assert(((BGENV *)e.bgenv)->data == &envdata[0]);
	ck_assert_int_eq(((BGENV *)e.bgenv)->data->ustate, USTATE_OK);
	ck_assert_int_eq(
		((BGENV *)e.bgenv)->data->revision, ENV_NUM_CONFIG_PARTS+1);

	ret = ebg_env_finalize_update(&e);

	ck_assert_int_eq(ret, 0);
	ck_assert_int_eq(((BGENV *)e.bgenv)->data->ustate, USTATE_INSTALLED);
	ck_assert_int_eq(((BGENV *)e.bgenv)->data->in_progress, 0);

	(void)ebg_env_close(&e);
}
END_TEST

START_TEST(ebgenv_api_ebg_env_open_current)
{
	ebgenv_t e = { };
	int ret;

	init_test();

	/* Test if ebg_env_open_current returns EIO if bgenv_init returns false
	 */
	bgenv_init_fake.return_val = false;
	ret = ebg_env_open_current(&e);

	ck_assert_int_eq(ret, EIO);

#if ENV_NUM_CONFIG_PARTS > 1

	/* Test if ebg_env_open_current opens the environment with the highest
	 * revision
	 */
	for (int i = 0; i < ENV_NUM_CONFIG_PARTS; i++) {
		envdata[i].revision = i + 1;
	}

	bgenv_init_fake.return_val = true;
	ret = ebg_env_open_current(&e);

	ck_assert_int_eq(ret, 0);
	ck_assert(((BGENV *)e.bgenv)->data == &envdata[ENV_NUM_CONFIG_PARTS-1]);

	(void)ebg_env_close(&e);

	envdata[0].revision = 0xFFFF;

	ret = ebg_env_open_current(&e);

	ck_assert_int_eq(ret, 0);
	ck_assert(((BGENV *)e.bgenv)->data == &envdata[0]);

	(void)ebg_env_close(&e);
#endif

}
END_TEST

START_TEST(ebgenv_api_ebg_env_get)
{
	ebgenv_t e = { };
	int ret;
	char buffer[1];

	init_test();

	/* Test if ebg_env_get calls bg_env_get correctly and that it returns
	 * -EINVAL if no key is provided
	 */
	bgenv.get_call_count = 0;

	ret = ebg_env_get(&e, NULL, NULL);
	ck_assert_int_eq(ret, -EINVAL);

	ck_assert(bgenv.get_call_count == 1);
	ck_assert(bgenv.getset_arg0 == e.bgenv);
	ck_assert(bgenv.getset_arg1 == NULL);
	ck_assert(bgenv.get_arg2 == NULL);

	/* Test if ebg_env_get retrieves correct data if given a valid
	 * environment handle.
	 */
	e.bgenv = (BGENV *)calloc(1, sizeof(BGENV));
	ck_assert(e.bgenv != NULL);

	((BGENV *)e.bgenv)->data = (BG_ENVDATA *)calloc(1, sizeof(BG_ENVDATA));
	ck_assert(((BGENV *)e.bgenv)->data != NULL);

	bgenv.get_call_count = 0;

	(void)ebg_env_get(&e, "kernelfile", buffer);

	ck_assert(bgenv.get_call_count == 1);
	ck_assert(bgenv.getset_arg0 == e.bgenv);
	ck_assert_int_eq(strcmp(bgenv.getset_arg1, "kernelfile"), 0);
	ck_assert(bgenv.getset_arg3 == buffer);

	free(((BGENV *)e.bgenv)->data);
	free(e.bgenv);

}
END_TEST

START_TEST(ebgenv_api_ebg_env_set)
{
	ebgenv_t e = { };
	const char *value = "dummy";

	init_test();

	/* Check if ebg_env_set correctly calls bgenv.set
	 */
	bgenv.set_call_count = 0;

	e.bgenv = (BGENV *)calloc(1, sizeof(BGENV));
	ck_assert(e.bgenv != NULL);

	((BGENV *)e.bgenv)->data = (BG_ENVDATA *)calloc(1, sizeof(BG_ENVDATA));
	ck_assert(((BGENV *)e.bgenv)->data != NULL);

	(void)ebg_env_set(&e, "kernelfile", value);

	ck_assert(bgenv.set_call_count == 1);
	ck_assert(bgenv.getset_arg0 == e.bgenv);
	ck_assert_int_eq(strcmp(bgenv.getset_arg1, "kernelfile"), 0);
	ck_assert(bgenv.getset_arg3 == value);
	ck_assert(bgenv.getset_arg4 == strlen(value) + 1);

	free(((BGENV *)e.bgenv)->data);
	free(e.bgenv);
}
END_TEST

START_TEST(ebgenv_api_ebg_env_set_ex)
{

	ebgenv_t e = { };
	const char *key = "mykey";
	const char *value = "dummy";
	uint64_t usertype = 1ULL << 36;
	int32_t datalen = 5;

	init_test();

	/* Check if ebg_env_set_ex correctly calls bgenv.set
	 */
	bgenv.set_call_count = 0;

	e.bgenv = (BGENV *)calloc(1, sizeof(BGENV));
	ck_assert(e.bgenv != NULL);

	((BGENV *)e.bgenv)->data = (BG_ENVDATA *)calloc(1, sizeof(BG_ENVDATA));
	ck_assert(((BGENV *)e.bgenv)->data != NULL);

	bgenv.set_call_count = 0;

	(void)ebg_env_set_ex(&e, key, usertype, (uint8_t *)value, datalen);

	ck_assert(bgenv.set_call_count == 1);
	ck_assert(bgenv.getset_arg0 == e.bgenv);
	ck_assert_int_eq(strcmp(bgenv.getset_arg1, key), 0);
	ck_assert_int_eq(bgenv.set_arg2, usertype);
	ck_assert(bgenv.getset_arg3 == value);
	ck_assert(bgenv.getset_arg4 == datalen);

	free(((BGENV *)e.bgenv)->data);
	free(e.bgenv);
}
END_TEST

START_TEST(ebgenv_api_ebg_env_get_ex)
{
	ebgenv_t e = { };
	const char *key = "mykey";
	char buffer[5];
	uint64_t type;
	int32_t datalen = 5;

	init_test();

	/* Check if ebg_env_get_ex correctly calls bgenv.get
	 */
	bgenv.get_call_count = 0;

	e.bgenv = (BGENV *)calloc(1, sizeof(BGENV));
	ck_assert(e.bgenv != NULL);

	((BGENV *)e.bgenv)->data = (BG_ENVDATA *)calloc(1, sizeof(BG_ENVDATA));
	ck_assert(((BGENV *)e.bgenv)->data != NULL);

	bgenv.get_call_count = 0;

	(void)ebg_env_get_ex(&e, key, &type, (uint8_t *)buffer, datalen);

	ck_assert(bgenv.get_call_count == 1);
	ck_assert(bgenv.getset_arg0 == e.bgenv);
	ck_assert_int_eq(strcmp(bgenv.getset_arg1, key), 0);
	ck_assert(bgenv.get_arg2 == &type);
	ck_assert(bgenv.getset_arg3 == buffer);
	ck_assert(bgenv.getset_arg4 == datalen);

	free(((BGENV *)e.bgenv)->data);
	free(e.bgenv);
}
END_TEST

START_TEST(ebgenv_api_ebg_env_user_free)
{
	ebgenv_t e = { };
	uint32_t ret;

	init_test();

	/* Check if ebg_env_user_free returns 0 if no environment handle
	 * is available (invalid context).
	 */
	ret = ebg_env_user_free(&e);
	ck_assert_int_eq(ret, 0);

	/* Check if ebg_env_user_free returns 0 if no environment data
	 * is available (NULL environment).
	 */
	e.bgenv = (BGENV *)calloc(1, sizeof(BGENV));
	ck_assert(e.bgenv != NULL);

	ret = ebg_env_user_free(&e);
	ck_assert_int_eq(ret, 0);

	/* Check if ebg_env_user_free returns ENV_MEM_USERVARS if environment
	 * user space is empty
	 */
	((BGENV *)e.bgenv)->data = (BG_ENVDATA *)calloc(1, sizeof(BG_ENVDATA));
	ck_assert(((BGENV *)e.bgenv)->data != NULL);

	ret = ebg_env_user_free(&e);
	ck_assert_int_eq(ret, ENV_MEM_USERVARS);

	free(((BGENV *)e.bgenv)->data);
	free(e.bgenv);
}
END_TEST

START_TEST(ebgenv_api_ebg_env_getglobalstate)
{
#if ENV_NUM_CONFIG_PARTS > 1
	ebgenv_t e = { };
	uint16_t state;

	init_test();

	/* Test if ebg_env_getglobalstate returns OK if current environment
	 * is set to OK
	 */
	e.bgenv = (BGENV *)calloc(1, sizeof(BGENV));
	ck_assert(e.bgenv != NULL);

	for (int i = 0; i < ENV_NUM_CONFIG_PARTS; i++) {
		envdata[i].revision = i + 1;
	}

	envdata[1].revision = 0;
	envdata[1].ustate = USTATE_OK;

	state = ebg_env_getglobalstate(&e);
	ck_assert_int_eq(state, USTATE_OK);

	/* Test if ebg_env_getglobalstate returns FAILED if current environment
	 * is set to FAILED with revision 0
	 */
	envdata[1].revision = 0;
	envdata[1].ustate = USTATE_FAILED;

	state = ebg_env_getglobalstate(&e);
	ck_assert_int_eq(state, USTATE_FAILED);

	/* Test if ebg_env_getglobalstate returns FAILED if current environment
	 * is set to FAILED with non-zero revision
	 */
	envdata[1].revision = 15;

	state = ebg_env_getglobalstate(&e);
	ck_assert_int_eq(state, USTATE_FAILED);

	/* Test if ebg_env_getglobalstate returns INSTALLED if current
	 * environment is set to INSTALLED
	 */
	envdata[1].revision = 15;
	envdata[1].ustate = USTATE_INSTALLED;

	state = ebg_env_getglobalstate(&e);
	ck_assert_int_eq(state, USTATE_INSTALLED);

	/* Test if ebg_env_getglobalstate returns FAILED if current environment
	 * is set to OK and any other is set to FAILED
	 */
	envdata[1].ustate = USTATE_OK;
	envdata[0].revision = 0;
	envdata[0].ustate = USTATE_FAILED;

	state = ebg_env_getglobalstate(&e);
	ck_assert_int_eq(state, USTATE_FAILED);

	/* Test if ebg_env_getglobalstate returns OK if current environment is
	 * set to OK and any other is set to INSTALLED
	 */
	envdata[0].ustate = USTATE_INSTALLED;

	state = ebg_env_getglobalstate(&e);
	ck_assert_int_eq(state, USTATE_OK);

	/* Test if ebg_env_getglobalstate returns TESTING if current
	 * environment is set to TESTING and none is FAILED
	 */
	envdata[0].ustate = USTATE_OK;
	envdata[1].ustate = USTATE_TESTING;

	state = ebg_env_getglobalstate(&e);
	ck_assert_int_eq(state, USTATE_TESTING);

	/* Test if ebg_env_getglobalstate returns OK if current environment is
	 * set to OK and none is TESTING
	 */
	envdata[0].ustate = USTATE_TESTING;
	envdata[1].ustate = USTATE_OK;

	state = ebg_env_getglobalstate(&e);
	ck_assert_int_eq(state, USTATE_OK);

	free(e.bgenv);
#endif
}
END_TEST

START_TEST(ebgenv_api_ebg_env_setglobalstate)
{
#if ENV_NUM_CONFIG_PARTS > 1
	ebgenv_t e = { };
	int ret;

	init_test();

	/* Test if ebg_env_setglobalstate sets only current to FAILED
	 */
	for (int i = 0; i < ENV_NUM_CONFIG_PARTS; i++) {
		envdata[i].revision = i + 1;
	}

	bgenv_init_fake.return_val = true;

	ret = ebg_env_open_current(&e);

	ck_assert_int_eq(ret, 0);
	ck_assert(((BGENV *)e.bgenv)->data == &envdata[ENV_NUM_CONFIG_PARTS-1]);

	envdata[0].ustate = USTATE_OK;
	envdata[1].ustate = USTATE_OK;

	ret = ebg_env_setglobalstate(&e, 0xFFF);
	ck_assert_int_eq(ret, -EINVAL);

	ret = ebg_env_setglobalstate(&e, USTATE_FAILED);

	ck_assert_int_eq(ret, 0);

	ck_assert_int_eq(envdata[0].ustate, USTATE_OK);
	ck_assert_int_eq(envdata[ENV_NUM_CONFIG_PARTS-1].ustate, USTATE_FAILED);

	envdata[1].ustate = USTATE_OK;
	envdata[0].ustate = USTATE_OK;

	(void)ebg_env_close(&e);

	envdata[0].revision = 1313;

	ret = ebg_env_open_current(&e);
	ck_assert_int_eq(ret, 0);

	ret = ebg_env_setglobalstate(&e, USTATE_FAILED);
	ck_assert_int_eq(ret, 0);
	ck_assert_int_eq(envdata[0].ustate, USTATE_FAILED);
	ck_assert_int_eq(envdata[1].ustate, USTATE_OK);

	/* Test if ebg_env_setglobalstate sets ALL environments to OK
	 */
	envdata[0].ustate = USTATE_FAILED;
	envdata[1].ustate = USTATE_FAILED;

	bgenv_write_fake.return_val = true;

	ret = ebg_env_setglobalstate(&e, USTATE_OK);

	ck_assert_int_eq(ret, 0);
	ck_assert_int_eq(envdata[0].ustate, USTATE_OK);
	ck_assert_int_eq(envdata[1].ustate, USTATE_OK);

	/* Test if ebg_env_setglobalstate sets current environment to TESTING
	 */
	envdata[0].ustate = USTATE_INSTALLED;
	envdata[1].ustate = USTATE_INSTALLED;

	ret = ebg_env_setglobalstate(&e, USTATE_TESTING);

	ck_assert_int_eq(ret, 0);
	ck_assert_int_eq(envdata[0].ustate, USTATE_TESTING);
	ck_assert_int_eq(envdata[1].ustate, USTATE_INSTALLED);

	/* Test if ebg_env_setglobalstate fails and returns EIO if bgenv_write
	 * fails
	 */
	bgenv_write_fake.return_val = false;

	ret = ebg_env_setglobalstate(&e, USTATE_OK);

	ck_assert_int_eq(ret, -EIO);

	(void)ebg_env_close(&e);
#endif
}
END_TEST

START_TEST(ebgenv_api_ebg_env_close)
{
	ebgenv_t e = { };
	int ret;

	init_test();

	/* Test if ebg_env_close fails with invalid context and returns EIO
	 */
	ret = ebg_env_close(&e);
	ck_assert_int_eq(ret, EIO);

	/* Test if ebg_env_close fails and returns EIO if bgenv_write fails
	 */
	e.bgenv = calloc(1, sizeof(BGENV));
	ck_assert(e.bgenv != NULL);

	void *data = calloc(1, sizeof(BG_ENVDATA));
	((BGENV *)e.bgenv)->data = data;
	bgenv_write_fake.return_val = false;
	ret = ebg_env_close(&e);

	ck_assert_int_eq(ret, EIO);

	/* Test if ebg_env_close is successful if all prerequisites are met
	 */
	e.bgenv = calloc(1, sizeof(BGENV));
	ck_assert(e.bgenv != NULL);
	((BGENV *)e.bgenv)->data = data;
	bgenv_write_fake.return_val = true;
	ret = ebg_env_close(&e);

	ck_assert_int_eq(ret, 0);
	ck_assert(e.bgenv == NULL);

	free(data);
}
END_TEST

START_TEST(ebgenv_api_ebg_env_register_gc_var)
{
	ebgenv_t e = { };
	int ret;

	init_test();

	bgenv_write_fake.return_val = true;

	bgenv_init_fake.return_val = true;

	for (int i = 0; i < ENV_NUM_CONFIG_PARTS; i++) {
		envdata[i].revision = i + 1;
	}

	ret = ebg_env_create_new(&e);
	ck_assert_int_eq(ret, 0);

	/* Create three user variables VarA, VarB and VarC */
	ebg_env_set(&e, "VarA", "TestA");
	ebg_env_set(&e, "VarB", "TestB");
	ebg_env_set(&e, "VarC", "TestC");

	/* Check if variables exist */
	int res;
	res = ebg_env_get(&e, "VarA", NULL);
	ck_assert_int_eq(res, strlen("TestA") + 1);
	res = ebg_env_get(&e, "VarB", NULL);
	ck_assert_int_eq(res, strlen("TestB") + 1);
	res = ebg_env_get(&e, "VarC", NULL);
	ck_assert_int_eq(res, strlen("TestC") + 1);

	/* Register variables for deletion */
	ebg_env_register_gc_var(&e, "VarA");
	ebg_env_register_gc_var(&e, "VarC");

	ebg_env_finalize_update(&e);

	/* Check if variables are deleted */
	res = ebg_env_get(&e, "VarA", NULL);
	ck_assert_int_eq(res, -ENOENT);
	res = ebg_env_get(&e, "VarB", NULL);
	ck_assert_int_eq(res, strlen("TestB") + 1);
	res = ebg_env_get(&e, "VarC", NULL);
	ck_assert_int_eq(res, -ENOENT);

	ebg_env_close(&e);
}
END_TEST

Suite *ebg_test_suite(void)
{
	Suite *s;
	TCase *tc_core;

	s = suite_create("ebgenv_api");

	tc_core = tcase_create("Core");

	tcase_add_test(tc_core, ebgenv_api_ebg_env_options);
	tcase_add_test(tc_core, ebgenv_api_ebg_env_create_new);
	tcase_add_test(tc_core, ebgenv_api_ebg_env_open_current);
	tcase_add_test(tc_core, ebgenv_api_ebg_env_get);
	tcase_add_test(tc_core, ebgenv_api_ebg_env_set);
	tcase_add_test(tc_core, ebgenv_api_ebg_env_set_ex);
	tcase_add_test(tc_core, ebgenv_api_ebg_env_get_ex);
	tcase_add_test(tc_core, ebgenv_api_ebg_env_user_free);
	tcase_add_test(tc_core, ebgenv_api_ebg_env_getglobalstate);
	tcase_add_test(tc_core, ebgenv_api_ebg_env_setglobalstate);
	tcase_add_test(tc_core, ebgenv_api_ebg_env_close);
	tcase_add_test(tc_core, ebgenv_api_ebg_env_register_gc_var);

	suite_add_tcase(s, tc_core);

	return s;
}
