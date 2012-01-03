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
#include <errno.h>

#include "abus.h"

#define RPC_TIMEOUT 1000 /* ms */

int main(int argc, char **argv)
{
	abus_t abus;
	json_rpc_t json_rpc;
	int ret;
	const char *service_name = "exampleattrsvc";
	const char *attr_name;
	int my_int;

	if (argc < 2) {
		printf("usage: %s ATTR newintegervalue\n", argv[0]);
		printf("usage: ATTR: some_int|some_other_int\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	abus_init(&abus);

	/* attr name is taken from command line */
	attr_name = argv[1];

	ret = abus_attr_get_int(&abus, service_name, attr_name, &my_int, RPC_TIMEOUT);
	if (ret) {
		printf("RPC failed with error %d\n", ret);
		abus_cleanup(&abus);
		exit(EXIT_FAILURE);
	}

	printf("Previous value: %s=%d\n", attr_name, my_int);

	my_int = atoi(argv[2]);

	ret = abus_attr_set_int(&abus, service_name, attr_name, my_int, RPC_TIMEOUT);
	if (ret) {
		printf("RPC failed with error %d\n", ret);
		abus_cleanup(&abus);
		exit(EXIT_FAILURE);
	}
	printf("New value: %s=%d\n", attr_name, my_int);

	abus_cleanup(&abus);

	return EXIT_SUCCESS;
}

