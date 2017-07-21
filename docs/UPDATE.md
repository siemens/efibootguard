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
    uint8_t testing;
    uint8_t boot_once;
    uint16_t watchdog_timeout_sec;
    uint32_t revision;
    uint32_t crc32;
};
```

The fields have the following meaning:
* `kernelfile`: Path to the kernel image, utf-16 encoded
* `kernelparams`: Arguments to the kernel, utf-16 encoded
* `testing`: A flag that specifies if the configuration is in test mode
* `boot_once`: Set by `efibootguard` if it first boots a test configuration
* `watchdog_timeout_sec`: Number of seconds, the watchdog times out after
* `revision`: The revision number explained above
* `crc32`: A crc32 checksum

The following example cases demonstrate the meaning of the update-specific
struct members.

## Example cases ##

Assume the following for the next examples: The system has been booted with a
configuration that has a `revision` of `4`.  The user can set a new
configuration with a `revision` of `5`, with `testing` flag set. On the next
reboot, `efibootguard` loads the configuration with the highest `revision`
number. If it detects, that `testing` is set, it will enable the `boot_once`
flag and boot the system with this configuration.

### Example scenario 1 - Successful update ###

Once booted, the user disables both `testing` and `boot_once` to confirm the
update using the [tools](TOOLS.md).

### Example scenario 2 - System crash during first boot after update ###

If the system freezes during boot, the watchdog will reset the system. On the
next boot `efibootguard` detects that this configuration had already been
tested before, because `boot_once` is already set. Thus, it will load the 2nd
latest configuration instead. The failed update is indicated by the revision
of the failed configuration set to `0` with both `boot_once` and `testing`
enabled. A revision of 0 is the lowest possible number and avoids that the
corresponding configuration is booted again in future.

## Visual explanation of the update process ##

```
                   +--------------++--------------+
                   |              ||              |
                   | Rev: latest  || Rev: oldest  |
        +--------> |  (working)   ||              |
        |          |              ||              |
        |          +--------------++--------------+
        |                          |
        |                          | update
        |                          v
        |          +---------------++--------------+
        |          |               ||              |
        |          | Rev: latest-1 || Rev: latest  |
        |          |   (working)   || testing: 1   |
        |          |               || boot_once: 0 |
        |          +---------------++--------------+
        |                          |
        |                          | reboot
        |                          v
        |          +---------------++--------------+
        |          |               ||              |
        |          | Rev: latest-1 || Rev: latest  |
        |          |   (working)   || testing: 1   |
        |          |               || boot_once: 1 |
        |          +---------------++--------------+
        |                          |                    no
        |                       success? ---------------------+
        |                          |          watchdog reboot |
        |             yes, confirm |                          |
        |                          v                          v
        |          +----------++--------------+   +----------++--------------+
        |          |          ||              |   |          ||              |
        |          | Rev:     || Rev: latest  |   | Rev:     || Rev: 0       |
        |          | latest-1 || testing: 0   |   | latest-1 || testing: 1   |
        |          |          || boot_once: 0 |   |          || boot_once: 1 |
        |          +----------++--------------+   +----------++--------------+
        |      boots                                          |
        +----- latest' = latest-1 +---------------------------+

```
