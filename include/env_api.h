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

#ifndef __ENV_API_H__
#define __ENV_API_H__

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
#include <zlib.h>
#include "envdata.h"
#include "ebgenv.h"

#ifdef DEBUG
#define printf_debug(fmt, ...) printf(fmt, __VA_ARGS__)
#else
#define printf_debug(fmt, ...)                                                 \
	{                                                                      \
	}
#endif

extern bool bgenv_verbosity;

#define VERBOSE(o, ...)                                                       \
	if (bgenv_verbosity)                                                    \
	fprintf(o, __VA_ARGS__)

typedef enum {
	EBGENV_KERNELFILE,
	EBGENV_KERNELPARAMS,
	EBGENV_WATCHDOG_TIMEOUT_SEC,
	EBGENV_REVISION,
	EBGENV_USTATE,
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

extern void bgenv_be_verbose(bool v);

extern char *str16to8(char *buffer, wchar_t *src);
extern wchar_t *str8to16(wchar_t *buffer, char *src);

extern bool bgenv_init(void);
extern BGENV *bgenv_open_by_index(uint32_t index);
extern BGENV *bgenv_open_oldest(void);
extern BGENV *bgenv_open_latest(void);
extern bool bgenv_write(BGENV *env);
extern BG_ENVDATA *bgenv_read(BGENV *env);
extern bool bgenv_close(BGENV *env);

extern BGENV *bgenv_create_new(void);
extern int bgenv_get(BGENV *env, char *key, uint64_t *type, void *data,
		     uint32_t maxlen);
extern int bgenv_set(BGENV *env, char *key, uint64_t type, void *data,
		     uint32_t datalen);
extern int bgenv_set_uservar_global(char *key, uint64_t type,
				    void *data, uint32_t datalen);
extern uint8_t *bgenv_find_uservar(uint8_t *userdata, char *key);

#endif // __ENV_API_H__
