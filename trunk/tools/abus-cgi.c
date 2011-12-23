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

#include "abus.h"

#define RPC_TIMEOUT 1000 /* ms */

/*
 * TODO: limit loop count (for memleak risk?)
 */

int main(int argc, char **argv)
{
	abus_t abus;
	char *buffer;
	int len, ret;

	buffer = malloc(JSONRPC_REQ_SZ_MAX);
	if (!buffer)
		exit(EXIT_FAILURE);

	abus_init(&abus);

	while (FCGI_Accept() >= 0) {

		len = fread(buffer, 1, JSONRPC_REQ_SZ_MAX, stdin);
		if (len <= 0) {
			len = sprintf(buffer, "{\"jsonrpc\": \"2.0\", \"error\": {\"code\": %d, \"message\": \""
							JSONRPC_PARSE_ERROR_MSG"\"}, \"id\":null}", JSONRPC_PARSE_ERROR);
			/* or punish broken request with break; ? */
		}
		else
		{
			printf("Content-type: application/json\r\n"
							"\r\n");
	
			ret = abus_forward_rpc(&abus, buffer, &len, 0, RPC_TIMEOUT);
	
			/* Forge JSON-RPC response in case of serious error */
			if (ret != 0)
				len = sprintf(buffer, "{\"jsonrpc\": \"2.0\", \"error\": {\"code\": %d, "
						"\"message\": \"abus-cgi internal error\"}, \"id\":null}",
						JSONRPC_INTERNAL_ERROR);
		}

		ret = fwrite(buffer, 1, len, stdout);
		if (ret <= 0) {
			/* or break; ? */
			continue;
		}
	}

	abus_cleanup(&abus);
	free(buffer);

	return EXIT_SUCCESS;
}
