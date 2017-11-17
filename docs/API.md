# API Library #

## General information ##

The library `libebgenv.a` provides an API to access the environment from a
user space program.

The header file with the interface definitions is
[/include/ebgenv.h](../include/ebgenv.h).

The interface provides functions to:
* enable/disable for output to stdout and stderr
* create a new environment
* edit the latest environment
* reset the error state
* check the last update state
* manage user variables

To link a program to the library, you can install `efibootguard` with

```
make install
```

This will install `libebgenv.a` into your system for your linker to find it (usually
to `/usr/lib/libebgenv.a`).

If you want to cross-compile this library, you have to cross-compile the
`efibootguard tools` since both tools and this API library both depend on a
common static library. Refer to [compilation instructions](COMPILE.md).

## User variables ##

User variables are automatically set if the given variable key is not part of
the internally pre-defined variable set. They reside in a special memory area
of the environment managed by the API.

The structure of an entry is explained in the [source code](../env/uservars.c).
Also see the example program below.

## Example programs ##

The following example program creates a new environment with the latest revision
and sets it to the testing state:

```c
#include <stdbool.h>
#include "ebgenv.h"

int main(void)
{
    ebgenv_t e;

    ebg_env_create_new(&e);
    ebg_env_set(&e, "kernelfile", "vmlinux-new");
    ebg_env_set(&e, "kernelparams", "root=/dev/bootdevice");
    ebg_env_set(&e, "watchdog_timeout_sec", "30");
    ebg_env_close(&e);
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
    ebgenv_t e;

    ebg_env_open_current(&e);
    ebg_env_set(&e, "kernelfile", "vmlinux-new");
    ebg_env_close(&e);
    return 0;
}
```

### Example on user variable usage ###

```c
#include <stdbool.h>
#include "ebgenv.h"

int main(void)
{
    ebgenv_t e;

    ebg_env_open_current(&e);

    /* This automatically creates a local user variable, stored in the
     * current environment */
    ebg_env_set(&e, "myvar", "myvalue");

    /* This sets the variable to an empty string */
    ebg_env_set(&e, "myvar", "");

    /* This deletes the variable from the current environment */
    ebg_env_set_ex(&e, "myvar", USERVAR_TYPE_DELETED, "", 1);

    ebg_env_close(&e);
}

```
