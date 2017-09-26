# Installation AND Usage #

In order to use `efibootguard` one needs to
* have a valid disk partitioning scheme
* have the bootloader binary installed in the proper place
* have valid configuration files
* configure the UEFI boot sequence (may be optional)

## Creating a valid partitioning scheme ##

UEFI by default supports FAT file systems, which are used to store
configuration data for `efibootguard`. The following partition type GUIDS are
supported for GPT partition entries:

GUID                                 | description
-------------------------------------|----------------------------------
EBD0A0A2-B9E5-4433-87C0-68B6B72699C7 | Microsoft default data partition
C12A7328-F81F-11D2-BA4B-00A0C93EC93B | EFI System partition

For robustness of the fail-safe mechanism, each configuration file revision is
stored into a separate FAT partition. The following example shows how to create a
new GPT using `parted`:

**IMPORTANT**: Replace `/dev/sdX` with the correct block device.

* Start `parted` for block device `/dev/sdX` and create an EFI system partition

```
# parted /dev/sdX
(parted) mklabel GPT
(parted) mkpart
Partition name?  []?
File system type?  [ext2]? fat32
Start? 0%
End? 20%
(parted) toggle 1
Flag to Invert? ESP
```

* Create two config partitions

```
(parted) mkpart
Partition name?  []?
File system type?  [ext2]? fat16
Start? 20%
End? 40%
(parted) mkpart
Partition name?  []?
File system type?  [ext2]? fat16
Start? 40%
End? 60%
```

* Create two root partitions and leave `parted`

```
(parted) mkpart
Partition name?  []?
File system type?  [ext2]? ext4
Start? 60%
End? 80%
(parted) mkpart
Partition name?  []?
File system type?  [ext2]? ext4
Start? 80%
End? 100%
(parted) q
```

* Create all file systems

```
# mkfs.fat /dev/sdX1
# mkfs.fat -F 16 /dev/sdX2
# mkfs.fat -F 16 /dev/sdX3
# mkfs.ext4 /dev/sdX4
# mkfs.ext4 /dev/sdX5
```

*NOTE*: `FAT16`, as specified by `-F 16` is usefull for smaller partitions
(i.e. 500 MB). `FAT12` and `FAT32` is also supported.

## Install the boot loader binary file ##

This example is for an `x64` architecture.

```
# mount /dev/sdX1 /mnt
# mkdir -p /mnt/EFI/boot
# cp efibootguardx64.efi /mnt/EFI/boot/bootx64.efi
# umount /mnt
```

## Create a default configuration ##

This step first creates a custom label contained in `EFILABEL`, which is later
used to specify the kernel location.

```
# mount /dev/sdX2 /mnt
# echo -n "KERNEL1" | iconv -f ascii -t UTF-16LE > /mnt/EFILABEL
# bg_setenv -f /mnt -r 1 --kernel="C:KERNEL1:vmlinuz-linux" --args="root=/dev/sdX4 noinitrd"
# umount /mnt
# mount /dev/sdX3 /mnt
# echo -n "KERNEL2" | iconv -f ascii -t UTF-16LE > /mnt/EFILABEL
# bg_setenv -f /mnt -r 2 --kernel="C:KERNEL2:vmlinuz-linux" --args="root=/dev/sdX5 noinitrd"
# umount /mnt
```

## Configuring UEFI boot sequence (Optional) ##

UEFI compliant firmwares fall back to a standard search path for the boot loader binary. This is

```
/EFI/BOOT/BOOT<arch>.EFI
```

In some cases, if the system does not select the correct `bootx64.efi` for
booting automatically, use the `efibootmgr` user space tool to setup the boot
sequence configuration.

Another possibility is to boot into `UEFI shell` and use the `bcfg` command.

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

Exit `UEFI shell` with the `reset` command.

## Kernel Location ##

If you just specify a file name as `--kernelfile`, `efibootguard` loads the
kernel from the same FAT partition as the boot loader binary itself.

To load the kernel from a different FAT partition than `efibootguard`, there are
two possible mechanisms. One directly uses the label of the FAT partition,
created with `dosfslabel`:

```
./bg_setenv -u --kernel="L:FATLABEL:kernelfile"
```

where `FATLABEL` is the label of the FAT partition. On some older UEFI
implementations, the label is not supported properly and a user defined label
can be created instead, which is a file named `EFILABEL` in the root directory
of the corresponding FAT partition. This file contains an UTF-16le encoded
partition name and can be used as follows:

```
./bg_setenv -u --kernel="C:USERLABEL:kernelfile"
```

*NOTE*: Do not mix-up the file system label and the GPT entry label.

