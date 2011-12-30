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

/*!
 * \addtogroup abus
 * @{
 */
#include "abus_config.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <pthread.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "hashtab.h"
#include "jsonrpc.h"
#include "abus.h"

#define LogError(...)    do { fprintf(stderr, ##__VA_ARGS__); fprintf(stderr, "\n"); } while (0)
#define LogDebug(...)    do { fprintf(stderr, ##__VA_ARGS__); fprintf(stderr, "\n"); } while (0)

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif

#define ABUS_INTROSPECT_METHOD "*"
#define ABUS_SUBSCRIBE_METHOD "subscribe"
#define ABUS_UNSUBSCRIBE_METHOD "unsubscribe"
#define ABUS_EVENT_METHOD "event"

static const char *abus_prefix = "/tmp/abus";

static void *abus_thread_routine(void *arg);

static int create_service_path(abus_t *abus, const char *service_name);
static int remove_service_path(abus_t *abus, const char *service_name);
static void abus_req_introspect_service_cb(json_rpc_t *json_rpc, void *arg);
static void abus_req_subscribe_service_cb(json_rpc_t *json_rpc, void *arg);
static void abus_req_unsubscribe_service_cb(json_rpc_t *json_rpc, void *arg);
static int abus_unsubscribe_service(abus_t *abus, const char *service_name, const char *event_name);
static int abus_process_msg(abus_t *abus, const char *buffer, int len, json_rpc_t *json_rpc, struct sockaddr_un *sock_src_addr, socklen_t sock_addrlen);

/*!
  \def ABUS_RPC_FLAG_NONE
  \brief A-Bus service method empty flag
 */
/*!
  \def ABUS_RPC_THREADED
  \brief A-Bus service method flag requesting the callback to be run in its own thread
 */
/*!
  \def ABUS_RPC_EXCL
  \brief A-Bus service method flag requesting to guarantee only one outstanding callback at a time
 */
/*!
  \var typedef void (*abus_callback_t)(json_rpc_t *json_rpc, void *arg)
  \brief A-Bus callback type definition
 */

/*!
 * Get version of A-Bus library
  \return pointer to a statically located string of the A-Bus version
 */
const char *abus_get_version()
{
	static const char abus_version[] = "A-Bus " PACKAGE_VERSION;

	return abus_version;
}

/*!
 * Get copyright information of A-Bus library
  \return pointer to a statically located string of the A-Bus copyright
 */
const char *abus_get_copyright()
{
	static const char abus_copyright[] =
  "Copyright (C) 2011 Stephane Fillod\n"
  "Copyright (C) 1996 Bob Jenkins (hashtab)\n"
  "Copyright (C) 2009-2011 Vincent Hanquez (libjson)\n"
  "This is free software; see the source for copying conditions.  There is NO\n"
  "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n";

	return abus_copyright;
}

static int create_un_sock(void)
{
	struct sockaddr_un sockaddrun;
	int sock;
	int reuse_addr = 1;

	sock = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (sock < 0) {
		LogError("%s: failed to bind server socket: %s", __func__, strerror(errno));
		return sock;
	}

	memset(&sockaddrun, 0, sizeof(sockaddrun));
	sockaddrun.sun_family = AF_UNIX;

	/* TODO: prefix from env variable */
	snprintf(sockaddrun.sun_path, sizeof(sockaddrun.sun_path)-1,
					"%s/_%d", abus_prefix, getpid());

	if (bind(sock, (struct sockaddr *) &sockaddrun, SUN_LEN(&sockaddrun)) < 0)
	{
		LogError("%s: failed to bind server socket: %s", __func__, strerror(errno));
		close(sock);
		return -1;
	}

	/* So that we can re-bind to it without TIME_WAIT problems */
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse_addr, sizeof(reuse_addr)) < 0)
	{
		LogError("%s: failed to set SO_REUSEADDR option on server socket: %s", __func__, strerror(errno));
		close(sock);
		return -1;
	}

	return sock;
}

static int close_un_sock(abus_t *abus)
{
	char pid_path[UNIX_PATH_MAX];

	if (abus->sock == -1)
		return 0;

	close(abus->sock);

	/* TODO: prefix from env variable */
	snprintf(pid_path, sizeof(pid_path)-1, "%s/_%d", abus_prefix, getpid());

	unlink(pid_path);

	return 0;
}

/*!
	Initialize for A-Bus operation

	The A-Bus thread may not be created if only using synchronous abus_request_method_invoke().

  \param abus pointer to an opaque handle for A-Bus operation
  \return   0 if successful, non nul value otherwise
 */
int abus_init(abus_t *abus)
{
	int ret;

	memset(abus, 0, sizeof(abus_t));

	pthread_mutex_init(&abus->mutex, NULL);

	/* make sure A-bus directory exist before creating socket */
	ret = mkdir(abus_prefix, 0777);
	if (ret == -1 && errno != EEXIST) {
		LogError("a-bus mkdir '%s' failed: %s\n", abus_prefix, strerror(errno));
		return -1;
	}

	abus->sock = -1;

#if 0
	/* TODO */
	on_exit(&abus_dtor, abus);
#endif

	return 0;
}

/*
   Lazy creation of socket and startup of A-Bus thread.
 */
static int abus_launch_thread_ondemand(abus_t *abus)
{
	int ret;

	/* launched already ? */
	if (abus->sock != -1)
		return 0;

	abus->sock = create_un_sock();
	if (abus->sock < 0)
		return -1;

	/*
	 TODO: a special for client-only synchronous usage, without a-bus thread ?
	 */
	ret = pthread_create(&abus->srv_thread, NULL, &abus_thread_routine, abus);
	if (ret != 0)
	{
		LogError("%s: pthread_create() failed: %s", __func__, strerror(ret));
		return -ret;
	}
	return 0;
}

/*!
	Cleanup of A-Bus operation, releases any allocated memory.

	Note: subscribed events are not unsubscribed from the services.

  \param abus pointer to an opaque handle for A-Bus operation
  \return   0 if successful, non nul value otherwise
 */
