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

#ifndef __H_ENV_DATA__
#define __H_ENV_DATA__

#define FAT_ENV_FILENAME "BGENV.DAT"
#define ENV_STRING_LENGTH 255

#define CONFIG_PARTITION_MAXCOUNT 64

#pragma pack(push)
#pragma pack(1)
struct _BG_ENVDATA {
	uint16_t kernelfile[ENV_STRING_LENGTH];
	uint16_t kernelparams[ENV_STRING_LENGTH];
	uint8_t testing;
	uint8_t boot_once;
	uint16_t watchdog_timeout_sec;
	uint32_t revision;
	uint32_t crc32;
};
#pragma pack(pop)

typedef struct _BG_ENVDATA BG_ENVDATA;

#endif // __H_ENV_DATA__
