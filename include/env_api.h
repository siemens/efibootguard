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

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <wchar.h>
#include <string.h>
#include <unistd.h>
#include <argp.h>
#include <mntent.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/mount.h>
#include "config.h"
#include "envdata.h"
#include "ebgenv.h"
#include <uchar.h>

#ifdef DEBUG
#define printf_debug(fmt, ...) printf(fmt, __VA_ARGS__)
#else
#define printf_debug(fmt, ...)                                                 \
	{                                                                      \
	}
#endif

#define DEFAULT_TIMEOUT_SEC 30

extern ebgenv_opts_t ebgenv_opts;

#define VERBOSE(o, ...)                                                        \
	if (ebgenv_opts.verbose) fprintf(o, __VA_ARGS__)

typedef enum {
	EBGENV_KERNELFILE,
	EBGENV_KERNELPARAMS,
	EBGENV_WATCHDOG_TIMEOUT_SEC,
	EBGENV_REVISION,
	EBGENV_USTATE,
	EBGENV_IN_PROGRESS,
	EBGENV_UNKNOWN
} EBGENVKEY;

typedef struct {
	char *devpath;
	char *mountpoint;
	bool not_mounted;
} CONFIG_PART;

typedef struct {
	void *desc;
	BG_ENVDATA *data;
} BGENV;

typedef struct gc_item {
	char *key;
	struct gc_item *next;
} GC_ITEM;

extern void bgenv_be_verbose(bool v);

extern char *str16to8(char *buffer, const char16_t *src);
extern char16_t *str8to16(char16_t *buffer, const char *src);

extern uint32_t bgenv_crc32(uint32_t, const void *, size_t);

extern bool bgenv_init(void);
extern void bgenv_finalize(void);
extern BGENV *bgenv_open_by_index(uint32_t index);
extern BGENV *bgenv_open_oldest(void);
extern BGENV *bgenv_open_latest(void);
extern bool bgenv_write(BGENV *env);
extern BG_ENVDATA *bgenv_read(const BGENV *env);
extern void bgenv_close(BGENV *env);

extern BGENV *bgenv_create_new(void);
extern int bgenv_get(BGENV *env, const char *key, uint64_t *type, void *data,
		     uint32_t maxlen);
extern int bgenv_set(BGENV *env, const char *key, uint64_t type,
		     const void *data, uint32_t datalen);
extern uint8_t *bgenv_find_uservar(uint8_t *userdata, const char *key);

extern bool validate_envdata(BG_ENVDATA *data);