int abus_cleanup(abus_t *abus)
{
	/* TODO: stop abus thread */
	pthread_cancel(abus->srv_thread);

	close_un_sock(abus);

	/* delete service method_htab */
	if (abus->service_htab) {
		if (hfirst(abus->service_htab)) do
		{
			abus_service_t *service = hstuff(abus->service_htab);

			/* delete method_htab */
			if (service->method_htab) {
				if (hfirst(service->method_htab)) do
				{
					abus_method_t *method;
					free(hkey(service->method_htab));
					method = hstuff(service->method_htab);
					if (method->fmt)
						free(method->fmt);
					if (method->result_fmt)
						free(method->result_fmt);
					free(method);
				}
				while (hnext(service->method_htab));
				hdestroy(service->method_htab);
			}

			/* delete event_htab */
			if (service->event_htab) {
				if (hfirst(service->event_htab)) do
				{
					abus_event_t *event = hstuff(service->event_htab);
					/* delete event_htab */
					if (hfirst(event->subscriber_htab)) do
					{
						/* TODO: unsubscribe from services ? */
						free(hkey(event->subscriber_htab));
						free(hstuff(event->subscriber_htab));
					}
					while (hnext(event->subscriber_htab));

					hdestroy(event->subscriber_htab);
					free(hkey(service->event_htab));
					free(hstuff(service->event_htab));
				}
				while (hnext(service->event_htab));
				hdestroy(service->event_htab);
			}

			remove_service_path(abus, (const char*)hkey(abus->service_htab));
			free(hkey(abus->service_htab));
			free(hstuff(abus->service_htab));
		}
		while (hnext(abus->service_htab));
		hdestroy(abus->service_htab);
	}

	if (abus->outstanding_req_htab)
		hdestroy(abus->outstanding_req_htab);

	pthread_mutex_destroy(&abus->mutex);

	return 0;
}

static int abus_resp_send(json_rpc_t *json_rpc)
{
	int len;

	LogDebug("## sendto %s: %s %*s\n", __func__, json_rpc->sock_src_addr.sun_path+1,
					json_rpc->msglen, json_rpc->msgbuf);

	len = sendto(json_rpc->sock, json_rpc->msgbuf, json_rpc->msglen, MSG_DONTWAIT|MSG_NOSIGNAL,
					(struct sockaddr*)&json_rpc->sock_src_addr, json_rpc->sock_addrlen);
	if (len == -1) {
		perror("abus srv sendto: ");
		return -1;
	}
	return 0;
}

/*
 * \param[in] timeout   receiving timeout in milliseconds
 * \result 1 if data available for receive, 0 if timeout or -1 in case of error
 */
static int select_for_read(int sock, int timeout)
{
	fd_set setr, sete;
	struct timeval tv;
	int ret;

	FD_ZERO(&setr);
	FD_SET(sock, &setr);

	memcpy(&sete, &setr, sizeof(fd_set));

	tv.tv_sec = timeout / 1000;
	tv.tv_usec = (timeout % 1000) * 1000;

	do {
		ret = select(sock + 1, &setr, NULL, &sete, &tv);
	} while (ret == -1 && errno == EINTR);

	if (ret < 0)
	{
		LogError("%s: select fails with error: %s", __func__, strerror(errno));
		return -1;
	}
	if (ret == 0)
	{
		return 0;
	}
	if (FD_ISSET(sock, &sete))
	{
		LogError("%s: error detected on sock by select", __func__);
		return -1;
	}
	return 1;
}


/*
 \internal
 */
void *abus_thread_routine(void *arg)
{
	abus_t *abus = (abus_t *)arg;
    ssize_t len;
	char *buffer;
	json_rpc_t *json_rpc;

	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_detach(pthread_self());

	buffer = malloc(JSONRPC_REQ_SZ_MAX);
	if (!buffer) {
		pthread_exit(NULL);
	}

	while (1) {
		struct sockaddr_un sock_src_addr;
		socklen_t sock_addrlen = sizeof(sock_src_addr);

		len = recvfrom(abus->sock, buffer, JSONRPC_REQ_SZ_MAX, 0,
						(struct sockaddr*)&sock_src_addr,
						&sock_addrlen);
		if (len == -1) {
			perror("abus srv recv: ");
			continue;
		}

		LogDebug("## Recv from %d %s: %*s\n", sock_addrlen, sock_src_addr.sun_path+1, len, buffer);

		json_rpc = calloc(1, sizeof(json_rpc_t));

		abus_process_msg(abus, buffer, len, json_rpc, &sock_src_addr, sock_addrlen);

		if (!abus_method_is_threaded((abus_method_t*)json_rpc->cb_context)) {
			if (json_rpc->msglen) {
				abus_resp_send(json_rpc);
				json_rpc_cleanup(json_rpc);
			}
			free(json_rpc);
		}

	}

	free(buffer);
	return NULL;
}

/*
  NB: '/' (slash) is forbidden in service_name
 */
int create_service_path(abus_t *abus, const char *service_name)
{
#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif
	char service_path[UNIX_PATH_MAX];
	char pid_rel_path[UNIX_PATH_MAX];
	int ret;

	if (strchr(service_name, '/'))
		return -1;

	ret = abus_launch_thread_ondemand(abus);
	if (ret)
		return ret;

	/* TODO: prefix from env variable */
	snprintf(pid_rel_path, sizeof(pid_rel_path)-1, "_%d", getpid());

	if (strlen(service_name) > 0) {
		snprintf(service_path, sizeof(service_path)-1, "%s/%s",
						abus_prefix, service_name);
	
		unlink(service_path);
	
		/* /tmp/abus/<servicename> -> _<pid> */
		ret = symlink(pid_rel_path, service_path);
		if (ret == -1)
			return ret;
	}

	return 0;
}

