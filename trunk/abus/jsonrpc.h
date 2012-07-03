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
#include <stdarg.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

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


/** Max length of a JSON-RPC request */
#define JSONRPC_REQ_SZ_MAX 16000
/** Max length of a JSON-RPC response */
#define JSONRPC_RESP_SZ_MAX 16000
/** Max length of a service name, limited anyway by max AF_UNIX socket address(108) */
#define JSONRPC_SVCNAME_SZ_MAX 32
/** Max length of a method name, limited anyway by max AF_UNIX socket address(108) */
#define JSONRPC_METHNAME_SZ_MAX 64

/* Opaque json rpc */
struct json_rpc;
typedef struct json_rpc json_rpc_t;

/* service side */
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

const char *json_rpc_strerror(int errnum);

#ifdef __cplusplus
}
#endif

#endif	/* _JSONRPC_H */
