/*
 * EFI Boot Guard
 *
 * Copyright (c) Siemens AG, 2017
 *
 * Authors:
 *  Andreas Reichel <andreas.reichel.ext@siemens.com>
 *  Michael Adler <michael.adler@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * SPDX-License-Identifier:	GPL-2.0-only
 */

#include <sys/queue.h>
#include <sys/stat.h>

#include "ebgenv.h"

#include "bg_envtools.h"
#include "bg_setenv.h"
#include "bg_printenv.h"

static char tool_doc[] =
	"bg_setenv - Environment tool for the EFI Boot Guard";

 /* if you change these, do not forget to update completion/bg_setenv/cli.py */
static struct argp_option options_setenv[] = {
	BG_CLI_OPTIONS_COMMON,
	OPT("preserve", 'P', 0, 0, "Preserve existing entries"),
	OPT("kernel", 'k', "KERNEL", 0, "Set kernel to load"),
	OPT("args", 'a', "KERNEL_ARGS", 0, "Set kernel arguments"),
	OPT("revision", 'r', "REVISION", 0, "Set revision value"),
	OPT("ustate", 's', "USTATE", 0, "Set update status for environment"),
	OPT("watchdog", 'w', "WATCHDOG_TIMEOUT", 0,
	    "Watchdog timeout in seconds"),
	OPT("confirm", 'c', 0, 0, "Confirm working environment"),
	OPT("update", 'u', 0, 0, "Automatically update oldest revision"),
	OPT("uservar", 'x', "KEY=VAL", 0,
	    "Set user-defined string variable. For setting multiple variables, "
	    "use this option multiple times."),
	OPT("in_progress", 'i', "IN_PROGRESS", 0,
	    "Set in_progress variable to simulate a running update process."),
	{0},
};

/* Arguments used by bg_setenv. */
struct arguments_setenv {
	struct arguments_common common;
	/* auto update feature automatically updates partition with
	 * oldest environment revision (lowest value) */
	bool auto_update;
	/* whether to keep existing entries in BGENV before applying new
	 * settings */
	bool preserve_env;
};

typedef enum { ENV_TASK_SET, ENV_TASK_DEL } BGENV_TASK;

struct stailhead *headp;
struct env_action {
	char *key;
	uint64_t type;
	uint8_t *data;
	BGENV_TASK task;
	STAILQ_ENTRY(env_action) journal;
};

STAILQ_HEAD(stailhead, env_action) head = STAILQ_HEAD_INITIALIZER(head);

static void journal_free_action(struct env_action *action)
{
	if (!action) {
		return;
	}
	free(action->data);
	free(action->key);
	free(action);
}

static error_t journal_add_action(BGENV_TASK task, const char *key,
				  uint64_t type, const uint8_t *data,
				  size_t datalen)
{
	struct env_action *new_action;

	new_action = calloc(1, sizeof(struct env_action));
	if (!new_action) {
		return ENOMEM;
	}
	new_action->task = task;
	if (key) {
		if (asprintf(&(new_action->key), "%s", key) == -1) {
			new_action->key = NULL;
			goto newaction_nomem;
		}
	}
	new_action->type = type;
	if (data && datalen) {
		new_action->data = (uint8_t *)malloc(datalen);
		if (!new_action->data) {
			new_action->data = NULL;
			goto newaction_nomem;
		}
		memcpy(new_action->data, data, datalen);
	}
	STAILQ_INSERT_TAIL(&head, new_action, journal);
	return 0;

newaction_nomem:
	journal_free_action(new_action);
	return ENOMEM;
}

