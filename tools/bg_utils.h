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

#ifndef __BG_UTILS_H__
#define __BG_UTILS_H__

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

#include "ebgpart.h"

#include <zlib.h>
#include "envdata.h"

#ifdef DEBUG
#define printf_debug(fmt, ...) printf(fmt, __VA_ARGS__)
#else
#define printf_debug(fmt, ...)                                                 \
	{                                                                      \
	}
#endif

#define VERBOSE(o, ...)                                                       \
	if (verbosity)                                                        \
	fprintf(o, __VA_ARGS__)

typedef enum { BGENVTYPE_FAT } BGENVTYPE;

typedef struct {
	char *devpath;
	char *mountpoint;
	bool not_mounted;
} CONFIG_PART;

typedef struct {
	BGENVTYPE type;
	void *desc;
	BG_ENVDATA *data;
} BGENV;

extern void be_verbose(bool v);

extern char *str16to8(char *buffer, wchar_t *src);
extern wchar_t *str8to16(wchar_t *buffer, char *src);

extern bool bgenv_init(BGENVTYPE type);
extern BGENV *bgenv_get_by_index(BGENVTYPE type, uint32_t index);
extern BGENV *bgenv_get_oldest(BGENVTYPE type);
extern BGENV *bgenv_get_latest(BGENVTYPE type);
extern bool bgenv_write(BGENV *env);
extern BG_ENVDATA *bgenv_read(BGENV *env);
extern bool bgenv_close(BGENV *env);

#endif // __BG_UTILS_H__
