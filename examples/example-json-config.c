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

#include <stdio.h>
#include <stdlib.h>

#include "json.h"

/* Example invocation:
 * $ ./example-json-config abus/libjson/tests/good/fileTestWrp.json
 * $ ./example-json-config abus/libjson/tests/good/fileTestWrp.json "" fake
 */

int main(int argc, char* argv[])
{
	json_dom_val_t *json_dom;
	const char *valItem = NULL;
	const char *item = "networking.ipaddress";
	int ret;

	if (argc < 2)
	{
		fprintf(stderr, " [ERROR] Usage: %s <json file> [item_query]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if (argc > 2)
		item = argv[3];

	/* Load and parse the json file */
	json_dom = json_config_open(argv[1]);
	if (NULL == json_dom)
	{
		fprintf(stderr, " [ERROR] JSON init and converted into a DOM : NOK\n");
		return EXIT_FAILURE;
	}

	printf(" [DBG] JSON init and converted into a DOM : OK\n");

	ret = json_config_get_direct_strp(json_dom, item, &valItem, NULL);
	if (ret == 0)
	{
		printf("Item's value %s = '%s'\n", item, valItem);
	}
	else
	{
		printf(" [ERROR] Item %s not found or invalid type (%d)\n", item, ret);
	}

	/* Cleaning the dom */
	if (NULL != json_dom)
		json_config_cleanup(json_dom);

	return EXIT_SUCCESS;
}

