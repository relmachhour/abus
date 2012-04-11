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

using ::testing::TestWithParam;
using ::testing::Values;


/* Param is whether to perform JSON-RPC through AF_LOCAL (true)
                           or direct access to attribute (false)
 */
class AbusAttrs : public TestWithParam<bool>  {
    protected:
        virtual void SetUp() {
			m_separate_abus = GetParam();

			EXPECT_EQ(0, abus_init(&abus_svc_));

			if (m_separate_abus) {
				/* attr get&set through AF_LOCAL socket */
				EXPECT_EQ(0, abus_init(&abus_rq_));
				abus_ = &abus_rq_;
			} else {
				/* attr get&set through direct access, bypassing JSON-RPC */
				abus_ = &abus_svc_;
			}
        }
        virtual void TearDown() {

			if (m_separate_abus)
				EXPECT_EQ(0, abus_cleanup(&abus_rq_));

			EXPECT_EQ(0, abus_cleanup(&abus_svc_));
		}

		bool m_separate_abus;

		abus_t abus_svc_, abus_rq_, *abus_;
};

class AbusAttrTest : public AbusAttrs  {
    protected:
        virtual void SetUp() {

            AbusAttrs::SetUp();

			m_int = INT_MAX;
			m_llint = LLONG_MAX;
			m_bool = true;
			m_double = M_PI;
			strncpy(m_str, abus_get_copyright(), sizeof(m_str));

	        EXPECT_EQ(0, abus_decl_attr_int(&abus_svc_, SVC_NAME, "int", &m_int, 0, NULL));
	        EXPECT_EQ(0, abus_decl_attr_int(&abus_svc_, SVC_NAME, "int_ro", &m_int, ABUS_RPC_RDONLY, NULL));
	        EXPECT_EQ(0, abus_decl_attr_int(&abus_svc_, SVC_NAME, "int_const", &m_int, ABUS_RPC_CONST, NULL));
	        EXPECT_EQ(0, abus_decl_attr_llint(&abus_svc_, SVC_NAME, "llint", &m_llint, 0, NULL));
	        EXPECT_EQ(0, abus_decl_attr_bool(&abus_svc_, SVC_NAME, "bool", &m_bool, 0, NULL));
	        EXPECT_EQ(0, abus_decl_attr_double(&abus_svc_, SVC_NAME, "double", &m_double, 0, NULL));
	        EXPECT_EQ(0, abus_decl_attr_str(&abus_svc_, SVC_NAME, "str", m_str, sizeof(m_str), 0, NULL));
        }
        virtual void TearDown() {

			EXPECT_EQ(0, abus_undecl_attr(&abus_svc_, SVC_NAME, "int"));
			EXPECT_EQ(0, abus_undecl_attr(&abus_svc_, SVC_NAME, "int_ro"));
			EXPECT_EQ(0, abus_undecl_attr(&abus_svc_, SVC_NAME, "int_const"));
			EXPECT_EQ(0, abus_undecl_attr(&abus_svc_, SVC_NAME, "llint"));
			EXPECT_EQ(0, abus_undecl_attr(&abus_svc_, SVC_NAME, "bool"));
			EXPECT_EQ(0, abus_undecl_attr(&abus_svc_, SVC_NAME, "double"));
			// double undeclare
			EXPECT_EQ(JSONRPC_NO_METHOD, abus_undecl_attr(&abus_svc_, SVC_NAME, "double"));
			// no undeclare, to be catched by abus_cleanup()
			// EXPECT_EQ(0, abus_undecl_attr(&abus_svc_, SVC_NAME, "str"));

            AbusAttrs::TearDown();
		}

	    int m_int;
	    long long m_llint;
	    bool m_bool;
	    double m_double;
	    char m_str[512];
};

