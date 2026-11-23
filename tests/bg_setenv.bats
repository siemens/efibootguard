#!/usr/bin/env bats
# Copyright (c) Siemens AG, 2021-2023
#
# Authors:
#  Michael Adler <michael.adler@siemens.com>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#
# SPDX-License-Identifier: GPL-2.0-only
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

verify_envfile() {
    local envfile=$1 ; shift

    local envsize
    envsize=$(wc -c < "$envfile")

    local stored
    stored=$(od -tx4 -j $((envsize - 4)) < "$envfile" | awk 'NF>1 {print $2}')

    local computed
    computed=$(crc32 <(head -c $((envsize - 4)) < "$envfile"))

    [ -n "$stored$computed" -a "$stored" = "$computed" ]
}

resize_envfile() {
    local envfile=$1 ; shift
    local targetsize=$1 ; shift

    # Before resizing, verify that the contents are uncorrupted to
    # avoid truncating the mismatching checksum.

    verify_envfile "$envfile" || return $?

    # Only resize if the target size matches the default size
    # named in the configuration. This is primarily a sanity check
    # because the default size is also encoded in the MD5 checksum
    # validation.

    grep -qF "ENV_MEM_USERVARS=$targetsize" "$BATS_TEST_DIRNAME"/../configure.ac || return $?

    local envsize
    envsize=$(awk '/ ENV_MEM_USERVARS / { print $3 }' config.h) &&
            [ -n "$envsize" ] || {
        echo Expected to find ENV_MEM_USERVARS in $PWD/config.h >&2
        exit 1
    }

    # Compute the difference between the actual size, and the target
    # size. This will be used to expand or contract the payload to match.

    local deltasize=$((targetsize - envsize))
    [[ $deltasize -lt 0 ]] || deltasize=+$deltasize

    # Remove the existing checksum, and then adjust the length
    # to reach the target sizes.

    truncate -s -4 "$envfile" && truncate -s "$deltasize" "$envfile"

    ls -l "$envfile" >&2

    # Install a checksum that matches the content included or
    # excluded to meet the target size.

    local computed
    computed=$(( 0x$(crc32 <(cat "$envfile") ) ))
    echo "$computed" >&2

    local byte0=$(printf %02x $(( (computed >>  0) & 0xff)) )
    local byte1=$(printf %02x $(( (computed >>  8) & 0xff)) )
    local byte2=$(printf %02x $(( (computed >> 16) & 0xff)) )
    local byte3=$(printf %02x $(( (computed >> 24) & 0xff)) )

    printf "%s" "\x$byte0\x$byte1\x$byte2\x$byte3" >&2
    printf "%b" "\x$byte0\x$byte1\x$byte2\x$byte3" >> "$envfile"

    verify_envfile "$envfile"
}

# Print a value as little-endian 32-bit word, in printf %b escape notation
le32() {
    printf '\\x%02x\\x%02x\\x%02x\\x%02x' $(($1 & 0xff)) \
        $((($1 >> 8) & 0xff)) $((($1 >> 16) & 0xff)) $((($1 >> 24) & 0xff))
}

# Print a raw uservar record in printf %b escape notation
#   uservar_record <key> <type> <data in printf %b escape notation>
uservar_record() {
    local key=$1 ; shift
    local type=$1 ; shift
    local data=$1 ; shift

    local datalen
    datalen=$(printf "%b" "$data" | wc -c)

    printf '%s\\x00' "$key"
    le32 $((4 + 8 + datalen))
    le32 $((type & 0xffffffff))
    le32 $((type >> 32))
    printf '%s' "$data"
}

