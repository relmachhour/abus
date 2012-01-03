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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "abus.h"

int main(int argc, char **argv)
{
	abus_t abus;

	int my_int = 42;
	int my_other_int = -2;


	abus_init(&abus);

	abus_decl_attr_int(&abus, "exampleattrsvc", "tree.some_int", &my_int,
					ABUS_RPC_FLAG_NONE,
					"Some integer, for demo purpose");

	abus_decl_attr_int(&abus, "exampleattrsvc", "tree.some_other_int", &my_other_int,
					ABUS_RPC_FLAG_NONE,
					"Some other integer, still for demo purpose");

	/* do other stuff */
	sleep(10000);

	abus_cleanup(&abus);

	return EXIT_SUCCESS;
}

