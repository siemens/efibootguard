/*
 * EFI Boot Guard
 *
 * Copyright (c) Siemens AG, 2017
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * SPDX-License-Identifier:	GPL-2.0-only
 */

#pragma once

#include "env_api.h"

bool read_env(CONFIG_PART *part, BG_ENVDATA *env);
bool write_env(CONFIG_PART *part, const BG_ENVDATA *env);

EBGENVKEY bgenv_str2enum(const char *key);
