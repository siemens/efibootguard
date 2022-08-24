# Compilation Instructions #

## Required libraries and headers for compilation ##

If you are building from a git checkout, make sure that you have cloned
the git submodules as well, e.g.

```
git submodule update --init
```

### Arch Linux ###

```
# libs
pacman -S gnu-efi-libs pciutils

# build tools
pacman -S gcc make automake autoconf autoconf-archive libtool pkg-config python

# test dependencies
pacman -S check bash-bats
```

### Debian ###

Debian 8 or newer:

```
# libs
apt-get install gnu-efi libpci-dev

# build tools
apt-get install make automake autoconf autoconf-archive libtool pkg-config python3

# test dependencies
apt-get install check bats
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

## Testing ##

* `make check` will run all unit tests.
* `bats tests` will run all integration tests.
