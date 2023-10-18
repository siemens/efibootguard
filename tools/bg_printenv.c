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

#include "uservars.h"

#include "bg_envtools.h"
#include "bg_printenv.h"

static char tool_doc[] =
	"bg_printenv - Environment tool for the EFI Boot Guard";

/* if you change these, do not forget to update completion/bg_printenv/cli.py */
static struct argp_option options_printenv[] = {
	BG_CLI_OPTIONS_COMMON,
	OPT("current", 'c', 0, 0,
	    "Only print values from the current environment"),
	OPT("output", 'o', "LIST", 0,
	    "Comma-separated list of fields which are printed. "
	    "Available fields: in_progress, revision, kernel, kernelargs, "
	    "watchdog_timeout, ustate, user. "
	    "If omitted, all available fields are printed."),
	OPT("raw", 'r', 0, 0, "Raw output mode, e.g. for shell scripting"),
	{0},
};

/* Arguments used by bg_printenv. */
struct arguments_printenv {
	struct arguments_common common;
	bool current;
	/* a bitset to decide which fields are printed */
	struct fields output_fields;
	bool raw;
};

const struct fields ALL_FIELDS = {1, 1, 1, 1, 1, 1, 1};

static error_t parse_output_fields(char *fields, struct fields *output_fields)
{
	char *token;
	memset(output_fields, 0, sizeof(struct fields));
	while ((token = strsep(&fields, ","))) {
		if (*token == '\0') continue;
		if (strcmp(token, "in_progress") == 0) {
			output_fields->in_progress = true;
		} else if (strcmp(token, "revision") == 0) {
			output_fields->revision = true;
		} else if (strcmp(token, "kernel") == 0) {
			output_fields->kernel = true;
		} else if (strcmp(token, "kernelargs") == 0) {
			output_fields->kernelargs = true;
		} else if (strcmp(token, "watchdog_timeout") == 0) {
			output_fields->wdog_timeout = true;
		} else if (strcmp(token, "ustate") == 0) {
			output_fields->ustate = true;
		} else if (strcmp(token, "user") == 0) {
			output_fields->user = true;
		} else {
			fprintf(stderr, "Unknown output field: %s\n", token);
			return 1;
		}
	}
	return 0;
}

static void dump_uservars(uint8_t *udata, bool raw)
{
	char *key, *value;
	uint64_t type;
	uint32_t rsize, dsize;
	uint64_t val_unum;
	int64_t val_snum;

	while (*udata) {
		bgenv_map_uservar(udata, &key, &type, (uint8_t **)&value,
				  &rsize, &dsize);
		fprintf(stdout, "%s", key);
		type &= USERVAR_STANDARD_TYPE_MASK;
		if (type == USERVAR_TYPE_STRING_ASCII) {
			fprintf(stdout, raw ? "=%s\n" : " = %s\n", value);
		} else if (type >= USERVAR_TYPE_UINT8 &&
			   type <= USERVAR_TYPE_UINT64) {
			switch(type) {
			case USERVAR_TYPE_UINT8:
				val_unum = *((uint8_t *) value);
				break;
			case USERVAR_TYPE_UINT16:
				val_unum = *((uint16_t *) value);
				break;
			case USERVAR_TYPE_UINT32:
				val_unum = *((uint32_t *) value);
				break;
			case USERVAR_TYPE_UINT64:
				val_unum = *((uint64_t *) value);
				break;
			}
			fprintf(stdout, raw ? "=%llu\n" : " = %llu\n",
				(long long unsigned int)val_unum);
		} else if (type >= USERVAR_TYPE_SINT8 &&
			   type <= USERVAR_TYPE_SINT64) {
			switch(type) {
			case USERVAR_TYPE_SINT8:
				val_snum = *((int8_t *) value);
				break;
			case USERVAR_TYPE_SINT16:
				val_snum = *((int16_t *) value);
				break;
			case USERVAR_TYPE_SINT32:
				val_snum = *((int32_t *) value);
				break;
			case USERVAR_TYPE_SINT64:
				val_snum = *((int64_t *) value);
				break;
			}
			fprintf(stdout, raw ? "=%lld\n" : " = %lld\n",
				(long long signed int)val_snum);
		} else {
			switch(type) {
			case USERVAR_TYPE_CHAR:
				fprintf(stdout, raw ? "=%c\n" : " = %c\n",
					(char)*value);
				break;
			case USERVAR_TYPE_BOOL:
				fprintf(stdout, raw ? "=%s\n" : " = %s\n",
					(bool)*value ? "true" : "false");
				break;
			default:
				fprintf(stdout, " ( Type is not printable )\n");
			}
		}

		udata = bgenv_next_uservar(udata);
	}
}

