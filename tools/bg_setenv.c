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

#include <sys/queue.h>

#include "env_api.h"
#include "ebgenv.h"
#include "uservars.h"
#include "version.h"

static char doc[] =
    "bg_setenv/bg_printenv - Environment tool for the EFI Boot Guard";

static struct argp_option options_setenv[] = {
    {"kernel", 'k', "KERNEL", 0, "Set kernel to load"},
    {"args", 'a', "KERNEL_ARGS", 0, "Set kernel arguments"},
    {"part", 'p', "ENV_PART", 0, "Set environment partition to update. "
				 "If no partition is specified, the one "
				 "with the smallest revision value "
				 "above zero is updated."},
    {"revision", 'r', "REVISION", 0, "Set revision value"},
    {"ustate", 's', "USTATE", 0, "Set update status for environment"},
    {"filepath", 'f', "ENVFILE_DIR", 0, "Output environment to file. Expects "
					"an output path where the file name "
					"is automatically appended."},
    {"watchdog", 'w', "WATCHDOG_TIMEOUT", 0, "Watchdog timeout in seconds"},
    {"confirm", 'c', 0, 0, "Confirm working environment"},
    {"update", 'u', 0, 0, "Automatically update oldest revision"},
    {"verbose", 'v', 0, 0, "Be verbose"},
    {"uservar", 'x', "KEY=VAL", 0, "Set user-defined string variable. For "
				   "setting multiple variables, use this "
				   "option multiple times."},
    {"version", 'V', 0, 0, "Print version"},
    {0}};

static struct argp_option options_printenv[] = {
    {"verbose", 'v', 0, 0, "Be verbose"},
    {"version", 'V', 0, 0, "Print version"},
    {0}};

struct arguments {
	bool output_to_file;
	int which_part;
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
	if (!action)
		return;
	free(action->data);
	free(action->key);
	free(action);
}

static error_t journal_add_action(BGENV_TASK task, char *key, uint64_t type,
				  uint8_t *data, size_t datalen)
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
	uint8_t *var;
	ebgenv_t e;
	char *tmp;

	switch (action->task) {
	case ENV_TASK_SET:
		VERBOSE(stdout, "Task = SET, key = %s, type = %llu, val = %s\n",
			action->key, (long long unsigned int)action->type,
			(char *)action->data);
		if (strncmp(action->key, "ustate", strlen("ustate")+1) == 0) {
			uint16_t ustate;
			unsigned long t;
			char *arg;
			int ret;
			e.bgenv = env;
			arg = (char *)action->data;
			errno = 0;
			t = strtol(arg, &tmp, 10);
			if ((errno == ERANGE && (t == LONG_MAX ||
			                         t == LONG_MIN)) ||
                            (errno != 0 && t == 0) || (tmp == arg)) {
				fprintf(stderr, "Invalid value for ustate: %s",
						(char *)action->data);
				return;
			}
			ustate = (uint16_t)t;;
			if ((ret = ebg_env_setglobalstate(&e, ustate)) != 0) {
				fprintf(stderr,
					"Error setting global state: %s.",
					strerror(ret));
			}
			return;
		}
		bgenv_set(env, action->key, action->type, action->data,
			  strlen((char *)action->data) + 1);
		break;
	case ENV_TASK_DEL:
		VERBOSE(stdout, "Task = DEL, key = %s\n", action->key);
		var = bgenv_find_uservar(env->data->userdata, action->key);
		if (var) {
			bgenv_del_uservar(env->data->userdata, var);
		}
		break;
	}
}

/* auto update feature automatically updates partition with
 * oldest environment revision (lowest value)
 */
static bool auto_update = false;

static bool part_specified = false;

static bool verbosity = false;

static char *envfilepath = NULL;

static char *ustatemap[] = {"OK", "INSTALLED", "TESTING", "FAILED", "UNKNOWN"};

static uint8_t str2ustate(char *str)
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

static char *ustate2str(uint8_t ustate)
{
	if (ustate > USTATE_MAX) {
		ustate = USTATE_MAX;
	}
	return ustatemap[ustate];
}

static error_t set_uservars(char *arg)
{
	char *key, *value;

	key = strtok(arg, "=");
	if (key == NULL) {
		return 0;
	}

	value = strtok(NULL, "=");
	if (value == NULL) {
		return journal_add_action(ENV_TASK_DEL, key, 0, NULL, 0);
	}
	return journal_add_action(ENV_TASK_SET, key, USERVAR_TYPE_DEFAULT,
				  (uint8_t *)value, strlen(value) + 1);
}

