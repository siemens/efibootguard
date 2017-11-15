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

#include <string.h>
#include "env_api.h"
#include "uservars.h"

void bgenv_map_uservar(uint8_t *udata, char **key, uint64_t *type, uint8_t **val,
		       uint32_t *record_size, uint32_t *data_size)
{
	/* Each user variable is encoded as follows:
	 * |------------|--------------|---------------|----------------|
	 * | char KEY[] | uint32_t len | uint64_t type | uint8_t data[] |
	 * |------------|--------------|---------------|----------------|
	 * |   KEY      | < - - - - - - - - PAYLOAD - - - - - - - - - > |
	 *
	 * here char[] is a null-terminated string
	 * 'len' is the payload size (visualized by the horizontal dashes)
	 *
	 * type is partitioned into the following bit fields:
	 * | 63      ...       49 | 48     ...     32 | 31     ...    0 |
	 * |    internal flags    |   user defined    |  standard types |
	 * |      (reserved)      |  (free for user)  |    (reserved)   |
	 *
	 * internal flags and standard types are declared in ebgenv.h
	 */
	char *var_key;
	uint32_t *payload_size;
	uint64_t *var_type;
	uint8_t *data;

	/* Get the key */
	var_key = (char *)udata;
	if (key) {
		*key = var_key;
	}

	/* Get position of payload size */
	payload_size = (uint32_t *)(var_key + strlen(var_key) + 1);

	/* Calculate the record size (size of the whole thing) */
	if (record_size) {
		*record_size = *payload_size + strlen(var_key) + 1;
	}

	/* Get position of the type field */
	var_type = (uint64_t *)((uint8_t *)payload_size + sizeof(uint32_t));
	if (type) {
		*type = *var_type;
	}

	/* Calculate the data size */
	if (data_size) {
		*data_size = *payload_size - sizeof(uint32_t) -
			     sizeof(uint64_t);
	}
	/* Get the pointer to the data field */
	data = (uint8_t *)var_type + sizeof(uint64_t);
	if (val) {
		*val = data;
	}
}

void bgenv_serialize_uservar(uint8_t *p, char *key, uint64_t type, void *data,
			    uint32_t record_size)
{
	uint32_t payload_size, data_size;

	/* store key */
	strncpy((char *)p, key, strlen(key) + 1);
	p += strlen(key) + 1;

	/* store payload_size after key */
	payload_size = record_size - strlen(key) - 1;
	memcpy(p, &payload_size, sizeof(uint32_t));
	p += sizeof(uint32_t);

	/* store datatype */
	*((uint64_t *)p) = type;
	p += sizeof(uint64_t);

	/* store data */
	data_size = payload_size - sizeof(uint32_t) - sizeof(uint64_t);
	memcpy(p, data, data_size);
}

int bgenv_get_uservar(uint8_t *udata, char *key, uint64_t *type, void *data,
		      uint32_t maxlen)
{
	uint8_t *uservar, *value;
	char *lkey;
	uint32_t dsize;
	uint64_t ltype;

	uservar = bgenv_find_uservar(udata, key);

	if (!uservar) {
		return -EINVAL;
	}

	bgenv_map_uservar(uservar, &lkey, &ltype, &value, NULL, &dsize);

	if (dsize > maxlen) {
		dsize = maxlen;
	}

	memcpy(data, value, dsize);

	if (type) {
		*type = ltype;
	}

	return 0;
}

int bgenv_set_uservar(uint8_t *udata, char *key, uint64_t type, void *data,
	              uint32_t datalen)
{
	uint32_t total_size;
	uint8_t *p;

	total_size = datalen + sizeof(uint64_t) + sizeof(uint32_t) +
		     strlen(key) + 1;

	p = bgenv_find_uservar(udata, key);
	if (p) {
		if (type & USERVAR_TYPE_DELETED) {
			bgenv_del_uservar(udata, p);
			return 0;
		}

		p = bgenv_uservar_realloc(udata, total_size, p);
	} else {
		if ((type & USERVAR_TYPE_DELETED) == 0) {
			p = bgenv_uservar_alloc(udata, total_size);
		} else {
			return 0;
		}
	}
	if (!p) {
		return -errno;
	}

	bgenv_serialize_uservar(p, key, type, data, total_size);

	return 0;
}

uint8_t *bgenv_find_uservar(uint8_t *udata, char *key)
{
	char *varkey;

	if (!udata) {
		return NULL;
	}
	while (*udata) {
		bgenv_map_uservar(udata, &varkey, NULL, NULL, NULL, NULL);

		if (strncmp(varkey, key, strlen(key) + 1) == 0) {
			return udata;
		}
		udata = bgenv_next_uservar(udata);
	}
	return NULL;
}

uint8_t *bgenv_next_uservar(uint8_t *udata)
{
	uint32_t record_size;

	bgenv_map_uservar(udata, NULL, NULL, NULL, &record_size, NULL);

	return udata + record_size;
}

uint8_t *bgenv_uservar_alloc(uint8_t *udata, uint32_t datalen)
{
	uint32_t spaceleft;

	if (!udata) {
		errno = EINVAL;
		return NULL;
	}
	spaceleft = bgenv_user_free(udata);
	VERBOSE(stdout, "uservar_alloc: free: %lu requested: %lu \n",
		(unsigned long)spaceleft, (unsigned long)datalen);

	/* To find the end of user variables, a 2nd 0 must be there after the
	 * last variable content, thus, we need one extra byte if appending a
	 * new variable. */
	if (spaceleft < datalen + 1) {
		errno = ENOMEM;
		return NULL;
	}

	return udata + (ENV_MEM_USERVARS - spaceleft);
}

uint8_t *bgenv_uservar_realloc(uint8_t *udata, uint32_t new_rsize,
			       uint8_t *p)
{
	uint32_t spaceleft;
	uint32_t rsize;

	bgenv_map_uservar(p, NULL, NULL, NULL, &rsize, NULL);

	/* Is the new record size equal to the old, so that we can
	 * keep the variable in place? */
	if (new_rsize == rsize) {
		return p;
	}

	/* Delete variable and return pointer to end of whole user vars */
	bgenv_del_uservar(udata, p);

	spaceleft = bgenv_user_free(udata);

	if (spaceleft < new_rsize - 1) {
		errno = ENOMEM;
		return NULL;
	}

	return udata + ENV_MEM_USERVARS - spaceleft;
}

void bgenv_del_uservar(uint8_t *udata, uint8_t *var)
{
	uint32_t spaceleft;
	uint32_t rsize;

	/* Get the record size of the variable */
	bgenv_map_uservar(var, NULL, NULL, NULL, &rsize, NULL);

	/* Move variable out of place and close gap. */
	spaceleft = bgenv_user_free(udata);

	memmove(var,
	        var + rsize,
	        ENV_MEM_USERVARS - spaceleft - (var - udata) - rsize);

	spaceleft = spaceleft + rsize;

	memset(udata + ENV_MEM_USERVARS - spaceleft, 0, spaceleft);
}

uint32_t bgenv_user_free(uint8_t *udata)
{
	uint32_t rsize;
	uint32_t spaceleft;

	spaceleft = ENV_MEM_USERVARS;

	if (!udata) {
		return 0;
	}
	if (!*udata) {
		return spaceleft;
	}

	while (*udata) {
		bgenv_map_uservar(udata, NULL, NULL, NULL, &rsize, NULL);
		spaceleft -= rsize;
		if (spaceleft == 0) {
			break;
		}
		udata = bgenv_next_uservar(udata);
	}

	return spaceleft;
}
