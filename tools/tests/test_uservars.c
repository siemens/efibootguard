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
 * SPDX-License-Identifier:	GPL-2.0-only
 */

#include <stdlib.h>
#include <check.h>
#include <fff.h>

#include <env_api.h>
#include <uservars.h>
#include <bg_envtools.h>

DEFINE_FFF_GLOBALS;

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

Suite *ebg_test_suite(void);

/* These variables substitute weakened symbols in the ebgenv library code
 * so that all environment functions use these as data sources
 */
CONFIG_PART config_parts[ENV_NUM_CONFIG_PARTS];
BG_ENVDATA envdata[ENV_NUM_CONFIG_PARTS];

static const struct {
	uint64_t type;
	uint32_t size;
} standard_types[] = {
	{ USERVAR_TYPE_CHAR, 1 },
	{ USERVAR_TYPE_UINT8, 1 },
	{ USERVAR_TYPE_UINT16, 2 },
	{ USERVAR_TYPE_UINT32, 4 },
	{ USERVAR_TYPE_UINT64, 8 },
	{ USERVAR_TYPE_SINT8, 1 },
	{ USERVAR_TYPE_SINT16, 2 },
	{ USERVAR_TYPE_SINT32, 4 },
	{ USERVAR_TYPE_SINT64, 8 },
	{ USERVAR_TYPE_STRING_ASCII, 1 },
	{ USERVAR_TYPE_BOOL, 1 },
};

/* bits 32..48 of the type are free for user-defined information */
static const uint64_t user_bits[] = { 0, 1ULL << 32, 0x1ffffULL << 32 };

/* Append a raw uservar record, bypassing all checks of bgenv_set_uservar */
static uint8_t *put_uservar_record(uint8_t *p, const char *key, uint64_t type,
				   const void *data, uint32_t data_size)
{
	uint32_t payload_size = sizeof(uint32_t) + sizeof(uint64_t) + data_size;

	strcpy((char *)p, key);
	p += strlen(key) + 1;
	memcpy(p, &payload_size, sizeof(payload_size));
	p += sizeof(payload_size);
	memcpy(p, &type, sizeof(type));
	p += sizeof(type);
	memcpy(p, data, data_size);
	return p + data_size;
}

START_TEST(bgenv_validate_uservars_type_size)
{
	const uint8_t value[8] = { 0 };
	uint8_t *udata, *p;

	udata = calloc(1, ENV_MEM_USERVARS);
	ck_assert_ptr_nonnull(udata);

	for (unsigned int i = 0; i < ARRAY_SIZE(standard_types); i++) {
		for (unsigned int j = 0; j < ARRAY_SIZE(user_bits); j++) {
			uint64_t type = standard_types[i].type | user_bits[j];
			uint32_t size = standard_types[i].size;

			/* undersized value after a valid uservar */
			memset(udata, 0, ENV_MEM_USERVARS);
			p = put_uservar_record(udata, "valid",
					       USERVAR_TYPE_UINT8, value, 1);
			put_uservar_record(p, "var", type, value, size - 1);
			ck_assert_msg(!bgenv_validate_uservars(udata),
				      "type 0x%llx accepted with %u bytes",
				      (unsigned long long)type, size - 1);

			memset(udata, 0, ENV_MEM_USERVARS);
			p = put_uservar_record(udata, "valid",
					       USERVAR_TYPE_UINT8, value, 1);
			put_uservar_record(p, "var", type, value, size);
			ck_assert_msg(bgenv_validate_uservars(udata),
				      "type 0x%llx rejected with %u bytes",
				      (unsigned long long)type, size);
		}
	}

	/* types without a standard part have no minimal size */
	memset(udata, 0, ENV_MEM_USERVARS);
	put_uservar_record(udata, "custom", 1ULL << 36, value, 0);
	ck_assert(bgenv_validate_uservars(udata));

	free(udata);
}
END_TEST

