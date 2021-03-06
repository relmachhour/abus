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

class CExamplesvc {
public:
	CExamplesvc(const char *foo) {
		m_foo = foo;
	}

	abus_decl_method_member(CExamplesvc, svc_sum_cb);
	abus_decl_method_member(CExamplesvc, svc_mult_cb);

private:
	const char *m_foo;
};

void CExamplesvc::svc_sum_cb(json_rpc_t *json_rpc)
{
	int a, b;
	int ret;

	ret  = json_rpc_get_int(json_rpc, "a", &a);
	if (ret == 0)
		ret = json_rpc_get_int(json_rpc, "b", &b);


	printf("## %s: arg=%s, ret=%d, a=%d, b=%d, => result=%d\n", __func__,
					m_foo, ret, a, b, a+b);

	if (ret)
		json_rpc_set_error(json_rpc, ret, NULL);
	else
		json_rpc_append_int(json_rpc, "res_value", a+b);
}

void CExamplesvc::svc_mult_cb(json_rpc_t *json_rpc)
{
	int a, b;
	int ret;

	ret  = json_rpc_get_int(json_rpc, "a", &a);
	if (ret == 0)
		ret = json_rpc_get_int(json_rpc, "b", &b);


	printf("## %s: arg=%s, ret=%d, a=%d, b=%d, => result=%d\n", __func__,
					m_foo, ret, a, b, a*b);

	if (ret)
		json_rpc_set_error(json_rpc, ret, NULL);
	else
		json_rpc_append_int(json_rpc, "res_value", a*b);
}

int main(int argc, char **argv)
{
	abus_t *abus;
	CExamplesvc *examplesvc = new CExamplesvc("foo");
	int ret;

	abus = abus_init(NULL);

	ret = abus_decl_method_cxx(abus, "examplesvc", "sum", examplesvc, svc_sum_cb,
					ABUS_RPC_FLAG_NONE,
					"Compute summation of two integers",
					"a:i:first operand,b:i:second operand",
					"res_value:i:summation");
	if (ret != 0) {
		abus_cleanup(abus);
		return EXIT_FAILURE;
	}

	ret = abus_decl_method_cxx(abus, "examplesvc", "mult", examplesvc, svc_mult_cb,
					ABUS_RPC_FLAG_NONE,
					"Compute multiplication of two integers",
					"a:i:first operand,b:i:second operand",
					"res_value:i:multiplication");
	if (ret != 0) {
		abus_cleanup(abus);
		return EXIT_FAILURE;
	}

	/* do other stuff */
	sleep(10000);

	delete examplesvc;

	abus_cleanup(abus);

	return EXIT_SUCCESS;
}

