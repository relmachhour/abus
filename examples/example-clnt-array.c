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
	int count, i, ret, res_value;
	const char *service_name = "examplearraysvc";

	if (argc < 4) {
		printf("usage: %s METHOD k values...\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	abus_init(&abus);

	/* method name is taken from command line */
	ret = abus_request_method_init(&abus, service_name, argv[1], &json_rpc);
	if (ret)
		exit(EXIT_FAILURE);

	/* pass 2 parameters: "k" and "my_array" */
	json_rpc_append_int(&json_rpc, "k", atoi(argv[2]));

	/* begin the array */
	json_rpc_append_args(&json_rpc,
					JSON_KEY, "my_array", -1,
					JSON_ARRAY_BEGIN,
					-1);

	for (i = 3; i<argc; i++) {
		/* each array element *must* be an "OBJECT", i.e. a dictonary */
		json_rpc_append_args(&json_rpc, JSON_OBJECT_BEGIN, -1);

		json_rpc_append_int(&json_rpc, "a", atoi(argv[i]));
		/* more stuff may be appended in there */
		json_rpc_append_int(&json_rpc, "arg_index", i);

		json_rpc_append_args(&json_rpc, JSON_OBJECT_END, -1);
	}

	/* end the array */
	json_rpc_append_args(&json_rpc, JSON_ARRAY_END, -1);


	ret = abus_request_method_invoke(&abus, &json_rpc, ABUS_RPC_FLAG_NONE, RPC_TIMEOUT);
	if (ret != 0) {
		printf("RPC failed with error %d\n", ret);
		exit(EXIT_FAILURE);
	}

	count = json_rpc_get_array_count(&json_rpc, "res_array");
	if (count < 0) {
		printf("No result? error %d\n", count);
		exit(EXIT_FAILURE);
	}

	ret = json_rpc_get_int(&json_rpc, "res_k", &res_value);
	if (ret == 0)
		printf("res_k=%d\n", res_value);
	else
		printf("No result? error %d\n", ret);


	for (i = 0; i<count; i++) {
		/* Aim at i-th element within array "res_array" */
		json_rpc_get_point_at(&json_rpc, "res_array", i);

		printf("res_array[%d]\n", i);

		ret = json_rpc_get_int(&json_rpc, "res_a", &res_value);
		if (ret == 0)
			printf("\tres_a=%d\n", res_value);
		else
			printf("\tNo result? error %d\n", ret);
	}

	/* Aim back out of array */

	json_rpc_get_point_at(&json_rpc, NULL, 0);

	ret = json_rpc_get_int(&json_rpc, "res_k", &res_value);
	if (ret == 0)
		printf("res_k=%d (should be the same as previously)\n", res_value);
	else
		printf("No result? error %d\n", ret);


	abus_request_method_cleanup(&abus, &json_rpc);
	abus_cleanup(&abus);

	return EXIT_SUCCESS;
}

