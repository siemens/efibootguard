#!/bin/bash
#
# EFI Boot Guard
#
# Copyright (c) Siemens AG, 2017
#
# Authors:
#  Claudius Heine <ch@denx.de>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#
# SPDX-License-Identifier:	GPL-2.0
#

set -euo pipefail

PARAM="${PARAM-"${1-""}"}"
TARGET="${TARGET-""}"

COVERITY_SCAN_BRANCH="${COVERITY_SCAN_BRANCH:-"0"}"
if [ "$COVERITY_SCAN_BRANCH" == "1" ]
then
    if [ "$TARGET" == "native" ]
    then
        TARGET_EFFECTIVE="${PARAM:-"success"}"
    else
        TARGET_EFFECTIVE="success"
    fi
else
    TARGET_EFFECTIVE="${PARAM:-"${TARGET}"}"
fi

install_common_deps()
{
    sudo apt-get install gcc-multilib gnu-efi libpci-dev
}

install_native_deps()
{
    sudo apt-get install --no-install-recommends \
         libz-dev check
}

install_i586_deps()
{
    sudo apt-get install --no-install-recommends \
         libz-dev:i386 check:i386
}

prepare_build()
{
    autoreconf -fi
}


enter_build()
{
    mkdir -p build
    cd build
}

install_cppcheck()
{
    git clone https://github.com/danmar/cppcheck.git
    git -C cppcheck checkout 1.80
    make -C cppcheck SRCDIR=build \
                     CFGDIR=/opt/cppcheck/cfg \
                     PREFIX=/opt/cppcheck \
                     HAVE_RULES=no install -j2 || \
            return -1
    rm -rf cppcheck
}

case "$TARGET_EFFECTIVE" in
    native)
        install_common_deps
        install_native_deps
        prepare_build
        enter_build
        ../configure
        exec make check
        ;;

    i586)
        install_common_deps
        install_i586_deps
        prepare_build
        enter_build
        export PKG_CONFIG_DIR=
        export PKG_CONFIG_PATH=/usr/lib/i386-linux-gnu/pkgconfig
        export PKG_CONFIG_LIBDIR=/usr/lib/i386-linux-gnu
        ../configure --with-gnuefi-lib-dir=/usr/lib32 CFLAGS=-m32 \
            host_alias=i586-linux
        exec make check
        ;;

    cppcheck)
        install_common_deps
        install_native_deps
        echo "Building and installing cppcheck..."
        if [ ! -x /opt/cppcheck/bin/cppcheck ]
        then
            install_cppcheck
        fi
        prepare_build
        ./configure

        ignore=""
        ignore+=" -i tests/fff"

        suppress=""
        # Justified suppressions:
        # Does not belong to the project
        suppress+=" --suppress=*:/usr/include/*"
        suppress+=" --suppress=*:/usr/include/bits/*"
        # Function 'efi_main' is called by efi:
        suppress+=" --suppress=unusedFunction:main.c"
        # Some functions are defined for API only
        suppress+=" --suppress=unusedFunction:utils.c"
        suppress+=" --suppress=unusedFunction:env/env_api.c"
        suppress+=" --suppress=unusedFunction:env/fatvars.c"
        suppress+=" --suppress=unusedFunction:tools/tests/test_environment.c"
        suppress+=" --suppress=unusedFunction:env/env_api_fat.c"
        # Some functions are used by linker wrapping
        suppress+=" --suppress=unusedFunction:tools/tests/test_probe_config_file.c"
        suppress+=" --suppress=unusedFunction:tools/tests/test_ebgenv_api.c"
        # EFI uses void* as ImageBase needed for further calculations
        suppress+=" --suppress=arithOperationsOnVoidPointer:main.c"

        enable="--enable=warning \
                --enable=style \
                --enable=performance \
                --enable=portability \
                --enable=unusedFunction"

        includes="-I . \
                  -I include \
                  -I /usr/include \
                  -I /usr/include/linux \
                  -I /usr/include/efi \
                  -I /usr/include/efi/x86_64 \
                  -I /usr/include/x86_64-linux-gnu \
                  -I /usr/lib/gcc/x86_64-linux-gnu/4.8/include"

        cpp_conf="-U__WINT_TYPE__ -U__GNUC__"
        # Exit code '1' is returned if arguments are not valid or if no input
        # files are provided. Compare 'cppcheck --help'.
        exec /opt/cppcheck/bin/cppcheck -f -q --error-exitcode=2 \
            $enable $suppress $ignore $cpp_conf $includes .
        ;;
    coverity_prepare)
        install_common_deps
        install_native_deps
        prepare_build
        enter_build
        ../configure
        exit 0
        ;;
    coverity_build)
        enter_build
        exec make
        ;;
    success)
        echo "Skipping $TARGET" >&2
        exit 0
        ;;
    *)
        echo "No or not existing targen choosen." \
             "($TARGET, $TARGET_EFFECTIVE)" >&2
        exit -1
        ;;
esac