static void journal_process_action(BGENV *env, struct env_action *action)
{
	ebgenv_t e;
	memset(&e, 0, sizeof(ebgenv_t));

	switch (action->task) {
	case ENV_TASK_SET:
		VERBOSE(stdout, "Task = SET, key = %s, type = %llu, val = %s\n",
			action->key, (long long unsigned int)action->type,
			(char *)action->data);
		if (strcmp(action->key, "ustate") == 0) {
			const char *arg;
			int ustate;
			int ret;
			e.bgenv = env;
			arg = (char *)action->data;
			ustate = parse_int(arg);
			if (ustate < 0 || ustate > UINT16_MAX) {
				fprintf(stderr, "Invalid ustate value: %s", arg);
				return;
			}
			if ((ret = ebg_env_setglobalstate(&e, ustate)) != 0) {
				fprintf(stderr,
					"Error setting global state: %s.",
					strerror(-ret));
			}
			return;
		}
		bgenv_set(env, action->key, action->type, action->data,
			  strlen((char *)action->data) + 1);
		break;
	case ENV_TASK_DEL:
		VERBOSE(stdout, "Task = DEL, key = %s\n", action->key);
		bgenv_set(env, action->key, action->type, "", 1);
		break;
	}
}

static error_t set_uservars(char *arg)
{
	const char *key, *value;

	key = strtok(arg, "=");
	if (key == NULL) {
		return 0;
	}

	value = strtok(NULL, "=");
	if (value == NULL) {
		return journal_add_action(ENV_TASK_DEL, key,
					  USERVAR_TYPE_DEFAULT |
					  USERVAR_TYPE_DELETED, NULL, 0);
	}
	return journal_add_action(ENV_TASK_SET, key, USERVAR_TYPE_DEFAULT |
				  USERVAR_TYPE_STRING_ASCII,
				  (uint8_t *)value, strlen(value) + 1);
}

static error_t parse_setenv_opt(int key, char *arg, struct argp_state *state)
{
	struct arguments_setenv *arguments = state->input;
	int i, res;
	char *tmp;
	error_t e = 0;

	switch (key) {
	case 'k':
		if (strlen(arg) > ENV_STRING_LENGTH) {
			fprintf(stderr, "Error, kernel filename is too long. "
					"Maximum of %d "
					"characters permitted.\n",
				ENV_STRING_LENGTH);
			return 1;
		}
		e = journal_add_action(ENV_TASK_SET, "kernelfile", 0,
				       (uint8_t *)arg, strlen(arg) + 1);
		break;
	case 'a':
		if (strlen(arg) > ENV_STRING_LENGTH) {
			fprintf(stderr,
				"Error, kernel arguments string is too long. "
				"Maximum of %d characters permitted.\n",
				ENV_STRING_LENGTH);
			return 1;
		}
		e = journal_add_action(ENV_TASK_SET, "kernelparams", 0,
				       (uint8_t *)arg, strlen(arg) + 1);
		break;
	case 's':
		i = parse_int(arg);
		if (errno) {
			// maybe user specified an enum string
			i = str2ustate(arg);
			if (i == USTATE_UNKNOWN) {
				fprintf(stderr, "Invalid state specified.\n");
				return 1;
			}
		}
		if (i < 0 || i >= USTATE_MAX) {
			fprintf(stderr,
				"Invalid ustate value specified. Possible "
				"values: ");
			for (int j = 0; j < USTATE_MAX; j++) {
				fprintf(stderr, "%d (%s)%s", j,
					ustate2str(j),
					j < USTATE_MAX - 1 ? ", " : "\n");
			}
			return 1;
		} else {
			res = asprintf(&tmp, "%u", i);
			if (res == -1) {
				return ENOMEM;
			}
			e = journal_add_action(ENV_TASK_SET, "ustate", 0,
					       (uint8_t *)tmp, strlen(tmp) + 1);
			free(tmp);
			VERBOSE(stdout, "Ustate set to %d (%s).\n", i,
				ustate2str(i));
		}
		break;
	case 'i':
		i = parse_int(arg);
		if (errno) {
			fprintf(stderr, "Invalid value specified.\n");
			return 1;
		}
		if (i < 0 || i > 1) {
			fprintf(stderr,
				"Invalid value specified. Possible values: "
				"0 (no), 1 (yes)\n");
			return 1;
		} else {
			res = asprintf(&tmp, "%u", i);
			if (res == -1) {
				return ENOMEM;
			}
			e = journal_add_action(ENV_TASK_SET, "in_progress", 0,
					       (uint8_t *)tmp, strlen(tmp) + 1);
			free(tmp);
			VERBOSE(stdout, "in_progress set to %d.\n", i);
		}
		break;
	case 'r':
		i = parse_int(arg);
		if (errno) {
			fprintf(stderr, "Invalid revision specified.\n");
			return 1;
		}
		VERBOSE(stdout, "Revision is set to %u.\n", (unsigned int) i);
		e = journal_add_action(ENV_TASK_SET, "revision", 0,
				       (uint8_t *)arg, strlen(arg) + 1);
		break;
	case 'w':
		i = parse_int(arg);
		if (errno || i < 0) {
			fprintf(stderr,
				"Invalid watchdog timeout specified.\n");
			return 1;
		}
		VERBOSE(stdout,
			"Setting watchdog timeout to %d seconds.\n", i);
		e = journal_add_action(ENV_TASK_SET,
				       "watchdog_timeout_sec", 0,
				       (uint8_t *)arg, strlen(arg) + 1);
		break;
	case 'c':
		VERBOSE(stdout,
			"Confirming environment to work. Removing boot-once "
			"and testing flag.\n");
		e = journal_add_action(ENV_TASK_SET, "ustate", 0,
				       (uint8_t *)"0", 2);
		break;
	case 'u':
		arguments->auto_update = true;
		break;
	case 'x':
		/* Set user-defined variable(s) */
		e = set_uservars(arg);
		break;
	case 'P':
		arguments->preserve_env = true;
		break;
	case ARGP_KEY_ARG:
		/* too many arguments - program terminates with call to
		 * argp_usage with non-zero return code */
		argp_usage(state);
		break;
	default:
		return parse_common_opt(key, arg, true, &arguments->common);
	}

	if (e) {
		fprintf(stderr, "Error creating journal: %s\n", strerror(e));
	}
	return e;
}

