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
#ifndef _ABUS_H
#define _ABUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>

#include "hashtab.h"
#include "jsonrpc.h"

typedef void (*abus_callback_t)(json_rpc_t *json_rpc, void *arg);

#define ABUS_RPC_FLAG_NONE	0x00
#define ABUS_RPC_THREADED	0x01
#define ABUS_RPC_EXCL		0x02
/* TODO flags for VISIBILITY: process, network, default: host */

typedef struct abus_method {
	/* method name from htab key */
	abus_callback_t callback;
	void *arg;
	int flags;
	char *fmt;
	char *result_fmt;
	pthread_mutex_t excl_mutex;	/* for ABUS_RPC_EXCL */
} abus_method_t;

typedef struct abus_event {
	/* event name from htab key */
	htab *subscriber_htab;	// uniq_subscriber_cnt->subscriber's sockaddr_un
	unsigned uniq_subscriber_cnt;
	char *fmt;
} abus_event_t;

static inline int abus_method_is_threaded(const abus_method_t *method) { return method && (method->flags & ABUS_RPC_THREADED); }
static inline int abus_method_is_excl(const abus_method_t *method) { return method && (method->flags & ABUS_RPC_EXCL); }

typedef struct abus_service {
	/* service name from htab key */
	htab *method_htab;	// method name->abus_method_t

	htab *event_htab;	// event name->abus_event_t
} abus_service_t;

typedef struct abus {
	/* service */
	htab *service_htab;	// service name->abus_service_t

	/* async request */
	htab *outstanding_req_htab;	// id string -> json_rpc_t

	pthread_t srv_thread;
	int sock;
	unsigned id;

	pthread_mutex_t mutex;
} abus_t;

/* user API */
const char *abus_get_version();
const char *abus_get_copyright();

int abus_init(abus_t *abus);
int abus_cleanup(abus_t *abus);

int abus_decl_method(abus_t *abus, const char *service_name, const char *method_name, abus_callback_t method_callback, int flags, void *arg, const char *fmt, const char *result_fmt);
int abus_undecl_method(abus_t *abus, const char *service_name, const char *method_name);

/* synchronous call */

int abus_request_method_init(abus_t *abus, const char *service_name, const char *method_name, json_rpc_t *json_rpc);
int abus_request_method_invoke(abus_t *abus, json_rpc_t *json_rpc, int flags, int timeout);
int abus_request_method_cleanup(abus_t *abus, json_rpc_t *json_rpc);

/* TODO: int abus_request_method_invoke_args(abus_t *abus, const char *service_name, const char *method_name, int flags, int timeout, ...); */

/* asynchronous call, with callback upon response or timeout; callback can be threaded or not */
int abus_request_method_invoke_async(abus_t *abus, json_rpc_t *json_rpc, int timeout, abus_callback_t callback, int flags, void *arg);
int abus_request_method_wait_async(abus_t *abus, json_rpc_t *json_rpc, int timeout);
/* TODO: abus_request_method_cancel_async() ? */

/* publish/subscribe */
int abus_decl_event(abus_t *abus, const char *service_name, const char *event_name, const char *fmt);
/* TODO: int abus_undecl_event(abus_t *abus, const char *service_name, const char *event_name); */
int abus_request_event_init(abus_t *abus, const char *service_name, const char *event_name, json_rpc_t *json_rpc);
int abus_request_event_publish(abus_t *abus, json_rpc_t *json_rpc, int flags);
int abus_request_event_cleanup(abus_t *abus, json_rpc_t *json_rpc);
int abus_event_subscribe(abus_t *abus, const char *service_name, const char *event_name, abus_callback_t callback, int flags, void *arg, int timeout);
int abus_event_unsubscribe(abus_t *abus, const char *service_name, const char *event_name, abus_callback_t callback, void *arg, int timeout);

/* Fast/CGI helper */
int abus_forward_rpc(abus_t *abus, char *buffer, int *buflen, int flags, int timeout);

#ifdef __cplusplus
}

/* Some mild sugar for declaring C++ methods as callback */

/* To be used in public class declaration */
#define abus_decl_method_member(_cl, _method) \
	static void _method##Wrapper(json_rpc_t *json_rpc, void *arg) \
		{ \
			_cl *_pObj = (_cl *) arg; \
			_pObj->_method(json_rpc); \
		} \
	void _method(json_rpc_t *json_rpc)

/* To be used instead of abus_decl_method() */
#define abus_decl_method_cxx(_abus, _service_name, _method_name, _obj, _method, _flags, fmt, res_fmt) \
		abus_decl_method((_abus), (_service_name), (_method_name), &(_obj)->_method##Wrapper, (_flags), (void *)(_obj), fmt, res_fmt)

#endif	/* __cplusplus */

#endif	/* _ABUS_H */
