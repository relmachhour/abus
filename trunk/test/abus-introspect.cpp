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

#include <abus.h>

#include <gtest/gtest.h>

#include "AbusTest.hpp"

TEST(ListSvc, ListSvcIntrospectionEmpty) {

    abus_t *abus;
	json_rpc_t *json_rpc;

    abus = abus_init(NULL);
    EXPECT_TRUE(NULL != abus);

	json_rpc = abus_request_method_init(abus, "", "*");
	EXPECT_TRUE(NULL != json_rpc);

	EXPECT_EQ(0, abus_request_method_invoke(abus, json_rpc, ABUS_RPC_FLAG_NONE, RPC_TIMEOUT));

    EXPECT_EQ(0, json_rpc_get_array_count(json_rpc, "services"));
    
    EXPECT_EQ(0, abus_request_method_cleanup(abus, json_rpc));
    EXPECT_EQ(0, abus_cleanup(abus));
}

TEST_F(AbusTest, ListSvcIntrospection) {

	json_rpc_t *json_rpc;
    const char *pname;
    size_t name_len;

	json_rpc = abus_request_method_init(abus_, "", "*");
	EXPECT_TRUE(NULL != json_rpc);

	EXPECT_EQ(0, abus_request_method_invoke(abus_, json_rpc, ABUS_RPC_FLAG_NONE, RPC_TIMEOUT));

    EXPECT_EQ(1, json_rpc_get_array_count(json_rpc, "services"));

    EXPECT_EQ(0, json_rpc_get_point_at(json_rpc, "services", 0));

    EXPECT_EQ(0, json_rpc_get_strp(json_rpc, "name", &pname, &name_len));

    EXPECT_STREQ(SVC_NAME, pname);

    EXPECT_EQ(0, abus_request_method_cleanup(abus_, json_rpc));
}

TEST(SvcIntrospect, IntrospectionNone) {

    abus_t *abus;
	json_rpc_t *json_rpc;

    abus = abus_init(NULL);
    EXPECT_TRUE(NULL != abus);

	json_rpc = abus_request_method_init(abus, SVC_NAME, "*");
	EXPECT_TRUE(NULL != json_rpc);

	EXPECT_EQ(-ENOENT, abus_request_method_invoke(abus, json_rpc, ABUS_RPC_FLAG_NONE, RPC_TIMEOUT));

    EXPECT_EQ(-1, json_rpc_get_array_count(json_rpc, "methods"));
    
    EXPECT_EQ(0, abus_request_method_cleanup(abus, json_rpc));
    EXPECT_EQ(0, abus_cleanup(abus));
}

TEST_F(AbusTest, Introspection) {

	json_rpc_t *json_rpc;
    const char *pname;
    size_t name_len;

	json_rpc = abus_request_method_init(abus_, SVC_NAME, "*");
	EXPECT_TRUE(NULL != json_rpc);

	EXPECT_EQ(0, abus_request_method_invoke(abus_, json_rpc, ABUS_RPC_FLAG_NONE, RPC_TIMEOUT));

    EXPECT_EQ(4, json_rpc_get_array_count(json_rpc, "methods"));

    EXPECT_EQ(0, json_rpc_get_point_at(json_rpc, "methods", 0));

    EXPECT_EQ(0, json_rpc_get_strp(json_rpc, "name", &pname, &name_len));

    EXPECT_STREQ("echo", pname);

    /* TODO: 3 others methods, and attrs, events, .. */

    EXPECT_EQ(0, abus_request_method_cleanup(abus_, json_rpc));
}

