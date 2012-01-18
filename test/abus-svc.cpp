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
#include "abus.h"

#include <gtest/gtest.h>

#define RPC_TIMEOUT 1000 /* ms */
#define SVC_NAME "gtestsvc"
#define DEPSILON 1e-12

class AbusTest : public testing::Test {
    protected:
        virtual void SetUp() {
	        EXPECT_EQ(0, abus_init(&abus_));

	        EXPECT_EQ(0, abus_decl_method_cxx(&abus_, SVC_NAME, "sum", this, svc_sum_cb,
					ABUS_RPC_FLAG_NONE,
					"Compute summation of two integers",
					"a:i:first operand,b:i:second operand",
					"res_value:i:summation"));

	        EXPECT_EQ(0, abus_decl_method_cxx(&abus_, SVC_NAME, "jtypes", this, svc_jtypes_cb,
					ABUS_RPC_FLAG_NONE, NULL, NULL, NULL));

	        EXPECT_EQ(0, abus_decl_method_cxx(&abus_, SVC_NAME, "sqr", this, svc_array_sqr_cb,
					ABUS_RPC_FLAG_NONE,
					"Compute square value of array of integers",
					"k:i:some contant,my_array:(a:i:value to be squared,arg_index:i:index of arg for demo):array of stuff",
					"res_k:i:same contant,res_array:(res_a:i:squared value):array of squared stuff"));

	        EXPECT_EQ(0, abus_decl_method_cxx(&abus_, SVC_NAME, "echo", this, svc_echo_cb,
					ABUS_RPC_FLAG_NONE,
					"echo of message",
					"msg:s:message",
					"msg:s:echoed message,msg_len:i:message length"));
        }
        virtual void TearDown() {
	        EXPECT_EQ(0, abus_cleanup(&abus_));
        }

	abus_t abus_;
	static const int m_method_count = 4;

    abus_decl_method_member(AbusTest, svc_sum_cb) ;
    abus_decl_method_member(AbusTest, svc_jtypes_cb) ;
    abus_decl_method_member(AbusTest, svc_array_sqr_cb) ;
    abus_decl_method_member(AbusTest, svc_echo_cb) ;
};

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

class AbusReqTest : public AbusTest {
    protected:
        virtual void SetUp() {
            AbusTest::SetUp();

	        EXPECT_EQ(0, abus_request_method_init(&abus_, SVC_NAME, "sum", &json_rpc_));
        }
        virtual void TearDown() {

	        EXPECT_EQ(0, abus_request_method_cleanup(&abus_, &json_rpc_));

            AbusTest::TearDown();
        }

	    json_rpc_t json_rpc_;
};

class AbusJtypesTest : public AbusTest {
    protected:
        virtual void SetUp() {
            AbusTest::SetUp();

	        EXPECT_EQ(0, abus_request_method_init(&abus_, SVC_NAME, "jtypes", &json_rpc_));
        }
        virtual void TearDown() {

	        EXPECT_EQ(0, abus_request_method_cleanup(&abus_, &json_rpc_));

            AbusTest::TearDown();
        }

	    json_rpc_t json_rpc_;
};

class AbusArrayTest : public AbusTest {
    protected:
        virtual void SetUp() {
            AbusTest::SetUp();

	        EXPECT_EQ(0, abus_request_method_init(&abus_, SVC_NAME, "sqr", &json_rpc_));
        }
        virtual void TearDown() {

	        EXPECT_EQ(0, abus_request_method_cleanup(&abus_, &json_rpc_));

            AbusTest::TearDown();
        }

	    json_rpc_t json_rpc_;
};

class AbusEchoTest : public AbusTest {
    protected:
        virtual void SetUp() {
            AbusTest::SetUp();

	        EXPECT_EQ(0, abus_request_method_init(&abus_, SVC_NAME, "echo", &json_rpc_));
        }
        virtual void TearDown() {

	        EXPECT_EQ(0, abus_request_method_cleanup(&abus_, &json_rpc_));

            AbusTest::TearDown();
        }

	    json_rpc_t json_rpc_;
};

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
			EXPECT_EQ(0, abus_undecl_attr(&abus_, SVC_NAME, "str"));

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

