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

#pragma once

#include <stdbool.h>
#include <stdint.h>

void bgenv_map_uservar(uint8_t *udata, char **key, uint64_t *type,
		       uint8_t **val, uint32_t *record_size,
		       uint32_t *data_size);

int bgenv_get_uservar(uint8_t *udata, const char *key, uint64_t *type,
		      void *data, uint32_t maxlen);
int bgenv_set_uservar(uint8_t *udata, const char *key, uint64_t type,
		      const void *data, uint32_t datalen);

uint8_t *bgenv_find_uservar(uint8_t *udata, const char *key);
uint8_t *bgenv_next_uservar(uint8_t *udata);

void bgenv_del_uservar(uint8_t *udata, uint8_t *var);
uint32_t bgenv_user_free(uint8_t *udata);

bool bgenv_validate_uservars(uint8_t *udata);