static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
	struct arguments *arguments = state->input;
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
	case 'p':
		errno = 0;
		i = strtol(arg, &tmp, 10);
		if ((errno == ERANGE && (i == LONG_MAX || i == LONG_MIN)) ||
		    (errno != 0 && i == 0) || (tmp == arg)) {
			fprintf(stderr, "Invalid number specified for -p.\n");
			return 1;
		}
		if (i == 0 || i == 1) {
			printf("Updating config partition #%d\n", i);
			arguments->which_part = i;
			part_specified = true;
		} else {
			fprintf(stderr,
				"Selected partition out of range. Valid range: "
				"0..1.\n");
			return 1;
		}
		break;
	case 's':
		errno = 0;
		i = strtol(arg, &tmp, 10);
		if ((errno == ERANGE && (i == LONG_MAX || i == LONG_MIN)) ||
		    (errno != 0 && i == 0) || (tmp == arg)) {
			// maybe user specified an enum string
			i = str2ustate(arg);
			if (i == USTATE_UNKNOWN) {
				fprintf(stderr, "Invalid state specified.\n");
				return 1;
			}
		}
		if (i < 0 || i > 3) {
			fprintf(
			    stderr,
			    "Invalid ustate value specified. Possible values: "
			    "0 (%s), 1 (%s), 2 (%s), 3 (%s)\n",
			    ustatemap[0], ustatemap[1], ustatemap[2],
			    ustatemap[3]);
			return 1;
		} else {
			res = asprintf(&tmp, "%u", i);
			if (res == -1) {
				return ENOMEM;
			}
			e = journal_add_action(ENV_TASK_SET, "ustate", 0,
					       (uint8_t *)tmp, strlen(tmp) + 1);
			VERBOSE(stdout, "Ustate set to %d (%s).\n", i,
				ustate2str(i));
		}
		break;
	case 'r':
		i = atoi(arg);
		VERBOSE(stdout, "Revision is set to %d.\n", i);
		e = journal_add_action(ENV_TASK_SET, "revision", 0,
				       (uint8_t *)arg, strlen(arg) + 1);
		break;
	case 'w':
		i = atoi(arg);
		if (i != 0) {
			VERBOSE(stdout,
				"Setting watchdog timeout to %d seconds.\n", i);
			e = journal_add_action(ENV_TASK_SET,
					       "watchdog_timeout_sec", 0,
					       (uint8_t *)arg, strlen(arg) + 1);
		} else {
			fprintf(stderr, "Watchdog timeout must be non-zero.\n");
			return 1;
		}
		break;
	case 'f':
		arguments->output_to_file = true;
		res = asprintf(&envfilepath, "%s/%s", arg, FAT_ENV_FILENAME);
		if (res == -1) {
			return ENOMEM;
		}
		break;
	case 'c':
		VERBOSE(stdout,
			"Confirming environment to work. Removing boot-once "
			"and testing flag.\n");
		e = journal_add_action(ENV_TASK_SET, "ustate", 0,
				       (uint8_t *)"0", 2);
		break;
	case 'u':
		if (part_specified) {
			fprintf(stderr,
				"Error, both automatic and manual partition "
				"selection. Cannot use -p and -u "
				"simultaneously.\n");
			return 1;
		}
		auto_update = true;
		break;
	case 'v':
		/* Set verbosity in this program */
		verbosity = true;
		/* Set verbosity in the library */
		bgenv_be_verbose(true);
		break;
	case 'x':
		/* Set user-defined variable(s) */
		e = set_uservars(arg);
		break;
	case 'V':
		printf("EFI Boot Guard %s\n", EFIBOOTGUARD_VERSION);
		exit(0);
	case ARGP_KEY_ARG:
		/* too many arguments - program terminates with call to
		 * argp_usage with non-zero return code */
		argp_usage(state);
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}

	if (e) {
		fprintf(stderr, "Error creating journal: %s\n", strerror(e));
	}
	return e;
}

static void dump_uservars(uint8_t *udata)
{
	char *key, *value;
	uint64_t type;
	uint32_t rsize, dsize;
	uint64_t val_unum;
	int64_t val_snum;

	while (*udata) {
		bgenv_map_uservar(udata, &key, &type, (uint8_t **)&value,
				  &rsize, &dsize);
		printf("%s ", key);
		type &= USERVAR_STANDARD_TYPE_MASK;
		if (type == USERVAR_TYPE_STRING_ASCII) {
			printf("= %s\n", value);
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
			printf("= %llu\n", (long long unsigned int) val_unum);
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
			printf("= %lld\n", (long long signed int) val_snum);
		} else {
			switch(type) {
			case USERVAR_TYPE_CHAR:
				printf("= %c\n", (char) *value);
				break;
			case USERVAR_TYPE_BOOL:
				printf("= %s\n",
				       (bool) *value ? "true" : "false");
				break;
			default:
				printf("( Type is not printable )\n");
			}
		}

		udata = bgenv_next_uservar(udata);
	}
}

static void dump_env(BG_ENVDATA *env)
{
	char buffer[ENV_STRING_LENGTH];
	printf("Values: \n");
	printf("revision: %u\n", env->revision);
	printf("kernel: %s\n", str16to8(buffer, env->kernelfile));
	printf("kernelargs: %s\n", str16to8(buffer, env->kernelparams));
	printf("watchdog timeout: %u seconds\n", env->watchdog_timeout_sec);
	printf("ustate: %u (%s)\n", (uint8_t)env->ustate,
	       ustate2str(env->ustate));
	printf("\n");
	printf("user variables:\n");
	dump_uservars(env->userdata);
	printf("\n\n");
}

