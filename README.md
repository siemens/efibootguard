# EFI Boot Guard #

A bootloader based on UEFI.

Provides the following functionality:
* Arm a hardware watchdog prior to loading an OS
* Provides a simple update mechanism with fail-save algorithm

## Watchdog support ##

The following watchdog drivers are implemented:
* Intel Quark
* Intel TCO
* Intel i6300esb

## Configuration ##

`efibootguard` reads its configuration from an environment storage. Currently,
the following environment backends are implemented:
* Dual FAT Partition storage

## Update Mechanism ##

Each environment configuration has a revision. The number of possible
environments must be configured at compile-time. The larger the revision value,
the newer the environment is. `efibootguard` always loads the latest environment
data, meaning the one with the greatest revision value.

The structure of the environment data is as follows:

```
struct _BG_ENVDATA {
    uint16_t kernelfile[ENV_STRING_LENGTH];
    uint16_t kernelparams[ENV_STRING_LENGTH];
    uint8_t testing;
    uint8_t boot_once;
    uint16_t watchdog_timeout_sec;
    uint32_t revision;
    uint32_t crc32;
};
```

The fields have the following meaning:
* `kernelfile`: Path to the kernel image, utf-16 encoding
* `kernelparams`: Arguments to the kernel, utf-16 encoding
* `testing`: A flag that specifies if the configuration is in test mode
* `boot_once`: Set by `efibootguard` if it first boots a test configuration
* `watchdog_timeout_sec`: Number of seconds, the watchdog times out after
* `revision`: The revision number explained above
* `crc32`: A crc32 checksum


Assume, the system has been booted with a configuration that has a `revision` of
`4`. The user can set a new configuration with a revision of `5`, with `testing`
flag set. On the next reboot, `efibootguard` loads the configuration with the
highest `revision` number. If it detects, that `testing` is set, it will enable
the `boot_once` flag and boot the system with this configuration.

### Example scenario 1 - Successful update ###

Once booted, the user disables both `testing` and `boot_once` to confirm the
udpate.

### Example scenario 2 - System crash during first boot after update ###

If the system crashes during boot, the watchdog will reset the system.
Afterwards, `efibootguard` sees, that this configuration had already been tested
before, because `boot_once` is already set. Thus, it will deactivate both
`boot_once` and `testing` and load the 2nd latest configuration instead. It will
signal to the user, that the update failed by setting the revision of the failed
configuration to `0`.

### Visual explanation of the update process ###

                             +--------------++--------------+
                             |              ||              |
                             | Rev: latest  || Rev: oldest  |
                             |  (working)   ||              |
                             |              ||              |
                             +--------------++--------------+
                                    |                |
                                +---+                | update
                                |                    |
                                v                    v
                         +---------------+    +--------------+
                         |               |    |              |
                  +----> | Rev: latest-1 |    | Rev: latest  |
                  |      |   (working)   |    | testing: 1   |
                  |      |               |    | boot_once: 0 |
                  |      +---------------+    +--------------+
                  |                                  |
                  |                                  | reboot
                  |                                  v
                  |                           +--------------+
                  |                           |              |
                  |                           | Rev: latest  |
                  |                           | testing: 1   |
                  |                           | boot_once: 1 |
                  |                           +--------------+
                  |                                  |             no
                  |                               success? ------------------+
                  |                watchdog reboot   |       watchdog reboot |
                  |                     yes, confirm |                       |
                  |                                  v                       v
                  |                           +--------------+    +-------------------+
                  |                           |              |    |                   |
                  |                           | Rev: latest  |    | Rev: 0            |
                  |                           | testing: 0   |    | testing: 1        |
                  |                           | boot_once: 0 |    | boot_once: 1      |
                  |                           +--------------+    +-------------------+
                  |      boots                                               |         
                  +----- latest' = latest-1 +--------------------------------+


## Environment Tools ##

In the `tools` directory, there is a utility named `bg_setenv`/`bg_printenv`.
With this, the user can display the configuration data or update as needed.

**NOTE**: The environment tools only work, if the correct number of config partitions is detected. This also means that the stored configuration data has a valid checksum. If this is not the case, environments must be repaired first. To do so, follow the initial setup step explained below in the `Installation` section.