# Create an environment with the given raw uservar records, bypassing all
# checks bg_setenv would apply
create_bgenv_with_uservars() {
    local envfile=$1 ; shift
    local uservars=$1 ; shift

    local uservars_size
    uservars_size=$(awk '/ ENV_MEM_USERVARS / { print $3 }' config.h) &&
            [ -n "$uservars_size" ] || {
        echo Expected to find ENV_MEM_USERVARS in $PWD/config.h >&2
        exit 1
    }

    # kernelfile, kernelparams, in_progress, ustate, watchdog_timeout_sec
    # and revision come first, all zero
    head -c 1028 /dev/zero > "$envfile"
    printf "%b" "$uservars" >> "$envfile"
    truncate -s $((1028 + uservars_size)) "$envfile"

    local computed
    computed=$(( 0x$(crc32 "$envfile") ))
    printf "%b" "$(le32 $computed)" >> "$envfile"

    verify_envfile "$envfile"
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

    resize_envfile "$envfile" 131072

    run md5sum "$envfile"
    [[ "$output" =~ ^6ad1dd1d98209a03d7b4fc2d2f16f9ec\s*.* ]]
}

@test "create an empty BGENV.DAT" {
    local envfile
    envfile="$BATS_TEST_TMPDIR/BGENV.DAT"

    run bg_setenv -f "$envfile"
    [[ "$output" = "Output written to $envfile." ]]

    run bg_printenv -f "$envfile"
    [[ "$output" =  "Values:
in_progress:      no
revision:         0
kernel:           
kernelargs:       
watchdog timeout: 0 seconds
ustate:           0 (OK)

user variables:" ]]

    resize_envfile "$envfile" 131072

    run md5sum "$envfile"
    [[ "$output" =~ ^441b49e907a117d2fe1dc1d69d8ea1b0\s*.* ]]
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

    resize_envfile "$envfile" 131072

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

    resize_envfile "$envfile" 131072

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

@test "bg_printenv respects the size of string uservars" {
    local envfile
    envfile="$BATS_TEST_TMPDIR/BGENV.DAT"

    # unterminated string, directly followed by the next uservar
    create_bgenv_with_uservars "$envfile" \
        "$(uservar_record foo 32 'bar')$(uservar_record baz 32 'qux\x00')"

    run bg_printenv -f "$envfile" --output user --raw
    [[ "$output" = "foo=bar
baz=qux" ]]

    run bg_printenv -f "$envfile" --output user
    [[ "$output" = "Values:

user variables:
foo = bar
baz = qux" ]]

    # the same with user-defined bits in the type
    create_bgenv_with_uservars "$envfile" \
        "$(uservar_record foo $(((1 << 32) | 32)) 'bar')$(uservar_record \
          baz $(((0x1ffff << 32) | 32)) 'qux\x00')"

    run bg_printenv -f "$envfile" --output user --raw
    [[ "$output" = "foo=bar
baz=qux" ]]
}

@test "bg_printenv rejects uservars that are too small for their type" {
    local envfile
    envfile="$BATS_TEST_TMPDIR/BGENV.DAT"

    # UINT64 with only 2 bytes
    create_bgenv_with_uservars "$envfile" \
        "$(uservar_record foo 32 'bar\x00')$(uservar_record num 5 '\x01\x02')"

    run bg_printenv -f "$envfile" --output user --raw
    [ "$status" -eq 1 ]
    [[ "$output" = "Error reading environment file." ]]

    create_bgenv_with_uservars "$envfile" \
        "$(uservar_record foo 32 'bar\x00')$(uservar_record num 5 \
          '\x01\x02\x00\x00\x00\x00\x00\x00')"

    run bg_printenv -f "$envfile" --output user --raw
    [[ "$output" = "foo=bar
num=513" ]]

    # the same with user-defined bits in the type
    create_bgenv_with_uservars "$envfile" \
        "$(uservar_record foo 32 'bar\x00')$(uservar_record num \
          $(((1 << 32) | 5)) '\x01\x02')"

    run bg_printenv -f "$envfile" --output user --raw
    [ "$status" -eq 1 ]
    [[ "$output" = "Error reading environment file." ]]

    create_bgenv_with_uservars "$envfile" \
        "$(uservar_record foo 32 'bar\x00')$(uservar_record num \
          $(((1 << 32) | 5)) '\x01\x02\x00\x00\x00\x00\x00\x00')"

    run bg_printenv -f "$envfile" --output user --raw
    [[ "$output" = "foo=bar
num=513" ]]
}
