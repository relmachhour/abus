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

TEST(AbusTorTest, DoubleInit) {
	abus_t abus;

	EXPECT_EQ(0, abus_init(&abus));

	EXPECT_EQ(0, abus_init(&abus));

	EXPECT_EQ(0, abus_cleanup(&abus));
}

TEST(AbusTorTest, DoubleCleanup) {
	abus_t abus;

	EXPECT_EQ(0, abus_init(&abus));

	EXPECT_EQ(0, abus_cleanup(&abus));

	EXPECT_EQ(0, abus_cleanup(&abus));
}

