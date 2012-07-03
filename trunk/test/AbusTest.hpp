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
#ifndef _ABUS_TEST_H
#define _ABUS_TEST_H

#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include "abus.h"

#include <gtest/gtest.h>

#define RPC_TIMEOUT 1000 /* ms */
#define SVC_NAME "gtestsvc"
#define DABSERROR 1e-12

class AbusTest : public testing::Test {
    protected:
        virtual void SetUp() {
	        abus_ = abus_init(NULL);
	        EXPECT_TRUE(NULL != abus_);

	        EXPECT_EQ(0, abus_decl_method_cxx(abus_, SVC_NAME, "sum", this, svc_sum_cb,
					ABUS_RPC_FLAG_NONE,
					"Compute summation of two integers",
					"a:i:first operand,b:i:second operand",
					"res_value:i:summation"));

	        EXPECT_EQ(0, abus_decl_method_cxx(abus_, SVC_NAME, "jtypes", this, svc_jtypes_cb,
					ABUS_RPC_FLAG_NONE, NULL, NULL, NULL));

	        EXPECT_EQ(0, abus_decl_method_cxx(abus_, SVC_NAME, "sqr", this, svc_array_sqr_cb,
					ABUS_RPC_FLAG_NONE,
					"Compute square value of array of integers",
					"k:i:some contant,my_array:(a:i:value to be squared,arg_index:i:index of arg for demo):array of stuff",
					"res_k:i:same contant,res_array:(res_a:i:squared value):array of squared stuff"));

	        EXPECT_EQ(0, abus_decl_method_cxx(abus_, SVC_NAME, "echo", this, svc_echo_cb,
					ABUS_RPC_FLAG_NONE,
					"echo of message",
					"msg:s:message",
					"msg:s:echoed message,msg_len:i:message length"));
        }
        virtual void TearDown() {
			EXPECT_EQ(0, abus_undecl_method(abus_, SVC_NAME, "sum"));
			EXPECT_EQ(0, abus_undecl_method(abus_, SVC_NAME, "jtypes"));
			EXPECT_EQ(0, abus_undecl_method(abus_, SVC_NAME, "sqr"));
			// double undeclare
			EXPECT_EQ(JSONRPC_NO_METHOD, abus_undecl_method(abus_, SVC_NAME, "sqr"));
			// no undeclare, to be catched by abus_cleanup()
			//EXPECT_EQ(0, abus_undecl_method(abus_, SVC_NAME, "echo"));

	        EXPECT_EQ(0, abus_cleanup(abus_));
        }

	abus_t *abus_;
	static const int m_method_count = 4;

    abus_decl_method_member(AbusTest, svc_sum_cb) ;
    abus_decl_method_member(AbusTest, svc_jtypes_cb) ;
    abus_decl_method_member(AbusTest, svc_array_sqr_cb) ;
    abus_decl_method_member(AbusTest, svc_echo_cb) ;
};

#endif  /* _ABUS_TEST_H */
