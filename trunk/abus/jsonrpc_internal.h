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
#ifndef _JSONRPC_INTERNAL_H
#define _JSONRPC_INTERNAL_H

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/un.h>

#include "jsonrpc.h"
#include "json.h"
#include "hashtab.h"

#define JSON_LLINT 32765
#define JSON_ARRAY_HTAB 32766

typedef struct json_val {
	int type;
	size_t length;
	union {
		char *data;		/* malloc'ed memory */
		/* struct json_val **array; */
		htab **htab_array;	/* type=JSON_ARRAY_HTAB; malloc'ed memory */
		/* struct json_val_elem **object; */
	} u;
} json_val_t;

enum json_rpc_key_token_e {
		TOK_NONE = 0,
		TOK_UNKNOWN,
		/* sorted, to pinpoint same first letter */
		TOK_CODE,
		TOK_DATA,
		TOK_ERROR,
		TOK_ID,
		TOK_JSONRPC,
		TOK_METHOD,
		TOK_MESSAGE,
		TOK_PARAMS,
		TOK_RESULT,
};

struct json_rpc {
	char *msgbuf;	/* req or resp */
	int msglen;
	int msgbufsz;

	/* request */
	char *service_name;
	char *method_name;
	struct json_val id;
	htab *params_htab;	/* list of params keys->char* */
	htab *pointed_htab;	/* array alias */
	int sock;
	void *cb_context;	/* method or response handler */
	void *async_req_context;	/* async req in response rpc */
	const char *evt_service_name;	/* event only */

	pthread_cond_t cond;
	pthread_mutex_t mutex;

	/* response */
	int error_code;

	struct sockaddr_un sock_src_addr;
	socklen_t sock_addrlen;

	/* parsing stuff */
	bool param_state;
	bool error_token_seen;
	int nesting_level;
	enum json_rpc_key_token_e last_key_token;
	enum { PARSING_UNKNOWN, PARSING_V2_0, PARSING_INVALID, PARSING_OK } parsing_status;
	char *last_param_key;
	json_val_t *last_array;
};


/* internal use */
void json_val_free(json_val_t *json_val);
json_rpc_t *json_rpc_init(void);
void json_rpc_cleanup(json_rpc_t *json_rpc);
int json_rpc_resp_init(json_rpc_t *json_rpc);
int json_rpc_resp_finalize(json_rpc_t *json_rpc);
int json_rpc_parse_msg(json_rpc_t *json_rpc, const char *buffer, size_t len);
int json_rpc_type_eq(int type1, int type2);
int json_val_is_undef(const json_val_t *json_val);
int json_rpc_add_val(json_rpc_t *json_rpc, int type, const char *data, size_t length);
int json_rpc_add_array(json_rpc_t *json_rpc);
int json_rpc_add_object_to_array(json_rpc_t *json_rpc);
int abus_check_valid_service_name(const char *name, size_t maxlen);

/* service side */
json_rpc_t *json_rpc_req_init(const char *service_name, const char *method_name, unsigned id);
int json_rpc_req_finalize(json_rpc_t *json_rpc);

#endif	/* _JSONRPC_INTERNAL_H */
