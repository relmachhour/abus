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

#include <limits.h>
#include "abus.h"

#include <gtest/gtest.h>

#define RPC_TIMEOUT 1000 /* ms */

static void svc_sum_cb(json_rpc_t *json_rpc, void *arg)
{
	int a, b;
	int ret;

	ret  = json_rpc_get_int(json_rpc, "a", &a);
	ret |= json_rpc_get_int(json_rpc, "b", &b);

	if (ret)
		json_rpc_set_error(json_rpc, JSONRPC_INVALID_METHOD, NULL);
	else
		json_rpc_append_int(json_rpc, "res_value", a+b);
}

static void svc_mult_cb(json_rpc_t *json_rpc, void *arg)
{
	int a, b;
	int ret;

	ret  = json_rpc_get_int(json_rpc, "a", &a);
	ret |= json_rpc_get_int(json_rpc, "b", &b);

	if (ret)
		json_rpc_set_error(json_rpc, JSONRPC_INVALID_METHOD, NULL);
	else
		json_rpc_append_int(json_rpc, "res_value", a*b);
}

TEST(AbusSvc, BasicSvc) {
	abus_t abus;
	json_rpc_t json_rpc;
	const char *service_name = "gtestsvc";
	int res_value;

	EXPECT_EQ(0, abus_init(&abus));

	EXPECT_EQ(0, abus_decl_method(&abus, service_name, "sum", &svc_sum_cb,
					ABUS_RPC_FLAG_NONE, 0,
					"a:i:first operand,b:i:second operand",
					"res_value:i:summation"));

	EXPECT_EQ(0, abus_decl_method(&abus, service_name, "mult", &svc_mult_cb,
					ABUS_RPC_FLAG_NONE, 0,
					"a:i:first operand,b:i:second operand",
					"res_value:i:multiplication"));

	EXPECT_EQ(0, abus_request_method_init(&abus, service_name, "sum", &json_rpc));

	/* pass 2 parameters: "a" and "b" */
	EXPECT_EQ(0, json_rpc_append_int(&json_rpc, "a", 2));
	EXPECT_EQ(0, json_rpc_append_int(&json_rpc, "b", 3));

	EXPECT_EQ(0, abus_request_method_invoke(&abus, &json_rpc, ABUS_RPC_FLAG_NONE, RPC_TIMEOUT));

	EXPECT_EQ(0, json_rpc_get_int(&json_rpc, "res_value", &res_value));

	EXPECT_EQ(2+3, res_value);

	EXPECT_EQ(0, abus_request_method_cleanup(&abus, &json_rpc));

	EXPECT_EQ(0, abus_cleanup(&abus));
}