int remove_service_path(abus_t *abus, const char *service_name)
{
	char service_path[UNIX_PATH_MAX];
	char pid_rel_path[UNIX_PATH_MAX];

	if (strchr(service_name, '/'))
		return -1;

	/* TODO: prefix from env variable */
	snprintf(pid_rel_path, sizeof(pid_rel_path)-1, "_%d", getpid());

	snprintf(service_path, sizeof(service_path)-1, "%s/%s",
					abus_prefix, service_name);

	unlink(service_path);

	return 0;
}


static int service_lookup(abus_t *abus, const char *service_name, bool create, abus_service_t **service_p)
{
	int srv_len = strlen(service_name);
	abus_service_t *service;
	abus_method_t *new_method;

	if (!abus->service_htab)
		abus->service_htab = hcreate(3);

	if (hfind(abus->service_htab, service_name, srv_len)) {
		service = hstuff(abus->service_htab);
	} else if (!create) {
		service = NULL;
	} else {
		service = calloc(1, sizeof(abus_service_t));

		create_service_path(abus, service_name);

		service->method_htab = hcreate(3);
		service->event_htab = hcreate(3);
		hadd(abus->service_htab, strdup(service_name), srv_len, service);

		new_method = calloc(1, sizeof(abus_method_t));
		new_method->callback = &abus_req_introspect_service_cb;
		new_method->flags = 0;
		new_method->arg = abus;
		hadd(service->method_htab, strdup(ABUS_INTROSPECT_METHOD), strlen(ABUS_INTROSPECT_METHOD), new_method);

		new_method = calloc(1, sizeof(abus_method_t));
		new_method->callback = &abus_req_subscribe_service_cb;
		new_method->flags = 0;
		new_method->arg = abus;
		hadd(service->method_htab, strdup(ABUS_SUBSCRIBE_METHOD), strlen(ABUS_SUBSCRIBE_METHOD), new_method);

		new_method = calloc(1, sizeof(abus_method_t));
		new_method->callback = &abus_req_unsubscribe_service_cb;
		new_method->flags = 0;
		new_method->arg = abus;
		hadd(service->method_htab, strdup(ABUS_UNSUBSCRIBE_METHOD), strlen(ABUS_UNSUBSCRIBE_METHOD), new_method);
	}
	*service_p = service;

	return 0;
}

static int method_lookup(abus_t *abus, const char *service_name, const char *method_name, bool create, abus_service_t **service_p, abus_method_t **method)
{
	int mth_len = strlen(method_name);
	abus_service_t *service;

	pthread_mutex_lock(&abus->mutex);

	service_lookup(abus, service_name, create, &service);

	if (service_p)
		*service_p = service;

	if (!service && !create) {
		pthread_mutex_unlock(&abus->mutex);
		*method = NULL;
		return 0;
	}

	if (hfind(service->method_htab, method_name, mth_len)) {
		*method = hstuff(service->method_htab);
	} else if (!create) {
		pthread_mutex_unlock(&abus->mutex);
		*method = NULL;
		return 0;
	} else {
		*method = calloc(1, sizeof(abus_method_t));
		hadd(service->method_htab, strdup(method_name), mth_len, *method);
	}

	pthread_mutex_unlock(&abus->mutex);

	return 0;
}

static int event_lookup(abus_t *abus, const char *service_name, const char *event_name, bool create, abus_service_t **service_p, abus_event_t **event)
{
	int evt_len = strlen(event_name);
	abus_service_t *service;

	pthread_mutex_lock(&abus->mutex);

	service_lookup(abus, service_name, create, &service);

	if (service_p)
		*service_p = service;

	if (!service && !create) {
		pthread_mutex_unlock(&abus->mutex);
		*event = NULL;
		return 0;
	}

	if (hfind(service->event_htab, event_name, evt_len)) {
		*event = hstuff(service->event_htab);
	} else if (!create) {
		pthread_mutex_unlock(&abus->mutex);
		*event = NULL;
		return 0;
	} else {
		*event = calloc(1, sizeof(abus_event_t));
		(*event)->subscriber_htab = hcreate(1);
		hadd(service->event_htab, strdup(event_name), evt_len, *event);
	}

	pthread_mutex_unlock(&abus->mutex);

	return 0;
}

static void abus_call_callback(abus_method_t *method, json_rpc_t *json_rpc)
{
	if (!method->callback)
		return;

	if (abus_method_is_excl(method))
		pthread_mutex_lock(&method->excl_mutex);

	method->callback(json_rpc, method->arg);

	if (abus_method_is_excl(method))
		pthread_mutex_lock(&method->excl_mutex);
}

/*
  Execute RPC in its own thread
 */
static void *abus_threaded_rpc_routine(void *arg)
{
	json_rpc_t *json_rpc = (json_rpc_t *)arg;
	abus_method_t *method = (abus_method_t *)json_rpc->cb_context;
    int ret;

	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_detach(pthread_self());

	abus_call_callback(method, json_rpc);

	if (json_rpc->service_name && json_rpc->method_name &&
					(!json_val_is_undef(&json_rpc->id) || json_rpc->error_code)) {
		ret = json_rpc_resp_finalize(json_rpc);
		if (ret == 0)
			abus_resp_send(json_rpc);
	}

	json_rpc->msglen = 0;
	json_rpc_cleanup(json_rpc);

	free(json_rpc);

	return NULL;
}

/* internal to libabus
 * json_rpc is to be malloc'ed for ABUS_RPC_THREADED
 * TODO: there might be more than one call for subscribed events..
 */
static int abus_service_rpc(abus_t *abus, json_rpc_t *json_rpc, abus_method_t *method)
{
	if (abus_method_is_threaded(method)) {
		pthread_t th;
		int ret;

		json_rpc->cb_context = method;

		ret = pthread_create(&th, NULL, &abus_threaded_rpc_routine, json_rpc);
		if (ret != 0)
		{
			LogError("%s: pthread_create() failed: %s", __func__, strerror(ret));
			return -ret;
		}
	} else {
		abus_call_callback(method, json_rpc);
	}

	return 0;
}

