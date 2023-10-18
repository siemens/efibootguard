/*
 * EFI Boot Guard
 *
 * Copyright (c) Siemens AG, 2017-2021
 *
 * Authors:
 *  Andreas Reichel <andreas.reichel.ext@siemens.com>
 *  Michael Adler <michael.adler@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <sys/stat.h>
#include "env_config_file.h"
#include "version.h"

#include "bg_envtools.h"

static char *ustatemap[] = {"OK", "INSTALLED", "TESTING", "FAILED", "UNKNOWN"};

char *ustate2str(uint8_t ustate)
{
	if (ustate > USTATE_MAX) {
		ustate = USTATE_MAX;
	}
	return ustatemap[ustate];
}

uint8_t str2ustate(char *str)
{
	uint8_t i;

	if (!str) {
		return USTATE_UNKNOWN;
	}
	for (i = USTATE_MIN; i < USTATE_MAX; i++) {
		if (strncasecmp(str, ustatemap[i], strlen(ustatemap[i])) == 0) {
			return i;
		}
	}
	return USTATE_UNKNOWN;
}

int parse_int(char *arg)
{
	char *tmp;
	long i;

	errno = 0;
	i = strtol(arg, &tmp, 10);
	if (errno == ERANGE ||		  /* out of range */
	    (errno != 0 && i == 0) ||	  /* no conversion was performed */
	    tmp == arg || *tmp != '\0' || /* invalid input */
	    i < INT_MIN || i > INT_MAX) { /* not a valid int */
		errno = EINVAL;
		return -1;
	}
	return (int)i;
}

error_t parse_common_opt(int key, char *arg, bool compat_mode,
			 struct arguments_common *arguments)
{
	bool found = false;
	int i;
	switch (key) {
	case 'A':
		found = true;
		arguments->search_all_devices = true;
		break;
	case 'f':
		found = true;
		free(arguments->envfilepath);
		arguments->envfilepath = NULL;

		if (compat_mode) {
			/* compat mode, permitting "bg_setenv -f <dir>" */
			struct stat sb;

			int res = stat(arg, &sb);
			if (res == 0 && S_ISDIR(sb.st_mode)) {
				fprintf(stderr,
					"WARNING: Using -f to specify only the "
					"ouptut directory is deprecated.\n");
				res = asprintf(&arguments->envfilepath, "%s/%s",
					       arg, FAT_ENV_FILENAME);
				if (res == -1) {
					return ENOMEM;
				}
			}
		}

		if (!arguments->envfilepath) {
			arguments->envfilepath = strdup(arg);
			if (!arguments->envfilepath) {
				return ENOMEM;
			}
		}
		break;
	case 'p':
		found = true;
		i = parse_int(arg);
		if (errno) {
			fprintf(stderr, "Invalid number specified for -p.\n");
			return 1;
		}
		if (i >= 0 && i < ENV_NUM_CONFIG_PARTS) {
			arguments->which_part = i;
			arguments->part_specified = true;
		} else {
			fprintf(stderr,
				"Selected partition out of range. Valid range: "
				"0..%d.\n",
				ENV_NUM_CONFIG_PARTS - 1);
			return 1;
		}
		break;
	case 'v':
		found = true;
		/* Set verbosity in this program */
		arguments->verbosity = true;
		/* Set verbosity in the library */
		bgenv_be_verbose(true);
		break;
	case 'V':
		found = true;
		fprintf(stdout, "EFI Boot Guard %s\n", EFIBOOTGUARD_VERSION);
		exit(0);
	}
	if (!found) {
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

bool get_env(char *configfilepath, BG_ENVDATA *data)
{
	FILE *config;
	bool result = true;

	if (!(config = open_config_file(configfilepath, "rb"))) {
		return false;
	}

	if (!(fread(data, sizeof(BG_ENVDATA), 1, config) == 1)) {
		VERBOSE(stderr, "Error reading environment data from %s\n",
			configfilepath);
		if (feof(config)) {
			VERBOSE(stderr, "End of file encountered.\n");
		}
		result = false;
	}

	if (fclose(config)) {
		VERBOSE(stderr,
			"Error closing environment file after reading.\n");
	};

	if (result == false) {
		return false;
	}

	/* enforce NULL-termination of strings */
	data->kernelfile[ENV_STRING_LENGTH - 1] = 0;
	data->kernelparams[ENV_STRING_LENGTH - 1] = 0;

	return validate_envdata(data);
}
