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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "abus.h"

static void chomp(char *s)
{
	int len = strlen(s);

	if (len > 0 && s[len-1] == '\n')
		s[len-1] = '\0';
}

int main(int argc, char **argv)
{
	abus_t abus;
	char s[128];

	abus_init(&abus);

#define MYSVCNAME "examplesvc"
#define MYEVTNAME "enter_pressed"
	abus_decl_event(&abus, MYSVCNAME, MYEVTNAME,
					"Event sent each time the ENTER key is press. Serves as publish/subscribe example.",
					"typed_char:s:keys pressed before the ENTER key");

	/* cheap event generator: press ENTER key on stdin
	   Attached to the event, the chars typed in before the ENTER key
	 */

	while (fgets(s, sizeof(s), stdin) != NULL) {
		json_rpc_t json_rpc;

		chomp(s);
		abus_request_event_init(&abus, MYSVCNAME, MYEVTNAME, &json_rpc);
		json_rpc_append_str(&json_rpc, "typed_char", s);
		abus_request_event_publish(&abus, &json_rpc, 0);
		abus_request_event_cleanup(&abus, &json_rpc);
	}

	abus_cleanup(&abus);

	return EXIT_SUCCESS;
}