TEST_F(AbusReqTest, BasicSvc) {
	int res_value;

	/* pass 2 parameters: "a" and "b" */
	EXPECT_EQ(0, json_rpc_append_int(&json_rpc_, "a", 2));
	EXPECT_EQ(0, json_rpc_append_int(&json_rpc_, "b", 3));

	EXPECT_EQ(0, abus_request_method_invoke(&abus_, &json_rpc_, ABUS_RPC_FLAG_NONE, RPC_TIMEOUT));

    EXPECT_EQ(0, json_rpc_get_int(&json_rpc_, "res_value", &res_value));

	EXPECT_EQ(2+3, res_value);
}

TEST_F(AbusReqTest, PlentyOfParams) {
	int res_value;
    char parm_name[16];

    for (int i=0; i<1024; i++) {
        sprintf(parm_name, "dummy%d", i);
	    EXPECT_EQ(0, json_rpc_append_int(&json_rpc_, parm_name, i));
    }

	/* pass 2 parameters: "a" and "b" */
	EXPECT_EQ(0, json_rpc_append_int(&json_rpc_, "a", 2));
	EXPECT_EQ(0, json_rpc_append_int(&json_rpc_, "b", 3));

	EXPECT_EQ(0, abus_request_method_invoke(&abus_, &json_rpc_, ABUS_RPC_FLAG_NONE, RPC_TIMEOUT));

    EXPECT_EQ(0, json_rpc_get_int(&json_rpc_, "res_value", &res_value));

	EXPECT_EQ(2+3, res_value);
}

TEST_F(AbusReqTest, PlentyOfMethods) {
	int res_value;
    char method_name[16];
    json_rpc_t json_rpc_introspect;
    const int plenty_count = 64;

    for (int i=0; i<plenty_count; i++) {
        sprintf(method_name, "sum%d", i);

        EXPECT_EQ(0, abus_decl_method_cxx(&abus_, SVC_NAME, method_name, this, svc_sum_cb,
                    ABUS_RPC_FLAG_NONE,
					"Compute summation of two integers, plenty of methods",
                    "a:i:first operand,b:i:second operand",
                    "res_value:i:summation"));
    }

	/* pass 2 parameters: "a" and "b" */
	EXPECT_EQ(0, json_rpc_append_int(&json_rpc_, "a", 2));
	EXPECT_EQ(0, json_rpc_append_int(&json_rpc_, "b", 3));

	EXPECT_EQ(0, abus_request_method_invoke(&abus_, &json_rpc_, ABUS_RPC_FLAG_NONE, RPC_TIMEOUT));

    EXPECT_EQ(0, json_rpc_get_int(&json_rpc_, "res_value", &res_value));

	EXPECT_EQ(2+3, res_value);

    /* Huge introspection */
    EXPECT_EQ(0, abus_request_method_init(&abus_, SVC_NAME, "*", &json_rpc_introspect));

    EXPECT_EQ(0, abus_request_method_invoke(&abus_, &json_rpc_introspect, ABUS_RPC_FLAG_NONE, RPC_TIMEOUT));
    EXPECT_EQ(plenty_count+m_method_count, json_rpc_get_array_count(&json_rpc_introspect, "methods"));

    EXPECT_EQ(0, abus_request_method_cleanup(&abus_, &json_rpc_introspect));
}

TEST_F(AbusReqTest, MissingArg) {
	int res_value;

	/* pass only 1 parameter: "a", make "b" missing */
	EXPECT_EQ(0, json_rpc_append_int(&json_rpc_, "a", 2));

	EXPECT_EQ(JSONRPC_INVALID_METHOD, abus_request_method_invoke(&abus_, &json_rpc_, ABUS_RPC_FLAG_NONE, RPC_TIMEOUT));

    EXPECT_EQ(JSONRPC_INVALID_METHOD, json_rpc_get_int(&json_rpc_, "res_value", &res_value));
}

TEST_F(AbusReqTest, InvalidType) {
	int res_value;

	/* pass 2 parameters: "a", and mis-typed "b" */
	EXPECT_EQ(0, json_rpc_append_int(&json_rpc_, "a", 2));
	EXPECT_EQ(0, json_rpc_append_str(&json_rpc_, "b", "crook"));

	EXPECT_EQ(JSONRPC_INVALID_METHOD, abus_request_method_invoke(&abus_, &json_rpc_, ABUS_RPC_FLAG_NONE, RPC_TIMEOUT));

    EXPECT_EQ(JSONRPC_INVALID_METHOD, json_rpc_get_int(&json_rpc_, "res_value", &res_value));
}

