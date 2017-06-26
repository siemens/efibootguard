# Update process with swupdate #

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

## Installation of Update ##

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


### Resulting environment ###

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

## Rebooting ##

efibootguard will detect the `testing` flag and set `boot_once`. This can be simulated by

```
bg_setenv -p X -b
```

where `X` is the 0-based index of the config partition to be updated. This sets the `boot_once` flag.

### Resulting Environment ###

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

## Confirming working update ##

```
bg_setenv -c
```

### Resulting environment ###

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

## Not confirming and rebooting ##

After rebooting with state == INSTALLED and not confirming,
the resulting environment is:

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
* ebg_env_isupdatesuccessful == FALSE (since a revision is 0 and both flags set in this config)

### Manually resetting failure state ###

```
bg_setenv -u -t 0
```
*NOTE: The -u option takes all values from the current environment,
replaces the oldest environment with these and then updates the
newly set values (-t 0).*

### Resulting environment ###

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

# State mapping #

Condition A is ebg_env_isupdatesuccessful == true

Suricatta state | condition | function
-------------------------------------------------------------------------------------
FAILED		| !A				      | !ebg_env_isupdatesuccessful()
OK              | A && testing == 0                   | ebg_env_isokay()
INSTALLED       | A && testing == 1 && boot_once == 0 | ebg_env_isinstalled()
TESTING         | A && testing == 1 && boot_once == 1 | ebg_env_istesting()

