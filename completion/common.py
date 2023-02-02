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

import shtab


def add_common_opts(parser):
    parser.add_argument(
        "-f", "--filepath", metavar="ENVFILE", help="Environment to use. Expects a file name, usually called BGENV.DAT."
    ).complete = shtab.FILE
    parser.add_argument("-p", "--part", metavar="ENV_PART", type=int, help="Set environment partition to use")
    parser.add_argument("-v", "--verbose", action="store_true", help="Be verbose")
    parser.add_argument("-V", "--version", action="store_true", help="Print version")
    # there is a bug in shtab which currently prohibits "-?"
    parser.add_argument("--help", action="store_true", help="Show help")