/*!
	Decalare an A-Bus method

  \param abus	pointer to A-Bus handle
  \param[in] service_name	name of service where the method belongs to
  \param[in] method_name	name of method to be exposed
  \param[in] method_callback	pointer to call-back function
  \param[in] flags	flags specifying method callback
  \param[in] arg	cookie to be passed back to method_callback
  \param[in] fmt	abus_format describing the argument of method, may be NULL
  \param[in] result_fmt	abus_format describing the result of method, may be NULL

  \return 0 if successful, non nul value otherwise
  \todo Allow redeclaration?
  \sa abus_undecl_method()
 */
int abus_decl_method(abus_t *abus, const char *service_name, const char *method_name,
				abus_callback_t method_callback, int flags, void *arg,
				const char *fmt, const char *result_fmt)
{
	abus_method_t *method;
	int ret;

	ret = method_lookup(abus, service_name, method_name, true, NULL, &method);
	if (ret)
		return ret;

	method->callback = method_callback;
	method->flags = flags;
	method->arg = arg;
	method->fmt = fmt ? strdup(fmt) : NULL;
	method->result_fmt = result_fmt ? strdup(result_fmt) : NULL;

	if (abus_method_is_excl(method))
		pthread_mutex_init(&method->excl_mutex, NULL);

	return 0;
}

/*!
	Undeclare an A-Bus method

  \param abus	pointer to A-Bus handle
  \return 0 if successful, non nul value otherwise
  \sa abus_decl_method()
 */
int abus_undecl_method(abus_t *abus, const char *service_name, const char *method_name)
{
	abus_method_t *method;
	abus_service_t *service;
	int ret;

	ret = method_lookup(abus, service_name, method_name, false, &service, &method);
	if (ret)
		return ret;

	if (!service || !method)
		return JSONRPC_NO_METHOD;

	if (abus_method_is_excl(method))
		pthread_mutex_destroy(&method->excl_mutex);

	if (method->fmt)
		free(method->fmt);

	if (method->result_fmt)
		free(method->result_fmt);

	pthread_mutex_lock(&abus->mutex);

	/* FIXME: assumes the hashtab still pointing at element found */
	free(hkey(service->method_htab));
	free(hstuff(service->method_htab));
	hdel(service->method_htab);

	/* no more methods in service? */
	if (hcount(service->method_htab) == 0)
	{
		remove_service_path(abus, service_name);
		free(hkey(abus->service_htab));
		free(hstuff(abus->service_htab));
		hdel(abus->service_htab);
	}
	pthread_mutex_unlock(&abus->mutex);

	return 0;
}


/*!
	Initialize for request method, client side

  Implicit: expects a response, otherwise use notification/event
  \param abus	pointer to A-Bus handle
  \return 0 if successful, non nul value otherwise
 */
int abus_request_method_init(abus_t *abus, const char *service_name, const char *method_name, json_rpc_t *json_rpc)
{
	int ret;

	ret = json_rpc_req_init(json_rpc, service_name, method_name, abus->id++);
	if (ret != 0)
		return ret;

	/* request for event subsribe MUST be sent on A-Bus socket
	 * in order to get the notifications on that socket
	 */
	if (!strcmp(method_name, ABUS_SUBSCRIBE_METHOD)) {
		/* A-Bus thread most likely already started/socket created
		 * by abus_event_subscribe()
		 */
		ret = abus_launch_thread_ondemand(abus);
		if (ret != 0)
			return ret;
		json_rpc->sock = abus->sock;
	}

	return 0;
}

static int sendto_un_sock(int sock, const void *buf, size_t len, int flags, const char *service_name)
{
	struct sockaddr_un sockaddrun;
	ssize_t ret;

	memset(&sockaddrun, 0, sizeof(sockaddrun));
	sockaddrun.sun_family = AF_UNIX;

	/* TODO: prefix from env variable */
	snprintf(sockaddrun.sun_path, sizeof(sockaddrun.sun_path)-1,
					"%s/%s", abus_prefix, service_name);

	ret = sendto(sock, buf, len, flags,
					(const struct sockaddr *)&sockaddrun, SUN_LEN(&sockaddrun));
	if (ret == -1) {
		perror("sendto_un_sock(): sendto failed:");
		return ret;
	}

	return 0;
}


/*
	To be used by abus_request_method_invoke_async()
 */
static int abus_req_track(abus_t *abus, json_rpc_t *json_rpc)
{
	pthread_mutex_lock(&abus->mutex);

	if (!abus->outstanding_req_htab)
		abus->outstanding_req_htab = hcreate(2);

	/* no strndup() for key, since it shares the json_rpc memory */
	hadd(abus->outstanding_req_htab, json_rpc->id.u.data, json_rpc->id.length, json_rpc);

	pthread_mutex_unlock(&abus->mutex);

	return 0;
}

static json_rpc_t *abus_req_untrack(abus_t *abus, const json_val_t *id_val)
{
	json_rpc_t *json_rpc;

	if (!abus->outstanding_req_htab)
		return NULL;

	pthread_mutex_lock(&abus->mutex);

	if (hfind(abus->outstanding_req_htab, id_val->u.data, id_val->length)) {
		json_rpc = hstuff(abus->outstanding_req_htab);
		/* no free() for key, since it shared the json_rpc id val memory */
		hdel(abus->outstanding_req_htab);
	} else {
		json_rpc = NULL;
	}

	pthread_mutex_unlock(&abus->mutex);

	return json_rpc;
}


/*!
	Wait for an asynchronous method request

  \param abus	pointer to A-Bus handle
  \param[in] timeout waiting timeout in milliseconds
  \return   0 if successful, non nul value otherwise
  \sa abus_request_method_invoke_async()
 */
