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

#include "abus_config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <getopt.h>

#include "abus.h"

static int opt_timeout = 1000; /* ms */
static int opt_verbose;

static void print_basic_type(const char *prefix, const char *name, const json_val_t *val)
{
	switch (val->type) {
		case JSON_INT:
		case JSON_FLOAT:
			printf("%s%s=%*s\n", prefix, name, val->length, val->u.data);
			break;
		case JSON_STRING:
			printf("%s%s=\"%*s\"\n", prefix, name, val->length, val->u.data);
			break;
		case JSON_TRUE:
			printf("%s%s=true\n", prefix, name);
			break;
		case JSON_FALSE:
			printf("%s%s=false\n", prefix, name);
			break;
		case JSON_NULL:
			printf("%s%s=null\n", prefix, name);
			break;
		default:
			fprintf(stderr, "unknown type %d for param '%s'\n", val->type, name);
			exit(EXIT_FAILURE);
	}
}

static void async_print_all_cb(json_rpc_t *json_rpc, void *arg)
{
	/* Print all the result params */
	if (hfirst(json_rpc->params_htab)) do
	{
		char *name;
		json_val_t *val;

		name = (char *)hkey(json_rpc->params_htab);
		val = hstuff(json_rpc->params_htab);

		if (val->type == JSON_ARRAY_HTAB) {
			int i, array_count;
			array_count = json_rpc_get_array_count(json_rpc, name);
			for (i = 0; i < array_count; i++) {
				json_rpc_get_point_at(json_rpc, name, i);
				printf("%s[%d]:\n", name, i);
				if (hfirst(json_rpc->pointed_htab)) do
				{
					char *ha_name;
					json_val_t *ha_val;

					ha_name = (char *)hkey(json_rpc->pointed_htab);
					ha_val = hstuff(json_rpc->pointed_htab);

					print_basic_type("  ", ha_name, ha_val);
				}
				while (hnext(json_rpc->pointed_htab));
			}
		} else {
			print_basic_type("", name, val);
		}
	
	}
	while (hnext(json_rpc->params_htab));

}

int forward_rpc_stdinout(abus_t *abus)
{
	char *buffer;
	int len, ret;

	buffer = malloc(JSONRPC_REQ_SZ_MAX);
	if (!buffer)
		return -1;

	len = read(STDIN_FILENO, buffer, JSONRPC_REQ_SZ_MAX);
	if (len <= 0) {
		free(buffer);
		return len;
	}

	ret = abus_forward_rpc(abus, buffer, &len, 0, opt_timeout);

	/* TODO: forge JSON-RPC response in case of error */
	/* 	'{"jsonrpc": "2.0", "error": {"code": XXX, "message": "XXX"}, "id":XXX}' */

	if (!ret) {
		ret = write(STDOUT_FILENO, buffer, len);
		if (ret <= 0) {
			free(buffer);
			return -1;
		} else {
			ret = 0;
		}
	}

	free(buffer);
	return ret;
}

static int usage(const char *argv0, int exit_code)
{
	printf("usage: %s [options] SERVICE.METHOD [key:[bdis]]=value]...\n", argv0);
    printf(
    "  -h, --help                 this help message\n"
    "  -t, --timeout=TIMEOUT      timeout in milliseconds (%d)\n"
    "  -v, --verbose              verbose\n"
    "  -V, --version              version of A-Bus\n"
    "  -y, --async                asynchronous query\n"
    "  -w, --wait-async           wait for asynchronous query, without callback\n",
	opt_timeout);

	exit(exit_code);
}

