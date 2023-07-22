/*
 * EFI Boot Guard
 *
 * Copyright (c) Siemens AG, 2017
 *
 * Authors:
 *  Andreas Reichel <andreas.reichel.ext@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <stdlib.h>
#include <stdio.h>
#include <check.h>
#include <fff.h>

extern Suite *ebg_test_suite(void);

int main(void)
{
	int number_failed = 1;

	Suite *s;
	SRunner *sr;

	s = ebg_test_suite();
	sr = srunner_create(s);

	if (srunner_fork_status(sr) != CK_FORK) {
		fprintf(stderr, "Tests assume fork() support");
	} else {
		srunner_run_all(sr, CK_NORMAL);
		number_failed = srunner_ntests_failed(sr);
		srunner_free(sr);
	}

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