int abus_request_method_wait_async(abus_t *abus, json_rpc_t *json_rpc, int timeout)
{
	int ret;
	struct timespec ts;

	pthread_mutex_lock(&json_rpc->mutex);

	clock_gettime(CLOCK_REALTIME, &ts);

	ts.tv_sec  +=  timeout / 1000;
	ts.tv_nsec += (timeout % 1000) * 1000000;
	if (ts.tv_nsec >= 1000000000) {
		ts.tv_nsec  -= 1000000000;
		ts.tv_sec++;
	}

	ret = 0;
	/* FIXME */
	while (! (json_rpc->error_code) && ret == 0)
			ret = pthread_cond_timedwait(&json_rpc->cond, &json_rpc->mutex, &ts);
	pthread_mutex_unlock(&json_rpc->mutex);

	if (ret != 0)
		return ret;

	return 0;
}

/*!
  Asynchronous invocation of a RPC

  \param abus	pointer to A-Bus handle
  \param[in] timeout	receiving timeout in milliseconds
  \param[in] callback	function to be called upon response or timeout. may be NULL.
  \param[in] arg			opaque pointer value to be passed to callback. may be NULL.
  \return   0 if successful, non nul value otherwise

  \sa abus_request_method_wait_async()
  \todo implement timeout callback
*/
int abus_request_method_invoke_async(abus_t *abus, json_rpc_t *json_rpc, int timeout, abus_callback_t callback, int flags, void *arg)
{
	int ret;
	abus_method_t *resp_handler;

	ret = abus_launch_thread_ondemand(abus);
	if (ret)
		return ret;

	ret = json_rpc_req_finalize(json_rpc);

	resp_handler = calloc(1, sizeof(abus_method_t));
	resp_handler->callback = callback;
	resp_handler->flags = flags;
	resp_handler->arg = arg;

	json_rpc->cb_context = resp_handler;

	assert(!json_val_is_undef(&json_rpc->id));
	abus_req_track(abus, json_rpc);

	/* send the request through serv socket, response coming from this sock */

	ret = sendto_un_sock(abus->sock, json_rpc->msgbuf, json_rpc->msglen, MSG_NOSIGNAL, json_rpc->service_name);
	if (ret != 0)
		return ret;

	return 0;
}


/*!
 * synchronous invocation

  \param abus	pointer to A-Bus handle
  \param[in] timeout   receiving timeout in milliseconds
  \return   0 if successful, non nul value otherwise
 */
int abus_request_method_invoke(abus_t *abus, json_rpc_t *json_rpc, int flags, int timeout)
{
	int sock, ret;
	int passcred;
	ssize_t len;

	ret = json_rpc_req_finalize(json_rpc);

	if (json_rpc->sock == -1) {
		/* Don't re-use abus->sock as a default,
		 * so as to have faster response time (saves 2 thread switches)
		 */
		sock = socket(AF_UNIX, SOCK_DGRAM, 0);
		if (sock < 0) {
			LogError("%s: failed to create socket: %s", __func__, strerror(errno));
			return sock;
		}
	
		/* autobind */
		passcred = 1;
		ret = setsockopt(sock, SOL_SOCKET, SO_PASSCRED, &passcred, sizeof(passcred));
		if (ret != 0) {
			perror("abus clnt setsockopt(SO_PASSCRED): ");
			close(sock);
			return ret;
		}
	} else {
		sock = json_rpc->sock;
	}

	ret = sendto_un_sock(sock, json_rpc->msgbuf, json_rpc->msglen, MSG_NOSIGNAL, json_rpc->service_name);
	if (ret != 0) {
		if (json_rpc->sock == -1)
			close(sock);
		return ret;
	}

	ret = select_for_read(sock, timeout);
	if (ret == -1) {
		if (json_rpc->sock == -1)
			close(sock);
		return -1;
	}
	if (ret == 0) {
		if (json_rpc->sock == -1)
			close(sock);
		return -2;	/* FIXME: timeout */
	}

	/* recycle req msgbuf */

	len = recv(sock, json_rpc->msgbuf, json_rpc->msgbufsz, 0);
	if (len == -1) {
		perror("abus clnt recv: ");
		if (json_rpc->sock == -1)
			close(sock);
		return 0;
	}
	json_rpc->msglen = len;

	ret = json_rpc_parse_msg(json_rpc, json_rpc->msgbuf, json_rpc->msglen);
	if (ret || json_rpc->parsing_status != PARSING_OK) {
		if (json_rpc->error_code)
			ret = json_rpc->error_code;
		else if (!ret)
			ret = JSONRPC_PARSE_ERROR;
	}

	free(json_rpc->msgbuf);
	json_rpc->msgbuf = NULL;
	json_rpc->msglen = 0;

	if (json_rpc->sock == -1)
		close(sock);

	return ret;
}

/*!
  Release ressources associated with a RPC

  \param abus	pointer to A-Bus handle
  \return   0 if successful, non nul value otherwise
 */
int abus_request_method_cleanup(abus_t *abus, json_rpc_t *json_rpc)
{
	json_rpc_cleanup(json_rpc);

	/* TODO: abus_req_untrack() in case of async req ? */

	return 0;
}


