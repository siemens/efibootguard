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
 */

#ifndef __EBGENV_H__
#define __EBGENV_H__

#include <errno.h>

typedef struct {
	void *bgenv;
	bool ebg_new_env_created;
} ebgenv_t;

/** @brief Tell the library to output information for the user.
 *  @param e A pointer to an ebgenv_t context.
 *  @param v A boolean to set verbosity.
 */
void ebg_beverbose(ebgenv_t *e, bool v);

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
 *  @param buffer pointer to buffer containing requested value
 *  @return 0 on success, errno on failure
 */
int ebg_env_get(ebgenv_t *e, char *key, char* buffer);

/** @brief Store new content into variable
 *  @param e A pointer to an ebgenv_t context.
 *  @param key name of the environment variable to set
 *  @param value a string to be stored into the variable
 *  @return 0 on success, errno on failure
 */
int ebg_env_set(ebgenv_t *e, char *key, char *value);

/** @brief Get global ustate value, accounting for all environments
 *  @param e A pointer to an ebgenv_t context.
 *  @return ustate value
 */
uint16_t ebg_env_getglobalstate(ebgenv_t *e);

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

#endif //__EBGENV_H__
