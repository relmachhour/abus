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
#ifndef _ABUS_H
#define _ABUS_H

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>

#include "jsonrpc.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*abus_callback_t)(json_rpc_t *json_rpc, void *arg);

#define ABUS_RPC_FLAG_NONE	0x00
#define ABUS_RPC_THREADED	0x01
#define ABUS_RPC_EXCL		0x02
#define ABUS_RPC_RDONLY		0x04
#define ABUS_RPC_WITHOUTVAL	0x08
#define ABUS_RPC_ASYNC		0x40	/* internal use */
#define ABUS_RPC_CONST		0x80
/* TODO flags:
	VISIBILITY: process, network, default: host ?
	NO_REPLY?
 */

typedef struct abus_conf {
	/** don't want A-Bus system thread */
	bool poll_operation;

} abus_conf_t;

/* Opaque abus stuff */
struct abus;
typedef struct abus abus_t;

/* user API */
const char *abus_get_version();
const char *abus_get_copyright();

abus_t *abus_init(const abus_conf_t *conf);
int abus_cleanup(abus_t *abus);
int abus_get_conf(abus_t *abus, abus_conf_t *conf);
int abus_set_conf(abus_t *abus, const abus_conf_t *conf);

int abus_decl_method(abus_t *abus, const char *service_name, const char *method_name, abus_callback_t method_callback, int flags, void *arg, const char *descr, const char *fmt, const char *result_fmt);
int abus_undecl_method(abus_t *abus, const char *service_name, const char *method_name);

int abus_get_fd(abus_t *abus);
int abus_process_incoming(abus_t *abus);

/* synchronous call */

json_rpc_t *abus_request_method_init(abus_t *abus, const char *service_name, const char *method_name);
int abus_request_method_invoke(abus_t *abus, json_rpc_t *json_rpc, int flags, int timeout);
int abus_request_method_cleanup(abus_t *abus, json_rpc_t *json_rpc);

/* TODO: int abus_request_method_invoke_args(abus_t *abus, const char *service_name, const char *method_name, int flags, int timeout, ...); */

/* asynchronous call, with callback upon response or timeout; callback can be threaded or not */
int abus_request_method_invoke_async(abus_t *abus, json_rpc_t *json_rpc, int timeout, abus_callback_t callback, int flags, void *arg);
int abus_request_method_wait_async(abus_t *abus, json_rpc_t *json_rpc, int timeout);
int abus_request_method_cancel_async(abus_t *abus, json_rpc_t *json_rpc);

/* publish/subscribe */
int abus_decl_event(abus_t *abus, const char *service_name, const char *event_name, const char *descr, const char *fmt);
int abus_undecl_event(abus_t *abus, const char *service_name, const char *event_name);
json_rpc_t *abus_request_event_init(abus_t *abus, const char *service_name, const char *event_name);
int abus_request_event_publish(abus_t *abus, json_rpc_t *json_rpc, int flags);
int abus_request_event_cleanup(abus_t *abus, json_rpc_t *json_rpc);
int abus_event_subscribe(abus_t *abus, const char *service_name, const char *event_name, abus_callback_t callback, int flags, void *arg, int timeout);
int abus_event_unsubscribe(abus_t *abus, const char *service_name, const char *event_name, abus_callback_t callback, void *arg, int timeout);

/* attributes/data model service side*/
int abus_decl_attr_int(abus_t *abus, const char *service_name, const char *attr_name, int *val, int flags, const char *descr);
int abus_decl_attr_llint(abus_t *abus, const char *service_name, const char *attr_name, long long *val, int flags, const char *descr);
int abus_decl_attr_bool(abus_t *abus, const char *service_name, const char *attr_name, bool *val, int flags, const char *descr);
int abus_decl_attr_double(abus_t *abus, const char *service_name, const char *attr_name, double *val, int flags, const char *descr);
int abus_decl_attr_str(abus_t *abus, const char *service_name, const char *attr_name, char *val, size_t n, int flags, const char *descr);
int abus_undecl_attr(abus_t *abus, const char *service_name, const char *attr_name);
int abus_attr_changed(abus_t *abus, const char *service_name, const char *attr_name);
int abus_append_attr(abus_t *abus, json_rpc_t *json_rpc, const char *service_name, const char *attr_name);

/* attributes/data model client side*/
int abus_attr_get_int(abus_t *abus, const char *service_name, const char *attr_name, int *val, int timeout);
int abus_attr_get_llint(abus_t *abus, const char *service_name, const char *attr_name, long long *val, int timeout);
int abus_attr_get_bool(abus_t *abus, const char *service_name, const char *attr_name, bool *val, int timeout);
int abus_attr_get_double(abus_t *abus, const char *service_name, const char *attr_name, double *val, int timeout);
int abus_attr_get_str(abus_t *abus, const char *service_name, const char *attr_name, char *val, size_t n, int timeout);

int abus_attr_set_int(abus_t *abus, const char *service_name, const char *attr_name, int val, int timeout);
int abus_attr_set_llint(abus_t *abus, const char *service_name, const char *attr_name, long long val, int timeout);
int abus_attr_set_bool(abus_t *abus, const char *service_name, const char *attr_name, bool val, int timeout);
int abus_attr_set_double(abus_t *abus, const char *service_name, const char *attr_name, double val, int timeout);
int abus_attr_set_str(abus_t *abus, const char *service_name, const char *attr_name, const char *val, int timeout);

int abus_attr_subscribe_onchange(abus_t *abus, const char *service_name, const char *attr_name, abus_callback_t callback, int flags, void *arg, int timeout);
int abus_attr_unsubscribe_onchange(abus_t *abus, const char *service_name, const char *attr_name, abus_callback_t callback, void *arg, int timeout);

static inline const char *abus_strerror(int errnum) { return json_rpc_strerror(errnum); }

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
#define abus_decl_method_cxx(_abus, _service_name, _method_name, _obj, _method, _flags, descr, fmt, res_fmt) \
		abus_decl_method((_abus), (_service_name), (_method_name), &(_obj)->_method##Wrapper, (_flags), (void *)(_obj), descr, fmt, res_fmt)

#define abus_request_method_invoke_async_cxx(_abus, _json_rpc, _timeout, _obj, _method, _flags) \
		abus_request_method_invoke_async((_abus), (_json_rpc), (_timeout), &(_obj)->_method##Wrapper, (_flags), (void *)(_obj))

#define abus_event_subscribe_cxx(_abus, _service_name, _event_name, _obj, _method, _flags, _timeout) \
		abus_event_subscribe((_abus), (_service_name), (_event_name), &(_obj)->_method##Wrapper, (_flags), (void *)(_obj), (_timeout))
#define abus_event_unsubscribe_cxx(_abus, _service_name, _event_name, _obj, _method, _timeout) \
		abus_event_unsubscribe((_abus), (_service_name), (_event_name), &(_obj)->_method##Wrapper, (void *)(_obj), (_timeout))

#define abus_attr_subscribe_onchange_cxx(_abus, _service_name, _attr_name, _obj, _method, _flags, _timeout) \
		abus_attr_subscribe_onchange((_abus), (_service_name), (_attr_name), &(_obj)->_method##Wrapper, (_flags), (void *)(_obj), (_timeout))
#define abus_attr_unsubscribe_onchange_cxx(_abus, _service_name, _attr_name, _obj, _method, _timeout) \
		abus_attr_unsubscribe_onchange((_abus), (_service_name), (_attr_name), &(_obj)->_method##Wrapper, (void *)(_obj), (_timeout))

#endif	/* __cplusplus */

#endif	/* _ABUS_H */
