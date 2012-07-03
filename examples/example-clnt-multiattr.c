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

#include <abus.h>
#include <json.h>

#define RPC_TIMEOUT 1000 /* ms */

int main(int argc, char **argv)
{
	abus_t *abus;
	json_rpc_t *json_rpc;
	int ret, res_value;
	const char *service_name = "exampleattrsvc";

	abus = abus_init(NULL);

	/* special method name to get attributes */
	json_rpc = abus_request_method_init(abus, service_name, "get");
	if (!json_rpc)
		exit(EXIT_FAILURE);

	/* JSON-RPC syntax is "params":{"attr":[{"name":"tree.some_other_int"},{"name":"tree.some_int"}]} */

	/* begin the array */
	json_rpc_append_args(json_rpc,
					JSON_KEY, "attr", -1,
					JSON_ARRAY_BEGIN,
					-1);

	/* each array element *must* be an "OBJECT", i.e. a dictonary */
	json_rpc_append_args(json_rpc, JSON_OBJECT_BEGIN, -1);

	json_rpc_append_str(json_rpc, "name", "tree.some_int");

	json_rpc_append_args(json_rpc, JSON_OBJECT_END, -1);


	json_rpc_append_args(json_rpc, JSON_OBJECT_BEGIN, -1);

	json_rpc_append_str(json_rpc, "name", "tree.some_other_int");

	json_rpc_append_args(json_rpc, JSON_OBJECT_END, -1);


	/* end the array */
	json_rpc_append_args(json_rpc, JSON_ARRAY_END, -1);


	ret = abus_request_method_invoke(abus, json_rpc, ABUS_RPC_FLAG_NONE, RPC_TIMEOUT);
	if (ret != 0) {
		printf("RPC failed with error %d\n", ret);
		exit(EXIT_FAILURE);
	}

	ret = json_rpc_get_int(json_rpc, "tree.some_int", &res_value);

	if (ret == 0)
		printf("tree.some_int=%d\n", res_value);
	else
		printf("No result? error %s\n", json_rpc_strerror(ret));

	ret = json_rpc_get_int(json_rpc, "tree.some_other_int", &res_value);

	if (ret == 0)
		printf("tree.some_other_int=%d\n", res_value);
	else
		printf("No result? error %s\n", json_rpc_strerror(ret));


	abus_request_method_cleanup(abus, json_rpc);
	abus_cleanup(abus);

	return EXIT_SUCCESS;
}