static void update_environment(BGENV *env, bool verbosity)
{
	if (verbosity) {
		fprintf(stdout, "Processing journal...\n");
	}

	while (!STAILQ_EMPTY(&head)) {
		struct env_action *action = STAILQ_FIRST(&head);

		journal_process_action(env, action);
		STAILQ_REMOVE_HEAD(&head, journal);
		journal_free_action(action);
	}

	env->data->crc32 =
		bgenv_crc32(0, env->data,
			    sizeof(BG_ENVDATA) - sizeof(env->data->crc32));

}

static int dumpenv_to_file(const char *envfilepath, bool verbosity,
			   bool preserve_env)
{
	/* execute journal and write to file */
	int result = 0;
	BGENV env;
	BG_ENVDATA data;

	memset(&env, 0, sizeof(BGENV));
	memset(&data, 0, sizeof(BG_ENVDATA));
	env.data = &data;

	if (preserve_env && !get_env(envfilepath, &data)) {
		return 1;
	}

	update_environment(&env, verbosity);
	if (verbosity) {
		dump_env(env.data, &ALL_FIELDS, false);
	}
	FILE *of = fopen(envfilepath, "wb");
	if (of) {
		if (fwrite(&data, sizeof(BG_ENVDATA), 1, of) != 1) {
			fprintf(stderr,
				"Error writing to output file: %s\n",
				strerror(errno));
			result = 1;
		} else {
			fprintf(stdout, "Output written to %s.\n", envfilepath);
		}
		if (fclose(of)) {
			fprintf(stderr, "Error closing output file.\n");
			result = 1;
		};
	} else {
		fprintf(stderr, "Error opening output file %s (%s).\n",
			envfilepath, strerror(errno));
		result = 1;
	}

	return result;
}