TEST_F(AbusJtypesTest, AllTypes)
{
	int a = INT_MAX, res_a;
    bool b = true, res_b;
    long long ll = LLONG_MAX, res_ll;
    double d1 = M_PI, res_d1;
    double d2 = 3, res_d2;
    char res_s[512];

	EXPECT_EQ(0, json_rpc_append_int(&json_rpc_, "int", a));
	EXPECT_EQ(0, json_rpc_append_llint(&json_rpc_, "llint", ll));
	EXPECT_EQ(0, json_rpc_append_bool(&json_rpc_, "bool", b));
	EXPECT_EQ(0, json_rpc_append_double(&json_rpc_, "double1", d1));
	EXPECT_EQ(0, json_rpc_append_double(&json_rpc_, "double2", d2));
	EXPECT_EQ(0, json_rpc_append_str(&json_rpc_, "str", abus_get_copyright()));

	EXPECT_EQ(0, abus_request_method_invoke(&abus_, &json_rpc_, ABUS_RPC_FLAG_NONE, RPC_TIMEOUT));

    EXPECT_EQ(JSON_INT, json_rpc_get_type(&json_rpc_, "res_int"));
	EXPECT_EQ(0, json_rpc_get_int(&json_rpc_, "res_int", &res_a));
	EXPECT_EQ(0, json_rpc_get_llint(&json_rpc_, "res_llint", &res_ll));
	EXPECT_EQ(0, json_rpc_get_bool(&json_rpc_, "res_bool", &res_b));
	EXPECT_EQ(0, json_rpc_get_double(&json_rpc_, "res_double1", &res_d1));
	EXPECT_EQ(0, json_rpc_get_double(&json_rpc_, "res_double2", &res_d2));
	EXPECT_EQ(0, json_rpc_get_str(&json_rpc_, "res_str", res_s, sizeof(res_s)));

	EXPECT_EQ(a, res_a);
	EXPECT_EQ(b, res_b);
	EXPECT_EQ(ll, res_ll);
	EXPECT_TRUE(d1-res_d1 < DEPSILON);
	EXPECT_TRUE(d2-res_d2 < DEPSILON);
	EXPECT_TRUE(strncmp(res_s, abus_get_copyright(), sizeof(res_s)) == 0);
}

TEST_F(AbusArrayTest, SqrArray)
{
	int count, i, res_value;
    const int array_count = 199;

	/* pass 2 parameters: "k" and "my_array" */
	json_rpc_append_int(&json_rpc_, "k", -1);

	/* begin the array */
	EXPECT_EQ(0, json_rpc_append_args(&json_rpc_,
					JSON_KEY, "my_array", -1,
					JSON_ARRAY_BEGIN,
					-1));

	for (i = 0; i<array_count; i++) {
		/* each array element *must* be an "OBJECT", i.e. a dictonary */
		EXPECT_EQ(0, json_rpc_append_args(&json_rpc_, JSON_OBJECT_BEGIN, -1));

		EXPECT_EQ(0, json_rpc_append_int(&json_rpc_, "a", i));

		EXPECT_EQ(0, json_rpc_append_args(&json_rpc_, JSON_OBJECT_END, -1));
	}

	/* end the array */
	EXPECT_EQ(0, json_rpc_append_args(&json_rpc_, JSON_ARRAY_END, -1));


	EXPECT_EQ(0, abus_request_method_invoke(&abus_, &json_rpc_, ABUS_RPC_FLAG_NONE, RPC_TIMEOUT));

	count = json_rpc_get_array_count(&json_rpc_, "res_array");
	EXPECT_EQ (count, array_count);

	EXPECT_EQ(0, json_rpc_get_int(&json_rpc_, "res_k", &res_value));
	EXPECT_EQ (res_value, -1);

	for (i = 0; i<count; i++) {
		/* Aim at i-th element within array "res_array" */
		EXPECT_EQ(0, json_rpc_get_point_at(&json_rpc_, "res_array", i));

		EXPECT_EQ(0, json_rpc_get_int(&json_rpc_, "res_a", &res_value));
		EXPECT_EQ(res_value, i*i);
	}

	/* Aim back out of array */

	EXPECT_EQ(0, json_rpc_get_point_at(&json_rpc_, NULL, i));

	EXPECT_EQ(0, json_rpc_get_int(&json_rpc_, "res_k", &res_value));
	EXPECT_EQ (res_value, -1);
}

