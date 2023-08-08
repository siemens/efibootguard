#!/usr/bin/env bats
# Copyright (c) Siemens AG, 2021-2023
#
# Authors:
#  Michael Adler <michael.adler@siemens.com>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#
# SPDX-License-Identifier: GPL-2.0
#

setup() {
    # get the containing directory of this file
    # use $BATS_TEST_FILENAME instead of ${BASH_SOURCE[0]} or $0,
    # as those will point to the bats executable's location or the preprocessed
    # file respectively
    DIR="$( cd "$( dirname "$BATS_TEST_FILENAME" )" >/dev/null 2>&1 && pwd )"
    PATH="$DIR/..:$PATH"

    # BATS_TEST_TMPDIR was introduced by BATS 1.4.0, account for older versions
    [ -n "${BATS_TEST_TMPDIR:++}" ] || {
        BATS_TEST_TMPDIR="$BATS_RUN_TMPDIR/test/$BATS_TEST_NUMBER"
        mkdir -p "$BATS_TEST_TMPDIR"
    }
}

create_sample_bgenv() {
    bg_setenv -f "$1" \
        --kernel=C:BOOT:kernel.efi \
        --args=root=/dev/sda \
        --uservar=foo=bar \
        --revision=1
}

@test "ensure BGENV.DAT backwards compatbility" {
    local envfile
    envfile="$BATS_TEST_TMPDIR/BGENV.DAT"
    create_sample_bgenv "$envfile"

    run bg_printenv -f "$envfile"
    [[ "$output" =  "Values:
in_progress:      no
revision:         1
kernel:           C:BOOT:kernel.efi
kernelargs:       root=/dev/sda
watchdog timeout: 0 seconds
ustate:           0 (OK)

user variables:
foo = bar" ]]

    run md5sum "$envfile"
    [[ "$output" =~ ^6ad1dd1d98209a03d7b4fc2d2f16f9ec\s*.* ]]
}

@test "create an empty BGENV.DAT" {
    local envfile
    envfile="$BATS_TEST_TMPDIR/BGENV.DAT"

    run bg_setenv -f "$envfile"
    [[ "$output" = "Output written to $envfile." ]]

    run md5sum "$envfile"
    [[ "$output" =~ ^441b49e907a117d2fe1dc1d69d8ea1b0\s*.* ]]

    run bg_printenv -f "$envfile"
    [[ "$output" =  "Values:
in_progress:      no
revision:         0
kernel:           
kernelargs:       
watchdog timeout: 0 seconds
ustate:           0 (OK)

user variables:" ]]
}

@test "modify BGENV, discard existing values" {
    local envfile
    envfile="$BATS_TEST_TMPDIR/BGENV.DAT"

    create_sample_bgenv "$envfile"
    run bg_setenv -f "$envfile" -k C:BOOTNEW:kernel.efi

    run bg_printenv -f "$envfile"
    [[ "$output" = "Values:
in_progress:      no
revision:         0
kernel:           C:BOOTNEW:kernel.efi
kernelargs:       
watchdog timeout: 0 seconds
ustate:           0 (OK)

user variables:" ]]

    run md5sum "$envfile"
    [[ "$output" =~ ^15bc40c9feae99cc879cfc55e0132caa\s*.* ]]
}

@test "modify BGENV, preserve existing values" {
    local envfile
    envfile="$BATS_TEST_TMPDIR/BGENV.DAT"

    create_sample_bgenv "$envfile"
    run bg_setenv -f "$envfile" -k C:BOOTNEW:kernel.efi -P

    run bg_printenv -f "$envfile"
    [[ "$output" =  "Values:
in_progress:      no
revision:         1
kernel:           C:BOOTNEW:kernel.efi
kernelargs:       root=/dev/sda
watchdog timeout: 0 seconds
ustate:           0 (OK)

user variables:
foo = bar" ]]

    run md5sum "$envfile"
    [[ "$output" =~ ^a24b154a48e1f33b79b87e0fa5eff8a1\s*.* ]]
}

@test "bg_printenv ustate" {
    local envfile
    envfile="$BATS_TEST_TMPDIR/BGENV.DAT"

    create_sample_bgenv "$envfile"
    run bg_printenv "--filepath=$envfile" --output ustate
    [[ "$output" = "Values:
ustate:           0 (OK)" ]]
}

@test "bg_printenv with all fields is the same as omitting fields" {
    local envfile
    envfile="$BATS_TEST_TMPDIR/BGENV.DAT"

    create_sample_bgenv "$envfile"
    expected_output=$(bg_printenv "--filepath=$envfile")
    run bg_printenv "--filepath=$envfile" --output in_progress,revision,kernel,kernelargs,watchdog_timeout,ustate,user
    [[ "$output" = "$expected_output" ]]
}

@test "bg_printenv ustate raw" {
    local envfile
    envfile="$BATS_TEST_TMPDIR/BGENV.DAT"

    create_sample_bgenv "$envfile"
    run bg_printenv "--filepath=$envfile" --output ustate --raw
    [[ "$output" = "USTATE=0" ]]
}

@test "bg_printenv multiple fields raw" {
    local envfile
    envfile="$BATS_TEST_TMPDIR/BGENV.DAT"

    create_sample_bgenv "$envfile"
    run bg_printenv "--filepath=$envfile" --output ustate,kernel,kernelargs --raw
    [[ "$output" = "KERNEL=C:BOOT:kernel.efi
KERNELARGS=root=/dev/sda
USTATE=0" ]]
}
