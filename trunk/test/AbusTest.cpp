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

#include <limits.h>
#include <errno.h>
#include <unistd.h>

#include <abus.h>
#include <json.h>

#include <gtest/gtest.h>

#include "AbusTest.hpp"

void AbusTest::svc_sum_cb(json_rpc_t *json_rpc)
{
	int a, b;
	int ret;

	ret  = json_rpc_get_int(json_rpc, "a", &a);
    if (ret == 0)
        ret = json_rpc_get_int(json_rpc, "b", &b);

	if (ret)
		json_rpc_set_error(json_rpc, ret, NULL);
	else
		json_rpc_append_int(json_rpc, "res_value", a+b);
}

void AbusTest::svc_jtypes_cb(json_rpc_t *json_rpc)
{
	int a;
    bool b;
    long long ll;
    double d1, d2;
    char s[512];
	int ret = 0;

    EXPECT_EQ(JSON_INT, json_rpc_get_type(json_rpc, "int"));
	ret = json_rpc_get_int(json_rpc, "int", &a);
    if (ret == 0)
	    ret = json_rpc_get_llint(json_rpc, "llint", &ll);
    if (ret == 0)
	    ret = json_rpc_get_bool(json_rpc, "bool", &b);
    if (ret == 0)
	    ret = json_rpc_get_double(json_rpc, "double1", &d1);
    if (ret == 0)
	    ret = json_rpc_get_double(json_rpc, "double2", &d2);
    if (ret == 0)
	    ret = json_rpc_get_str(json_rpc, "str", s, sizeof(s));

	if (ret) {
		json_rpc_set_error(json_rpc, ret, NULL);
        return;
    }

	EXPECT_EQ(0, json_rpc_append_int(json_rpc, "res_int", a));
	EXPECT_EQ(0, json_rpc_append_llint(json_rpc, "res_llint", ll));
	EXPECT_EQ(0, json_rpc_append_bool(json_rpc, "res_bool", b));
	EXPECT_EQ(0, json_rpc_append_double(json_rpc, "res_double1", d1));
	EXPECT_EQ(0, json_rpc_append_double(json_rpc, "res_double2", d2));
	EXPECT_EQ(0, json_rpc_append_str(json_rpc, "res_str", s));
}

void AbusTest::svc_array_sqr_cb(json_rpc_t *json_rpc)
{
	int k;
	int ret, count, i;
	int *ary = NULL;

	ret = json_rpc_get_int(json_rpc, "k", &k);
	if (ret) {
		json_rpc_set_error(json_rpc, ret, NULL);
        return;
    }
	count = json_rpc_get_array_count(json_rpc, "my_array");
	if (count >= 0) {

		/* first put all the values in an array, in order to not mix
		   json_rpc_get's and json_rpc_append's for readability sake
		 */
		ary = (int*)malloc(count*sizeof(int));

		for (i = 0; i<count; i++) {
			/* Aim at i-th element within array "my_array" */
			ret = json_rpc_get_point_at(json_rpc, "my_array", i);
            if (ret) {
                json_rpc_set_error(json_rpc, ret, NULL);
                return;
            }

			/* from that dictionary, get parameter "a"
			 * Rem: expects all array elements to contain at least a param "a"
			 */
			ret = json_rpc_get_int(json_rpc, "a", &ary[i]);
            if (ret) {
                json_rpc_set_error(json_rpc, ret, NULL);
                return;
            }
		}
	}

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


void AbusTest::svc_echo_cb(json_rpc_t *json_rpc)
{
	char s[JSONRPC_RESP_SZ_MAX];
	int ret;
    size_t slen = sizeof(s);

	ret  = json_rpc_get_strn(json_rpc, "msg", s, &slen);
	if (ret) {
		json_rpc_set_error(json_rpc, ret, NULL);
    } else {
		json_rpc_append_strn(json_rpc, "msg", s, slen);
		json_rpc_append_int(json_rpc, "msg_len", slen);
    }
}