class AbusAutoAttrTest : public AbusAttrs {
    protected:
        virtual void SetUp() {
            AbusAttrs::SetUp();

			// Auto-allocated attributes (i.e. no pointer)
	        EXPECT_EQ(0, abus_decl_attr_int(&abus_svc_, SVC_NAME, "int", NULL, 0, NULL));
	        EXPECT_EQ(0, abus_decl_attr_int(&abus_svc_, SVC_NAME, "int_ro", NULL, ABUS_RPC_RDONLY, NULL));
	        EXPECT_EQ(0, abus_decl_attr_llint(&abus_svc_, SVC_NAME, "llint", NULL, 0, NULL));
	        EXPECT_EQ(0, abus_decl_attr_bool(&abus_svc_, SVC_NAME, "bool", NULL, 0, NULL));
	        EXPECT_EQ(0, abus_decl_attr_double(&abus_svc_, SVC_NAME, "double", NULL, 0, NULL));
	        EXPECT_EQ(0, abus_decl_attr_str(&abus_svc_, SVC_NAME, "str", NULL, 256, 0, NULL));
        }
        virtual void TearDown() {

			EXPECT_EQ(0, abus_undecl_attr(&abus_svc_, SVC_NAME, "int"));
			EXPECT_EQ(0, abus_undecl_attr(&abus_svc_, SVC_NAME, "int_ro"));
			EXPECT_EQ(0, abus_undecl_attr(&abus_svc_, SVC_NAME, "llint"));
			EXPECT_EQ(0, abus_undecl_attr(&abus_svc_, SVC_NAME, "bool"));
			EXPECT_EQ(0, abus_undecl_attr(&abus_svc_, SVC_NAME, "double"));
			EXPECT_EQ(0, abus_undecl_attr(&abus_svc_, SVC_NAME, "str"));

            AbusAttrs::TearDown();
        }
};


