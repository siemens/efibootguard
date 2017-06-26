/** @file

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

/** @brief Tell the library to output information for the user.
*/
void ebg_beverbose(bool v);

/** @brief Initialize environment library and open environment. The first
 * time this function is called, it will create a new environment with the
 * highest revision number for update purposes. Every next time it will
 * just open the environment with the highest revision number.
 *  @return 0 on success, errno on failure
*/
int ebg_env_create_new(void);

/** @brief Initialize environment library and open current environment.
 *  @return 0 on success, errno on failure
*/
int ebg_env_open_current(void);

/** @brief Retrieve variable content
 *  @param key an enum constant to specify the variable
 *  @return a pointer to the buffer with the variable content on success,
 * NULL on failure. The returned pointer must not be freed and is freed
 * automatically on closing the environment. If NULL is returned, errno is
 * set.
*/
char *ebg_env_get(char *key);

/** @brief Store new content into variable
 *  @param key name of the environment variable to set
 *  @param value a string to be stored into the variable
 *  @return 0 on success, errno on failure
*/
int ebg_env_set(char *key, char *value);

/** @brief Check if last update was successful
 *  @return true if successful, false if not
*/
bool ebg_env_isupdatesuccessful(void);

/** @brief Reset all stored failure states
 *  @return 0 if successful, errno on failure
*/
int ebg_env_clearerrorstate(void);

/** @brief Check if active env is clean
 *  @return true if yes, errno set on failure
*/
bool ebg_env_isokay(void);

/** @brief Check if active env has state 'installed'
 *  @return true if yes, errno set on failure
*/
bool ebg_env_isinstalled(void);

/** @brief Check if active env is in testing state
 *  @return true if yes, errno set on failure
*/
bool ebg_env_istesting(void);

/** @brief Confirm environment after update - sets testing and boot_once
 * both to 0
 * @return 0 if successful, errno on failure
*/
int ebg_env_confirmupdate(void);

/** @brief Closes environment and finalize library. Changes are written
 * before closing.
 *  @return 0 on success, errno on failure
*/
int ebg_env_close(void);

#endif //__EBGENV_H__