int abus_process_msg(abus_t *abus, const char *buffer, int len, json_rpc_t *json_rpc, struct sockaddr_un *sock_src_addr, socklen_t sock_addrlen)
{
	int ret;
	abus_method_t *method;

	ret = json_rpc_init(json_rpc);
	if (ret)
		return ret;

	if (sock_src_addr) {
		memcpy(&json_rpc->sock_src_addr, sock_src_addr, sizeof(struct sockaddr_un));
		json_rpc->sock_addrlen = sock_addrlen;
	}

	json_rpc->sock = abus->sock;

	ret = json_rpc_parse_msg(json_rpc, buffer, len);
	if (ret || json_rpc->parsing_status != PARSING_OK) {
		/* TODO send "error" JSON-RPC */
		json_rpc->error_code = JSONRPC_PARSE_ERROR;
	}

	if (!json_rpc->error_code && json_rpc->service_name && json_rpc->method_name)
	{
		/* this is a request */
		/* TODO: event & more than on cb possible */

		method_lookup(abus, json_rpc->service_name, json_rpc->method_name, false, NULL, &method);
	
		if (!method)
			json_rpc->error_code = JSONRPC_NO_METHOD;
	
		json_rpc_resp_init(json_rpc);
	
		if (json_rpc->error_code == 0) {
	
			ret = abus_service_rpc(abus, json_rpc, method);
	
			/* no response for notification */
			if (json_val_is_undef(&json_rpc->id)) {
				json_rpc->msglen = 0;
				json_rpc_cleanup(json_rpc);
				return 0;
			}
		}
	}
	else if (!json_val_is_undef(&json_rpc->id))
	{
		/* this is an async response */
		/* TODO: threaded response */
		json_rpc_t *req_json_rpc;

		req_json_rpc = abus_req_untrack(abus, &json_rpc->id);
	
		if (req_json_rpc) {
			method = req_json_rpc->cb_context;

			abus_service_rpc(abus, json_rpc, method);
			json_rpc = req_json_rpc;
		}

		json_rpc->msglen = 0;
		json_rpc_cleanup(json_rpc);

		/* no json_rpc_resp_finalize() */
		return 0;
	}

	ret = json_rpc_resp_finalize(json_rpc);
	if (ret)
		return ret;

	return 0;
}

/*
  Do not perform a full JSON parsing, but simply identify the dest service name for delivery
  Assumes somewhat sane format and no quoting/escaping in service name
*/
static int json_rpc_quickparse_service_name(const char *buffer, int buflen, char *service_name)
{
	const char *p, *q;
	int svc_len;

#define METHOD_STR "\"method\""
	p = strstr(buffer, METHOD_STR);
	if (!p)
		return JSONRPC_PARSE_ERROR;
	p += strlen(METHOD_STR);
	p = strchr(p, ':');
	if (!p)
		return JSONRPC_PARSE_ERROR;
	p = strchr(++p, '"');
	if (!p)
		return JSONRPC_PARSE_ERROR;
	q = strchr(++p, '.');
	if (!q)
		return JSONRPC_NO_METHOD;

	svc_len = q-p;
	if (svc_len >= JSONRPC_SVCNAME_SZ_MAX)
		return JSONRPC_NO_METHOD;

	memcpy(service_name, p, svc_len);

	service_name[svc_len] = '\0';

	return 0;
}

/*!
   Forward a JSON-RPC request in the A-Bus, and get the response.
   Applicable for implementing an http<->A-Bus relay.

 * \param abus		pointer to A-Bus context
 * \param[in,out] buffer	pointer to buffer (size at least JSONRPC_RESP_SZ_MAX) containing JSON-RPC request, and later the response
 * \param[in,out] buffer	pointer to length of buffer of JSON-RPC request/response
 * \param[in] flags		ABUS_RPC flags
 * \param[in] timeout	receiving timeout in milliseconds
  \return   0 if successful, non nul value otherwise
 *
 * \todo ABUS_RPC_THREADED
 */
int abus_forward_rpc(abus_t *abus, char *buffer, int *buflen, int flags, int timeout)
{
	int sock, ret;
	int passcred;
	char service_name[JSONRPC_SVCNAME_SZ_MAX];
	ssize_t len;

	ret = json_rpc_quickparse_service_name(buffer, *buflen, service_name);
	if (ret != 0)
		return ret;

	sock = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (sock < 0) {
		LogError("%s: failed to create socket: %s", __func__, strerror(errno));
		return sock;
	}

	/* autobind for response */
	passcred = 1;
	ret = setsockopt(sock, SOL_SOCKET, SO_PASSCRED, &passcred, sizeof(passcred));
	if (ret != 0) {
		perror("abus clnt setsockopt(SO_PASSCRED): ");
		close(sock);
		return ret;
	}

	ret = sendto_un_sock(sock, buffer, *buflen, MSG_NOSIGNAL, service_name);
	if (ret != 0) {
		close(sock);
		return ret;
	}

	ret = select_for_read(sock, timeout);
	if (ret == -1) {
		close(sock);
		return -1;
	}
	if (ret == 0) {
		close(sock);
		return -2;	/* FIXME: timeout */
	}

	/* recycle req buffer */

	len = recv(sock, buffer, JSONRPC_RESP_SZ_MAX, 0);
	if (len == -1) {
		perror("abus forword recv: ");
		close(sock);
		return 0;
	}
	close(sock);
	*buflen = len;

	return 0;
}

/*
  callback for internal use, to offer introspection of a service
 */
