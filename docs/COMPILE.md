# Compilation Instructions #

## Required libraries and headers for compilation ##

### Arch Linux ###

```
pacman -S gnu-efi-libs pciutils
```

### Debian 8 ###

```
apt-get install gnu-efi libpci-dev
```

## Compilation ##

This project uses autotools. Here you find useful documentations for
[autoconf](https://www.gnu.org/software/autoconf/manual/autoconf.html), and
[automake](https://www.gnu.org/software/automake/manual/automake.html).

To compile `efibootguard`, checkout the sources and run:

```
autoreconf -fi
./configure
make
```

To cross-compile, the environment variables must be set accordingly, i.e.
`CXX=<compiler-to-use>`. The following example shows how to specify needed
paths for an out-of-tree build, where cross-compilation environment variables
have already been set before (i.e. by an embedded SDK from `yocto` or alike):

```
mkdir build
cd build
autoreconf -fi ..
../configure --host=i586 --build=x86_64-unknown-linux-gnu \
 --with-gnuefi-sys-dir=<sys-root-dir> \
 --with-gnuefi-include-dir=<sys-root-dir>/usr/include/efi \
 --with-gnuefi-lib-dir=<sys-root-dir>/usr/lib
make
```

where `<sys-root-dir>` points to the wanted sysroot for cross-compilation.
