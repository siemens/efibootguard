# Update process with swupdate #


## Update state mapping ##

Swupdate-Suricatta works with an internal state variable, which is called `ustate`
per default.

The values of common interest are:

ustate  | meaning
--------|-----------------------------------
0       | nothing to do
1       | update installed (reboot pending)
2       | update testing (after reboot)
3       | update failed
4       | state not available

`efibootguard` works with three internal variables regarding the update mechanism:

* `revision`
* `testing`
* `boot_once`

The values of these variables are mapped onto ustate according to the following matrix:

*Note*: A failed revision exists, if its `revision` is `0` and at the same time,
both `boot_once` and `testing` are set to `1`. If such a revision exists in any of
the stored environment partitions, this is marked as [FAILED] in the matrix below.

`efibootguard`                                                             | `suricatta`
---------------------------------------------------------------------------|------------
 (current env.)<br />testing = 0<br />boot_once = 0<br /><br />NOT [FAILED]| ustate = 0
 (current env.)<br />testing = 1<br />boot_once = 0<br /><br />NOT [FAILED]| ustate = 1
 (current env.)<br />testing = 1<br />boot_once = 1<br /><br />NOT [FAILED]| ustate = 2
 [FAILED]                                                                  | ustate = 3
 Environment<br />Error                                                    | ustate = 4


## Update state mapping with API functions ##

1. Call `ebg_env_open_current`, which will initialize the configuration environment.

2. Use the following logic

```
ebg_env_isupdatesuccessful() is false?
	ustate = 3
else
	ebg_env_isokay() is true?
		ustate = 0

	ebg_env_isinstalled() is true?
		ustate = 1

	ebg_env_istesting() is true?
		ustate = 2

```

3. call `ebg_env_close()`


## Detailed example ##

Test environment: 2 config partitions, FAT16, GPT

**Initial suricatta state: OK**

raw data:


```
 Config Partition #0 Values:
revision: 15
kernel: L:CONFIG1:vmlinuz-linux
kernelargs: root=/dev/sda4 rw initrd=initramfs-linux.img nomodeset
watchdog timeout: 30 seconds
test flag: disabled
boot once flag: not set

 Config Partition #1Values:
revision: 14
kernel: L:CONFIG1:vmlinuz-linux
kernelargs: root=/dev/sda4 rw initrd=initramfs-linux.img nomodeset
watchdog timeout: 30 seconds
test flag: disabled
boot once flag: not set
```

### Installation of Update ###

Used sw-description:

```
software =
{
	version = "0.1.0";
	bootenv: (
		{
			name = "testing";
			value = "1";
		},
		{
			name = "kernelfile";
			value = "L:CONFIG1:vmlinuz-linux";
		},
		{
			name = "kernelparams";
			value = "root=/dev/sda4 rw initrd=initramfs-linux.img nomodeset";
		}
	);
}
```

Command to generate swupdate cpio payload:

```
echo -n "sw-description" | cpio --format=crc -o > test.swu
```


Command with block dev access to update efibootguard environment:

```
swupdate -v -i test.swu
```


#### Resulting environment ####

```
 Config Partition #0 Values:
revision: 15
kernel: L:CONFIG1:vmlinuz-linux
kernelargs: root=/dev/sda4 rw initrd=initramfs-linux.img nomodeset
watchdog timeout: 30 seconds
test flag: disabled
boot once flag: not set

 Config Partition #1 Values:
revision: 16
kernel: L:CONFIG1:vmlinuz-linux
kernelargs: root=/dev/sda4 rw initrd=initramfs-linux.img nomodeset
watchdog timeout: 30 seconds
test flag: enabled
boot once flag: not set
```

**suricatta state: INSTALLED**

Test conditions:
* ebg_env_isupdatesuccessful == TRUE (since no revision is 0)
* testing == 1 && boot_once == 0

Function to retrieve state: `ebg_env_isinstalled()`

### Rebooting ###

efibootguard will detect the `testing` flag and set `boot_once`. This can be
simulated by

```
bg_setenv -p X -b
```

where `X` is the 0-based index of the config partition to be updated. This sets
the `boot_once` flag.

#### Resulting Environment ####

```
 Config Partition #0 Values:
revision: 15
kernel: L:CONFIG1:vmlinuz-linux
kernelargs: root=/dev/sda4 rw initrd=initramfs-linux.img nomodeset
watchdog timeout: 30 seconds
test flag: disabled
boot once flag: not set

 Config Partition #1 Values:
revision: 16
kernel: L:CONFIG1:vmlinuz-linux
kernelargs: root=/dev/sda4 rw initrd=initramfs-linux.img nomodeset
watchdog timeout: 30 seconds
test flag: enabled
boot once flag: set
```

**suricatta state: TESTING**

Test conditions:
* ebg_env_isupdatesuccessful == TRUE (since no revision is 0)
* testing == 1 && boot_once == 1

Function to retrieve state: `ebg_env_istesting()`

### Confirming working update ###

```
bg_setenv -c
```

#### Resulting environment ####

```
 Config Partition #0 Values:
revision: 15
kernel: L:CONFIG1:vmlinuz-linux
kernelargs: root=/dev/sda4 rw initrd=initramfs-linux.img nomodeset
watchdog timeout: 30 seconds
test flag: disabled
boot once flag: not set

 Config Partition #1 Values:
revision: 16
kernel: L:CONFIG1:vmlinuz-linux
kernelargs: root=/dev/sda4 rw initrd=initramfs-linux.img nomodeset
watchdog timeout: 30 seconds
test flag: disabled
boot once flag: not set
```

**suricatta state: OK**

Test conditions:
* ebg_env_isupdatesuccessful == TRUE (since no revision is 0)
* testing == 0

Function to retrieve state: `ebg_env_isokay()`

### Not confirming and rebooting ###

After rebooting with state == INSTALLED and not confirming,
the resulting environment is:

#### Resulting environment ####
```
 Config Partition #0 Values:
revision: 15
kernel: L:CONFIG1:vmlinuz-linux
kernelargs: root=/dev/sda4 rw initrd=initramfs-linux.img nomodeset
watchdog timeout: 30 seconds
test flag: disabled
boot once flag: not set

 Config Partition #1 Values:
revision: 0
kernel: L:CONFIG1:vmlinuz-linux
kernelargs: root=/dev/sda4 rw initrd=initramfs-linux.img nomodeset
watchdog timeout: 30 seconds
test flag: enabled
boot once flag: set
```

**suricatta state: FAILED**

Test conditions:
* ebg_env_isupdatesuccessful == FALSE (since a revision is 0 and both flags set
  in this config)

### Manually resetting failure state ###

```
bg_setenv -u -t 0
```
*NOTE: The -u option takes all values from the current environment,
replaces the oldest environment with these and then updates the
newly set values (-t 0).*

#### Resulting environment ####

```
 Config Partition #0 Values:
revision: 15
kernel: L:CONFIG1:vmlinuz-linux
kernelargs: root=/dev/sda4 rw initrd=initramfs-linux.img nomodeset
watchdog timeout: 30 seconds
test flag: disabled
boot once flag: not set

 Config Partition #1 Values:
revision: 16
kernel: L:CONFIG1:vmlinuz-linux
kernelargs: root=/dev/sda4 rw initrd=initramfs-linux.img nomodeset
watchdog timeout: 30 seconds
test flag: disabled
boot once flag: not set
```

