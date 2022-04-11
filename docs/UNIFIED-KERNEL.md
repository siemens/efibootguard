# Unified Kernel Images #

A unified kernel image combines all artifacts needed to start an OS, typically
Linux, from a single UEFI binary, even in complex scenarios. This includes:
* Kernel (as UEFI binary)
* Kernel command line
* initrd/initramfs (optional, requires kernel version 5.8+)
* alternative device trees (optional)

Using a single binary enables secure boot setups by allowing to sign and later
on validate this binary during boot-up.

The ability to embed and select from multiple device trees permits to replace
the firmware-provide device tree with an alternative one if the kernel requires
deviation or the firmware does not permit easy updates. The final device tree
is selected by matching its compatible property against the firmware device
tree.

## Building unified kernel images ##

EFI Boot Guard provides the `bg_gen_unified_kernel` command to generate the
image from all required artifacts, e.g.:

```
bg_gen_unified_kernel \
    kernel-stubaa64.efi \
    vmlinux-5.17.1 \
    unified-linux.efi \
    --cmdline "console=ttyS0,115200" \
    --initrd initrd-5.17.1 \
    --dtb board-variant-1.dtb \
    --dtb board-variant-2.dtb
```

See also `bg_gen_unified_kernel --help`.

The generated `unified-linux.efi` can then be signed with tools like `pesign`
or `sbsign` to enable secure boot.
