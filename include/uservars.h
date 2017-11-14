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

#ifndef __USER_VARS_H__
#define __USER_VARS_H__

#include <stdint.h>

void bgenv_map_uservar(uint8_t *udata, char **key, uint64_t *type,
		       uint8_t **val, uint32_t *record_size,
		       uint32_t *data_size);
void bgenv_serialize_uservar(uint8_t *p, char *key, uint64_t type, void *data,
			     uint32_t record_size);

int bgenv_get_uservar(uint8_t *udata, char *key, uint64_t *type, void *data,
		      uint32_t maxlen);
int bgenv_set_uservar(uint8_t *udata, char *key, uint64_t type, void *data,
	              uint32_t datalen);

uint8_t *bgenv_find_uservar(uint8_t *udata, char *key);
uint8_t *bgenv_next_uservar(uint8_t *udata);

uint8_t *bgenv_uservar_alloc(uint8_t *udata, uint32_t datalen);
uint8_t *bgenv_uservar_realloc(uint8_t *udata, uint32_t new_rsize,
			       uint8_t *p);
void bgenv_del_uservar(uint8_t *udata, uint8_t *var);
uint32_t bgenv_user_free(uint8_t *udata);

#endif // __USER_VARS_H__
