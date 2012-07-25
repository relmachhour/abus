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

#include <abus.h>
#include <json.h>

#include <gtest/gtest.h>

#include "AbusTest.hpp"

static int msleep(int ms)
{
	return usleep(ms*1000);
}

#define EVT_NAME "gtestevent"

class AbusEvtTest : public AbusTest {
	protected:
		virtual void SetUp() {
			m_res_value = 0;
			m_res_value2 = 0;
			AbusTest::SetUp();

			// service side
			EXPECT_EQ(0, abus_decl_event(abus_, SVC_NAME, EVT_NAME, "gtest event", "magicvalue:i:"));

			json_rpc_ = abus_request_event_init(abus_, SVC_NAME, EVT_NAME);

			EXPECT_TRUE(NULL != json_rpc_);

			json_rpc_append_int(json_rpc_, "magicvalue", 42);
		}
		virtual void TearDown() {

			EXPECT_EQ(0, abus_request_event_cleanup(abus_, json_rpc_));

			EXPECT_EQ(0, abus_undecl_event(abus_, SVC_NAME, EVT_NAME));

			AbusTest::TearDown();
		}

		abus_decl_method_member(AbusEvtTest, event_cb);
		abus_decl_method_member(AbusEvtTest, event2_cb);
		int m_res_value, m_res_value2;

		json_rpc_t *json_rpc_;
};

void AbusEvtTest::event_cb(json_rpc_t *json_rpc)
{
	EXPECT_EQ(0, json_rpc_get_int(json_rpc, "magicvalue", &m_res_value));
}

void AbusEvtTest::event2_cb(json_rpc_t *json_rpc)
{
	EXPECT_EQ(0, json_rpc_get_int(json_rpc, "magicvalue", &m_res_value2));
}

TEST_F(AbusEvtTest, BasicEvt) {

	// client side
	EXPECT_EQ(0, abus_event_subscribe_cxx(abus_, SVC_NAME, EVT_NAME, this, event_cb, ABUS_RPC_FLAG_NONE, RPC_TIMEOUT));

	// service side
	abus_request_event_publish(abus_, json_rpc_, ABUS_RPC_FLAG_NONE);

	// back to client side: give time to do abus_request_event_publish() propagation
	// TODO: replace with a pthread_cond_t
	msleep(200);

	// check callback has been called
	EXPECT_EQ(42, m_res_value);

	EXPECT_EQ(0, abus_event_unsubscribe_cxx(abus_, SVC_NAME, EVT_NAME, this, event_cb, RPC_TIMEOUT));
}

TEST_F(AbusEvtTest, TwoServicesSameEvtName) {

	// Second service, with same event name
#define SVC2_NAME SVC_NAME "bis"

	EXPECT_EQ(0, abus_decl_event(abus_, SVC2_NAME, EVT_NAME, "gtest event", "magicvalue:i:"));

	json_rpc_t *json_rpc2 = abus_request_event_init(abus_, SVC2_NAME, EVT_NAME);

	EXPECT_TRUE(NULL != json_rpc2);

	json_rpc_append_int(json_rpc2, "magicvalue", -1000000);


	// client side, subscribe to decl events
	EXPECT_EQ(0, abus_event_subscribe_cxx(abus_, SVC_NAME,  EVT_NAME, this, event_cb,  ABUS_RPC_FLAG_NONE, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_event_subscribe_cxx(abus_, SVC2_NAME, EVT_NAME, this, event2_cb, ABUS_RPC_FLAG_NONE, RPC_TIMEOUT));

	// (first) service side
	abus_request_event_publish(abus_, json_rpc_, ABUS_RPC_FLAG_NONE);

	// back to client side: give time to do abus_request_event_publish() propagation
	// TODO: replace with a pthread_cond_t
	msleep(200);

	// check only the right callback has been called
	EXPECT_EQ(42, m_res_value);
	EXPECT_EQ(0, m_res_value2);


	// (second) service side
	abus_request_event_publish(abus_, json_rpc2, ABUS_RPC_FLAG_NONE);

	// back to client side: give time to do abus_request_event_publish() propagation
	// TODO: replace with a pthread_cond_t
	msleep(200);

	// check only the right callback has been called
	EXPECT_EQ(42, m_res_value);
	EXPECT_EQ(-1000000, m_res_value2);

	// cleanup

	EXPECT_EQ(0, abus_event_unsubscribe_cxx(abus_, SVC_NAME, EVT_NAME, this, event_cb, RPC_TIMEOUT));
	EXPECT_EQ(0, abus_event_unsubscribe_cxx(abus_, SVC2_NAME, EVT_NAME, this, event2_cb, RPC_TIMEOUT));

	EXPECT_EQ(0, abus_request_event_cleanup(abus_, json_rpc2));

	EXPECT_EQ(0, abus_undecl_event(abus_, SVC2_NAME, EVT_NAME));
}

// TODO: subscribe to inexistant service/event, etc.

