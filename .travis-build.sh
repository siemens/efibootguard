#!/bin/bash

set -euo pipefail

TARGET="${TARGET-"$1"}"

prepare_build()
{
    autoreconf -fi
    mkdir build
    cd build
}
case "$TARGET" in
    native)
        prepare_build
        ../configure
        exec make check
        ;;
    i586)
        sudo apt-get install --no-install-recommends \
            --target-release xenial libcmocka-dev:i386
        prepare_build
        ../configure --with-gnuefi-lib-dir=/usr/lib32 CFLAGS=-m32 \
            host_alias=i586-linux
        exec make check
        ;;
    *)
        exit -1
        ;;
esac