TEST_F(AbusEchoTest, BasicEchoSvc) {
	int res_value;

	/* pass 2 parameters: "a" and "b" */
	EXPECT_EQ(0, json_rpc_append_str(&json_rpc_, "msg", "test"));

	EXPECT_EQ(0, abus_request_method_invoke(&abus_, &json_rpc_, ABUS_RPC_FLAG_NONE, RPC_TIMEOUT));

    EXPECT_EQ(0, json_rpc_get_int(&json_rpc_, "msg_len", &res_value));

	EXPECT_EQ(4, res_value);
}

TEST_F(AbusEchoTest, EmptyStringParam) {
	int res_value;

	EXPECT_EQ(0, json_rpc_append_str(&json_rpc_, "msg", ""));

	EXPECT_EQ(0, abus_request_method_invoke(&abus_, &json_rpc_, ABUS_RPC_FLAG_NONE, RPC_TIMEOUT));

    EXPECT_EQ(0, json_rpc_get_int(&json_rpc_, "msg_len", &res_value));

	EXPECT_EQ(0, res_value);
}

TEST_F(AbusEchoTest, SpecialCharStringParam) {
	int res_value;
	size_t bufsnd_len, bufrcv_len;
    char bufsnd[512] = "FIXME", bufrcv[512];
    const int ascii_count = 127;

    // Including \, "
    // TODO \0, invalid UTF-8 start code
    for (int i=1; i<ascii_count; i++) {
        bufsnd[i] = i;
    }
    bufsnd_len = ascii_count;
#if 1
    static const char special_chars[] = "éèàùçâĝĥ";
    for (int i=0; i<strlen(special_chars); i++) {
        bufsnd[ascii_count+i] = special_chars[i];
    }
    bufsnd_len += strlen(special_chars);
#endif

	EXPECT_EQ(0, json_rpc_append_strn(&json_rpc_, "msg", bufsnd, bufsnd_len));

	EXPECT_EQ(0, abus_request_method_invoke(&abus_, &json_rpc_, ABUS_RPC_FLAG_NONE, RPC_TIMEOUT));

    EXPECT_EQ(0, json_rpc_get_int(&json_rpc_, "msg_len", &res_value));
    bufrcv_len = sizeof(bufrcv);
    EXPECT_EQ(0, json_rpc_get_strn(&json_rpc_, "msg", bufrcv, &bufrcv_len));

	EXPECT_EQ(bufsnd_len, bufrcv_len);
	EXPECT_EQ(bufsnd_len, res_value);
	EXPECT_TRUE(strncmp(bufsnd, bufrcv, bufsnd_len) == 0);

	size_t bufrcvp_len = 0;
	const char *pbufrcv;
	EXPECT_EQ(0, json_rpc_get_strp(&json_rpc_, "msg", &pbufrcv, &bufrcvp_len));
	EXPECT_TRUE(memcmp(pbufrcv, bufrcv, bufrcvp_len) == 0);
}

