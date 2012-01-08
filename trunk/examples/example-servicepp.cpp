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

#include <iostream>
#include <unistd.h>

/* Use the C++ wrapper */
#include "abus.hpp"

class CExamplesvc {
public:
	CExamplesvc(const char *foo) {
		m_foo = foo;
	}

	/* make use of macro to declare C++ friendly callbacks */
	abus_declpp_method_member(CExamplesvc, svc_sum_cb);
	abus_declpp_method_member(CExamplesvc, svc_mult_cb);

private:
	const char *m_foo;
};


void CExamplesvc::svc_sum_cb(cABusRPC *svc_rpc)
{
	int a, b;
	int ret;

	ret  = svc_rpc->get_int("a", &a);
	if (ret == 0)
		ret = svc_rpc->get_int("b", &b);


	std::cout << "## " << __func__ << ": arg=" << m_foo << ", ret=" << ret 
			<< ", a=" << a << ", b=" << b << ", => result=" << a+b << std::endl;

	if (ret)
		svc_rpc->set_error(ret);
	else
		svc_rpc->append_int("res_value", a+b);
}

void CExamplesvc::svc_mult_cb(cABusRPC *svc_rpc)
{
	int a, b;
	int ret;

	ret  = svc_rpc->get_int("a", &a);
	if (ret == 0)
		ret = svc_rpc->get_int("b", &b);


	std::cout << "## " << __func__ << ": arg=" << m_foo << ", ret=" << ret 
			<< ", a=" << a << ", b=" << b << ", => result=" << a*b << std::endl;

	if (ret)
		svc_rpc->set_error(ret);
	else
		svc_rpc->append_int("res_value", a*b);
}

int main(int argc, char **argv)
{
	cABus *abus;
	CExamplesvc *examplesvc = new CExamplesvc("foo");
	int ret;

	abus = new cABus();

	ret = abus->declpp_method("examplesvc", "sum", examplesvc, svc_sum_cb,
					ABUS_RPC_FLAG_NONE,
					"Compute summation of two integers",
					"a:i:first operand,b:i:second operand",
					"res_value:i:summation");
	if (ret != 0) {
		delete abus;
		return EXIT_FAILURE;
	}

	ret = abus->declpp_method("examplesvc", "mult", examplesvc, svc_mult_cb,
					ABUS_RPC_FLAG_NONE,
					"Compute multiplication of two integers",
					"a:i:first operand,b:i:second operand",
					"res_value:i:multiplication");
	if (ret != 0) {
		delete abus;
		return EXIT_FAILURE;
	}

	/* do other stuff */
	sleep(10000);

	delete examplesvc;
	delete abus;

	return EXIT_SUCCESS;
}