To access configuration data on FAT partitions, the partition must either
already be mounted, with access rights for the user using the tool, or the tool
can mount the partition by itself, provided that it has the `CAP_SYS_ADMIN`
capability. This is the case if the user is granted `root` privileges or the
corresponding capability is set in the filesystem.

*NOTE*: `CHAR16` environment variables are limited to 255 characters as stated in `include/envdata.h`.

### Creating a new configuration ###

In most cases, the user wants to create a new environment configuration, which
can be done by

```
./bg_setenv -u --kernel="XXXX" --args="YYYY" --watchdog=25 --testing=1
```

The `-u` parameter tells `bg_setenv` to automatically overwrite the oldest
configuration set and sets the revision value to the newest.

If the user wants to specify the revision number and the configuration partition
to overwrite manually, he can do so by

```
./bg_setenv -p 4 -r 13 [...]
```

which specifies to set the data set in partition number 4 to revision 13. Please
keep in mind, that counting of configuration partitions starts with 0.

Some debug output can be activated if compiling with
```
make DEBUG=1
```

#### Kernel Location ####

To load the kernel from a different FAT partition than `efibootguard`, there are two possible mechanisms. One directly uses the label of the FAT partition, created with `dosfslabel`:

```
./bg_setenv -u --kernel="L:FATLABEL:kernelfile"
```

where `FATLABEL` is the label of the FAT partition. On some older UEFI implementations, the label is not supported properly and a user defined label can be created instead, which is a file named `EFILABEL` in the root directory of the corresponding FAT partition. This file contains an UTF-16le encoded partition name and can be used as follows:

```
./bg_setenv -u --kernel="C:USERLABEL:kernelfile"
```

### Interface API ###

The library `libebgenv.a` provides an API to access the environment from a user program.

The header file with the prototypes and a short description is `ebgenv.h`.

Documentation and further examples can be found in `swupdate-adapter/swupdate.md`.

The following example program opens the current environment and modifies the kernel file name:

```c
#include <stdbool.h>
#include "ebgenv.h"

int main(void)
{
    ebg_env_open_current();
    ebg_env_set("kernelfile", "vmlinux-new");
    ebg_env_close();
    return 0;
}
```

The following example program creates a new environment with the latest revision and sets it to
the testing state:

```c
#include <stdbool.h>
#include "ebgenv.h"

int main(void)
{
    ebg_env_create_new();
    ebg_env_set("kernelfile", "vmlinux-new");
    ebg_env_set("kernelparams", "root=/dev/bootdevice");
    ebg_env_set("watchdog_timeout_sec", "30");
    ebg_env_close();
    return 0;
}
```

*Note*: If no watchdog timeout value is specified, a default of 30 seconds is set.

## Required libraries and headers for compilation ##

### Arch Linux ###

```
pacman -S gnu-efi-libs pciutils
```

### Debian 8 ###

```
apt-get install gnu-efi libparted-dev libpci-dev
```

## Installation ##

### Environment setup ###

Create the needed number of FAT16 partitions as defined by `CONFIG_PARTITION_COUNT` in `include/envdata.h`. Create a new `BGENV.DAT` configuration file with the `bg_setenv` tool and the `-f` option and copy the files to the FAT16 partitions.

*NOTE*: Currently, FAT partitions must neither be `FAT12`, `FAT32` nor `FAT32e`. During detection of config partitions, all non-FAT16 partitions are ignored and do not count to the valid number of config partitions. 

### Bootloader installation ###

Copy `efibootguard.efi` to `EFI/boot/` and rename it to bootx64.efi.

If the system does not select this file for booting automatically, boot into `UEFI shell` and use the `bcfg` command:

Issue the following command to list the currently configured boot sequence:
```
bcfg boot dump
```

The following command deletes item number `n`:
```
bcfg boot rm `n`
```

The following command create an entry for `bootx64.efi`:
```
bcfg boot add 0 fs0:\efi\boot\bootx64.efi "efi boot guard"
```
where the binary is on drive `fs0:`.

Exit efi shell with the `reset` command.

## Future work ##

* The number of valid config partitions expected by the bootloader and the tools is currently fixed to the number defined by `CONFIG_PARTITION_COUNT` in `include/envdata.h`. This value should be made configurable by a config flag.
