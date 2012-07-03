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
#ifndef _ABUS_INTERNAL_H
#define _ABUS_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>

#include "abus.h"

#include "hashtab.h"
#include "jsonrpc_internal.h"

typedef struct abus_method {
	/* method name from htab key */
	abus_callback_t callback;
	void *arg;
	int flags;
	char *descr;
	char *fmt;
	char *result_fmt;
	pthread_mutex_t excl_mutex;	/* for ABUS_RPC_EXCL */
} abus_method_t;

typedef struct abus_event {
	/* event name from htab key. TODO: per subsr's flags */
	htab *subscriber_htab;	// uniq_subscriber_cnt->subscriber's sockaddr_un
	unsigned uniq_subscriber_cnt;
	char *descr;
	char *fmt;
} abus_event_t;

typedef struct abus_attr {
	/* attr name from htab key */
	json_val_t ref;
	int flags;
	char *descr;
	bool auto_alloc;
} abus_attr_t;

static inline int abus_method_is_threaded(const abus_method_t *method) { return method && (method->flags & ABUS_RPC_THREADED); }
static inline int abus_method_is_excl(const abus_method_t *method) { return method && (method->flags & ABUS_RPC_EXCL); }

typedef struct abus_service {
	/* service name from htab key */
	htab *method_htab;	// method name->abus_method_t

	htab *event_htab;	// event name->abus_event_t
	htab *attr_htab;	// attr name->abus_attr_t

	pthread_mutex_t attr_mutex;	/* for get/set */
} abus_service_t;

struct abus {
	/* service */
	htab *service_htab;	// service name->abus_service_t

	/* async request */
	htab *outstanding_req_htab;	// id string -> json_rpc_t

	pthread_t srv_thread;
	int sock;
	/* JSON RPC "id" field for requests */
	unsigned id;

	/* preallocated buffer, may be NULL */
	char *incoming_buffer;

	pthread_mutex_t mutex;

	abus_conf_t conf;
};

#ifdef __cplusplus
}
#endif	/* __cplusplus */

#endif	/* _ABUS_INTERNAL_H */
