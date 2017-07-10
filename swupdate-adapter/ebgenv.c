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

#include "bg_utils.h"
#include "ebgdefs.h"
#include "ebgenv.h"

typedef enum {
	EBGENV_KERNELFILE,
	EBGENV_KERNELPARAMS,
	EBGENV_WATCHDOG_TIMEOUT_SEC,
	EBGENV_REVISION,
	EBGENV_BOOT_ONCE,
	EBGENV_TESTING,
	EBGENV_UNKNOWN
} EBGENVKEY;

static BGENV *env_current = NULL;

typedef struct _PCOLLECTOR {
	void *p;
	struct _PCOLLECTOR *next;
} PCOLLECTOR;

static PCOLLECTOR ebg_gc;

static bool ebg_new_env_created = false;

static bool ebg_gc_addpointer(void *pnew)
{
	PCOLLECTOR *pc = &ebg_gc;

	while (pc->next) {
		pc = pc->next;
	}
	pc->next = calloc(sizeof(PCOLLECTOR), 1);
	if (!pc->next) {
		return false;
	}
	pc = pc->next;
	pc->p = pnew;
	return true;
}

static void ebg_gc_cleanup()
{
	PCOLLECTOR *pc = &ebg_gc;

	while (pc) {
		if (pc->p) {
			free(pc->p);
		}
		PCOLLECTOR *tmp = pc;
		pc = pc->next;
		if (tmp != &ebg_gc) {
			free(tmp);
		}
	}
	ebg_gc.p = NULL;
	ebg_gc.next = NULL;
}

static EBGENVKEY ebg_env_str2enum(char *key)
{
	if (strncmp(key, "kernelfile", strlen("kernelfile") + 1) == 0) {
		return EBGENV_KERNELFILE;
	}
	if (strncmp(key, "kernelparams", strlen("kernelparams") + 1) == 0) {
		return EBGENV_KERNELPARAMS;
	}
	if (strncmp(key, "watchdog_timeout_sec",
		    strlen("watchdog_timeout_sec") + 1) == 0) {
		return EBGENV_WATCHDOG_TIMEOUT_SEC;
	}
	if (strncmp(key, "revision", strlen("revision") + 1) == 0) {
		return EBGENV_REVISION;
	}
	if (strncmp(key, "boot_once", strlen("boot_once") + 1) == 0) {
		return EBGENV_BOOT_ONCE;
	}
	if (strncmp(key, "testing", strlen("testing") + 1) == 0) {
		return EBGENV_TESTING;
	}
	return EBGENV_UNKNOWN;
}

void ebg_beverbose(bool v)
{
	be_verbose(v);
}

int ebg_env_create_new(void)
{
	/* initialize garbage collector */
	ebg_gc.p = NULL;
	ebg_gc.next = NULL;

	if (!bgenv_init(BGENVTYPE_FAT)) {
		return EIO;
	}

	env_current = bgenv_get_latest(BGENVTYPE_FAT);

	if (ebg_new_env_created)
		return env_current == NULL ? EIO : 0;

	if (!env_current) {
		return EIO;
	}
	/* first time env is opened, a new one is created for update
	 * purpose */
	int new_rev = env_current->data->revision + 1;

	if (!bgenv_close(env_current)) {
		return EIO;
	}
	env_current = bgenv_get_oldest(BGENVTYPE_FAT);
	if (!env_current) {
		return EIO;
	}
	/* zero fields */
	memset(env_current->data, 0, sizeof(BG_ENVDATA));
	/* update revision field and testing mode */
	env_current->data->revision = new_rev;
	env_current->data->testing = 1;
	/* set default watchdog timeout */
	env_current->data->watchdog_timeout_sec = 30;
	ebg_new_env_created = true;

	return env_current == NULL ? EIO : 0;
}

int ebg_env_open_current(void)
{
	/* initialize garbage collector */
	ebg_gc.p = NULL;
	ebg_gc.next = NULL;

	if (!bgenv_init(BGENVTYPE_FAT)) {
		return EIO;
	}

	env_current = bgenv_get_latest(BGENVTYPE_FAT);

	return env_current == NULL ? EIO : 0;
}

char *ebg_env_get(char *key)
{
	EBGENVKEY e;
	char *buffer;

	if (!key) {
		errno = EINVAL;
		return NULL;
	}
	e = ebg_env_str2enum(key);
	if (e == EBGENV_UNKNOWN) {
		errno = EINVAL;
		return NULL;
	}
	if (!env_current) {
		errno = EPERM;
		return NULL;
	}
	switch (e) {
	case EBGENV_KERNELFILE:
		buffer = (char *)malloc(ENV_STRING_LENGTH);
		if (!buffer) {
			errno = ENOMEM;
			return NULL;
		}
		if (!ebg_gc_addpointer(buffer)) {
			errno = ENOMEM;
			return NULL;
		};
		str16to8(buffer, env_current->data->kernelfile);
		return buffer;
	case EBGENV_KERNELPARAMS:
		buffer = (char *)malloc(ENV_STRING_LENGTH);
		if (!buffer) {
			errno = ENOMEM;
			return NULL;
		}
		if (!ebg_gc_addpointer(buffer)) {
			errno = ENOMEM;
			return NULL;
		}
		str16to8(buffer, env_current->data->kernelparams);
		return buffer;
	case EBGENV_WATCHDOG_TIMEOUT_SEC:
		if (asprintf(&buffer, "%lu",
			     env_current->data->watchdog_timeout_sec) < 0) {
			errno = ENOMEM;
			return NULL;
		}
		if (!ebg_gc_addpointer(buffer)) {
			errno = ENOMEM;
			return NULL;
		}
		return buffer;
	case EBGENV_REVISION:
		if (asprintf(&buffer, "%lu", env_current->data->revision) < 0) {
			errno = ENOMEM;
			return NULL;
		}
		if (!ebg_gc_addpointer(buffer)) {
			errno = ENOMEM;
			return NULL;
		}
		return buffer;
	case EBGENV_BOOT_ONCE:
		if (asprintf(&buffer, "%lu", env_current->data->boot_once) <
		    0) {
			errno = ENOMEM;
			return NULL;
		}
		if (!ebg_gc_addpointer(buffer)) {
			errno = ENOMEM;
			return NULL;
		}
		return buffer;
	case EBGENV_TESTING:
		if (asprintf(&buffer, "%lu", env_current->data->testing) < 0) {
			errno = ENOMEM;
			return NULL;
		}
		if (!ebg_gc_addpointer(buffer)) {
			errno = ENOMEM;
			return NULL;
		}
		return buffer;
	default:
		errno = EINVAL;
		return NULL;
	}
	errno = EINVAL;
	return NULL;
}

