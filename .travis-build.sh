#!/bin/bash

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
                     CFGDIR=/usr/share/cppcheck \
                     HAVE_RULES=no -j2 || \
            return -1
    sudo make -C cppcheck install >/dev/null \
            || return -1
    # On travis cppcheck ignores CFGDIR. Instead, it looks in $PWD. Compare
    # strace output.
    sudo install -m644 ./cppcheck/cfg/* ./ || return -1
    rm -rf cppcheck
}

case "$TARGET_EFFECTIVE" in
    native)
        prepare_build
        enter_build
        ../configure
        exec make check
        ;;

    i586)
        sudo apt-get install --no-install-recommends \
            --target-release xenial libcmocka-dev:i386
        prepare_build
        enter_build
        ../configure --with-gnuefi-lib-dir=/usr/lib32 CFLAGS=-m32 \
            host_alias=i586-linux
        exec make check
        ;;

    cppcheck)
        echo "Building and installing cppcheck..."
        if ! install_cppcheck >cppcheck_build.log 2>&1
        then
            cat cppcheck_build.log
            exit -1
        fi
        prepare_build
        ./configure

        suppress=""
        # Justified suppressions:
        # Not part of the project:
        suppress+=" --suppress=variableScope:/usr/include/bits/stdlib-bsearch.h"
        # Function 'efi_main' is called by efi:
        suppress+=" --suppress=unusedFunction:main.c"

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

        cpp_conf="-U__WINT_TYPE__"
        # Exit code '1' is returned if arguments are not valid or if no input
        # files are provided. Compare 'cppcheck --help'.
        exec cppcheck -f -q --error-exitcode=2 --std=posix \
            $enable $suppress $cpp_conf $includes .
        ;;
    coverity_prepare)
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
        exit -1
        ;;
esac

