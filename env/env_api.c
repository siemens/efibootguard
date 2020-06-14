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

	BGENV *latest_env = bgenv_open_latest();
	if (!latest_env) {
		return EIO;
	}

	BG_ENVDATA *latest_data = ((BGENV *)latest_env)->data;

	if (latest_data->in_progress != 1) {
		e->bgenv = (void *)bgenv_create_new();
		if (!e->bgenv) {
			bgenv_close(latest_env);
			return errno;
		}
		BG_ENVDATA *new_data = ((BGENV *)e->bgenv)->data;
		uint32_t new_rev = new_data->revision;
		uint8_t new_in_progress = new_data->in_progress;
		memcpy(new_data, latest_env->data, sizeof(BG_ENVDATA));
		new_data->revision = new_rev;
		new_data->in_progress = new_in_progress;
		bgenv_close(latest_env);
	} else {
		e->bgenv = latest_env;
	}

	return 0;
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
	return bgenv_set((BGENV *)e->bgenv, key, USERVAR_TYPE_DEFAULT |
			 USERVAR_TYPE_STRING_ASCII, value,
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
		bgenv_close(env);
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
		return -EINVAL;
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
		if (env->data->ustate != ustate) {
			env->data->ustate = ustate;
			env->data->crc32 = crc32(0, (Bytef *)env->data,
				sizeof(BG_ENVDATA) - sizeof(env->data->crc32));
			if (!bgenv_write(env)) {
				bgenv_close(env);
				return -EIO;
			}
		}
		bgenv_close(env);
	}
	return 0;
}

int ebg_env_close(ebgenv_t *e)
{
	int res = 0;

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
		res = EIO;
	}
	bgenv_close(env_current);
	e->bgenv = NULL;
	bgenv_finalize();
	return res;
}

int ebg_env_register_gc_var(ebgenv_t *e, char *key)
{
	GC_ITEM **pgci;
	pgci = (GC_ITEM **)&e->gc_registry;

	if (!key) {
		return EINVAL;
	}
	while (*pgci) {
		pgci = &((*pgci)->next);
	}
	*pgci = (GC_ITEM *)calloc(1, sizeof(GC_ITEM));
	if (!*pgci) {
		return ENOMEM;
	}
	if (asprintf(&((*pgci)->key), "%s", key) == -1) {
		free(*pgci);
		*pgci = NULL;
		return ENOMEM;
	}
	return 0;
}

int ebg_env_finalize_update(ebgenv_t *e)
{
	if (!e->bgenv || !((BGENV *)e->bgenv)->data) {
		return EIO;
	}

	GC_ITEM *pgci, *tmp;
	uint8_t *udata;

	pgci = (GC_ITEM *)e->gc_registry;
	udata = ((BGENV *)e->bgenv)->data->userdata;
	while (pgci) {
		uint8_t *var;
		var = bgenv_find_uservar(udata, pgci->key);
		if (var) {
			bgenv_del_uservar(udata, var);
		}
		free(pgci->key);
		tmp = pgci->next;
		free(pgci);
		pgci = tmp;
	}

	((BGENV *)e->bgenv)->data->in_progress = 0;
	((BGENV *)e->bgenv)->data->ustate = USTATE_INSTALLED;
	return 0;
}