START_TEST(bgenv_set_uservar_type_size)
{
	const uint8_t value[8] = { 0 };
	uint8_t *udata;
	int res;

	udata = calloc(1, ENV_MEM_USERVARS);
	ck_assert_ptr_nonnull(udata);

	for (unsigned int i = 0; i < ARRAY_SIZE(standard_types); i++) {
		for (unsigned int j = 0; j < ARRAY_SIZE(user_bits); j++) {
			uint64_t type = standard_types[i].type | user_bits[j];
			uint32_t size = standard_types[i].size;

			res = bgenv_set_uservar(udata, "var", type, value,
						size - 1);
			ck_assert_int_eq(res, -EINVAL);
			ck_assert_ptr_null(bgenv_find_uservar(udata, "var"));

			res = bgenv_set_uservar(udata, "var", type, value,
						size);
			ck_assert_int_eq(res, 0);
			ck_assert_ptr_nonnull(bgenv_find_uservar(udata, "var"));
			ck_assert(bgenv_validate_uservars(udata));

			/* a rejected update must preserve the existing value */
			res = bgenv_set_uservar(udata, "var", type, value,
						size - 1);
			ck_assert_int_eq(res, -EINVAL);
			ck_assert_ptr_nonnull(bgenv_find_uservar(udata, "var"));

			bgenv_set_uservar(udata, "var", USERVAR_TYPE_DELETED,
					  NULL, 0);
			ck_assert_ptr_null(bgenv_find_uservar(udata, "var"));
		}
	}

	/* types without a standard part have no minimal size */
	res = bgenv_set_uservar(udata, "custom", 1ULL << 36, value, 0);
	ck_assert_int_eq(res, 0);
	ck_assert(bgenv_validate_uservars(udata));

	free(udata);
}
END_TEST

START_TEST(bgenv_set_uservar_string_termination)
{
	uint8_t *udata;
	char *value;
	char out[8];
	int res;

	udata = calloc(1, ENV_MEM_USERVARS);
	ck_assert_ptr_nonnull(udata);

	/* sized exactly, so that reading beyond it is detectable */
	value = malloc(3);
	ck_assert_ptr_nonnull(value);
	memcpy(value, "xyz", 3);

	for (unsigned int j = 0; j < ARRAY_SIZE(user_bits); j++) {
		uint64_t type = USERVAR_TYPE_STRING_ASCII | user_bits[j];

		res = bgenv_set_uservar(udata, "str", type, value, 3);
		ck_assert_int_eq(res, -EINVAL);
		ck_assert_ptr_null(bgenv_find_uservar(udata, "str"));

		/* terminator present but not covered by datalen */
		res = bgenv_set_uservar(udata, "str", type, "abc", 3);
		ck_assert_int_eq(res, -EINVAL);
		ck_assert_ptr_null(bgenv_find_uservar(udata, "str"));

		res = bgenv_set_uservar(udata, "str", type, "abc", 4);
		ck_assert_int_eq(res, 0);

		/* a rejected update must preserve the existing value */
		res = bgenv_set_uservar(udata, "str", type, value, 3);
		ck_assert_int_eq(res, -EINVAL);
		res = bgenv_get_uservar(udata, "str", NULL, out, sizeof(out));
		ck_assert_int_eq(res, 0);
		ck_assert_str_eq(out, "abc");

		bgenv_set_uservar(udata, "str", USERVAR_TYPE_DELETED, NULL, 0);
	}

	/* values of types without a standard part are not interpreted */
	res = bgenv_set_uservar(udata, "custom", 1ULL << 36, value, 3);
	ck_assert_int_eq(res, 0);
	ck_assert(bgenv_validate_uservars(udata));

	free(value);
	free(udata);
}
END_TEST

START_TEST(bgenv_get_from_manipulated)
{
	const char *key = "mykey";
	uint64_t usertype = 1ULL << 36;

	BG_ENVDATA data = {.ustate = USTATE_OK};
	/* create a manipulated BGENV (in-memory) */
	{
		const char *value = "dummy";
		size_t value_len = strlen(value);
		bgenv_set_uservar(data.userdata, key, usertype, value,
				  value_len);
		/* get position of payload size */
		char *var_key = (char *)bgenv_find_uservar(data.userdata, key);
		void *payload_size = var_key + strlen(var_key) + 1;
		uint32_t payload_val;
		memcpy(&payload_val, payload_size, sizeof(payload_val));
		/* sanity check */
		ck_assert_int_eq(payload_val, sizeof(uint32_t) +
					      sizeof(uint64_t) +
					      value_len);

		/* manipulate payload_size */
		payload_val = UINT32_MAX;
		memcpy(payload_size, &payload_val, sizeof(payload_val));
		/* fix checksum */
		data.crc32 =
			bgenv_crc32(0, &data,
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
	tcase_add_test(tc_core, bgenv_validate_uservars_type_size);
	tcase_add_test(tc_core, bgenv_set_uservar_type_size);
	tcase_add_test(tc_core, bgenv_set_uservar_string_termination);

	suite_add_tcase(s, tc_core);

	return s;
}
