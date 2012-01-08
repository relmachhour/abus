/*
 * Copyright (C) 2012 Stephane Fillod
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2.1 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "abus.h"

int main(int argc, char **argv)
{
	abus_t abus;
	const char *servicename = "exampleattrsvc";
	int i, ret;

	int my_int = 42;
	int my_other_int = -2;
	int my_auto_count = 0;


	abus_init(&abus);

	ret = abus_decl_attr_int(&abus, servicename, "tree.some_int", &my_int,
					ABUS_RPC_FLAG_NONE,
					"Some integer, for demo purpose");
	if (ret != 0) {
	    abus_cleanup(&abus);
	    return EXIT_FAILURE;
	}

	abus_decl_attr_int(&abus, servicename, "tree.some_other_int", &my_other_int,
					ABUS_RPC_FLAG_NONE,
					"Some other integer, still for demo purpose");

	abus_decl_attr_int(&abus, servicename, "tree.auto_count", &my_auto_count,
					ABUS_RPC_FLAG_NONE,
					"Counter incremented every 5 seconds");

	/* do other stuff */
	for (i = 0; i < 1000; i++) {
		sleep(5);

		my_auto_count++;
		/* trigger event notification */
		abus_attr_changed(&abus, servicename, "tree.auto_count");
	}

	abus_cleanup(&abus);

	return EXIT_SUCCESS;
}

