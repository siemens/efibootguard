# Update Mechanism #

`efibootguard` works with a predefined number of configuration environments.
The number is defined at compile-time.  Each environment has a revision.
`efibootguard` always loads the latest environment data, which is indicated by
the highest revision value.

The structure of the environment data is as follows:

```c
struct _BG_ENVDATA {
    uint16_t kernelfile[ENV_STRING_LENGTH];
    uint16_t kernelparams[ENV_STRING_LENGTH];
    uint8_t padding;
    uint8_t ustate;
    uint16_t watchdog_timeout_sec;
    uint32_t revision;
    uint8_t userdata[ENV_MEM_USERVARS];
    uint32_t crc32;
};
```

The fields have the following meaning:
* `kernelfile`: Path to the kernel image, utf-16 encoded
* `kernelparams`: Arguments to the kernel, utf-16 encoded
* `padding`: Padding byte to stay compatible with the offsets of the previous
             version.
* `ustate`: Update status (`0` OK, `1` INSTALLED, `2` TESTING, `3`: FAILED)
* `watchdog_timeout_sec`: Number of seconds, the watchdog times out after
* `revision`: The revision number explained above
* `userdata`: Stores user variables. See [API.md](API.md) for explanation.
* `crc32`: A crc32 checksum

The following example cases demonstrate the meaning of the update-specific
struct members.

## Example cases ##

Assume the following for the next examples: The system has been booted with a
configuration that has a `revision` of `4`.  The user can set a new
configuration with a `revision` of `5`, with `ustate` set to `"INSTALLED"`
(`1`). On the next reboot, `efibootguard` loads the configuration with the
highest `revision` number. If it detects, that `ustate` is `"INSTALLED"` `(1)`,
it will update the value to `"TESTING"` `(2)` and boot the system with this
configuration.

### Example scenario 1 - Successful update ###

Once booted, the user resets `ustate` to a value of `"OK"` (`0`) to confirm the
update using the [tools](TOOLS.md).

### Example scenario 2 - System crash during first boot after update ###

If the system freezes during boot, the watchdog will reset the system. On the
next boot `efibootguard` detects that this configuration had already been
tested before, because `ustate` is `"TESTING"` (`2`). Thus, it will load the
2nd latest configuration instead. The failed update is indicated by the
revision of the failed configuration set to `"OK"` (`0`) with `ustate` set to
`"FAILED"` (`3`) . A revision of `0` is the lowest possible number and avoids
that the corresponding configuration is booted again in future.

## Visual explanation of the update process ##

```
                   +--------------++--------------+
                   |              ||              |
                   | Rev: latest  || Rev: oldest  |
        +--------> |  (working)   ||              |
        |          +--------------++--------------+
        |                          |
        |                          | update
        |                          v
        |          +---------------++-----------------------+
        |          |               ||                       |
        |          | Rev: latest-1 || Rev: latest           |
        |          |   (working)   || ustate: INSTALLED (1) |
        |          +---------------++-----------------------+
        |                          |
        |                          | reboot
        |                          v
        |          +---------------++---------------------+
        |          |               ||                     |
        |          | Rev: latest-1 || Rev: latest         |
        |          |   (working)   || ustate: TESTING (2) |
        |          +---------------++---------------------+
        |                          |                    no
        |                       success? ---------------------+
        |                          |          watchdog reboot |
        |             yes, confirm |                          |
        |                          v                          v
        |  +----------++---------------+   +----------++--------------------+
        |  |          ||               |   |          ||                    |
        |  | Rev:     || Rev: latest   |   | Rev:     || Rev: 0             |
        |  | latest-1 || ustate: OK (0)|   | latest-1 || ustate: FAILED (3) |
        |  +----------++---------------+   +----------++--------------------+
        |      boots                                          |
        +----- latest' = latest-1 +---------------------------+

```
