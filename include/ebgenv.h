/** @file
 *
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
 *
 */

#pragma once

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#define USERVAR_TYPE_CHAR		1
#define USERVAR_TYPE_UINT8		2
#define USERVAR_TYPE_UINT16		3
#define USERVAR_TYPE_UINT32		4
#define USERVAR_TYPE_UINT64		5
#define USERVAR_TYPE_SINT8		6
#define USERVAR_TYPE_SINT16		7
#define USERVAR_TYPE_SINT32		8
#define USERVAR_TYPE_SINT64		9
#define USERVAR_TYPE_STRING_ASCII      32
#define USERVAR_TYPE_BOOL	       64
#define USERVAR_TYPE_DELETED  (1ULL << 63)
#define USERVAR_TYPE_DEFAULT		0

#define USERVAR_STANDARD_TYPE_MASK ((1ULL << 32) - 1)

typedef struct {
	bool search_all_devices;
	bool verbose;
} ebgenv_opts_t;

typedef struct {
	void *bgenv;
	void *gc_registry;
	ebgenv_opts_t opts;
} ebgenv_t;

typedef enum { EBG_OPT_PROBE_ALL_DEVICES, EBG_OPT_VERBOSE } ebg_opt_t;

/**
 * @brief Set a global EBG option. Call before creating the ebg env.
 * @param opt option to set
 * @param value option value
 * @return 0 on success
 */
int ebg_set_opt_bool(ebg_opt_t opt, bool value);

/**
 * @brief Get a global EBG option.
 * @param opt option to set
 * @param value out variable to retrieve option value
 * @return 0 on success
 */
int ebg_get_opt_bool(ebg_opt_t opt, bool *value);

/** @brief Tell the library to output information for the user.
 *  @param e A pointer to an ebgenv_t context.
 *  @param v A boolean to set verbosity.
 *  @note deprecated. Use \c ebg_set_opt_bool(EBG_OPT_VERBOSE,true) instead
 */
void __attribute__((deprecated)) ebg_beverbose(ebgenv_t *e, bool v);

/** @brief Initialize environment library and open environment. The first
 *         time this function is called, it will create a new environment with
 *         the highest revision number for update purposes. Every next time it
 *         will just open the environment with the highest revision number.
 *  @param e A pointer to an ebgenv_t context.
 *  @return 0 on success, errno on failure
 */
int ebg_env_create_new(ebgenv_t *e);

/** @brief Initialize environment library and open current environment.
 *  @param e A pointer to an ebgenv_t context.
 *  @return 0 on success, errno on failure
 */
int ebg_env_open_current(ebgenv_t *e);

/** @brief Retrieve variable content
 *  @param e A pointer to an ebgenv_t context.
 *  @param key an enum constant to specify the variable
 *  @param buffer pointer to buffer containing requested value.
 *         If buffer is NULL, return needed buffer size.
 *  @return If buffer != NULL: 0 on success, -errno on failure
 *          If buffer == NULL: needed buffer size, 0 if variable
 *                             is not found.
 */
int ebg_env_get(ebgenv_t *e, const char *key, char* buffer);

/** @brief Store new content into variable
 *  @param e A pointer to an ebgenv_t context.
 *  @param key name of the environment variable to set
 *  @param value a string to be stored into the variable
 *  @return 0 on success, -errno on failure. If buffer is NULL,
 *	    the required buffer size is returned.
 */
int ebg_env_set(ebgenv_t *e, const char *key, const char *value);

/** @brief Store new content into variable
 *  @param e A pointer to an ebgenv_t context.
 *  @param key name of the environment variable to set
 *  @param user specific or predefined datatype of the value
 *  @param value arbitrary data to be stored into the variable
 *  @param datalen length of the data to be stored into the variable
 *  @return 0 on success, -errno on failure
 */
int ebg_env_set_ex(ebgenv_t *e, const char *key, uint64_t datatype,
		   const uint8_t *value, uint32_t datalen);

/** @brief Get content of user variable
 *  @param e A pointer to an ebgenv_t context.
 *  @param key name of the environment variable to retrieve
 *  @param buffer to store the datatype of the value
 *  @param buffer destination for data to be stored into the variable
 *  @param maxlen size of provided buffer
 *  @return 0 on success, -errno on failure
 */
int ebg_env_get_ex(ebgenv_t *e, const char *key, uint64_t *datatype,
		   uint8_t *buffer, uint32_t maxlen);

/** @brief Get available space for user variables
 *  @param e A pointer to an ebgenv_t context.
 *  @return Free space in bytes
 */
uint32_t ebg_env_user_free(ebgenv_t *e);

/** @brief Get global ustate value, accounting for all environments
 *  @param reserved Historic parameter, must be NULL.
 *  @return ustate value
 */
uint16_t ebg_env_getglobalstate(void *reserved);

/** @brief Set global ustate value, accounting for all environments
 *         if state is set to zero and updating only current environment if
 *         state is set to a non-zero value.
 *  @param e A pointer to an ebgenv_t context.
 *  @param ustate The global ustate value to set.
 *  @return errno on error, 0 if okay.
 */
int ebg_env_setglobalstate(ebgenv_t *e, uint16_t ustate);

/** @brief Closes environment and finalize library. Changes are written before
 *         closing.
 *  @param e A pointer to an ebgenv_t context.
 *  @return 0 on success, errno on failure
 */
int ebg_env_close(ebgenv_t *e);

/** @brief Register a variable that will be deleted on finalize
 *  @param e A pointer to an ebgenv_t context.
 *  @param key A string containing the variable key
 *  @return 0 on success, errno on failure
 */
int ebg_env_register_gc_var(ebgenv_t *e, char *key);

/** @brief Finalizes a currently running update procedure
 *  @param e A pointer to an ebgenv_t context.
 *  @return 0 on success, errno on failure
 */
int ebg_env_finalize_update(ebgenv_t *e);
