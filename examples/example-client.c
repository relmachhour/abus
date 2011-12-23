/*
 * Copyright (C) 2011 Stephane Fillod
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
	int ret, res_value;
	const char *service_name = "examplesvc";

	if (argc < 4) {
		printf("usage: %s METHOD firstvalue secondvalue\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	abus_init(&abus);

	/* method name is taken from command line */
	ret = abus_request_method_init(&abus, service_name, argv[1], &json_rpc);
	if (ret)
		exit(EXIT_FAILURE);

	/* pass 2 parameters: "a" and "b" */
	json_rpc_append_int(&json_rpc, "a", atoi(argv[2]));
	json_rpc_append_int(&json_rpc, "b", atoi(argv[3]));

	ret = abus_request_method_invoke(&abus, &json_rpc, ABUS_RPC_FLAG_NONE, RPC_TIMEOUT);
	if (ret != 0) {
		printf("RPC failed with error %d\n", ret);
		exit(EXIT_FAILURE);
	}

	ret = json_rpc_get_int(&json_rpc, "res_value", &res_value);

	if (ret == 0)
		printf("res_value=%d\n", res_value);
	else
		printf("No result? error %d\n", ret);

	abus_request_method_cleanup(&abus, &json_rpc);
	abus_cleanup(&abus);

	return EXIT_SUCCESS;
}