int main(int argc, char **argv)
{
	abus_t abus;
	json_rpc_t jr, *json_rpc = &jr;
	char *service_name;
	char *method_name;
	int ret;
	int opt_async = 0;
	int opt_wait_async = 0;
	char *endptr;

	while (1) {
		int option_index;
		struct option long_options[] = {
			{ 0 },
		};
		int c = getopt_long(argc, argv, "ht:vywV", long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
		case 'h':
			usage(argv[0], EXIT_SUCCESS);
			break;
		case 't':
			opt_timeout = strtol(optarg, &endptr, 10);
			if (optarg == endptr)
				usage(argv[0], EXIT_FAILURE);
			break;
		case 'v':
			++opt_verbose;
			setenv("ABUS_MSG_VERBOSE", "1", 1);
			break;
		case 'y':
			opt_async = 1;
			break;
		case 'w':
			opt_wait_async = 1;
			break;
		case 'V':
			printf("%s: %s\n%s", argv[0], abus_get_version(), abus_get_copyright());
			exit(EXIT_SUCCESS);
			break;
		default:
			break;
		}
	}
	if (optind >= argc)
		usage(argv[0], EXIT_SUCCESS);


	abus_init(&abus);

	if (optind+1 == argc && !strcmp(argv[optind], "-")) {
		
		ret = forward_rpc_stdinout(&abus);

		abus_cleanup(&abus);

		return ret ? EXIT_FAILURE : EXIT_SUCCESS;
	}

	service_name = argv[optind];
	method_name = strchr(argv[optind], '.');
	if (method_name)
		*method_name++ = '\0';
	else
		method_name = "";

	if (!strcmp(method_name, "subscribe") && ++optind < argc) {
		ret = abus_event_subscribe(&abus, service_name, argv[optind], &async_print_all_cb, 0, "evt cookie", opt_timeout);
		sleep(10);
		ret = abus_event_unsubscribe(&abus, service_name, argv[optind], &async_print_all_cb, "evt cookie", opt_timeout);
		exit(EXIT_SUCCESS);
	}

	ret = abus_request_method_init(&abus, service_name, method_name, json_rpc);
	if (ret)
		exit(EXIT_FAILURE);


	if (!strcmp(method_name, "get")) {
		/* begin the array */
		json_rpc_append_args(json_rpc,
						JSON_KEY, "attr", -1,
						JSON_ARRAY_BEGIN,
						-1);

		while (++optind < argc) {
			json_rpc_append_args(json_rpc, JSON_OBJECT_BEGIN, -1);

			json_rpc_append_str(json_rpc, "name", argv[optind]);

			json_rpc_append_args(json_rpc, JSON_OBJECT_END, -1);
		}
		/* end the array */
		json_rpc_append_args(json_rpc,
						JSON_ARRAY_END,
						-1);
	}
	else
	{
		while (++optind < argc) {
			char *key, *type, *val, *endptr;
	
			key = argv[optind];
	
			type = strchr(key, ':');
			if (key[0] == '\0' || !type) {
				fprintf(stderr, "incomplete definition for key '%s'\n", key);
				usage(argv[0], EXIT_FAILURE);
			}
			*type++ = '\0';
			val = strchr(type, ':');
			if (!val || val[0] == '\0') {
				json_rpc_append_null(json_rpc, key);
				continue;
			}
			*val++ = '\0';
	
			switch(*type) {
			case 'i':
					json_rpc_append_int(json_rpc, key, strtol(val, &endptr, 0));
					if (val == endptr) {
						fprintf(stderr, "invalid format for key '%s'\n", key);
						usage(argv[0], EXIT_FAILURE);
					}
					break;
			case 'b':
					json_rpc_append_bool(json_rpc, key, !strcmp(val, "true"));
					break;
			case 'f':
					json_rpc_append_double(json_rpc, key, strtod(val, &endptr));
					if (val == endptr) {
						fprintf(stderr, "invalid format for key '%s'\n", key);
						usage(argv[0], EXIT_FAILURE);
					}
					break;
			case 's':
					json_rpc_append_str(json_rpc, key, val);
					break;
			default:
				fprintf(stderr, "unknown type '%s' for key '%s'\n", type, key);
				usage(argv[0], EXIT_FAILURE);
			}
		}
	}

	/* for unitary A-Bus testing purpose */
	if (opt_async) {
		/* no timeout for abus_request_method_invoke_async() */
		ret = abus_request_method_invoke_async(&abus, json_rpc,
							opt_wait_async ? 0 : opt_timeout,
							&async_print_all_cb, 0, "async cookie");
		if (ret != 0) {
			abus_request_method_cleanup(&abus, json_rpc);
			abus_cleanup(&abus);
			exit(EXIT_FAILURE);
		}
		if (opt_wait_async) {
			ret = abus_request_method_wait_async(&abus, json_rpc, opt_timeout);
		} else {
			sleep(2);
		}
	} else {
		ret = abus_request_method_invoke(&abus, json_rpc, 0, opt_timeout);
		if (ret != 0) {
			abus_request_method_cleanup(&abus, json_rpc);
			abus_cleanup(&abus);
			exit(EXIT_FAILURE);
		}
		async_print_all_cb(json_rpc, "printall cookie");
		abus_request_method_cleanup(&abus, json_rpc);
	}

	abus_cleanup(&abus);

	return EXIT_SUCCESS;
}

