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

/*
 * CGI/FastCGI wrapper for the A-Bus
 * Needs pkg libfcgi-dev
 */

#include <fcgi_stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "abus.h"

#define RPC_TIMEOUT 1000 /* ms */

/*
 * TODO: limit loop count (for memleak risk?)
 */

int main(int argc, char **argv)
{
	abus_t abus;
	char *buffer;
	int len, ret, opt;
	int timeout = RPC_TIMEOUT;
	size_t bufsz = JSONRPC_REQ_SZ_MAX;

	while ((opt = getopt(argc, argv, "t:b:")) != -1) {
		switch (opt) {
			case 't':
				timeout = atoi(optarg);
				break;
			case 'b':
				bufsz = atoi(optarg);
				break;
			default:
				exit(EXIT_FAILURE);
		}
	}

	buffer = malloc(bufsz);
	if (!buffer)
		exit(EXIT_FAILURE);

	abus_init(&abus);

	while (FCGI_Accept() >= 0) {

		len = fread(buffer, 1, bufsz-1, stdin);

		if (len <= 0) {
			len = sprintf(buffer, "{\"jsonrpc\": \"2.0\", \"error\": {\"code\": %d, \"message\": \""
							JSONRPC_PARSE_ERROR_MSG"\"}, \"id\":null}", JSONRPC_PARSE_ERROR);
			/* or punish broken request with break; ? */
		}
		else
		{
			buffer[len] = '\0';

			ret = abus_forward_rpc(&abus, buffer, &len, 0, RPC_TIMEOUT);
	
			/* Forge JSON-RPC response in case of serious error */
			if (ret != 0)
				len = sprintf(buffer, "{\"jsonrpc\": \"2.0\", \"error\": {\"code\": %d, "
						"\"message\": \"abus-cgi internal error\"}, \"id\":null}",
						JSONRPC_INTERNAL_ERROR);
		}

		printf("Content-type: application/json\r\n"
						"\r\n");

		ret = fwrite(buffer, len, 1, stdout);
		if (ret != 1)
			break;
	}

	abus_cleanup(&abus);
	free(buffer);

	return EXIT_SUCCESS;
}