static void update_environment(BGENV *env)
{
	printf("Processing journal...\n");

	while (!STAILQ_EMPTY(&head)) {
		struct env_action *action = STAILQ_FIRST(&head);

		journal_process_action(env, action);
		STAILQ_REMOVE_HEAD(&head, journal);
		journal_free_action(action);
	}

	env->data->crc32 = crc32(0, (const Bytef *)env->data,
				 sizeof(BG_ENVDATA) - sizeof(env->data->crc32));

}

static void dump_envs(void)
{
	for (int i = 0; i < ENV_NUM_CONFIG_PARTS; i++) {
		if (verbosity) {
			printf("\n----------------------------\n");
			printf(" Config Partition #%d ", i);
		}
		BGENV *env = bgenv_open_by_index(i);
		if (env) {
			if (verbosity) {
				dump_env(env->data);
			}
		} else {
			fprintf(stderr, "Error, could not read environment "
					"for index %d\n",
				i);
			return;
		}
		bgenv_close(env);
	}
}

int main(int argc, char **argv)
{
	static struct argp argp_setenv = {options_setenv, parse_opt, NULL, doc};
	static struct argp argp_printenv = {options_printenv, parse_opt, NULL,
					    doc};
	static struct argp *argp;

	bool write_mode = (bool)strstr(argv[0], "bg_setenv");
	if (write_mode) {
		argp = &argp_setenv;
	} else {
		argp = &argp_printenv;
		verbosity = true;
	}

	struct arguments arguments;
	arguments.output_to_file = false;
	arguments.which_part = 0;

	STAILQ_INIT(&head);

	error_t e;
	e = argp_parse(argp, argc, argv, 0, 0, &arguments);
	if (e) {
		return e;
	}

	int result = 0;

	/* arguments are parsed, journal is filled */

	/* is output to file ? */
	if (arguments.output_to_file) {
		/* execute journal and write to file */
		BGENV env;
		BG_ENVDATA data;

		memset(&env, 0, sizeof(BGENV));
		memset(&data, 0, sizeof(BG_ENVDATA));
		env.data = &data;

		update_environment(&env);
		if (verbosity) {
			dump_env(env.data);
		}
		FILE *of = fopen(envfilepath, "wb");
		if (of) {
			if (fwrite(&data, sizeof(BG_ENVDATA), 1, of) != 1) {
				fprintf(stderr,
					"Error writing to output file: %s\n",
					strerror(errno));
				result = errno;
			}
			if (fclose(of)) {
				fprintf(stderr, "Error closing output file.\n");
				result = errno;
			};
			printf("Output written to %s.\n", envfilepath);
		} else {
			fprintf(stderr, "Error opening output file %s (%s).\n",
				envfilepath, strerror(errno));
			result = 1;
		}
		free(envfilepath);

		return 0;
	}

	/* not in file mode */
	if (!bgenv_init()) {
		fprintf(stderr, "Error initializing FAT environment.\n");
		return 1;
	}

	dump_envs();

	if (!write_mode) {
		return 0;
	}

	BGENV *env_new;
	BGENV *env_current;

	if (auto_update) {
		/* clone latest environment */

		env_current = bgenv_open_latest();
		if (!env_current) {
			fprintf(stderr, "Failed to retrieve latest environment."
					"\n");
			return 1;
		}
		env_new = bgenv_open_oldest();
		if (!env_new) {
			fprintf(stderr, "Failed to retrieve oldest environment."
					"\n");
			return 1;
		}
		if (verbosity) {
			printf("Updating environment with revision %u\n",
			       env_new->data->revision);
		}

		if (!env_current->data || !env_new->data) {
			fprintf(stderr, "Invalid environment data pointer.\n");
			bgenv_close(env_new);
			bgenv_close(env_current);
			return 1;
		}

		memcpy((char *)env_new->data, (char *)env_current->data,
		       sizeof(BG_ENVDATA));
		env_new->data->revision = env_current->data->revision + 1;

		if (!bgenv_close(env_current)) {
			fprintf(stderr, "Error closing environment.\n");
		}
	} else {
		if (part_specified) {
			env_new = bgenv_open_by_index(arguments.which_part);
		} else {
			env_new = bgenv_open_latest();
		}
		if (!env_new) {
			fprintf(stderr, "Failed to retrieve environment by "
					"index.\n");
			return 1;
		}
	}

	update_environment(env_new);

	if (verbosity) {
		printf("New environment data:\n");
		printf("---------------------\n");
		dump_env(env_new->data);
	}
	if (!bgenv_write(env_new)) {
		fprintf(stderr, "Error storing environment.\n");
		return 1;
	}
	if (!bgenv_close(env_new)) {
		fprintf(stderr, "Error closing environment.\n");
		return 1;
	}
	return result;
}