void dump_env(BG_ENVDATA *env, const struct fields *output_fields, bool raw)
{
	char buffer[ENV_STRING_LENGTH];
	if (!raw) {
		fprintf(stdout, "Values:\n");
	}
	if (output_fields->in_progress) {
		if (raw) {
			fprintf(stdout, "IN_PROGRESS=%d\n", env->in_progress);
		} else {
			fprintf(stdout, "in_progress:      %s\n",
				env->in_progress ? "yes" : "no");
		}
	}
	if (output_fields->revision) {
		if (raw) {
			fprintf(stdout, "REVISION=%u\n", env->revision);
		} else {
			fprintf(stdout, "revision:         %u\n",
				env->revision);
		}
	}
	if (output_fields->kernel) {
		char *kernelfile = str16to8(buffer, env->kernelfile);
		if (raw) {
			fprintf(stdout, "KERNEL=%s\n", kernelfile);
		} else {
			fprintf(stdout, "kernel:           %s\n", kernelfile);
		}
	}
	if (output_fields->kernelargs) {
		char *kernelargs = str16to8(buffer, env->kernelparams);
		if (raw) {
			fprintf(stdout, "KERNELARGS=%s\n", kernelargs);
		} else {
			fprintf(stdout, "kernelargs:       %s\n", kernelargs);
		}
	}
	if (output_fields->wdog_timeout) {
		if (raw) {
			fprintf(stdout, "WATCHDOG_TIMEOUT=%u\n",
				env->watchdog_timeout_sec);
		} else {
			fprintf(stdout, "watchdog timeout: %u seconds\n",
				env->watchdog_timeout_sec);
		}
	}
	if (output_fields->ustate) {
		if (raw) {
			fprintf(stdout, "USTATE=%u\n", env->ustate);
		} else {
			fprintf(stdout, "ustate:           %u (%s)\n",
				(uint8_t)env->ustate, ustate2str(env->ustate));
		}
	}
	if (output_fields->user) {
		if (!raw) {
			fprintf(stdout, "\n");
			fprintf(stdout, "user variables:\n");
		}
		dump_uservars(env->userdata, raw);
	}
	if (!raw) {
		fprintf(stdout, "\n\n");
	}
}

void dump_envs(const struct fields *output_fields, bool raw)
{
	for (int i = 0; i < ENV_NUM_CONFIG_PARTS; i++) {
		if (!raw) {
			fprintf(stdout, "\n----------------------------\n");
			fprintf(stdout, " Config Partition #%d ", i);
		}
		BGENV *env = bgenv_open_by_index(i);
		if (!env) {
			fprintf(stderr, "Error, could not read environment "
					"for index %d\n",
				i);
			return;
		}
		dump_env(env->data, output_fields, raw);
		bgenv_close(env);
	}
}

static void dump_latest_env(const struct fields *output_fields, bool raw)
{
	BGENV *env = bgenv_open_latest();
	if (!env) {
		fprintf(stderr, "Failed to retrieve latest environment.\n");
		return;
	}
	dump_env(env->data, output_fields, raw);
	bgenv_close(env);
}

static void dump_env_by_index(uint32_t index, struct fields output_fields,
			      bool raw)
{
	BGENV *env = bgenv_open_by_index(index);
	if (!env) {
		fprintf(stderr, "Failed to retrieve latest environment.\n");
		return;
	}
	dump_env(env->data, &output_fields, raw);
	bgenv_close(env);
}

static int printenv_from_file(char *envfilepath, const struct fields *output_fields,
			      bool raw)
{
	int success = 0;
	BG_ENVDATA data;

	success = get_env(envfilepath, &data);
	if (success) {
		dump_env(&data, output_fields, raw);
		return 0;
	} else {
		fprintf(stderr, "Error reading environment file.\n");
		return 1;
	}
}

static error_t parse_printenv_opt(int key, char *arg, struct argp_state *state)
{
	struct arguments_printenv *arguments = state->input;
	error_t e = 0;

	switch (key) {
	case 'c':
		arguments->current = true;
		break;
	case 'o':
		e = parse_output_fields(arg, &arguments->output_fields);
		break;
	case 'r':
		arguments->raw = true;
		break;
	case ARGP_KEY_ARG:
		/* too many arguments - program terminates with call to
		 * argp_usage with non-zero return code */
		argp_usage(state);
		break;
	default:
		return parse_common_opt(key, arg, false, &arguments->common);
	}

	return e;
}

/* This is the entrypoint for the command bg_printenv. */
error_t bg_printenv(int argc, char **argv)
{
	struct argp argp_printenv = {
		.options = options_printenv,
		.parser = parse_printenv_opt,
		.doc = tool_doc,
	};

	struct arguments_printenv arguments = {
		.output_fields = ALL_FIELDS,
	};

	error_t e = argp_parse(&argp_printenv, argc, argv, 0, 0, &arguments);
	if (e) {
		return e;
	}

	const struct arguments_common *common = &arguments.common;

	/* count the number of arguments which result in bg_printenv
	 * operating on a single partition; to avoid ambiguity, we only
	 * allow one such argument. */
	int counter = 0;
	if (common->envfilepath) ++counter;
	if (common->part_specified) ++counter;
	if (arguments.current) ++counter;
	if (counter > 1) {
		fprintf(stderr, "Error, only one of -c/-f/-p can be set.\n");
		return 1;
	}
	if (arguments.raw && counter != 1) {
		/* raw mode makes only sense if applied to a single
		 * partition */
		fprintf(stderr, "Error, raw is set but "
				"current/filepath/which_part is not set. "
				"Must use -r and -c/-f/-p simultaneously.\n");
		return 1;
	}

	if (common->envfilepath) {
		e = printenv_from_file(common->envfilepath,
				       &arguments.output_fields, arguments.raw);
		free(common->envfilepath);
		return e;
	}

	/* not in file mode */
	if (arguments.common.search_all_devices) {
		ebg_set_opt_bool(EBG_OPT_PROBE_ALL_DEVICES, true);
	}
	if (!bgenv_init()) {
		fprintf(stderr, "Error initializing FAT environment.\n");
		return 1;
	}

	if (arguments.current) {
		if (!arguments.raw) {
			fprintf(stdout, "Using latest config partition\n");
		}
		dump_latest_env(&arguments.output_fields, arguments.raw);
	} else if (common->part_specified) {
		if (!arguments.raw) {
			fprintf(stdout, "Using config partition #%d\n",
				arguments.common.which_part);
		}
		dump_env_by_index(common->which_part, arguments.output_fields,
				  arguments.raw);
	} else {
		dump_envs(&arguments.output_fields, arguments.raw);
	}

	bgenv_finalize();
	return 0;
}
