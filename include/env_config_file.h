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

#ifndef __ENV_CONFIG_FILE_H__
#define __ENV_CONFIG_FILE_H__

FILE *open_config_file(CONFIG_PART *cfgpart, char *mode);
int close_config_file(FILE *config_file_handle);
bool probe_config_file(CONFIG_PART *cfgpart);

#endif // __ENV_CONFIG_FILE_H__
