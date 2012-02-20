/*
 * Copyright (C) 2012 Stephane Fillod
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
#include <stdio.h>
#include <errno.h>
#include <math.h>

#include "json.h"

#include <gtest/gtest.h>

static const char confContent[] = 
    "{ \"match\":null, \"array0\": [1, 2, 4, 8], "
        "\"str0\": \"string 0\", \"int0\":42, \"bool0\": true, \"double0\": 3.14159265358979323846, "
        "\"level1\": { \"str1\": \"string 1\", \"int1\": 2147483647, \"bool1\": false, \"double1\": 2.7182818284590452354, "
            "\"level2\": { \"str2\": \"string 2\", \"int2\":-1, \"bool2\": true, \"double2\": -1.0}"
        "}"
    "}";

class JsonConfigTest : public testing::Test {
    protected:
        virtual void SetUp() {
            const char *json_fname = tmpnam(NULL);

            /* create and populate the temp file */
            FILE *f = fopen(json_fname, "wb");
	        EXPECT_TRUE(f != NULL);
	        EXPECT_EQ(1, fwrite(confContent, sizeof(confContent)-1, 1, f));
	        EXPECT_EQ(0, fclose(f));

            /* Load and parse the json file */
	        m_json_dom = json_config_open(json_fname);
            EXPECT_TRUE(m_json_dom != NULL);

            unlink(json_fname);
        }

        virtual void TearDown() {
		    json_config_cleanup(m_json_dom);
        }

	json_dom_val_t *m_json_dom;
};

TEST(JsonConfigTestOpen, NoSuchFile) {

	EXPECT_TRUE(NULL == json_config_open("/no_such_json_config_file"));
}

TEST(JsonConfigTestOpen, ParseImpossible) {

	EXPECT_TRUE(NULL == json_config_open("/"));
	EXPECT_TRUE(NULL == json_config_open("/etc/passwd"));
}

TEST_F(JsonConfigTest, Nominal) {
    int myint = 0;
    bool mybool = false;
    const char *mystr = NULL;
    double mydouble = 0.;
    int array_count, i;

	EXPECT_EQ(0, json_config_get_direct_strp(m_json_dom, "", "str0", &mystr, NULL));
	EXPECT_STREQ("string 0", mystr);
	EXPECT_EQ(0, json_config_get_direct_int(m_json_dom, "", "int0", &myint));
	EXPECT_EQ(42, myint);
	EXPECT_EQ(0, json_config_get_direct_bool(m_json_dom, "", "bool0", &mybool));
	EXPECT_TRUE(mybool);
	EXPECT_EQ(0, json_config_get_direct_double(m_json_dom, "", "double0", &mydouble));
	EXPECT_NEAR(M_PI, mydouble, 1e-12);

	EXPECT_EQ(0, json_config_get_direct_strp(m_json_dom, "level1", "str1", &mystr, NULL));
	EXPECT_STREQ("string 1", mystr);
	EXPECT_EQ(0, json_config_get_direct_int(m_json_dom, "level1", "int1", &myint));
	EXPECT_EQ(2147483647, myint);
	EXPECT_EQ(0, json_config_get_direct_bool(m_json_dom, "level1", "bool1", &mybool));
	EXPECT_FALSE(mybool);
	EXPECT_EQ(0, json_config_get_direct_double(m_json_dom, "level1", "double1", &mydouble));
	EXPECT_NEAR(M_E, mydouble, 1e-12);

    // TODO: "level2" when implemented

	// Array
	array_count = json_config_get_direct_array_count(m_json_dom, "", "array0");
	EXPECT_EQ(4, array_count);
	for (i = 0; i < array_count; i++) {
		json_dom_val_t *aryval = json_config_get_direct_array(m_json_dom, "", "array0", i);
		EXPECT_EQ(0, json_config_get_int(aryval, &myint));
		EXPECT_EQ(1<<i, myint);
	}

    // error cases
	EXPECT_EQ(-ENOENT, json_config_get_direct_int(m_json_dom, "", "no_such_key", &myint));
	EXPECT_EQ(-ENOTTY, json_config_get_direct_int(m_json_dom, "", "match", &myint));
	EXPECT_EQ(-ENOTTY, json_config_get_direct_int(m_json_dom, "", "str0", &myint));
	EXPECT_EQ(-ENOTTY, json_config_get_direct_int(m_json_dom, "", "level1", &myint));
	EXPECT_EQ(-ENOTTY, json_config_get_direct_int(m_json_dom, "", "", &myint));

	EXPECT_EQ(-ENOTTY, json_config_get_direct_array_count(m_json_dom, "", "str0"));
	EXPECT_EQ(-ENOENT, json_config_get_direct_array_count(m_json_dom, "", "no_such_array"));
	EXPECT_EQ(NULL, json_config_get_direct_array(m_json_dom, "", "str0", 0));
	EXPECT_EQ(NULL, json_config_get_direct_array(m_json_dom, "", "no_such_array", 0));
	EXPECT_EQ(NULL, json_config_get_direct_array(m_json_dom, "", "array0", 2147483647));
}

