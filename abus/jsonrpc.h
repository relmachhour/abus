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
#ifndef _JSONRPC_H
#define _JSONRPC_H

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/un.h>

#include "json.h"
#include "hashtab.h"

/* Invalid JSON was received by the server.
   An error occurred on the server while parsing the JSON text.
   TODO:
	-32701 ---> parse error. unsupported encoding
	-32702 ---> parse error. invalid character for encoding
	-32500 ---> application error
	-32400 ---> system error
	-32300 ---> transport error 
 */
#define JSONRPC_PARSE_ERROR -32700
#define JSONRPC_PARSE_ERROR_MSG "Parse error"
/* The JSON sent is not a valid Request object. */
#define JSONRPC_INVALID_REQUEST -32600
#define JSONRPC_INVALID_REQUEST_MSG "Invalid Request"
/* The method does not exist / is not available. */
#define JSONRPC_NO_METHOD -32601
#define JSONRPC_NO_METHOD_MSG "Method not found"
/* Invalid method parameter(s). */
#define JSONRPC_INVALID_METHOD -32602
#define JSONRPC_INVALID_METHOD_MSG "Invalid params"
/* Internal JSON-RPC error. */
#define JSONRPC_INTERNAL_ERROR -32603
#define JSONRPC_INTERNAL_ERROR_MSG "Internal error"
/* Reserved for implementation-defined server-errors. -32099 to -32000 */
#define JSONRPC_SERVER_ERROR -32099
#define JSONRPC_SERVER_ERROR_MSG "Server error"

#define JSON_LLINT 32765
#define JSON_ARRAY_HTAB 32766
typedef struct json_val {
	int type;
	int length;
	union {
		char *data;		/* malloc'ed memory */
		/* struct json_val **array; */
		htab **htab_array;	/* type=JSON_ARRAY_HTAB; malloc'ed memory */
		/* struct json_val_elem **object; */
	} u;
} json_val_t;

/** Max length of a JSON-RPC request */
#define JSONRPC_REQ_SZ_MAX 16000
/** Max length of a JSON-RPC response */
#define JSONRPC_RESP_SZ_MAX 16000
/** Max length of a service name, limited anyway by max AF_UNIX socket address(108) */
#define JSONRPC_SVCNAME_SZ_MAX 32
/** Max length of a method name, limited anyway by max AF_UNIX socket address(108) */
#define JSONRPC_METHNAME_SZ_MAX 64

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

typedef struct json_rpc {
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
} json_rpc_t;


/* internal use */
void json_val_free(json_val_t *json_val);
int json_rpc_init(json_rpc_t *json_rpc);
void json_rpc_cleanup(json_rpc_t *json_rpc);
int json_rpc_resp_init(json_rpc_t *json_rpc);
int json_rpc_resp_finalize(json_rpc_t *json_rpc);
int json_rpc_parse_msg(json_rpc_t *json_rpc, const char *buffer, size_t len);
int json_val_is_undef(const json_val_t *json_val);

/* service side */
int json_rpc_req_init(json_rpc_t *json_rpc, const char *service_name, const char *method_name, unsigned id);
int json_rpc_req_finalize(json_rpc_t *json_rpc);
int json_rpc_set_error(json_rpc_t *json_rpc, int error_code, const char *message);

/* both sides */
int json_rpc_get_type(json_rpc_t *json_rpc, const char *name);

int json_rpc_get_int(json_rpc_t *json_rpc, const char *name, int *val);
int json_rpc_get_llint(json_rpc_t *json_rpc, const char *name, long long *val);
int json_rpc_get_bool(json_rpc_t *json_rpc, const char *name, bool *val);
int json_rpc_get_double(json_rpc_t *json_rpc, const char *name, double *val);
int json_rpc_get_str(json_rpc_t *json_rpc, const char *name, char *val, size_t n);
int json_rpc_get_strn(json_rpc_t *json_rpc, const char *name, char *val, size_t *n);
int json_rpc_get_strp(json_rpc_t *json_rpc, const char *name, const char **pval, size_t *n);

int json_rpc_append_int(json_rpc_t *json_rpc, const char *name, int val);
int json_rpc_append_llint(json_rpc_t *json_rpc, const char *name, long long val);
int json_rpc_append_bool(json_rpc_t *json_rpc, const char *name, bool val);
int json_rpc_append_double(json_rpc_t *json_rpc, const char *name, double val);
int json_rpc_append_null(json_rpc_t *json_rpc, const char *name);
int json_rpc_append_strn(json_rpc_t *json_rpc, const char *name, const char *val, size_t n);
static inline int json_rpc_append_str(json_rpc_t *json_rpc, const char *name, const char *s) {
	return json_rpc_append_strn(json_rpc, name, s, strlen(s));
}
int json_rpc_append_args(json_rpc_t *json_rpc, ...);
int json_rpc_append_vargs(json_rpc_t *json_rpc, va_list ap);

int json_rpc_get_array_count(json_rpc_t *json_rpc, const char *name);
int json_rpc_get_point_at(json_rpc_t *json_rpc, const char *name, int idx);

#endif	/* _JSONRPC_H */