TEST_P(AbusAttrTest, AllTypes) {
	int a;
    long long ll;
    bool b;
    double d;
    char s[512];

	EXPECT_EQ(0, abus_attr_get_int(abus_, SVC_NAME, "int", &a, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_get_llint(abus_, SVC_NAME, "llint", &ll, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_get_bool(abus_, SVC_NAME, "bool", &b, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_get_double(abus_, SVC_NAME, "double", &d, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_get_str(abus_, SVC_NAME, "str", s, sizeof(s), RPC_TIMEOUT));

	EXPECT_EQ(a, m_int);
	EXPECT_EQ(ll, m_llint);
	EXPECT_EQ(b, m_bool);
	EXPECT_NEAR(d, m_double, DABSERROR);
	EXPECT_STREQ(m_str, s);

	EXPECT_EQ(0, abus_attr_set_int(abus_, SVC_NAME, "int", -1, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_set_llint(abus_, SVC_NAME, "llint", -2LL, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_set_bool(abus_, SVC_NAME, "bool", false, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_set_double(abus_, SVC_NAME, "double", M_E, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_set_str(abus_, SVC_NAME, "str", abus_get_version(), RPC_TIMEOUT));

	EXPECT_EQ(-1, m_int);
	EXPECT_EQ(-2LL, m_llint);
	EXPECT_FALSE(m_bool);
	EXPECT_NEAR(M_E, m_double, DABSERROR);
	EXPECT_STREQ(m_str, abus_get_version());

	/* inexistant attr name */
	EXPECT_EQ(JSONRPC_NO_METHOD, abus_attr_get_int(abus_, SVC_NAME, "no_such_int", &a, RPC_TIMEOUT));
	EXPECT_EQ(JSONRPC_NO_METHOD, abus_attr_set_int(abus_, SVC_NAME, "no_such_int", -2, RPC_TIMEOUT));
	/* wrong type */
	EXPECT_EQ(JSONRPC_INVALID_METHOD, abus_attr_set_bool(abus_, SVC_NAME, "int", true, RPC_TIMEOUT));
	EXPECT_EQ(JSONRPC_INVALID_METHOD, abus_attr_get_int(abus_, SVC_NAME, "bool", &a, RPC_TIMEOUT));
	EXPECT_EQ(-1, m_int);

	/* rem: RDONLY flag is not enforced for direct access */
	if (m_separate_abus) {
		EXPECT_EQ(JSONRPC_INVALID_METHOD, abus_attr_set_int(abus_, SVC_NAME, "int_ro", -3, RPC_TIMEOUT));
		EXPECT_EQ(-1, m_int);
	}

	EXPECT_EQ(JSONRPC_INVALID_METHOD, abus_attr_set_int(abus_, SVC_NAME, "int_const", -3, RPC_TIMEOUT));
	EXPECT_EQ(-1, m_int);

	/* TODO:
	   - empty set/get -> no error
	   - multiple attributes
	   - int abus_attr_changed(abus_t *abus, const char *service_name, const char *attr_name);
	 */
}

// TODO: factorize with AbusAttrTest
TEST_P(AbusAutoAttrTest, AllTypes) {
	int a;
    bool b;
    long long ll;
    double d;
    char s[512];

	// Rem: attributes were auto-allocated, initialized like in a bss

	EXPECT_EQ(0, abus_attr_get_int(abus_, SVC_NAME, "int", &a, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_get_llint(abus_, SVC_NAME, "llint", &ll, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_get_bool(abus_, SVC_NAME, "bool", &b, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_get_double(abus_, SVC_NAME, "double", &d, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_get_str(abus_, SVC_NAME, "str", s, sizeof(s), RPC_TIMEOUT));

	EXPECT_EQ(a, 0);
	EXPECT_EQ(ll, 0LL);
	EXPECT_EQ(b, false);
	EXPECT_EQ(d, 0.0);
	EXPECT_STREQ("", s);

	EXPECT_EQ(0, abus_attr_set_int(abus_, SVC_NAME, "int", -1, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_set_llint(abus_, SVC_NAME, "llint", -2LL, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_set_bool(abus_, SVC_NAME, "bool", true, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_set_double(abus_, SVC_NAME, "double", M_E, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_set_str(abus_, SVC_NAME, "str", abus_get_version(), RPC_TIMEOUT));

	EXPECT_EQ(0, abus_attr_get_int(abus_, SVC_NAME, "int", &a, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_get_llint(abus_, SVC_NAME, "llint", &ll, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_get_bool(abus_, SVC_NAME, "bool", &b, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_get_double(abus_, SVC_NAME, "double", &d, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_get_str(abus_, SVC_NAME, "str", s, sizeof(s), RPC_TIMEOUT));

	EXPECT_EQ(a, -1);
	EXPECT_EQ(ll, -2LL);
	EXPECT_TRUE(b);
	EXPECT_NEAR(M_E, d, DABSERROR);
	EXPECT_STREQ(abus_get_version(), s);

	/* inexistant attr name */
	EXPECT_EQ(JSONRPC_NO_METHOD, abus_attr_get_int(abus_, SVC_NAME, "no_such_int", &a, RPC_TIMEOUT));
	EXPECT_EQ(JSONRPC_NO_METHOD, abus_attr_set_int(abus_, SVC_NAME, "no_such_int", -2, RPC_TIMEOUT));
	/* wrong type */
	EXPECT_EQ(JSONRPC_INVALID_METHOD, abus_attr_set_bool(abus_, SVC_NAME, "int", true, RPC_TIMEOUT));
	EXPECT_EQ(JSONRPC_INVALID_METHOD, abus_attr_get_int(abus_, SVC_NAME, "bool", &a, RPC_TIMEOUT));

	/* rem: RDONLY flag is not enforced for direct access */
	if (m_separate_abus) {
		EXPECT_EQ(JSONRPC_INVALID_METHOD, abus_attr_set_int(abus_, SVC_NAME, "int_ro", -3, RPC_TIMEOUT));
	}
}

INSTANTIATE_TEST_CASE_P(AbusAttrVariations, AbusAttrTest, Values(true, false));
INSTANTIATE_TEST_CASE_P(AbusAutoAttrVariations, AbusAutoAttrTest, Values(true, false));

