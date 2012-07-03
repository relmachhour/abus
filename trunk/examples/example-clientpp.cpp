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

#include <iostream>
#include <stdlib.h>
#include <stdio.h>

/* The A-Bus C++ wrapper */
#include "abus.hpp"

#define RPC_TIMEOUT 1000 /* ms */

int main(int argc, char **argv)
{
	cABus *abus;
	cABusRequestMethod *myrpc;
	int ret, res_value;
	const char *service_name = "examplesvc";

	if (argc < 4) {
		printf("usage: %s METHOD firstvalue secondvalue\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	abus = new cABus();

	/* method name is taken from command line */
	myrpc = abus->RequestMethod(service_name, argv[1]);
	if (!myrpc)
		exit(EXIT_FAILURE);

	/* pass 2 parameters: "a" and "b" */
	myrpc->append_int("a", atoi(argv[2]));
	myrpc->append_int("b", atoi(argv[3]));

	ret = myrpc->invoke(ABUS_RPC_FLAG_NONE, RPC_TIMEOUT);
	if (ret != 0) {
		std::cerr << "RPC failed with error " << ret << std::endl;
		exit(EXIT_FAILURE);
	}

	ret = myrpc->get_int("res_value", &res_value);

	if (ret == 0)
		std::cout << "res_value=" << res_value << std::endl;
	else
		std::cerr << "No result? error " << ret << std::endl;

	delete myrpc;
	delete abus;

	return EXIT_SUCCESS;
}