void abus_req_introspect_service_cb(json_rpc_t *json_rpc, void *arg)
{
	abus_t *abus = (abus_t *)arg;
	abus_service_t *service;
	abus_method_t *method;
	abus_event_t *event;

	if (!abus->service_htab || !json_rpc->service_name) {
		json_rpc->error_code = JSONRPC_INTERNAL_ERROR;
		return;
	}

	pthread_mutex_lock(&abus->mutex);

	if (!hfind(abus->service_htab, json_rpc->service_name, strlen(json_rpc->service_name))) {
		pthread_mutex_unlock(&abus->mutex);
		json_rpc->error_code = JSONRPC_NO_METHOD;
		return;
	}

	service = hstuff(abus->service_htab);

	/* methods */
	if (service->method_htab && hfirst(service->method_htab))
	{
		json_rpc_append_args(json_rpc,
						JSON_KEY, "methods", -1,
						JSON_ARRAY_BEGIN, 
						-1);

		do {
			const char *method_name = (const char*)hkey(service->method_htab);
	
			if (!strncmp(ABUS_INTROSPECT_METHOD, method_name, hkeyl(service->method_htab)) ||
					!strncmp(ABUS_SUBSCRIBE_METHOD, method_name, hkeyl(service->method_htab)) ||
					!strncmp(ABUS_UNSUBSCRIBE_METHOD, method_name, hkeyl(service->method_htab)))
				continue;
	
			json_rpc_append_args(json_rpc, JSON_OBJECT_BEGIN, -1);

			method = hstuff(service->method_htab);
	
			json_rpc_append_strn(json_rpc, "name", method_name, hkeyl(service->method_htab));
			json_rpc_append_int(json_rpc, "flags", method->flags);
			/* TODO: interpret the param list in an array */
			if (method->fmt)
				json_rpc_append_str(json_rpc, "fmt", method->fmt);
			if (method->result_fmt)
				json_rpc_append_str(json_rpc, "result_fmt", method->result_fmt);

			json_rpc_append_args(json_rpc, JSON_OBJECT_END, -1);
		}
		while (hnext(service->method_htab));

		json_rpc_append_args(json_rpc, JSON_ARRAY_END, -1);
	}

	/* events */
	if (service->event_htab && hfirst(service->event_htab))
	{
		json_rpc_append_args(json_rpc,
						JSON_KEY, "events", -1,
						JSON_ARRAY_BEGIN, 
						-1);

		do {
			const char *event_name = (const char*)hkey(service->event_htab);
	
			event = hstuff(service->event_htab);

			json_rpc_append_args(json_rpc, JSON_OBJECT_BEGIN, -1);

			json_rpc_append_strn(json_rpc, "name", event_name, hkeyl(service->event_htab));
			/* TODO: interpret the param list in an array */
			if (event->fmt)
				json_rpc_append_str(json_rpc, "fmt", event->fmt);

			json_rpc_append_args(json_rpc, JSON_OBJECT_END, -1);
		}
		while (hnext(service->event_htab));

		json_rpc_append_args(json_rpc, JSON_ARRAY_END, -1);
	}

	pthread_mutex_unlock(&abus->mutex);

	return;
}

/* Event business */

#define EVTPREFIX "_event%%"
static int snprint_event_method(char *str, size_t size, const char *event_name)
{
	return snprintf(str, size, EVTPREFIX"%s", event_name);
}

static const char* event_name_from_method(const char *str)
{
	if (strlen(str) > strlen(EVTPREFIX))
		return str+strlen(EVTPREFIX);
	return NULL;
}


/*!
	Decalare an A-Bus event

  \param abus	pointer to A-Bus handle
  \param[in] service_name	name of service where the event belongs to
  \param[in] event_name	name of event that may be subscribed to
  \param[in] fmt	abus_format describing the arguments of the event, may be NULL

  \return 0 if successful, non nul value otherwise
  \todo Allow redeclaration?
  \todo abus_undecl_event()
 */
int abus_decl_event(abus_t *abus, const char *service_name, const char *event_name, const char *fmt)
{
	abus_event_t *event;
	int ret;

	ret = event_lookup(abus, service_name, event_name, true, NULL, &event);
	if (ret)
		return ret;

	event->fmt = fmt ? strdup(fmt) : NULL;

	return 0;
}

/*!
	Initialize a new RPC to be published by a service

  \param abus	pointer to A-Bus handle
  \return   0 if successful, non nul value otherwise
  \sa abus_request_event_cleanup()
 */
int abus_request_event_init(abus_t *abus, const char *service_name, const char *event_name, json_rpc_t *json_rpc)
{
	int ret;
	abus_event_t *event;
	char event_method_name[JSONRPC_METHNAME_SZ_MAX];

	ret = abus_launch_thread_ondemand(abus);
	if (ret)
		return ret;

	snprint_event_method(event_method_name, JSONRPC_METHNAME_SZ_MAX, event_name);

	ret = json_rpc_req_init(json_rpc, "", event_method_name, -1);

	json_val_free(&json_rpc->id);
	json_rpc->id.type = JSON_NONE;

	json_rpc_append_str(json_rpc, "service", service_name);
	json_rpc_append_str(json_rpc, "event", event_name);

	/* implicit abus_decl_event() */
	event_lookup(abus, service_name, event_name, true, NULL, &event);

	/* re-use cb_context for event context */
	json_rpc->cb_context = event;

	/* may be necessary in case of failed delivery.
	   no need of strdup for service name
	 */
	if (!hfind(abus->service_htab, service_name, strlen(service_name)))
		return JSONRPC_INTERNAL_ERROR;
	json_rpc->evt_service_name = (const char *)hkey(abus->service_htab);

	return ret;
}

/*!
	Publish (i.e. send) an event from a service

	The notification is sent to all the subscribed end-points.
	If notification delivery fails, the troublesome end-point get unsubscribed.

  \param abus	pointer to A-Bus handle
  \param[in] flags   unused so far
  \return   0 if successful, non nul value otherwise
  \sa abus_request_event_init()
 */
int abus_request_event_publish(abus_t *abus, json_rpc_t *json_rpc, int flags)
{
	abus_event_t *event;
	int ret;

	ret = json_rpc_req_finalize(json_rpc);

	event = json_rpc->cb_context;

	/* foreach subscribed A-Bus endpoints, deliver "id"-less rpc */
	if (event->subscriber_htab && hfirst(event->subscriber_htab)) do {
		struct sockaddr_un *sockaddrun;

		sockaddrun = hstuff(event->subscriber_htab);

		LogDebug("####%s: %*s %*s\n", __func__, SUN_LEN(sockaddrun)-3, sockaddrun->sun_path+1, json_rpc->msglen, json_rpc->msgbuf);

		ret = sendto(abus->sock, json_rpc->msgbuf, json_rpc->msglen, MSG_NOSIGNAL|MSG_DONTWAIT,
					(const struct sockaddr *)sockaddrun, SUN_LEN(sockaddrun));
		if (ret == -1) {
			const char *event_name;
			/* remove that subscriber if delivery failed */
			perror("abus_request_event_publish(): sendto failed:");

			event_name = event_name_from_method(json_rpc->method_name);
			if (event_name)
				abus_unsubscribe_service(abus, json_rpc->evt_service_name, event_name);
			continue;
		}
	}
	while (hnext(event->subscriber_htab));

	return 0;
}

