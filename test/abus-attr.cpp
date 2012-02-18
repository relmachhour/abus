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
#include <math.h>
#include <unistd.h>
#include "abus.h"

#include <gtest/gtest.h>
#include "AbusTest.hpp"

class AbusAttrTest : public AbusTest {
    protected:
        virtual void SetUp() {
            AbusTest::SetUp();

			m_int = INT_MAX;
			m_llint = LLONG_MAX;
			m_bool = true;
			m_double = M_PI;
			strncpy(m_str, abus_get_copyright(), sizeof(m_str));

	        EXPECT_EQ(0, abus_decl_attr_int(&abus_, SVC_NAME, "int", &m_int, 0, NULL));
	        EXPECT_EQ(0, abus_decl_attr_int(&abus_, SVC_NAME, "int_ro", &m_int, ABUS_RPC_RDONLY, NULL));
	        EXPECT_EQ(0, abus_decl_attr_llint(&abus_, SVC_NAME, "llint", &m_llint, 0, NULL));
	        EXPECT_EQ(0, abus_decl_attr_bool(&abus_, SVC_NAME, "bool", &m_bool, 0, NULL));
	        EXPECT_EQ(0, abus_decl_attr_double(&abus_, SVC_NAME, "double", &m_double, 0, NULL));
	        EXPECT_EQ(0, abus_decl_attr_str(&abus_, SVC_NAME, "str", m_str, sizeof(m_str), 0, NULL));
        }
        virtual void TearDown() {

			EXPECT_EQ(0, abus_undecl_attr(&abus_, SVC_NAME, "int"));
			EXPECT_EQ(0, abus_undecl_attr(&abus_, SVC_NAME, "int_ro"));
			EXPECT_EQ(0, abus_undecl_attr(&abus_, SVC_NAME, "llint"));
			EXPECT_EQ(0, abus_undecl_attr(&abus_, SVC_NAME, "bool"));
			EXPECT_EQ(0, abus_undecl_attr(&abus_, SVC_NAME, "double"));
			// double undeclare
			EXPECT_EQ(JSONRPC_NO_METHOD, abus_undecl_attr(&abus_, SVC_NAME, "double"));
			// no undeclare, to be catched by abus_cleanup()
			// EXPECT_EQ(0, abus_undecl_attr(&abus_, SVC_NAME, "str"));

            AbusTest::TearDown();
        }

	    int m_int;
	    long long m_llint;
	    bool m_bool;
	    double m_double;
	    char m_str[512];
};

class AbusAutoAttrTest : public AbusTest {
    protected:
        virtual void SetUp() {
            AbusTest::SetUp();

			// Auto-allocated attributes (i.e. no pointer)
	        EXPECT_EQ(0, abus_decl_attr_int(&abus_, SVC_NAME, "int", NULL, 0, NULL));
	        EXPECT_EQ(0, abus_decl_attr_int(&abus_, SVC_NAME, "int_ro", NULL, ABUS_RPC_RDONLY, NULL));
	        EXPECT_EQ(0, abus_decl_attr_llint(&abus_, SVC_NAME, "llint", NULL, 0, NULL));
	        EXPECT_EQ(0, abus_decl_attr_bool(&abus_, SVC_NAME, "bool", NULL, 0, NULL));
	        EXPECT_EQ(0, abus_decl_attr_double(&abus_, SVC_NAME, "double", NULL, 0, NULL));
	        EXPECT_EQ(0, abus_decl_attr_str(&abus_, SVC_NAME, "str", NULL, 256, 0, NULL));
        }
        virtual void TearDown() {

			EXPECT_EQ(0, abus_undecl_attr(&abus_, SVC_NAME, "int"));
			EXPECT_EQ(0, abus_undecl_attr(&abus_, SVC_NAME, "int_ro"));
			EXPECT_EQ(0, abus_undecl_attr(&abus_, SVC_NAME, "llint"));
			EXPECT_EQ(0, abus_undecl_attr(&abus_, SVC_NAME, "bool"));
			EXPECT_EQ(0, abus_undecl_attr(&abus_, SVC_NAME, "double"));
			EXPECT_EQ(0, abus_undecl_attr(&abus_, SVC_NAME, "str"));

            AbusTest::TearDown();
        }
};