TEST_F(AbusEchoTest, BigEscapedCharStringParam) {
	int res_value, bufsnd_len;
    char bufsnd[JSONRPC_RESP_SZ_MAX/2], bufrcv[JSONRPC_RESP_SZ_MAX/2];

    // to-be-escaped char
    memset(bufsnd, '"', sizeof(bufsnd));
    bufsnd_len = JSONRPC_RESP_SZ_MAX/2 - 128;

	EXPECT_EQ(0, json_rpc_append_strn(&json_rpc_, "msg", bufsnd, bufsnd_len));

	EXPECT_EQ(0, abus_request_method_invoke(&abus_, &json_rpc_, ABUS_RPC_FLAG_NONE, RPC_TIMEOUT));

    EXPECT_EQ(0, json_rpc_get_int(&json_rpc_, "msg_len", &res_value));
    // FIXME: json_rpc_get_str() should return real string length?
    EXPECT_EQ(0, json_rpc_get_str(&json_rpc_, "msg", bufrcv, sizeof(bufrcv)));

	EXPECT_EQ(strlen(bufrcv), bufsnd_len);
	EXPECT_TRUE(strncmp(bufsnd, bufrcv, bufsnd_len) == 0);
}

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
	EXPECT_TRUE(d-m_double < DEPSILON);
	EXPECT_TRUE(strncmp(m_str, s, sizeof(s)) == 0);

	EXPECT_EQ(0, abus_attr_set_int(&abus_, SVC_NAME, "int", -1, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_set_bool(&abus_, SVC_NAME, "bool", false, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_set_double(&abus_, SVC_NAME, "double", M_E, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_attr_set_str(&abus_, SVC_NAME, "str", abus_get_version(), RPC_TIMEOUT));

	EXPECT_EQ(-1, m_int);
	EXPECT_FALSE(m_bool);
	EXPECT_TRUE(M_E-m_double < DEPSILON);
	EXPECT_TRUE(strncmp(m_str, abus_get_version(), sizeof(m_str)) == 0);

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
	EXPECT_TRUE(strncmp("", s, sizeof(s)) == 0);

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
	EXPECT_TRUE(M_E-d < DEPSILON);
	EXPECT_TRUE(strncmp(abus_get_version(), s, sizeof(s)) == 0);

	/* inexistant attr name */
	EXPECT_EQ(JSONRPC_NO_METHOD, abus_attr_get_int(&abus_, SVC_NAME, "no_such_int", &a, RPC_TIMEOUT));
	EXPECT_EQ(JSONRPC_NO_METHOD, abus_attr_set_int(&abus_, SVC_NAME, "no_such_int", -2, RPC_TIMEOUT));

	EXPECT_EQ(JSONRPC_INVALID_METHOD, abus_attr_set_int(&abus_, SVC_NAME, "int_ro", -3, RPC_TIMEOUT));
}

TEST_F(AbusTest, NoService) {
	json_rpc_t json_rpc;

	EXPECT_EQ(0, abus_request_method_init(&abus_, "no_such_"SVC_NAME, "sum", &json_rpc));

	/* pass 2 parameters: "a" and "b" */
	EXPECT_EQ(0, json_rpc_append_int(&json_rpc, "a", 2));
	EXPECT_EQ(0, json_rpc_append_int(&json_rpc, "b", 3));

	EXPECT_EQ(-ENOENT, abus_request_method_invoke(&abus_, &json_rpc, ABUS_RPC_FLAG_NONE, RPC_TIMEOUT));

	EXPECT_EQ(0, abus_request_method_cleanup(&abus_, &json_rpc));
}

TEST_F(AbusTest, NoMethod) {
	json_rpc_t json_rpc;

	EXPECT_EQ(0, abus_request_method_init(&abus_, SVC_NAME, "no_such_method", &json_rpc));

	/* pass 2 parameters: "a" and "b" */
	EXPECT_EQ(0, json_rpc_append_int(&json_rpc, "a", 2));
	EXPECT_EQ(0, json_rpc_append_int(&json_rpc, "b", 3));

	EXPECT_EQ(JSONRPC_NO_METHOD, abus_request_method_invoke(&abus_, &json_rpc, ABUS_RPC_FLAG_NONE, RPC_TIMEOUT));

	EXPECT_EQ(0, abus_request_method_cleanup(&abus_, &json_rpc));
}

TEST_F(AbusTest, MethodRedefinition) {
	int res_value;

	// declare first "sum" with another callback
	EXPECT_EQ(0, abus_decl_method_cxx(&abus_, SVC_NAME, "sum", this, svc_jtypes_cb,
					ABUS_RPC_FLAG_NONE,
					"Compute summation of two integers, but wrong callback",
					"a:i:first operand,b:i:second operand",
					"res_value:i:supposed summation"));

	// redeclaration, but with right callback
	EXPECT_EQ(0, abus_decl_method_cxx(&abus_, SVC_NAME, "sum", this, svc_sum_cb,
					ABUS_RPC_FLAG_NONE,
					"Compute summation of two integers, with right callback this time",
					"a:i:first operand,b:i:second operand",
					"res_value:i:summation"));

	json_rpc_t json_rpc;

	EXPECT_EQ(0, abus_request_method_init(&abus_, SVC_NAME, "sum", &json_rpc));

	/* pass 2 parameters: "a" and "b" */
	EXPECT_EQ(0, json_rpc_append_int(&json_rpc, "a", 2));
	EXPECT_EQ(0, json_rpc_append_int(&json_rpc, "b", 3));

	EXPECT_EQ(0, abus_request_method_invoke(&abus_, &json_rpc, ABUS_RPC_FLAG_NONE, RPC_TIMEOUT));

	EXPECT_EQ(0, json_rpc_get_int(&json_rpc, "res_value", &res_value));

	EXPECT_EQ(2+3, res_value);

	EXPECT_EQ(0, abus_request_method_cleanup(&abus_, &json_rpc));
}


// TODO:
// - plenty of async reqs
// - http://code.google.com/p/abus/wiki/CornerCases (abus_undecl_method, ..)
// - events, abus.hpp, introspect
// - test all flags