int ebg_env_set(char *key, char *value)
{
	EBGENVKEY e;
	int val;
	char *p;

	if (!key || !value) {
		return EINVAL;
	}
	e = ebg_env_str2enum(key);
	if (e == EBGENV_UNKNOWN) {
		return EINVAL;
	}
	if (!env_current) {
		return EPERM;
	}
	switch (e) {
	case EBGENV_REVISION:
		errno = 0;
		val = strtol(value, &p, 10);
		if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) ||
		    (errno != 0 && val == 0)) {
			return errno;
		}
		if (p == value) {
			return EINVAL;
		}
		env_current->data->revision = val;
		break;
	case EBGENV_KERNELFILE:
		str8to16(env_current->data->kernelfile, value);
		break;
	case EBGENV_KERNELPARAMS:
		str8to16(env_current->data->kernelparams, value);
		break;
	case EBGENV_WATCHDOG_TIMEOUT_SEC:
		errno = 0;
		val = strtol(value, &p, 10);
		if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) ||
		    (errno != 0 && val == 0)) {
			return errno;
		}
		if (p == value) {
			return EINVAL;
		}
		env_current->data->watchdog_timeout_sec = val;
		break;
	case EBGENV_BOOT_ONCE:
		errno = 0;
		val = strtol(value, &p, 10);
		if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) ||
		    (errno != 0 && val == 0)) {
			return errno;
		}
		if (p == value) {
			return EINVAL;
		}
		env_current->data->boot_once = val;
		break;
	case EBGENV_TESTING:
		errno = 0;
		val = strtol(value, &p, 10);
		env_current->data->testing = val;
		if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) ||
		    (errno != 0 && val == 0)) {
			return errno;
		}
		if (p == value) {
			return EINVAL;
		}
		break;
	default:
		return EINVAL;
	}
	return 0;
}

bool ebg_env_isupdatesuccessful(void)
{
	/* find all environments with revision 0 */
	for (int i = 0; i < CONFIG_PARTITION_COUNT; i++) {
		BGENV *env = bgenv_get_by_index(BGENVTYPE_FAT, i);

		if (!env) {
			continue;
		}
		/* update was unsuccessful if there is a revision 0 config,
		 * with
		 * testing and boot_once set */
		if (env->data->revision == REVISION_FAILED &&
		    env->data->testing == 1 && env->data->boot_once == 1) {
			(void)bgenv_close(env);
			return false;
		}
		(void)bgenv_close(env);
	}
	return true;
}

int ebg_env_clearerrorstate(void)
{
	for (int i = 0; i < CONFIG_PARTITION_COUNT; i++) {
		BGENV *env = bgenv_get_by_index(BGENVTYPE_FAT, i);

		if (!env) {
			continue;
		}
		if (env->data->revision == REVISION_FAILED &&
		    env->data->testing == 1 && env->data->boot_once == 1) {
			env->data->testing = 0;
			env->data->boot_once = 0;
			if (!bgenv_write(env)) {
				(void)bgenv_close(env);
				return EIO;
			}
		}
		if (!bgenv_close(env)) {
			return EIO;
		}
	}
	return 0;
}

int ebg_env_confirmupdate(void)
{
	int ret = ebg_env_set("testing", "0");

	if (ret) {
		return ret;
	}
	return ebg_env_set("boot_once", "0");
}

bool ebg_env_isokay(void)
{
	BGENV *env;
	bool res = false;

	env = bgenv_get_latest(BGENVTYPE_FAT);
	if (!env) {
		errno = EIO;
		return res;
	}
	if (env->data->testing == 0) {
		res = true;
	}
	bgenv_close(env);
	return res;
}

bool ebg_env_isinstalled(void)
{
	BGENV *env;
	bool res = false;

	env = bgenv_get_latest(BGENVTYPE_FAT);
	if (!env) {
		errno = EIO;
		return res;
	}
	if (env->data->testing == 1 && env->data->boot_once == 0) {
		res = true;
	}
	bgenv_close(env);
	return res;
}

bool ebg_env_istesting(void)
{
	BGENV *env;
	bool res = false;

	env = bgenv_get_latest(BGENVTYPE_FAT);
	if (!env) {
		errno = EIO;
		return res;
	}
	if (env->data->testing == 1 && env->data->boot_once == 1) {
		res = true;
	}
	bgenv_close(env);
	return res;
}

int ebg_env_close(void)
{
	/* free all allocated memory */
	ebg_gc_cleanup();

	/* if no environment is open, just return EIO */
	if (!env_current) {
		return EIO;
	}

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
	env_current = NULL;
	return 0;
}