/* This is the entrypoint for the command bg_setenv. */
error_t bg_setenv(int argc, char **argv)
{
	if (argc < 2) {
		printf("No task to perform. Please specify at least one"
		       " optional argument. See --help for further"
		       " information.\n");
		return 1;
	}

	struct argp argp_setenv = {
		.options = options_setenv,
		.parser = parse_setenv_opt,
		.doc = tool_doc,
	};

	struct arguments_setenv arguments;
	memset(&arguments, 0, sizeof(struct arguments_setenv));

	STAILQ_INIT(&head);

	error_t e;
	e = argp_parse(&argp_setenv, argc, argv, 0, 0, &arguments);
	if (e) {
		return e;
	}

	if (arguments.auto_update && arguments.common.part_specified) {
		fprintf(stderr, "Error, both automatic and manual partition "
				"selection. Cannot use -p and -u "
				"simultaneously.\n");
		return 1;
	}

	int result = 0;

	/* arguments are parsed, journal is filled */

	/* is output to file or input from file ? */
	if (arguments.common.envfilepath) {
		result = dumpenv_to_file(arguments.common.envfilepath,
					 arguments.common.verbosity,
					 arguments.preserve_env);
		free(arguments.common.envfilepath);
		return result;
	}

	/* not in file mode */
	if (arguments.common.search_all_devices) {
		ebg_set_opt_bool(EBG_OPT_PROBE_ALL_DEVICES, true);
	}
	if (arguments.common.verbosity) {
		ebg_set_opt_bool(EBG_OPT_VERBOSE, true);
	}
	if (!bgenv_init()) {
		fprintf(stderr, "Error initializing FAT environment.\n");
		return 1;
	}

	if (arguments.common.verbosity) {
		dump_envs(&ALL_FIELDS, false);
	}

	BGENV *env_new = NULL;
	BGENV *env_current;

	if (arguments.auto_update) {
		/* clone latest environment */

		env_current = bgenv_open_latest();
		if (!env_current) {
			fprintf(stderr, "Failed to retrieve latest environment."
					"\n");
			result = 1;
			goto cleanup;
		}
		env_new = bgenv_open_oldest();
		if (!env_new) {
			fprintf(stderr, "Failed to retrieve oldest environment."
					"\n");
			bgenv_close(env_current);
			result = 1;
			goto cleanup;
		}
		if (arguments.common.verbosity) {
			fprintf(stdout,
				"Updating environment with revision %u\n",
				env_new->data->revision);
		}

		if (!env_current->data || !env_new->data) {
			fprintf(stderr, "Invalid environment data pointer.\n");
			bgenv_close(env_current);
			result = 1;
			goto cleanup;
		}

		memcpy((char *)env_new->data, (char *)env_current->data,
		       sizeof(BG_ENVDATA));
		env_new->data->revision = env_current->data->revision + 1;

		bgenv_close(env_current);
	} else {
		if (arguments.common.part_specified) {
			fprintf(stdout, "Using config partition #%d\n",
				arguments.common.which_part);
			env_new = bgenv_open_by_index(
				arguments.common.which_part);
		} else {
			env_new = bgenv_open_latest();
		}
		if (!env_new) {
			fprintf(stderr, "Failed to retrieve environment by "
					"index.\n");
			result = 1;
			goto cleanup;
		}
	}

	update_environment(env_new, arguments.common.verbosity);

	if (arguments.common.verbosity) {
		fprintf(stdout, "New environment data:\n");
		fprintf(stdout, "---------------------\n");
		dump_env(env_new->data, &ALL_FIELDS, false);
	}
	if (!bgenv_write(env_new)) {
		fprintf(stderr, "Error storing environment.\n");
		result = 1;
	} else {
		fprintf(stdout, "Environment update was successful.\n");
	}

cleanup:
	bgenv_close(env_new);
	bgenv_finalize();
	return result;
}
