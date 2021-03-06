/*
 * Copyright (C) 2011-2012 Stephane Fillod
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

#include <abus.h>
#include <json.h>

static void svc_array_sqr_cb(json_rpc_t *json_rpc, void *arg)
{
	int k;
	int ret, count, i;
	int *ary = NULL;

	ret = json_rpc_get_int(json_rpc, "k", &k);
	if (ret != 0) {
		json_rpc_set_error(json_rpc, ret, NULL);
		return;
	}

	count = json_rpc_get_array_count(json_rpc, "my_array");
	if (count >= 0) {

		/* first put all the values in an array, in order to not mix
		   json_rpc_get's and json_rpc_append's for readability sake
		 */
		ary = malloc(count*sizeof(int));

		for (i = 0; i<count; i++) {
			/* Aim at i-th element within array "my_array" */
			ret = json_rpc_get_point_at(json_rpc, "my_array", i);
			if (ret != 0)
				break;

			/* from that dictionary, get parameter "a"
			 * Rem: expects all array elements to contain at least a param "a"
			 */
			ret = json_rpc_get_int(json_rpc, "a", &ary[i]);
			if (ret != 0)
				break;
		}
	}

	printf("## %s: arg=%s, ret=%d, k=%d, array count=%d\n", __func__, (const char*)arg, ret, k, count);

	if (ret) {
		json_rpc_set_error(json_rpc, ret, NULL);
	} else {

		json_rpc_append_int(json_rpc, "res_k", k);

		/* begin the array */
		json_rpc_append_args(json_rpc,
						JSON_KEY, "res_array", -1,
						JSON_ARRAY_BEGIN,
						-1);

		for (i = 0; i<count; i++) {
			/* each array element *must* be an "OBJECT", i.e. a dictonary */
			json_rpc_append_args(json_rpc, JSON_OBJECT_BEGIN, -1);

			json_rpc_append_int(json_rpc, "res_a", ary[i]*ary[i]);
			/* more stuff may be appended in there */

			json_rpc_append_args(json_rpc, JSON_OBJECT_END, -1);
		}

		/* end the array */
		json_rpc_append_args(json_rpc, JSON_ARRAY_END, -1);
	}
	if (ary)
		free(ary);
}

int main(int argc, char **argv)
{
	abus_t *abus;
	int ret;

	abus = abus_init(NULL);

	ret = abus_decl_method(abus, "examplearraysvc", "sqr", &svc_array_sqr_cb,
					ABUS_RPC_FLAG_NONE,
					"square cookie",
					"Compute square value of all the elements of an array. Serves as an example of how to deal with array in A-Bus",
					"k:i:some contant,my_array:(a:i:value to be squared,arg_index:i:index of arg for demo):array of stuff",
					"res_k:i:same contant,res_array:(res_a:i:squared value):array of squared stuff");
	if (ret != 0) {
		abus_cleanup(abus);
		return EXIT_FAILURE;
	}

	/* do other stuff */
	sleep(10000);

	abus_cleanup(abus);

	return EXIT_SUCCESS;
}

