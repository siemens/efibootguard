#
# Copyright (c) Siemens AG, 2021
#
# Authors:
#  Michael Adler <michael.adler@siemens.com>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#
# SPDX-License-Identifier:	GPL-2.0

import argparse

from .common import add_common_opts


def bg_printenv():
    parser = argparse.ArgumentParser(prog="bg_printenv", add_help=False)
    add_common_opts(parser)
    parser.add_argument("-c", "--current", action="store_true", help="Only print values from the current environment")
    parser.add_argument(
        "-o",
        "--output",
        choices=["in_progress", "revision", "kernel", "kernelargs", "watchdog_timeout", "ustate", "user"],
        help="Comma-separated list of fields which are printed",
    )
    parser.add_argument("-r", "--raw", action="store_true", help="Raw output mode")
    parser.add_argument("--usage", action="store_true", help="Give a short usage message")
    return parser
