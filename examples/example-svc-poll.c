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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "abus.h"

static void svc_sum_cb(json_rpc_t *json_rpc, void *arg)
{
	int a, b;
	int ret;

	ret  = json_rpc_get_int(json_rpc, "a", &a);
	if (ret == 0)
		ret = json_rpc_get_int(json_rpc, "b", &b);


	printf("## %s: arg=%s, ret=%d, a=%d, b=%d, => result=%d\n", __func__, (const char*)arg, ret, a, b, a+b);

	if (ret)
		json_rpc_set_error(json_rpc, ret, NULL);
	else
		json_rpc_append_int(json_rpc, "res_value", a+b);
}

static void svc_mult_cb(json_rpc_t *json_rpc, void *arg)
{
	int a, b;
	int ret;

	ret  = json_rpc_get_int(json_rpc, "a", &a);
	if (ret == 0)
		ret = json_rpc_get_int(json_rpc, "b", &b);


	printf("## %s: arg=%s, ret=%d, a=%d, b=%d, => result=%d\n", __func__,
					(const char*)arg, ret, a, b, a*b);

	if (ret)
		json_rpc_set_error(json_rpc, ret, NULL);
	else
		json_rpc_append_int(json_rpc, "res_value", a*b);
}

int main(int argc, char **argv)
{
	abus_t *abus;
	abus_conf_t abus_conf;
	int ret;

	abus = abus_init(NULL);

	abus_get_conf(abus, &abus_conf);
	/* take over A-Bus thread */
	abus_conf.poll_operation = true;
	abus_set_conf(abus, &abus_conf);

	ret = abus_decl_method(abus, "examplesvc", "sum", &svc_sum_cb,
					ABUS_RPC_FLAG_NONE,
					"sumator cookie",
					"Compute summation of two integers",
					"a:i:first operand,b:i:second operand",
					"res_value:i:summation");
	if (ret != 0) {
		abus_cleanup(abus);
		return EXIT_FAILURE;
	}

	ret = abus_decl_method(abus, "examplesvc", "mult", &svc_mult_cb,
					ABUS_RPC_FLAG_NONE,
					"multiply cookie",
					"Compute multiplication of two integers",
					"a:i:first operand,b:i:second operand",
					"res_value:i:multiplication");
	if (ret != 0) {
		abus_cleanup(abus);
		return EXIT_FAILURE;
	}

	while (1) {
		fd_set rfds;
		int fd, retval;

		fd = abus_get_fd(abus);

		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);
		/* wait for an incoming message on the netlink socket */
		retval = select(fd+1, &rfds, NULL, NULL, NULL);

		if (retval) {
				/* FD_ISSET(fd, &rfds) will be true */
				abus_process_incoming(abus);
		}
	}

	abus_cleanup(abus);

	return EXIT_SUCCESS;
}

