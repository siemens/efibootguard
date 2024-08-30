#
# Copyright (c) Siemens AG, 2021
#
# Authors:
#  Michael Adler <michael.adler@siemens.com>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#
# SPDX-License-Identifier:	GPL-2.0-only

import argparse

from .common import add_common_opts


def bg_setenv():
    parser = argparse.ArgumentParser(prog="bg_setenv", add_help=False)
    add_common_opts(parser)
    parser.add_argument("-P", "--preserve", action="store_true", help="Preserve existing entries")
    parser.add_argument("-k", "--kernel", metavar="KERNEL", help="Set kernel to load")
    parser.add_argument("-a", "--args", metavar="KERNEL_ARGS", help="Set kernel arguments")
    parser.add_argument("-r", "--revision", metavar="REVISION", help="Set revision value")
    parser.add_argument(
        "-s",
        "--ustate",
        choices=["OK", "INSTALLED", "TESTING", "FAILED", "UNKNOWN"],
        metavar="USTATE",
        help="Set update status for environment",
    )
    parser.add_argument("-w", "--watchdog", metavar="WATCHDOG_TIMEOUT", help="Watchdog timeout in seconds")
    parser.add_argument("-c", "--confirm", action="store_true", help="Confirm working environment")
    parser.add_argument("-u", "--update", action="store_true", help="Automatically update oldest revision")
    parser.add_argument(
        "-x",
        "--uservar",
        metavar="KEY=VAL",
        help="Set user-defined string variable. For setting multiple variables, use this option multiple times.",
    )
    parser.add_argument(
        "-i",
        "--in_progress",
        metavar="IN_PROGRESS",
        choices=["0", "1"],
        help="Set in_progress variable to simulate a running update process.",
    )
    return parser