TEST_F(AbusAttrTest, AllTypes) {
	int a;
    bool b;
    double d;
    char s[512];

	EXPECT_EQ(0, abus_attr_get_int(&abus_, SVC_NAME, "int", &a, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_get_bool(&abus_, SVC_NAME, "bool", &b, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_get_double(&abus_, SVC_NAME, "double", &d, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_get_str(&abus_, SVC_NAME, "str", s, sizeof(s), RPC_TIMEOUT));

	EXPECT_EQ(a, m_int);
	EXPECT_EQ(b, m_bool);
	EXPECT_NEAR(d, m_double, DABSERROR);
	EXPECT_STREQ(m_str, s);

	EXPECT_EQ(0, abus_attr_set_int(&abus_, SVC_NAME, "int", -1, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_set_bool(&abus_, SVC_NAME, "bool", false, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_set_double(&abus_, SVC_NAME, "double", M_E, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_set_str(&abus_, SVC_NAME, "str", abus_get_version(), RPC_TIMEOUT));

	EXPECT_EQ(-1, m_int);
	EXPECT_FALSE(m_bool);
	EXPECT_NEAR(M_E, m_double, DABSERROR);
	EXPECT_STREQ(m_str, abus_get_version());

	/* inexistant attr name */
	EXPECT_EQ(JSONRPC_NO_METHOD, abus_attr_get_int(&abus_, SVC_NAME, "no_such_int", &a, RPC_TIMEOUT));
	EXPECT_EQ(JSONRPC_NO_METHOD, abus_attr_set_int(&abus_, SVC_NAME, "no_such_int", -2, RPC_TIMEOUT));
	EXPECT_EQ(-1, m_int);

	EXPECT_EQ(JSONRPC_INVALID_METHOD, abus_attr_set_int(&abus_, SVC_NAME, "int_ro", -3, RPC_TIMEOUT));
	EXPECT_EQ(-1, m_int);

	/* TODO:
	   - empty set/get -> no error
	   - multiple attributes
	   - int abus_attr_changed(abus_t *abus, const char *service_name, const char *attr_name);
	 */
}

// TODO: factorize with AbusAttrTest
TEST_F(AbusAutoAttrTest, AllTypes) {
	int a;
    bool b;
    long long ll;
    double d;
    char s[512];

	// Rem: attributes were auto-allocated, initialized like in a bss

	EXPECT_EQ(0, abus_attr_get_int(&abus_, SVC_NAME, "int", &a, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_get_llint(&abus_, SVC_NAME, "llint", &ll, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_get_bool(&abus_, SVC_NAME, "bool", &b, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_get_double(&abus_, SVC_NAME, "double", &d, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_get_str(&abus_, SVC_NAME, "str", s, sizeof(s), RPC_TIMEOUT));

	EXPECT_EQ(a, 0);
	EXPECT_EQ(ll, 0LL);
	EXPECT_EQ(b, false);
	EXPECT_EQ(d, 0.0);
	EXPECT_STREQ("", s);

	EXPECT_EQ(0, abus_attr_set_int(&abus_, SVC_NAME, "int", -1, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_set_llint(&abus_, SVC_NAME, "llint", -2, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_set_bool(&abus_, SVC_NAME, "bool", true, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_set_double(&abus_, SVC_NAME, "double", M_E, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_set_str(&abus_, SVC_NAME, "str", abus_get_version(), RPC_TIMEOUT));

	EXPECT_EQ(0, abus_attr_get_int(&abus_, SVC_NAME, "int", &a, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_get_llint(&abus_, SVC_NAME, "llint", &ll, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_get_bool(&abus_, SVC_NAME, "bool", &b, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_get_double(&abus_, SVC_NAME, "double", &d, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_get_str(&abus_, SVC_NAME, "str", s, sizeof(s), RPC_TIMEOUT));

	EXPECT_EQ(a, -1);
	EXPECT_EQ(ll, -2LL);
	EXPECT_TRUE(b);
	EXPECT_NEAR(M_E, d, DABSERROR);
	EXPECT_STREQ(abus_get_version(), s);

	/* inexistant attr name */
	EXPECT_EQ(JSONRPC_NO_METHOD, abus_attr_get_int(&abus_, SVC_NAME, "no_such_int", &a, RPC_TIMEOUT));
	EXPECT_EQ(JSONRPC_NO_METHOD, abus_attr_set_int(&abus_, SVC_NAME, "no_such_int", -2, RPC_TIMEOUT));

	EXPECT_EQ(JSONRPC_INVALID_METHOD, abus_attr_set_int(&abus_, SVC_NAME, "int_ro", -3, RPC_TIMEOUT));
}

