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

#include "abus_config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>

#include "abus.h"
#include "jsonrpc_internal.h"

static int opt_verbose;
static int opt_rpc_threaded;

static volatile int terminate;

static int print_rpc_args(json_rpc_t *json_rpc, char *arg, size_t n);

static void svc_terminate_cb(json_rpc_t *json_rpc, void *arg)
{
	terminate = 1;
}

static void svc_rpc_caller(json_rpc_t *json_rpc, void *arg)
{
	const char *method_command = (const char *)arg;
	int ret;
	char buf[256] = "";

	/* TODO export env variables */
	ret = print_rpc_args(json_rpc, buf, sizeof(buf));

	snprintf(buf+ret, sizeof(buf)-ret, "%s %s", method_command, json_rpc->method_name);
	buf[sizeof(buf)-1] = '\0';

	ret = system(buf);

	if (ret || WEXITSTATUS(ret))
		json_rpc_set_error(json_rpc, ret ? ret : WEXITSTATUS(ret), NULL);
}

static int usage(const char *argv0, int exit_code)
{
	printf("usage: %s [options] SERVICE [METHOD COMMAND]...\n", argv0);
	printf(
			"  -h              this help message\n"
			"  -T              let methods to run in parallel\n"
			"  -v              verbose\n"
			"  -V              version of A-Bus\n"
		  );

	exit(exit_code);
}

int main(int argc, char **argv)
{
	abus_t *abus;
	const char *service_name;
	int ret, opt;

	while ((opt = getopt(argc, argv, "hvVT")) != -1) {
		switch (opt) {
		case 'h':
			usage(argv[0], EXIT_SUCCESS);
			break;
		case 'v':
			++opt_verbose;
			setenv("ABUS_MSG_VERBOSE", "1", 1);
			break;
		case 'T':
			opt_rpc_threaded = 1;
			break;
		case 'V':
			printf("%s: %s\n%s", argv[0], abus_get_version(), abus_get_copyright());
			exit(EXIT_SUCCESS);
			break;
		default:
			break;
		}
	}
	if (optind+3 > argc)
		usage(argv[0], EXIT_FAILURE);

	abus = abus_init(NULL);

	service_name = argv[optind++];

	while (optind+2 <= argc) {
		const char *method_name = argv[optind++];
		const char *method_command = argv[optind++];

		ret = abus_decl_method(abus, service_name,
					method_name,
					&svc_rpc_caller,
					opt_rpc_threaded ? ABUS_RPC_THREADED : ABUS_RPC_FLAG_NONE,
					(void*)method_command,
					NULL, NULL, NULL);
		if (ret != 0) {
			fprintf(stderr, "A-Bus method declaration failed: %s\n", abus_strerror(ret));
			abus_cleanup(abus);
			exit(EXIT_FAILURE);
		}
	}

	ret = abus_decl_method(abus, service_name,
				"terminate",
				&svc_terminate_cb,
				ABUS_RPC_FLAG_NONE,
				NULL,
				"Terminate service", NULL, NULL);
	if (ret != 0) {
		abus_cleanup(abus);
		exit(EXIT_FAILURE);
	}

	/* do other stuff */
	while(!terminate) {
		sleep(1);
	}

	abus_cleanup(abus);

	return EXIT_SUCCESS;
}


static int print_basic_type(char *s, size_t n, const char *name, const json_val_t *val)
{
	/* TODO: escape JSONRPC args wrt to shell injection */

	switch (val->type) {
		case JSON_INT:
		case JSON_LLINT:
		case JSON_FLOAT:
			return snprintf(s, n, "%s=%*s ", name, (int)val->length, val->u.data);
		case JSON_STRING:
			return snprintf(s, n, "%s='%*s' ", name, (int)val->length, val->u.data);
		case JSON_TRUE:
			return snprintf(s, n, "%s=true ", name);
		case JSON_FALSE:
			return snprintf(s, n, "%s=false ", name);
		case JSON_NULL:
			return snprintf(s, n, "%s= ", name);
		default:
			fprintf(stderr, "unknown type %d for param '%s'\n", val->type, name);
	}
	return 0;
}

int print_rpc_args(json_rpc_t *json_rpc, char *s, size_t n)
{
	int ret;
	size_t rem = n;

	if (hfirst(json_rpc->params_htab)) do
	{
		const char *name;
		json_val_t *val;

		name = (const char *)hkey(json_rpc->params_htab);
		val = hstuff(json_rpc->params_htab);

		ret = print_basic_type(s, rem, name, val);

		s += ret;
		rem -= ret;
	}
	while (hnext(json_rpc->params_htab) && rem > 0);

	return n-rem;
}

