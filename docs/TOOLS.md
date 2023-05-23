# Environment Tools #

Two tools exist for handling `efibootguard`'s environment:
* `bg_setenv`
* `bg_printenv`.

With these, the user can change environment content or display it.

**NOTE**: The environment tools only work, if the correct number and type of
config partitions is detected. This also means that the stored configuration
data must have a valid checksum. If this is not the case, environments must be
repaired first. To do so, follow the `Initial Setup` step section below.

## Initial Setup ##

Generation of a valid configuration partition is described in
[docs/USAGE.md](USAGE.md).

*NOTE*: To access configuration data on FAT partitions, the partition must
either already be mounted, with access rights for the user using the tool, or
the tool can mount the partition by itself. In the former case, it's advised
to mount with `-o sync` to flush written data and associated metadata to the
underlying hardware immediately so to reduce data loss probability on power
cuts. The latter is only possible if the tool has the `CAP_SYS_ADMIN`
capability. This is the case if the user is `root` or the corresponding
capability is set in the filesystem.

## Updating a configuration ##

In most cases, the user wants to update to a new environment configuration,
which can be done with:

```
./bg_setenv --update --kernel="XXXX" --args="YYYY" --watchdog=25
```

The `--update` parameter tells `bg_setenv` to automatically overwrite the
configuration with the lowest revision and sets its revision value to the
highest. Hence, the oldest revision becomes the latest.

*NOTE*: Environment variables are limited to 255 characters as stated in
`include/envdata.h`.

To overwrite a given configuration, specified by a fixed zero-based `config
partition` number, i.e. `4`, execute:

```
./bg_setenv --part=4 [...]
```

To also specify a specific revision to set, i.e. `13`, execute:

```
./bg_setenv --part=4 --revision=13 [...]
```

This specifies the revision number of the configuration in config partition
number 4 to be set to 13.

Please note that the config partition index is zero-based as displayed with:

```
bg_printenv
```

To mark the current environment as working after having successfully booted
with it and having tested essential features, use the `--confirm` option:

```
bg_setenv --confirm
```

To simulate a failed update, with its environment data stored in config partition 1,
issue:

```
bg_setenv --partition=1 --ustate=FAILED --revision=0
```

*NOTE*: The user can either specify a number after `--ustate=` or a string resembling
the value.

To simulate a reboot of a recently updated configuration stored in config partition 1,
issue:

```
bg_setenv --partition=1 --ustate=TESTING
```

### Setting user variables ###

`bg_setenv` has support for default user variables, meaning of type "String". To set a user variable, specify the `-x` flag:

```
bg_setenv -x key=value
```

This will set the variable named `key` to `value` in the current environment.

If The user wants to delete such a variable, the value after the `=` must be omitted, e.g.

```
bg_setenv -x key=
```
will delete the variable with key `key`.

