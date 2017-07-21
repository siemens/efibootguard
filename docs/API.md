# API Library #

## General information ##

The library `libebgenv.a` provides an API to access the environment from a
user space program.

The header file with the interface definitions is
[/swupdate-adapter/ebgenv.h](../swupdate-adapter/ebgenv.h).

The interface provides functions to:
* enable/disable for output to stdout and stderr
* create a new environment
* edit the latest environment
* reset the error state
* check the last update state

An example and detailed information on how to interpret the returned state values
is given in the [swupdate-adapter documentation](../swupdate-adapter/swupdate.md).

To link a program to the library, you can install `efibootguard` with

```
make install
```

This will install `libebgenv.a` into your system for your linker to find it (usually
to `/usr/lib/libebgenv.a`).

If you want to cross-compile this library, you have to cross-compile the
`efibootguard tools` since both tools and this API library both depend on a
common static library. Refer to [compilation instructions](COMPILE.md).

## Example programs ##

The following example program creates a new environment with the latest revision
and sets it to the testing state:

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

*Note*: If no watchdog timeout value is specified, a default of 30 seconds is
set.

### Advanced Usage ###

In some cases, for example in tests, access to the current environment is
needed. The following example program opens the current environment and
modifies the kernel file name:

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
