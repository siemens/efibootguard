#!/usr/bin/env bash
#
# EFI Boot Guard
#
# Copyright (c) Siemens AG, 2021-2023
#
# Authors:
#  Claudius Heine <ch@denx.de>
#  Jan Kiszka <jan.kiszka@siemens.com>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#
# SPDX-License-Identifier:      GPL-2.0-only
#

ignore=""
ignore+=" -i tests/fff"

suppress=""
# Justified suppressions:
# Does not belong to the project
suppress+=" --suppress=*:/usr/include/*"
suppress+=" --suppress=*:/usr/include/bits/*"
# Function 'efi_main' is called by efi:
suppress+=" --suppress=unusedFunction:main.c"
suppress+=" --suppress=unusedFunction:kernel-stub/main.c"
# Some functions are defined for API only
suppress+=" --suppress=unusedFunction:utils.c"
suppress+=" --suppress=unusedFunction:env/env_api.c"
suppress+=" --suppress=unusedFunction:env/fatvars.c"
suppress+=" --suppress=unusedFunction:tools/tests/fake_devices.c"
suppress+=" --suppress=unusedFunction:tools/tests/test_environment.c"
suppress+=" --suppress=unusedFunction:env/env_api_fat.c"
# Some functions are used by linker wrapping
suppress+=" --suppress=unusedFunction:tools/tests/test_probe_config_file.c"
suppress+=" --suppress=unusedFunction:tools/tests/test_ebgenv_api.c"
# False positive on wdfuncs iteration
suppress+=" --suppress=comparePointers:main.c"
# False positive on constructors, first hit
suppress+=" --suppress=unusedFunction:drivers/watchdog/amdfch_wdt.c"
# False positive, noreturn is not recognized
suppress+=" --suppress=nullPointerRedundantCheck:kernel-stub/main.c"
# Avoid noise regarding Ignore* or otherwise unused fields
suppress+=" --suppress=unusedStructMember:kernel-stub/main.c"
suppress+=" --suppress=unusedStructMember:kernel-stub/fdt.c"
# Not applicable because of API requirements
suppress+=" --suppress=constParameterCallback:drivers/watchdog/ipc4x7e_wdt.c"
suppress+=" --suppress=constParameterCallback:drivers/watchdog/w83627hf_wdt.c"
suppress+=" --suppress=constParameterCallback:kernel-stub/initrd.c"

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
          -I /usr/lib/gcc/x86_64-linux-gnu/9/include"

cpp_conf="-U__WINT_TYPE__ -U__GNUC__"
path=${1-.}

# Exit code '1' is returned if arguments are not valid or if no input
# files are provided. Compare 'cppcheck --help'.
cppcheck -f -q --error-exitcode=2 $enable $suppress $ignore \
    $cpp_conf $includes $path "$@"
