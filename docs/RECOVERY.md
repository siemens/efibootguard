# Recovery Mode #

If more than the expected number of environments is detected during boot, the
system stops booting. This can be a problem if the user wants to boot the
system with a memory stick to update a broken installation.

In order to allow external boot media with other environment configurations,
the Recovery Mode was introduced. If any environment is found on the boot
device, the boot loader will only use environments from this device.

## Config filter ##

The config filter depends on the detection of the boot media. For currently
allowed boot media and GPT partitioning, the device path as retrieved from the
loaded image is something like

```
PciRoot(0)/Pci(0x1,0x1)/Ata(Primary,Master)/HD(Part1,Sig12345678-1234-1234-1234-123456789012)
```

The last node is taken off by the function GetBootMediumPath resulting in

```
PciRoot(0)/Pci(0x1,0x1)/Ata(Primary,Master)
```

After enumerating all config partitions, if recovery mode is active, all config
partitions that don't share this common 'BootMediumpath' are sorted out.
