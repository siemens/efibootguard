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

#include "env_api.h"
#include "ebgenv.h"
#include "uservars.h"

/* UEFI uses 16-bit wide unicode strings.
 * However, wchar_t support functions are fixed to 32-bit wide
 * characters in glibc. This code is compiled with
 *  -fshort-wchar
 * which enables 16-bit wide wchar_t support. However,
 * glibc functions do not work with 16-bit wchar_t input, except
 * it was specifically compiled for that, which is unusual.
 * Thus, the needed conversion by truncation function is
 * reimplemented here.
 */
char *str16to8(char *buffer, wchar_t *src)
{
	if (!src || !buffer) {
		return NULL;
	}
	char *tmp = buffer;
	while (*src) {
		*buffer = (char)*src;
		src++;
		buffer++;
	}
	*buffer = 0;
	return tmp;
}

wchar_t *str8to16(wchar_t *buffer, char *src)
{
	if (!src || !buffer) {
		return NULL;
	}
	wchar_t *tmp = buffer;
	while (*src) {
		*buffer = (wchar_t)*src;
		src++;
		buffer++;
	}
	*buffer = 0;
	return tmp;
}

void ebg_beverbose(ebgenv_t *e, bool v)
{
	bgenv_be_verbose(v);
}

int ebg_env_create_new(ebgenv_t *e)
{
	if (!bgenv_init()) {
		return EIO;
	}

	if (!e->ebg_new_env_created) {
		BGENV *latest_env = bgenv_open_latest();
		e->bgenv = (void *)bgenv_create_new();
		BG_ENVDATA *new_data = ((BGENV *)e->bgenv)->data;
		uint32_t new_rev = new_data->revision;
		uint8_t new_ustate = new_data->ustate;
		memcpy(new_data, latest_env->data, sizeof(BG_ENVDATA));
		new_data->revision = new_rev;
		new_data->ustate = new_ustate;
		bgenv_close(latest_env);
	}

	e->ebg_new_env_created = e->bgenv != NULL;

	return e->bgenv == NULL ? errno : 0;
}

int ebg_env_open_current(ebgenv_t *e)
{
	if (!bgenv_init()) {
		return EIO;
	}

	e->bgenv = (void *)bgenv_open_latest();

	return e->bgenv == NULL ? EIO : 0;
}

int ebg_env_get(ebgenv_t *e, char *key, char *buffer)
{
	return bgenv_get((BGENV *)e->bgenv, key, NULL, buffer,
			 ENV_STRING_LENGTH);
}

int ebg_env_get_ex(ebgenv_t *e, char *key, uint64_t *usertype, uint8_t *buffer,
		   uint32_t maxlen)
{
	return bgenv_get((BGENV *)e->bgenv, key, usertype, buffer, maxlen);
}

int ebg_env_set(ebgenv_t *e, char *key, char *value)
{
	return bgenv_set((BGENV *)e->bgenv, key, USERVAR_TYPE_DEFAULT, value,
		         strlen(value) + 1);
}

int ebg_env_set_ex(ebgenv_t *e, char *key, uint64_t usertype, uint8_t *value,
		   uint32_t datalen)
{
	return bgenv_set((BGENV *)e->bgenv, key, usertype, value, datalen);
}

uint32_t ebg_env_user_free(ebgenv_t *e)
{
	if (!e->bgenv) {
		return 0;
	}
	if (!((BGENV *)e->bgenv)->data) {
		return 0;
	}
	return bgenv_user_free(((BGENV *)e->bgenv)->data->userdata);
}

uint16_t ebg_env_getglobalstate(ebgenv_t *e)
{
	BGENV *env;
	int res = 4;

	/* find all environments with revision 0 */
	for (int i = 0; i < ENV_NUM_CONFIG_PARTS; i++) {
		BGENV *env = bgenv_open_by_index(i);

		if (!env) {
			continue;
		}
		/* update was unsuccessful if there is a config,
		 * with revision == REVISION_FAILED and
		 * with ustate == USTATE_FAILED */
		if (env->data->revision == REVISION_FAILED &&
		    env->data->ustate == USTATE_FAILED) {
			res = 3;
		}
		(void)bgenv_close(env);
		if (res == 3) {
			return res;
		}
	}

	env = bgenv_open_latest();
	if (!env) {
		errno = EIO;
		return res;
	}

	res = env->data->ustate;
	bgenv_close(env);

	return res;
}

int ebg_env_setglobalstate(ebgenv_t *e, uint16_t ustate)
{
	char buffer[2];
	int res;

	if (ustate > USTATE_FAILED) {
		return EINVAL;
	}
	(void)snprintf(buffer, sizeof(buffer), "%d", ustate);
	res = bgenv_set((BGENV *)e->bgenv, "ustate", 0, buffer,
			strlen(buffer) + 1);

	if (ustate != USTATE_OK) {
		return res;
	}

	for (int i = 0; i < ENV_NUM_CONFIG_PARTS; i++) {
		BGENV *env = bgenv_open_by_index(i);

		if (!env) {
			continue;
		}
		env->data->ustate = ustate;
		if (!bgenv_write(env)) {
			(void)bgenv_close(env);
			return EIO;
		}
		if (!bgenv_close(env)) {
			return EIO;
		}
	}
	return 0;
}

int ebg_env_close(ebgenv_t *e)
{
	/* if no environment is open, just return EIO */
	if (!e->bgenv) {
		return EIO;
	}

	BGENV *env_current;
	env_current = (BGENV *)e->bgenv;

	/* recalculate checksum */
	env_current->data->crc32 =
	    crc32(0, (Bytef *)env_current->data,
		  sizeof(BG_ENVDATA) - sizeof(env_current->data->crc32));
	/* save */
	if (!bgenv_write(env_current)) {
		(void)bgenv_close(env_current);
		return EIO;
	}
	if (!bgenv_close(env_current)) {
		return EIO;
	}
	e->bgenv = NULL;
	return 0;
}
