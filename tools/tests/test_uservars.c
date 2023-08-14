/*
 * EFI Boot Guard
 *
 * Copyright (c) Siemens AG, 2023
 *
 * Author: Michael Adler <michael.adler@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <stdlib.h>
#include <check.h>
#include <fff.h>

#include <env_api.h>
#include <uservars.h>
#include <bg_envtools.h>

DEFINE_FFF_GLOBALS;

Suite *ebg_test_suite(void);

/* These variables substitute weakened symbols in the ebgenv library code
 * so that all environment functions use these as data sources
 */
CONFIG_PART config_parts[ENV_NUM_CONFIG_PARTS];
BG_ENVDATA envdata[ENV_NUM_CONFIG_PARTS];

START_TEST(bgenv_get_from_manipulated)
{
	char *key = "mykey";
	char *value = "dummy";
	size_t value_len = strlen(value);
	uint64_t usertype = 1ULL << 36;

	BG_ENVDATA data = {.ustate = USTATE_OK};
	/* create a manipulated BGENV (in-memory) */
	{
		bgenv_set_uservar(data.userdata, key, usertype, value,
				  value_len);
		/* get position of payload size */
		char *var_key = (char *)bgenv_find_uservar(data.userdata, key);
		uint32_t *payload_size =
			(uint32_t *)(var_key + strlen(var_key) + 1);
		/* sanity check */
		ck_assert_int_eq(*payload_size, sizeof(uint32_t) +
							sizeof(uint64_t) +
							value_len);

		/* manipulate payload_size */
		*payload_size = UINT32_MAX;
		/* fix checksum */
		data.crc32 = crc32(0, (Bytef *)&data,
				   sizeof(BG_ENVDATA) - sizeof(data.crc32));
	}

	/* persist BGENV to a temporary file */
	char configfilepath[] = "/tmp/BGENV_XXXXXX";
	{
		int fd = mkstemp(configfilepath);
		ck_assert_int_ne(fd, -1);

		FILE *of = fdopen(fd, "w");
		ck_assert_ptr_nonnull(of);
		int count = fwrite(&data, sizeof(BG_ENVDATA), 1, of);
		ck_assert_int_eq(count, 1);

		fclose(of);
		close(fd);
	}

	/* load our manipulated BGENV */
	memset(&data, 0, sizeof(data));
	bool result = get_env(configfilepath, &data);

	/* ensure that we did not write invalid data */
	BGENV bgenv = {.desc = configfilepath, .data = &data};
	ebgenv_t e = {.bgenv = &bgenv};
	char out[16];
	memset(out, 0, sizeof(out));
	/* must not crash */
	ebg_env_get(&e, key, out);
	/* silences cppcheck nullPointerRedundantCheck over ck_assert_str_eq */
	const char *empty_str = "";
	/* ensure we did not read invalid data */
	ck_assert_str_eq(out, empty_str);

	/* assert that get_env reports an error */
	ck_assert_int_eq(result, false);

	/* clean up */
	unlink(configfilepath);
}
END_TEST

Suite *ebg_test_suite(void)
{
	Suite *s;
	TCase *tc_core;

	s = suite_create("uservars");

	tc_core = tcase_create("Core");

	tcase_add_test(tc_core, bgenv_get_from_manipulated);

	suite_add_tcase(s, tc_core);

	return s;
}