/*!
	Release ressources associated with a past event RPC

  \param abus	pointer to A-Bus handle
  \return   0 if successful, non nul value otherwise
  \sa abus_request_event_init()
 */
int abus_request_event_cleanup(abus_t *abus, json_rpc_t *json_rpc)
{
	json_rpc_cleanup(json_rpc);

	return 0;
}

/*!
 * Register callback for event notification and send subscription to service.

  \param abus	pointer to A-Bus handle
  \param[in] timeout	receive timeout of subscribe request in milliseconds
  \return   0 if successful, non nul value otherwise
 */
int abus_event_subscribe(abus_t *abus, const char *service_name, const char *event_name, abus_callback_t callback, int flags, void *arg, int timeout)
{
	char event_method_name[JSONRPC_METHNAME_SZ_MAX];
	json_rpc_t json_rpc;
	int ret;

	snprint_event_method(event_method_name, JSONRPC_METHNAME_SZ_MAX, event_name);

	ret = abus_decl_method(abus, "", event_method_name,
					callback, flags, arg, NULL, NULL);
	if (ret != 0)
		return ret;

	ret = abus_request_method_init(abus, service_name, ABUS_SUBSCRIBE_METHOD, &json_rpc);
	if (ret != 0)
		return ret;

	json_rpc_append_str(&json_rpc, "event", event_name);

	/* MUST use the A-Bus sock in order to get the event RPC issued on that socket,
		hence the use of abus_request_method_invoke_async()
	 */
	ret = abus_request_method_invoke_async(abus, &json_rpc, timeout, NULL, flags, NULL);
	if (ret != 0)
		return ret;

	ret = abus_request_method_wait_async(abus, &json_rpc, timeout);
	if (ret != 0)
		return ret;

	abus_request_method_cleanup(abus, &json_rpc);

	return 0;
}

/*!
 * Unregister callback for event notification and send unsubscription to service.

  \param abus	pointer to A-Bus handle
  \param[in] timeout	receive timeout of unsubscribe request in milliseconds
  \return   0 if successful, non nul value otherwise
 */
int abus_event_unsubscribe(abus_t *abus, const char *service_name, const char *event_name, abus_callback_t callback, void *arg, int timeout)
{
	char event_method_name[JSONRPC_METHNAME_SZ_MAX];
	json_rpc_t json_rpc;
	int ret;

	/* TODO: hash callback&arg into method name,
		in order to allow more than one subscribe for the same svc/event name
	 */
	snprint_event_method(event_method_name, JSONRPC_METHNAME_SZ_MAX, event_name);

	ret = abus_undecl_method(abus, "", event_method_name);
	if (ret != 0)
		return ret;

	ret = abus_request_method_init(abus, service_name, ABUS_UNSUBSCRIBE_METHOD, &json_rpc);
	if (ret != 0)
		return ret;

	json_rpc_append_str(&json_rpc, "event", event_name);

	ret = abus_request_method_invoke(abus, &json_rpc, 0, timeout);
	if (ret != 0)
		return ret;

	abus_request_method_cleanup(abus, &json_rpc);

	return 0;
}


static void *memdup(const void *s, size_t n)
{
	void *p = malloc(n);
	if (p)
		memcpy(p, s, n);
	return p;
}

/*
  callback for internal use, which is responsible for the event subscribing
 */
void abus_req_subscribe_service_cb(json_rpc_t *json_rpc, void *arg)
{
	abus_t *abus = (abus_t *)arg;
	abus_event_t *event;
	char event_name[JSONRPC_METHNAME_SZ_MAX];
	void *key, *stuff;
	int ret;

	ret = json_rpc_get_str(json_rpc, "event", event_name, sizeof(event_name));
	if (ret != 0) {
		json_rpc->error_code = JSONRPC_INVALID_METHOD;
		return;
	}

	event_lookup(abus, json_rpc->service_name, event_name, false, NULL, &event);
	if (!event) {
		json_rpc->error_code = JSONRPC_NO_METHOD;
		return;
	}

	/* FIXME: locking unneeded because the abus_thread is the only user? */
	if (!event->subscriber_htab)
		event->subscriber_htab = hcreate(1);

	LogDebug("####%s %s %p %u %*s\n", json_rpc->service_name, event_name, event->subscriber_htab, event->uniq_subscriber_cnt, json_rpc->sock_addrlen-1, json_rpc->sock_src_addr.sun_path+1);

	key = memdup(&event->uniq_subscriber_cnt, sizeof(event->uniq_subscriber_cnt));
	event->uniq_subscriber_cnt++;
	stuff = memdup(&json_rpc->sock_src_addr, json_rpc->sock_addrlen);

	hadd(event->subscriber_htab, key, sizeof(event->uniq_subscriber_cnt), stuff);

	return;
}

int abus_unsubscribe_service(abus_t *abus, const char *service_name, const char *event_name)
{
	abus_event_t *event;

	event_lookup(abus, service_name, event_name, false, NULL, &event);
	if (!event || !event->subscriber_htab) {
		return JSONRPC_NO_METHOD;
	}

	LogDebug("####%s %s\n", service_name, event_name);

	free(hkey(event->subscriber_htab));
	free(hstuff(event->subscriber_htab));

	hdel(event->subscriber_htab);

	return 0;
}

/*
  callback for internal use, which is responsible for the event unsubscribing
 */
void abus_req_unsubscribe_service_cb(json_rpc_t *json_rpc, void *arg)
{
	abus_t *abus = (abus_t *)arg;
	char event_name[JSONRPC_METHNAME_SZ_MAX];
	int ret;

	ret = json_rpc_get_str(json_rpc, "event", event_name, sizeof(event_name));
	if (ret != 0) {
		json_rpc->error_code = JSONRPC_INVALID_METHOD;
		return;
	}

	ret = abus_unsubscribe_service(abus, json_rpc->service_name, event_name);
	if (ret) {
		json_rpc->error_code = ret;
		return;
	}

	return;
}

/*! @} */
