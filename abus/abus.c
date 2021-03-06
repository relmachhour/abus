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
#include <limits.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>

#include "hashtab.h"
#include "jsonrpc_internal.h"
#include "abus_internal.h"

#include "sock_un.h"

#define LogError(...)    do { fprintf(stderr, ##__VA_ARGS__); fprintf(stderr, "\n"); } while (0)
#define LogDebug(...)    do { fprintf(stderr, ##__VA_ARGS__); fprintf(stderr, "\n"); } while (0)

#define ABUS_INTROSPECT_METHOD "*"
#define ABUS_SUBSCRIBE_METHOD "subscribe"
#define ABUS_UNSUBSCRIBE_METHOD "unsubscribe"
#define ABUS_EVENT_METHOD "event"
#define ABUS_GET_METHOD "get"
#define ABUS_SET_METHOD "set"

/* for use by {service,method,event,attr}_lookup() */
#define CreateIfNotThere true
#define LookupOnly false

static void *abus_thread_routine(void *arg);
static int abus_thread_stop(abus_t *abus);

static int create_service_path(abus_t *abus, const char *service_name);
static int remove_service_path(abus_t *abus, const char *service_name);
static void abus_req_introspect_service_cb(json_rpc_t *json_rpc, void *arg);
static void abus_req_subscribe_service_cb(json_rpc_t *json_rpc, void *arg);
static void abus_req_unsubscribe_service_cb(json_rpc_t *json_rpc, void *arg);
static void abus_req_attr_get_cb(json_rpc_t *json_rpc, void *arg);
static void abus_req_attr_set_cb(json_rpc_t *json_rpc, void *arg);
static int abus_req_service_list(abus_t *abus, json_rpc_t *json_rpc, int timeout);
static int abus_unsubscribe_service(abus_t *abus, const char *service_name, const char *event_name);
static json_rpc_t *abus_process_msg(abus_t *abus, const char *buffer, int len, const struct sockaddr *sock_src_addr, socklen_t sock_addrlen);
static char json_type2char(int json_type);

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

static const char abus_version[] = "A-Bus " PACKAGE_VERSION;
/*!
 * Get version of A-Bus library
  \return pointer to a statically located string of the A-Bus version
 */
const char *abus_get_version()
{
	return abus_version;
}

/*!
 * Get copyright information of A-Bus library
  \return pointer to a statically located string of the A-Bus copyright
 */
const char *abus_get_copyright()
{
	static const char abus_copyright[] =
  "Copyright (C) 2011-2012 Stephane Fillod\n"
  "Copyright (C) 1996 Bob Jenkins (hashtab)\n"
  "Copyright (C) 2009-2011 Vincent Hanquez (libjson)\n"
  "This is free software; see the source for copying conditions.  There is NO\n"
  "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n";

	return abus_copyright;
}


/*!
	Initialize for A-Bus operation

  The A-Bus thread may not be created if only using synchronous abus_request_method_invoke().

  When the external environement variable ABUS_MSG_VERBOSE is set to a non zero value,
  content of JSON-RPC messages will be displayed on the terminal.

  \param[in] conf pointer to the A-Bus conf to be applied, may be NULL for default conf
  \return pointer to an opaque handle for A-Bus operation if successful, a NULL pointer otherwise
 */
abus_t *abus_init(const abus_conf_t *conf)
{
	int ret;
	char *p;
	abus_t *abus;

	abus = calloc(1, sizeof(abus_t));
	if (!abus)
		return NULL;

	p = getenv("ABUS_MSG_VERBOSE");
	if (p)
		abus_msg_verbose = atoi(p);

	/* TODO: path prefix from env variable or conf file */

	pthread_mutex_init(&abus->mutex, NULL);

	/* make sure A-bus directory exists before creating socket */
	ret = mkdir(abus_prefix, 0777);
	if (ret == -1 && errno != EEXIST) {
		ret = -errno;
		LogError("A-Bus mkdir '%s' failed: %s", abus_prefix, strerror(errno));
		free(abus);
		return NULL;
	}

	abus->sock = -1;

	abus->conf.poll_operation = false;

	if (conf)
		abus_set_conf(abus, conf);

#if 0
	/* TODO */
	on_exit(&abus_dtor, abus);
#endif

	return abus;
}

/*!
  Get current configuration of A-Bus

  \param[in] abus pointer to an opaque handle for A-Bus operation
  \param[out] conf pointer to an area where to store current A-Bus conf
  \return   0
 */
int abus_get_conf(abus_t *abus, abus_conf_t *conf)
{
	pthread_mutex_lock(&abus->mutex);

	memcpy(conf, &abus->conf, sizeof(abus_conf_t));

	pthread_mutex_unlock(&abus->mutex);

	return 0;
}

static int set_fd_nonblock(int fd)
{
	int ret, flags = fcntl(fd, F_GETFL);
	if (flags < 0)
		flags = O_NONBLOCK;
	else
		flags |= O_NONBLOCK;
	ret = fcntl(fd, F_SETFL, flags);
	if (ret == -1) {
		ret = -errno;
		LogError("%s: fcntl(O_NONBLOCK) failed: %s", __func__, strerror(errno));
		return ret;
	}
	return 0;
}

/*!
  Apply configuration of A-Bus

  This function may be used to change the poll_operation mode,
  which will stop the A-Bus thread as necessary for pure polling
  operation, and set the A-Bus socket in non blocking mode.
\sa abus_get_conf(), abus_process_incoming(), abus_get_fd()

  If only one parameter is to be modified, it is recommended
  to follow the pattern herebelow:
\code
	abus_t *abus;
	abus_conf_t abus_conf;
	int ret;

	abus = abus_init(NULL);

	abus_get_conf(abus, &abus_conf);
	...
	conf.xxx = y;
	...
	ret = abus_set_conf(abus, &abus_conf);
	if (ret != 0)
		fprintf(stderr, "New conf failed: %s\n", abus_strerror(ret));
\endcode
  \param abus pointer to an opaque handle for A-Bus operation
  \param[in] conf pointer to the A-Bus conf to be applied
  \return   0 if successful, non nul value otherwise
 */
int abus_set_conf(abus_t *abus, const abus_conf_t *conf)
{
	int want_thread_cancel;

	pthread_mutex_lock(&abus->mutex);

	/* detect poll_operation rising edge */
	want_thread_cancel = abus->conf.poll_operation == false &&
			conf->poll_operation != false && abus->sock != -1;

	/* update poll_operation before issuing thread cancel */
	memcpy(&abus->conf, conf, sizeof(abus_conf_t));

	pthread_mutex_unlock(&abus->mutex);

	/* stop A-Bus thread */
	if (want_thread_cancel) {
		abus_thread_stop(abus);
		set_fd_nonblock(abus->sock);
	}

	return 0;
}

#ifdef HAVE_PRCTL

#include <sys/prctl.h>

static void set_thread_name(const char *th_new_name)
{
	if (prctl(PR_SET_NAME, (unsigned long)th_new_name, 0, 0, 0) < 0)
	{
		LogError("prctl(PR_SET_NAME) failed: %s", strerror(errno));
	}
}
static void get_thread_name(char *th_name)
{
	if (prctl(PR_GET_NAME, (unsigned long)th_name, 0, 0, 0) < 0)
	{
		LogError("prctl(PR_GET_NAME) failed: %s", strerror(errno));
	}
}
#else
static void set_thread_name(const char *th_new_name) { }
static void get_thread_name(char *th_name) { th_name[0] = '\0'; }
#endif

/*
   Lazy creation of socket and startup of A-Bus thread.
 */
static int abus_launch_thread_ondemand(abus_t *abus)
{
	pthread_attr_t attr;
	int ret;

	/* launched already ? */
	if (abus->sock != -1)
		return 0;

	abus->sock = un_sock_create();
	if (abus->sock < 0)
		return abus->sock;

	/* don't want A-Bus thread? */
	if (abus->conf.poll_operation)
		return 0;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	ret = pthread_create(&abus->srv_thread, &attr, &abus_thread_routine, abus);
	if (ret != 0)
	{
		LogError("%s: pthread_create() failed: %s", __func__, strerror(ret));
		pthread_attr_destroy(&attr);
		return -ret;
	}

	pthread_attr_destroy(&attr);

	return 0;
}

/*!
	Cleanup of A-Bus operation, releases any allocated memory.

  Note: subscribed events are not unsubscribed from remote services.

  Rem: there's no need to call abus_undecl_method() before calling abus_cleanup().

  \param abus pointer to an opaque handle for A-Bus operation
  \return   0 if successful, non nul value otherwise
 */
int abus_cleanup(abus_t *abus)
{
	if (abus->sock != -1) {
		abus_thread_stop(abus);
		un_sock_close(abus->sock);

		abus->sock = -1;
	}

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
					if (method->descr)
						free(method->descr);
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
						/* TODO: unsubscribe from remote services ? */
						free(hkey(event->subscriber_htab));
						free(hstuff(event->subscriber_htab));
					}
					while (hnext(event->subscriber_htab));

					hdestroy(event->subscriber_htab);
					free(hkey(service->event_htab));
					if (event->fmt)
						free(event->fmt);
					if (event->descr)
						free(event->descr);
					free(event);
				}
				while (hnext(service->event_htab));
				hdestroy(service->event_htab);
			}

			/* delete attr_htab */
			if (service->attr_htab) {
				if (hfirst(service->attr_htab)) do
				{
					abus_attr_t *attr = hstuff(service->attr_htab);
					free(hkey(service->attr_htab));
					if (attr->descr)
						free(attr->descr);
					if (attr->auto_alloc && attr->ref.u.data)
						free(attr->ref.u.data);
					free(attr);
				}
				while (hnext(service->attr_htab));
				hdestroy(service->attr_htab);
			}

			pthread_mutex_destroy(&service->attr_mutex);

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

	free(abus);

	return 0;
}

static void service_may_cleanup(abus_t *abus, abus_service_t *service, const char *service_name)
{
	/* no more stuff in service? */
	if (hcount(service->method_htab) == 0 &&
		hcount(service->event_htab) == 0 &&
		hcount(service->attr_htab) == 0)
	{
		remove_service_path(abus, service_name);

		hdestroy(service->method_htab);
		hdestroy(service->event_htab);
		hdestroy(service->attr_htab);

		free(hkey(abus->service_htab));
		free(hstuff(abus->service_htab));

		hdel(abus->service_htab);
	}
}

static int abus_resp_send(json_rpc_t *json_rpc)
{
	return un_sock_sendto_sock(json_rpc->sock, json_rpc->msgbuf, json_rpc->msglen,
					(struct sockaddr*)&json_rpc->sock_src_addr, json_rpc->sock_addrlen);
}


int abus_thread_stop(abus_t *abus)
{
	pthread_cancel(abus->srv_thread);
	pthread_join(abus->srv_thread, NULL);

	return 0;
}

static void thread_routine_free(void *arg)
{
	void **p = (void **)arg;

	if (*p != NULL) {
		free(*p);
		*p = NULL;
	}
}

/*!
	Get the file descriptor of A-Bus system, for use in poll()/select()

  \param[in] abus pointer to an opaque handle for A-Bus operation
  \return   socket file descriptor, might be -1 if socket not opened already (i.e. no service declared).
  \sa abus_process_incoming()
 */
int abus_get_fd(abus_t *abus)
{
	return abus->sock;
}

/*!
	Process one incoming message from A-Bus socket.

  This function is suitable for poll operation, as well as internal threaded operation.

  \param abus pointer to an opaque handle for A-Bus operation
  \return   0 if successful, non nul value otherwise
  \todo		fix memory-leak of locally allocated buffer and json_rpc upon pthread_cancel()
  \sa abus_get_fd()
 */
int abus_process_incoming(abus_t *abus)
{
	struct sockaddr_un sock_src_addr;
	socklen_t sock_addrlen = sizeof(sock_src_addr);
	json_rpc_t *json_rpc;
	char *buffer;
	ssize_t len;

	if (abus->sock == -1)
		return -EPIPE;

	if (abus->incoming_buffer) {
		buffer = abus->incoming_buffer;
	} else {
		buffer = malloc(JSONRPC_REQ_SZ_MAX);
		if (!buffer)
			return -ENOMEM;
	}

	len = un_sock_recvfrom(abus->sock, buffer, JSONRPC_REQ_SZ_MAX,
					(struct sockaddr*)&sock_src_addr,
					&sock_addrlen);
	if (len < 0) {
		if (!abus->incoming_buffer)
			free(buffer);
		return len;
	}

	json_rpc = abus_process_msg(abus, buffer, len, (const struct sockaddr *)&sock_src_addr, sock_addrlen);

	if (!abus->incoming_buffer)
		free(buffer);

	/* json_rpc==NULL may not mean failure
	   TODO: where to look at instead?
	 */

	if (json_rpc && !abus_method_is_threaded((abus_method_t*)json_rpc->cb_context)) {
		if (json_rpc->msglen)
			abus_resp_send(json_rpc);

		json_rpc_cleanup(json_rpc);
	}

	return 0;
}

/*
 \internal
 */
void *abus_thread_routine(void *arg)
{
	abus_t *abus = (abus_t *)arg;
	char *buffer;

	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

	buffer = malloc(JSONRPC_REQ_SZ_MAX);
	if (!buffer) {
		LogError("%s: allocation failed: %s", __func__, strerror(errno));
		pthread_exit(NULL);
	}

	/* NB: limited to 16 bytes according to prctl(2) */
	memset(buffer, 0, 17);
	strcpy(buffer, "abus:");
	get_thread_name(buffer+5);
	set_thread_name(buffer);

	pthread_cleanup_push(thread_routine_free, &abus->incoming_buffer);

	abus->incoming_buffer = buffer;

	while ((volatile int)abus->conf.poll_operation == false) {

		if (abus_process_incoming(abus) < 0)
			break;
	}

	pthread_cleanup_pop(1);

	return NULL;
}

static void abus_service_path(const abus_t *abus, const char *service_name, char *service_path)
{
	/* TODO: prefix from env variable */
	snprintf(service_path, UNIX_PATH_MAX-1, "%s/%s", abus_prefix, service_name);
}

/*
  NB: '/' (slash) is forbidden in service_name
 */
int create_service_path(abus_t *abus, const char *service_name)
{
	char service_path[UNIX_PATH_MAX];
	char pid_rel_path[UNIX_PATH_MAX];
	int ret;

	if (strchr(service_name, '/'))
		return -EINVAL;

	ret = abus_launch_thread_ondemand(abus);
	if (ret)
		return ret;

	snprintf(pid_rel_path, sizeof(pid_rel_path)-1, "_%d", getpid());

	if (strlen(service_name) > 0) {

		abus_service_path(abus, service_name, service_path);
#if 0
		struct stat svcstat;

		ret = stat(service_path, &svcstat);
		if (ret == 0) {
			char str[32];
			/* FIXME don't use abus_attr_get_str() because
			   it's not using the AF_UNIX socket
			 */
			if (abus_attr_get_str(abus, service_name, "abus.version",
										str, sizeof(str), 200) == 0)
				return -EEXIST;
		}
#endif
		unlink(service_path);
	
		/* /tmp/abus/<servicename> -> _<pid> */
		ret = symlink(pid_rel_path, service_path);
		if (ret == -1)
			return -errno;
	}

	return 0;
}

int remove_service_path(abus_t *abus, const char *service_name)
{
	char service_path[UNIX_PATH_MAX];

	if (strchr(service_name, '/'))
		return -EINVAL;

	abus_service_path(abus, service_name, service_path);

	unlink(service_path);

	return 0;
}


static int service_lookup(abus_t *abus, const char *service_name, bool create, abus_service_t **service_p)
{
	int srv_len = strlen(service_name);
	abus_service_t *service;
	abus_method_t *new_method;
	abus_attr_t *new_attr;
	int ret = 0;

	if (!abus->service_htab)
		abus->service_htab = hcreate(3);

	if (hfind(abus->service_htab, service_name, srv_len)) {
		service = hstuff(abus->service_htab);
	} else if (!create) {
		service = NULL;
		ret = JSONRPC_NO_METHOD;
	} else {
		*service_p = NULL;
		if (!abus_check_valid_service_name(service_name, JSONRPC_SVCNAME_SZ_MAX+1))
			return JSONRPC_INVALID_REQUEST;

		ret = create_service_path(abus, service_name);
		if (ret)
			return ret;

		service = calloc(1, sizeof(abus_service_t));

		service->method_htab = hcreate(3);
		service->event_htab = hcreate(3);
		service->attr_htab = hcreate(3);
		pthread_mutex_init(&service->attr_mutex, NULL);

		hadd(abus->service_htab, strdup(service_name), srv_len, service);

		new_attr = calloc(1, sizeof(abus_attr_t));
		new_attr->flags = ABUS_RPC_CONST;
		new_attr->auto_alloc = false;
		new_attr->ref.type = JSON_STRING;
		new_attr->ref.length = sizeof(abus_version);
		new_attr->ref.u.data = (char*)abus_version;
		new_attr->descr = strdup("Version of the A-Bus library for this service");
		hadd(service->attr_htab, strdup("abus.version"), strlen("abus.version"), new_attr);

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

		new_method = calloc(1, sizeof(abus_method_t));
		new_method->callback = &abus_req_attr_get_cb;
		new_method->flags = 0;
		new_method->arg = abus;
		hadd(service->method_htab, strdup(ABUS_GET_METHOD), strlen(ABUS_GET_METHOD), new_method);

		new_method = calloc(1, sizeof(abus_method_t));
		new_method->callback = &abus_req_attr_set_cb;
		new_method->flags = 0;
		new_method->arg = abus;
		hadd(service->method_htab, strdup(ABUS_SET_METHOD), strlen(ABUS_SET_METHOD), new_method);
	}
	*service_p = service;

	return ret;
}

static int method_lookup(abus_t *abus, const char *service_name, const char *method_name, bool create, abus_service_t **service_p, abus_method_t **method)
{
	int mth_len = strlen(method_name);
	abus_service_t *service;
	int ret;

	pthread_mutex_lock(&abus->mutex);

	ret = service_lookup(abus, service_name, create, &service);

	if (ret) {
		pthread_mutex_unlock(&abus->mutex);
		*method = NULL;
		return ret;
	}

	if (service_p)
		*service_p = service;

	if (hfind(service->method_htab, method_name, mth_len)) {
		*method = hstuff(service->method_htab);
	} else if (!create) {
		pthread_mutex_unlock(&abus->mutex);
		*method = NULL;
		return JSONRPC_NO_METHOD;
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
	int ret;

	pthread_mutex_lock(&abus->mutex);

	ret = service_lookup(abus, service_name, create, &service);

	if (ret) {
		pthread_mutex_unlock(&abus->mutex);
		*event = NULL;
		return ret;
	}

	if (service_p)
		*service_p = service;

	if (hfind(service->event_htab, event_name, evt_len)) {
		*event = hstuff(service->event_htab);
	} else if (!create) {
		pthread_mutex_unlock(&abus->mutex);
		*event = NULL;
		return JSONRPC_NO_METHOD;
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
	if (method->callback) {
		if (abus_method_is_excl(method))
			pthread_mutex_lock(&method->excl_mutex);

		method->callback(json_rpc, method->arg);

		if (abus_method_is_excl(method))
			pthread_mutex_unlock(&method->excl_mutex);
	}

	if (method->flags & ABUS_RPC_ASYNC) {
		json_rpc_t *req_json_rpc;

		req_json_rpc = json_rpc->async_req_context;

		pthread_mutex_lock(&req_json_rpc->mutex);
		/* mark as executed through req_json_rpc->cb_context */
		req_json_rpc->cb_context = NULL;
		pthread_cond_broadcast(&req_json_rpc->cond);
		/* release resp_handler helper */
		free(method);
		pthread_mutex_unlock(&req_json_rpc->mutex);
	}
}

/*
  Execute RPC in its own thread
 */
static void *abus_threaded_rpc_routine(void *arg)
{
	json_rpc_t *json_rpc = (json_rpc_t *)arg;
	abus_method_t *method = (abus_method_t *)json_rpc->cb_context;
	int ret;

	if (json_rpc->method_name)
		set_thread_name(json_rpc->method_name);

	abus_call_callback(method, json_rpc);

	if (json_rpc->service_name && json_rpc->method_name &&
					(!json_val_is_undef(&json_rpc->id) || json_rpc->error_code)) {
		ret = json_rpc_resp_finalize(json_rpc);
		if (ret == 0)
			abus_resp_send(json_rpc);
	}

	json_rpc->msglen = 0;
	json_rpc_cleanup(json_rpc);

	return NULL;
}

/* \internal to libabus
 * json_rpc is to be malloc'ed for ABUS_RPC_THREADED
 * TODO: there might be more than one call for subscribed events..
 */
static int abus_do_rpc(abus_t *abus, json_rpc_t *json_rpc, abus_method_t *method)
{
	if (abus_method_is_threaded(method)) {
		pthread_attr_t attr;
		pthread_t th;
		int ret;

		json_rpc->cb_context = method;

		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

		ret = pthread_create(&th, &attr, &abus_threaded_rpc_routine, json_rpc);
		pthread_attr_destroy(&attr);
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

  Redeclaration of an existing method is allowed.

  Rem: there's no need to call abus_undecl_method() before calling abus_cleanup().

  \param abus	pointer to A-Bus handle
  \param[in] service_name	name of service where the method belongs to
  \param[in] method_name	name of method to be exposed
  \param[in] method_callback	pointer to call-back function
  \param[in] flags	flags specifying method callback
  \param[in] arg	cookie to be passed back to method_callback
  \param[in] descr	string describing the method to be declared, may be NULL
  \param[in] fmt	abus_format describing the argument of method, may be NULL
  \param[in] result_fmt	abus_format describing the result of method, may be NULL

  \return 0 if successful, non nul value otherwise
  \sa abus_undecl_method()
 */
int abus_decl_method(abus_t *abus, const char *service_name, const char *method_name,
				abus_callback_t method_callback, int flags, void *arg,
				const char *descr, const char *fmt, const char *result_fmt)
{
	abus_method_t *method;
	int ret;

	ret = method_lookup(abus, service_name, method_name, CreateIfNotThere, NULL, &method);
	if (ret)
		return ret;

	pthread_mutex_lock(&abus->mutex);

	if (method->descr)
		free(method->descr);
	if (method->fmt)
		free(method->fmt);
	if (method->result_fmt)
		free(method->result_fmt);

	method->callback = method_callback;
	method->flags = flags;
	method->arg = arg;
	method->descr = descr ? strdup(descr) : NULL;
	method->fmt = fmt ? strdup(fmt) : NULL;
	method->result_fmt = result_fmt ? strdup(result_fmt) : NULL;

	if (abus_method_is_excl(method))
		pthread_mutex_init(&method->excl_mutex, NULL);

	pthread_mutex_unlock(&abus->mutex);

	return 0;
}

/*!
	Undeclare an A-Bus method

  \param[in] service_name	name of service where the method belongs to
  \param[in] method_name	name of method to be unexposed
  \param abus	pointer to A-Bus handle
  \return 0 if successful, non nul value otherwise
  \sa abus_decl_method()
 */
int abus_undecl_method(abus_t *abus, const char *service_name, const char *method_name)
{
	abus_method_t *method;
	abus_service_t *service;
	int ret;

	ret = method_lookup(abus, service_name, method_name, LookupOnly, &service, &method);
	if (ret)
		return ret;

	if (!service || !method)
		return JSONRPC_NO_METHOD;

	pthread_mutex_lock(&abus->mutex);

	if (abus_method_is_excl(method))
		pthread_mutex_destroy(&method->excl_mutex);

	if (method->descr)
		free(method->descr);

	if (method->fmt)
		free(method->fmt);

	if (method->result_fmt)
		free(method->result_fmt);

	/* FIXME: assumes the hashtab still pointing at element found */
	free(hkey(service->method_htab));
	free(hstuff(service->method_htab));
	hdel(service->method_htab);

	/* no more stuff in service? */
	service_may_cleanup(abus, service, service_name);

	pthread_mutex_unlock(&abus->mutex);

	return 0;
}


/*!
	Initialize for request method, client side

  Implicit: expects a response, otherwise use notification/event

  Rem: an empty \a service_name and a \a method_name of "*" will make
  abus_request_method_invoke() get the list of running services on the machine.

  \param[in] service_name	name of service where the method belongs to
  \param[in] method_name	name of method to be later invoked
  \param abus	pointer to A-Bus handle
  \return json_rpc pointer to an opaque handle of a JSON RPC if successful, a NULL pointer otherwise
  \sa abus_request_method_invoke(), abus_request_method_cleanup()
 */
json_rpc_t *abus_request_method_init(abus_t *abus, const char *service_name, const char *method_name)
{
	json_rpc_t *json_rpc;
	int ret;

	json_rpc = json_rpc_req_init(service_name, method_name, abus->id++);
	if (!json_rpc)
		return NULL;

	/* request for event subscribe MUST be sent on A-Bus socket
	 * in order to get the notifications on that socket
	 */
	if (!strcmp(method_name, ABUS_SUBSCRIBE_METHOD)) {
		/* A-Bus thread most likely already started/socket created
		 * by abus_event_subscribe()
		 */
		ret = abus_launch_thread_ondemand(abus);
		if (ret != 0) {
			json_rpc_cleanup(json_rpc);
			return NULL;
		}
		json_rpc->sock = abus->sock;
	}

	return json_rpc;
}

/*
	To be used by abus_request_method_invoke_async()
 */
static int abus_req_track(abus_t *abus, json_rpc_t *json_rpc)
{
	pthread_mutex_lock(&abus->mutex);

	if (!abus->outstanding_req_htab)
		abus->outstanding_req_htab = hcreate(1);

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
  \param json_rpc pointer to an opaque handle of a JSON RPC
  \param[in] timeout waiting timeout in milliseconds
  \return   0 if successful or reponse already received, non nul value otherwise
  \sa abus_request_method_invoke_async()
 */
int abus_request_method_wait_async(abus_t *abus, json_rpc_t *json_rpc, int timeout)
{
	int ret;
	struct timespec ts;

	if (!json_rpc->cb_context)
		return 0;

	pthread_mutex_lock(&json_rpc->mutex);

	clock_gettime(CLOCK_REALTIME, &ts);

	ts.tv_sec  +=  timeout / 1000;
	ts.tv_nsec += (timeout % 1000) * 1000000;
	if (ts.tv_nsec >= 1000000000) {
		ts.tv_nsec  -= 1000000000;
		ts.tv_sec++;
	}

	ret = 0;
	while (json_rpc->cb_context && ret == 0)
			ret = pthread_cond_timedwait(&json_rpc->cond, &json_rpc->mutex, &ts);

	pthread_mutex_unlock(&json_rpc->mutex);

	if (ret != 0)
		return -ret;

	return 0;
}

/*!
	Cancel an asynchronous method request

  \param abus	pointer to A-Bus handle
  \param json_rpc pointer to an opaque handle of a JSON RPC
  \return   0 if successful, non nul value otherwise
  \sa abus_request_method_invoke_async()
 */
int abus_request_method_cancel_async(abus_t *abus, json_rpc_t *json_rpc)
{
	json_rpc_t *json_rpc_found;
	abus_method_t *resp_handler = json_rpc->cb_context;

	if (json_val_is_undef(&json_rpc->id) ||
			(resp_handler && !(resp_handler->flags & ABUS_RPC_ASYNC)))
		return -ENXIO;

	json_rpc_found = abus_req_untrack(abus, &json_rpc->id);

	if (json_rpc_found != json_rpc)
		return -ENXIO;

	pthread_mutex_lock(&json_rpc->mutex);
	if (json_rpc->cb_context) {
		free(json_rpc->cb_context);
		json_rpc->cb_context = NULL;
	    /* bust any abus_request_method_wait_async() waiting */
		pthread_cond_broadcast(&json_rpc->cond);
	}
	/* TODO return code if nothing to cancel ? */
	pthread_mutex_unlock(&json_rpc->mutex);

	return 0;
}

/*!
  Asynchronous invocation of a RPC

  \param abus	pointer to A-Bus handle
  \param json_rpc pointer to an opaque handle of a JSON RPC
  \param[in] timeout	receiving timeout in milliseconds
  \param[in] callback	function to be called upon response or timeout. may be NULL.
  \param[in] flags		ABUS_RPC flags
  \param[in] arg			opaque pointer value to be passed to callback. may be NULL.
  \return   0 if successful, non nul value otherwise

  \sa abus_request_method_wait_async(), abus_request_method_cancel_async()
  \todo implement timeout callback
  \todo implement abus_req_service_list() for async
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
	resp_handler->flags = (flags & ~ABUS_RPC_EXCL) | ABUS_RPC_ASYNC;
	resp_handler->arg = arg;

	json_rpc->cb_context = resp_handler;

	assert(!json_val_is_undef(&json_rpc->id));
	abus_req_track(abus, json_rpc);

	/* send the request through serv socket, response coming from this sock */

	ret = un_sock_sendto_svc(abus->sock, json_rpc->msgbuf, json_rpc->msglen, json_rpc->service_name);
	if (ret != 0)
		return ret;

	return 0;
}


/*!
 * Synchronous invocation of a method

  \param abus	pointer to A-Bus handle
  \param json_rpc pointer to an opaque handle of a JSON RPC
  \param[in] flags		ABUS_RPC flags
  \param[in] timeout   receiving timeout in milliseconds
  \return   0 if successful, non nul value otherwise
  \sa abus_request_method_init(), abus_request_method_cleanup()
 */
int abus_request_method_invoke(abus_t *abus, json_rpc_t *json_rpc, int flags, int timeout)
{
	int ret;

	ret = json_rpc_req_finalize(json_rpc);

	if (json_rpc->service_name[0] == '\0' && !strcmp(json_rpc->method_name, "*")) {
		ret = abus_req_service_list(abus, json_rpc, timeout);

		free(json_rpc->msgbuf);
		json_rpc->msgbuf = NULL;
		json_rpc->msglen = 0;
		return ret;
	}

	/* Don't re-use abus->sock as a default,
	 * so as to have faster response time (saves 2 thread switches)
	 *
	 * recycle req msgbuf
	 */
	ret = un_sock_transaction(json_rpc->sock, json_rpc->msgbuf, json_rpc->msglen, json_rpc->msgbufsz, json_rpc->service_name, timeout);
	if (ret < 0)
		return ret;

	json_rpc->msglen = ret;

	ret = json_rpc_parse_msg(json_rpc, json_rpc->msgbuf, json_rpc->msglen);
	if (ret || json_rpc->parsing_status != PARSING_OK || json_rpc->error_code) {
		if (json_rpc->error_code)
			ret = json_rpc->error_code;
		else if (!ret)
			ret = JSONRPC_PARSE_ERROR;
	}

	free(json_rpc->msgbuf);
	json_rpc->msgbuf = NULL;
	json_rpc->msglen = 0;

	return ret;
}

/*!
  Release ressources associated with a RPC

  \param abus	pointer to A-Bus handle
  \param json_rpc pointer to an opaque handle of a JSON RPC
  \return   0 if successful, non nul value otherwise
  \sa abus_request_method_init()
 */
int abus_request_method_cleanup(abus_t *abus, json_rpc_t *json_rpc)
{
	/* potentially */
	abus_request_method_cancel_async(abus, json_rpc);

	json_rpc_cleanup(json_rpc);

	return 0;
}

/*
 \internal
 */
json_rpc_t *abus_process_msg(abus_t *abus, const char *buffer, int len, const struct sockaddr *sock_src_addr, socklen_t sock_addrlen)
{
	json_rpc_t *json_rpc;
	int ret;
	abus_method_t *method = NULL;

	json_rpc = json_rpc_init();
	if (!json_rpc)
		return NULL;

	memcpy(&json_rpc->sock_src_addr, sock_src_addr, sock_addrlen);
	json_rpc->sock_addrlen = sock_addrlen;

	json_rpc->sock = abus->sock;

	ret = json_rpc_parse_msg(json_rpc, buffer, len);
	if (!json_rpc->error_code && (ret || json_rpc->parsing_status != PARSING_OK)) {
		/* TODO send "error" JSON-RPC */
		json_rpc->error_code = ret ? ret : JSONRPC_PARSE_ERROR;
	}

	if (!json_rpc->error_code && json_rpc->service_name && json_rpc->method_name)
	{
		/* this is a request */
		/* TODO: more than one cb possible */

		ret = method_lookup(abus, json_rpc->service_name, json_rpc->method_name, LookupOnly, NULL, &method);
		if (ret)
			json_rpc->error_code = ret;
	
		json_rpc_resp_init(json_rpc);
	
		if (json_rpc->error_code == 0) {
	
			ret = abus_do_rpc(abus, json_rpc, method);
			/* TODO act upon failed ret? */
	
			/* no response for notification or threaded methods */
			if (abus_method_is_threaded(method))
				return json_rpc;

			if (json_val_is_undef(&json_rpc->id)) {
				json_rpc->msglen = 0;
				json_rpc_cleanup(json_rpc);
				return NULL;
			}
		}
	}
	else if (!json_val_is_undef(&json_rpc->id))
	{
		/* this is an async response */
		json_rpc_t *req_json_rpc;

		req_json_rpc = abus_req_untrack(abus, &json_rpc->id);
	
		if (req_json_rpc) {
			method = req_json_rpc->cb_context;
			json_rpc->async_req_context = req_json_rpc;

			ret = abus_do_rpc(abus, json_rpc, method);
			/* TODO act upon failed ret? */
			return json_rpc;
		}

		/* do not send anything */
		json_rpc->msglen = 0;
		json_rpc_cleanup(json_rpc);

		/* no json_rpc_resp_finalize() */
		return NULL;
	}

	ret = json_rpc_resp_finalize(json_rpc);
	/* TODO act upon failed ret? */

	return json_rpc;
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
 * \param[in,out] buflen	pointer to length of buffer of JSON-RPC request/response
 * \param[in] flags		ABUS_RPC flags
 * \param[in] timeout	receiving timeout in milliseconds
  \return   0 if successful, non nul value otherwise
 */
int abus_forward_rpc(abus_t *abus, char *buffer, int *buflen, int flags, int timeout)
{
	int ret;
	char service_name[JSONRPC_SVCNAME_SZ_MAX];

	ret = json_rpc_quickparse_service_name(buffer, *buflen, service_name);
	if (ret != 0)
		return ret;

	/*
	 * rem: recycle req msgbuf
	 */
	ret = un_sock_transaction(-1, buffer, *buflen, JSONRPC_RESP_SZ_MAX, service_name, timeout);
	if (ret < 0) {
		return ret;
	}

	*buflen = ret;

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
	abus_attr_t *attr;

	if (!abus->service_htab || !json_rpc->service_name) {
		json_rpc_set_error(json_rpc, JSONRPC_INTERNAL_ERROR, NULL);
		return;
	}

	pthread_mutex_lock(&abus->mutex);

	if (!hfind(abus->service_htab, json_rpc->service_name, strlen(json_rpc->service_name))) {
		pthread_mutex_unlock(&abus->mutex);
		json_rpc_set_error(json_rpc, JSONRPC_NO_METHOD, NULL);
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
					!strncmp(ABUS_GET_METHOD, method_name, hkeyl(service->method_htab)) ||
					!strncmp(ABUS_SET_METHOD, method_name, hkeyl(service->method_htab)) ||
					!strncmp(ABUS_SUBSCRIBE_METHOD, method_name, hkeyl(service->method_htab)) ||
					!strncmp(ABUS_UNSUBSCRIBE_METHOD, method_name, hkeyl(service->method_htab)))
				continue;
	
			json_rpc_append_args(json_rpc, JSON_OBJECT_BEGIN, -1);

			method = hstuff(service->method_htab);
	
			json_rpc_append_strn(json_rpc, "name", method_name, hkeyl(service->method_htab));
			json_rpc_append_int(json_rpc, "flags", method->flags);
			/* TODO: interpret the param list in an array, or use the JSON-RPC SMD notation? */
			if (method->descr)
				json_rpc_append_str(json_rpc, "descr", method->descr);
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
			/* TODO: interpret the param list in an array, or use the JSON-RPC SMD notation? */
			if (event->descr)
				json_rpc_append_str(json_rpc, "descr", event->descr);
			if (event->fmt)
				json_rpc_append_str(json_rpc, "fmt", event->fmt);

			json_rpc_append_args(json_rpc, JSON_OBJECT_END, -1);
		}
		while (hnext(service->event_htab));

		json_rpc_append_args(json_rpc, JSON_ARRAY_END, -1);
	}

	/* attributes */
	if (service->attr_htab && hfirst(service->attr_htab))
	{
		json_rpc_append_args(json_rpc,
						JSON_KEY, "attrs", -1,
						JSON_ARRAY_BEGIN, 
						-1);

		do {
			const char *attr_name = (const char*)hkey(service->attr_htab);
			char c;
	
			attr = hstuff(service->attr_htab);

			c = json_type2char(attr->ref.type);

			json_rpc_append_args(json_rpc, JSON_OBJECT_BEGIN, -1);

			json_rpc_append_strn(json_rpc, "name", attr_name, hkeyl(service->attr_htab));
			json_rpc_append_strn(json_rpc, "type", &c, 1);
			json_rpc_append_bool(json_rpc, "readonly", attr->flags & (ABUS_RPC_RDONLY|ABUS_RPC_CONST));
			json_rpc_append_bool(json_rpc, "constant", attr->flags & ABUS_RPC_CONST);
			if (attr->descr)
				json_rpc_append_str(json_rpc, "descr", attr->descr);

			json_rpc_append_args(json_rpc, JSON_OBJECT_END, -1);
		}
		while (hnext(service->attr_htab));

		json_rpc_append_args(json_rpc, JSON_ARRAY_END, -1);
	}

	pthread_mutex_unlock(&abus->mutex);

	return;
}

int abus_check_valid_service_name(const char *name, size_t maxlen)
{
	/* rem: an empty name is a valid name (e.g. event "methods", etc.) */
	return name && name[0] != '_'
		&& strnlen(name, maxlen) < JSONRPC_SVCNAME_SZ_MAX
		&& !strchr(name, '.')
		&& !strchr(name, '/');
}

/*
  pseudo callback for internal use, to offer get accessor of service list
 */
int abus_req_service_list(abus_t *abus, json_rpc_t *json_rpc, int timeout)
{
	DIR *dirp;
	struct dirent *entry;
	ssize_t ret = 0;
	char service_path[UNIX_PATH_MAX];
	char pid_rel_path[UNIX_PATH_MAX];
	char str[32];

	dirp = opendir(abus_prefix);
	if (!dirp) {
		ret = -errno;
		LogDebug("%s(): opendir(\"%s\") failed: %s", __func__, abus_prefix, strerror(errno));
		json_rpc_set_error(json_rpc, JSONRPC_INTERNAL_ERROR,
				"Failed accessing A-Bus directory");
		return ret;
	}

	json_rpc->last_key_token = TOK_PARAMS;

	json_rpc->last_param_key = strdup("services");
	if (json_rpc_add_array(json_rpc) != 0)
		goto error_dir;

	while ((entry = readdir(dirp)) != NULL) {

		if (entry->d_name[0] == '\0' ||
				!abus_check_valid_service_name(entry->d_name, NAME_MAX))
			continue;

		/* TODO check type is symlink: d_type ? */

		abus_service_path(abus, entry->d_name, service_path);

		/* must be a symbolic link */
		ret = readlink(service_path, pid_rel_path, sizeof(pid_rel_path));
		if (ret <= 0) {
			if (errno != EINVAL)
				LogDebug("%s(): readlink(\"%s\") failed: %s", __func__,
						service_path, strerror(errno));
			continue;
		}

		pid_rel_path[ret] = '\0';

		/* socket name must be in same dir, and begin with '_' */
		if (strchr(pid_rel_path, '/') || pid_rel_path[0] != '_')
			continue;

		/* TODO check pid is alive? */

		if (abus_attr_get_str(abus, entry->d_name, "abus.version",
					str, sizeof(str), timeout) != 0)
			continue;

		/* add object to array and add key "name" & string to object */

		if (json_rpc_add_object_to_array(json_rpc) != 0)
			goto error_dir;

		json_rpc->last_param_key = strdup("name");
		if (json_rpc_add_val(json_rpc, JSON_STRING, entry->d_name,
					strlen(entry->d_name)+1) != 0)
			goto error_dir;
	}

	json_rpc->parsing_status = PARSING_OK;
	ret = 0;

error_dir:
	json_rpc->last_array = NULL;
	json_rpc->pointed_htab = NULL;

	if (dirp)
		closedir(dirp);

	return ret;
}


/* Event business */

#define EVTPREFIX "_event%%"

/* TODO: replace event name mangling with a hash+duplicate handling
         in order to allow shorter method name
 */
static int snprint_event_method(char *str, size_t size, const char *service_name, const char *event_name)
{
	if (size < 1)
		return -EINVAL;

	str[size-1] = '\0';
	return snprintf(str, size-1, EVTPREFIX"%s%%%s", service_name, event_name);
}

static const char *event_name_from_method(const char *str)
{
	if (strlen(str) > strlen(EVTPREFIX))
		return str+strlen(EVTPREFIX);
	return NULL;
}


/*!
	Decalare an A-Bus event in a service

  Redeclaration of an existing event is allowed.

  \param abus	pointer to A-Bus handle
  \param[in] service_name	name of service where the event belongs to
  \param[in] event_name	name of event that may be subscribed to
  \param[in] descr	string describing the event to be declared, may be NULL
  \param[in] fmt	abus_format describing the arguments of the event, may be NULL

  \return 0 if successful, non nul value otherwise
  \todo abus_undecl_event()
 */
int abus_decl_event(abus_t *abus, const char *service_name, const char *event_name, const char *descr, const char *fmt)
{
	abus_event_t *event;
	int ret;

	ret = event_lookup(abus, service_name, event_name, CreateIfNotThere, NULL, &event);
	if (ret)
		return ret;

	pthread_mutex_lock(&abus->mutex);

	if (event->descr)
		free(event->descr);
	if (event->fmt)
		free(event->fmt);

	event->descr = descr ? strdup(descr) : NULL;
	event->fmt = fmt ? strdup(fmt) : NULL;

	pthread_mutex_unlock(&abus->mutex);

	return 0;
}

/**
	Undeclare an A-Bus event

  \param abus	pointer to A-Bus handle
  \param[in] service_name	name of service where the event belongs to
  \param[in] event_name	name of event to be unexposed
  \return 0 if successful, non nul value otherwise
  \sa abus_decl_event()
 */
int abus_undecl_event(abus_t *abus, const char *service_name, const char *event_name)
{
	abus_event_t *event;
	abus_service_t *service;
	int ret;

	ret = event_lookup(abus, service_name, event_name, LookupOnly, &service, &event);
	if (ret)
		return ret;

	if (!service || !event)
		return JSONRPC_NO_METHOD;

	pthread_mutex_lock(&abus->mutex);

	if (hfirst(event->subscriber_htab)) do
	{
		/* TODO: unsubscribe from remote services ? */
		free(hkey(event->subscriber_htab));
		free(hstuff(event->subscriber_htab));
	}
	while (hnext(event->subscriber_htab));
	hdestroy(event->subscriber_htab);

	if (event->descr)
		free(event->descr);
	if (event->fmt)
		free(event->fmt);

	/* FIXME: assumes the hashtab still pointing at element found */
	free(hkey(service->event_htab));
	free(hstuff(service->event_htab));
	hdel(service->event_htab);

	service_may_cleanup(abus, service, service_name);

	pthread_mutex_unlock(&abus->mutex);

	return 0;
}


/*!
	Initialize a new event RPC to be published by a service

  \param abus	pointer to A-Bus handle
  \param[in] service_name	name of service where the event belongs to
  \param[in] event_name	name of event that's about to be published
  \return json_rpc pointer to an opaque handle of a JSON RPC if successful, NULL pointer otherwise
  \sa abus_request_event_cleanup()
 */
json_rpc_t *abus_request_event_init(abus_t *abus, const char *service_name, const char *event_name)
{
	json_rpc_t *json_rpc;
	abus_event_t *event;
	int ret;
	char event_method_name[JSONRPC_METHNAME_SZ_MAX];

	ret = abus_launch_thread_ondemand(abus);
	if (ret)
		return NULL;

	snprint_event_method(event_method_name, JSONRPC_METHNAME_SZ_MAX, service_name, event_name);

	json_rpc = json_rpc_req_init("", event_method_name, -1);

	json_val_free(&json_rpc->id);
	json_rpc->id.type = JSON_NONE;

	json_rpc_append_str(json_rpc, "service", service_name);
	json_rpc_append_str(json_rpc, "event", event_name);

	/* implicit abus_decl_event() */
	event_lookup(abus, service_name, event_name, CreateIfNotThere, NULL, &event);

	/* re-use cb_context for event context */
	json_rpc->cb_context = event;

	/* may be necessary in case of failed delivery.
	   no need of strdup for service name
	 */
	if (!hfind(abus->service_htab, service_name, strlen(service_name))) {
		json_rpc_cleanup(json_rpc);
		return NULL;
	}
	json_rpc->evt_service_name = (const char *)hkey(abus->service_htab);

	return json_rpc;
}

/*!
	Publish (i.e. send) an event from a service

	The notification is sent to all the subscribed end-points.
	If notification delivery fails, the troublesome end-point get unsubscribed.

  \param abus	pointer to A-Bus handle
  \param json_rpc pointer to an opaque handle of a JSON RPC
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
		struct sockaddr *sockaddrun;

		sockaddrun = hstuff(event->subscriber_htab);

		ret = un_sock_sendto_sock(abus->sock, json_rpc->msgbuf, json_rpc->msglen,
					(const struct sockaddr *)sockaddrun, un_sock_socklen(sockaddrun));
		if (ret < 0) {
			const char *event_name;
			/* remove that subscriber if delivery failed */
			LogDebug("%s(): get rid of gone subscriber", __func__);

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
  \param json_rpc pointer to an opaque handle of a JSON RPC
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
  \param[in] service_name	name of service where the event belongs to
  \param[in] event_name	name of event to subscribe to
  \param[in] callback	function to be called upon event publication or subscribe timeout.
  \param[in] flags		ABUS_RPC flags
  \param[in] arg		opaque pointer value to be passed to \a callback. may be NULL.
  \param[in] timeout	A request has to be sent to the service to subscribe from.
                        This is the receive timeout of that subscribe request, in milliseconds.
  \return   0 if successful, non nul value otherwise
  \sa abus_event_unsubscribe(), abus_attr_subscribe_onchange()
  \todo support for more than one subscribe in a process to the same event
 */
int abus_event_subscribe(abus_t *abus, const char *service_name, const char *event_name, abus_callback_t callback, int flags, void *arg, int timeout)
{
	char event_method_name[JSONRPC_METHNAME_SZ_MAX];
	json_rpc_t *json_rpc;
	int ret;

	/* TODO: hash callback&arg into method name?
		in order to allow more than one subscribe for the same svc/event name
	 */
	snprint_event_method(event_method_name, JSONRPC_METHNAME_SZ_MAX, service_name, event_name);

	ret = abus_decl_method(abus, "", event_method_name,
					callback, flags, arg, NULL, NULL, NULL);
	if (ret != 0)
		return ret;

	json_rpc = abus_request_method_init(abus, service_name, ABUS_SUBSCRIBE_METHOD);
	if (!json_rpc)
		return -ENOMEM;

	json_rpc_append_str(json_rpc, "event", event_name);
	if (flags & ABUS_RPC_WITHOUTVAL)
		json_rpc_append_bool(json_rpc, "wihtout_value", flags & ABUS_RPC_WITHOUTVAL);

	/* MUST use the A-Bus sock in order to get the event RPC issued on that socket,
		hence the use of abus_request_method_invoke_async()
	 */
	ret = abus_request_method_invoke_async(abus, json_rpc, timeout, NULL, flags, NULL);
	if (ret != 0)
		return ret;

	ret = abus_request_method_wait_async(abus, json_rpc, timeout);
	if (ret != 0)
		return ret;

	abus_request_method_cleanup(abus, json_rpc);

	return 0;
}

/*!
 * Unregister callback for event notification and send unsubscription to service.

  \param abus	pointer to A-Bus handle
  \param[in] service_name	name of service where the event belongs to
  \param[in] event_name	name of event to unsubscribe from
  \param[in] callback	function pointer that was passed to the abus_event_subscribe() call,
                        because more than one callback may be subscribe to the same event.
  \param[in] arg		opaque pointer value that was passed to the abus_event_subscribe() call.
  \param[in] timeout	A request has to be sent to the service to unsubscribe from.
                        This is the receive timeout of that unsubscribe request, in milliseconds.
  \return   0 if successful, non nul value otherwise
  \sa abus_event_subscribe(), abus_attr_unsubscribe_onchange()
 */
int abus_event_unsubscribe(abus_t *abus, const char *service_name, const char *event_name, abus_callback_t callback, void *arg, int timeout)
{
	char event_method_name[JSONRPC_METHNAME_SZ_MAX];
	json_rpc_t *json_rpc;
	int ret;

	snprint_event_method(event_method_name, JSONRPC_METHNAME_SZ_MAX, service_name, event_name);

	ret = abus_undecl_method(abus, "", event_method_name);
	if (ret != 0)
		return ret;

	json_rpc = abus_request_method_init(abus, service_name, ABUS_UNSUBSCRIBE_METHOD);
	if (!json_rpc)
		return -ENOMEM;

	json_rpc_append_str(json_rpc, "event", event_name);

	ret = abus_request_method_invoke(abus, json_rpc, 0, timeout);
	if (ret != 0)
		return ret;

	abus_request_method_cleanup(abus, json_rpc);

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
	const char *event_name;
	void *key, *stuff;
	int ret;
	size_t event_len;
	bool withoutval = true;

	ret = json_rpc_get_strp(json_rpc, "event", &event_name, &event_len);
	if (ret != 0 || event_len == 0) {
		json_rpc_set_error(json_rpc, ret, NULL);
		return;
	}

	/* optional */
	json_rpc_get_bool(json_rpc, "without_value", &withoutval);

	ret = event_lookup(abus, json_rpc->service_name, event_name, LookupOnly, NULL, &event);
	if (ret) {
		json_rpc_set_error(json_rpc, ret, NULL);
		return;
	}

	/* FIXME: locking unneeded because the abus_thread is the only user? */
	if (!event->subscriber_htab)
		event->subscriber_htab = hcreate(1);

#if 0
	LogDebug("####%s %s %p %u %*s", json_rpc->service_name, event_name, event->subscriber_htab, event->uniq_subscriber_cnt,
					json_rpc->sock_addrlen-1, un_sock_name((const struct sockaddr *)&json_rpc->sock_src_addr));
#endif

	key = memdup(&event->uniq_subscriber_cnt, sizeof(event->uniq_subscriber_cnt));
	event->uniq_subscriber_cnt++;
	stuff = memdup(&json_rpc->sock_src_addr, json_rpc->sock_addrlen);

	/* TODO: add the withoutval flag to stuff */
	hadd(event->subscriber_htab, key, sizeof(event->uniq_subscriber_cnt), stuff);
}

int abus_unsubscribe_service(abus_t *abus, const char *service_name, const char *event_name)
{
	abus_event_t *event;
	int ret;

	ret = event_lookup(abus, service_name, event_name, LookupOnly, NULL, &event);
	if (ret || !event || !event->subscriber_htab) {
		return ret ? ret : JSONRPC_INTERNAL_ERROR;
	}

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
	const char *event_name;
	int ret;
	size_t event_len;

	ret = json_rpc_get_strp(json_rpc, "event", &event_name, &event_len);
	if (ret != 0 || event_len == 0) {
		json_rpc_set_error(json_rpc, ret, NULL);
		return;
	}

	ret = abus_unsubscribe_service(abus, json_rpc->service_name, event_name);
	if (ret) {
		json_rpc_set_error(json_rpc, ret, NULL);
		return;
	}
}

static int attr_lookup(abus_t *abus, const char *service_name, const char *attr_name, bool create, abus_service_t **service_p, abus_attr_t **attr)
{
	int attr_len = strlen(attr_name);
	abus_service_t *service;
	int ret;

	pthread_mutex_lock(&abus->mutex);

	ret = service_lookup(abus, service_name, create, &service);

	if (ret) {
		pthread_mutex_unlock(&abus->mutex);
		*attr = NULL;
		return ret;
	}

	if (service_p)
		*service_p = service;

	if (hfind(service->attr_htab, attr_name, attr_len)) {
		*attr = hstuff(service->attr_htab);
	} else if (!create) {
		pthread_mutex_unlock(&abus->mutex);
		*attr = NULL;
		return JSONRPC_NO_METHOD;
	} else {
		*attr = calloc(1, sizeof(abus_attr_t));
		hadd(service->attr_htab, strdup(attr_name), attr_len, *attr);
	}

	pthread_mutex_unlock(&abus->mutex);

	return 0;
}


static int attr_append_type(json_rpc_t *json_rpc, const char *attr_name, int type, const void *data)
{
	switch(type) {
	case JSON_INT:
		return json_rpc_append_int(json_rpc, attr_name, *(const int *)data);
	case JSON_LLINT:
		return json_rpc_append_llint(json_rpc, attr_name, *(const long long *)data);
	case JSON_FALSE:
	case JSON_TRUE:
		return json_rpc_append_bool(json_rpc, attr_name, *(const bool *)data);
	case JSON_FLOAT:
		return json_rpc_append_double(json_rpc, attr_name, *(const double *)data);
	case JSON_STRING:
		return json_rpc_append_str(json_rpc, attr_name, data);
	default:
		return json_rpc_set_error(json_rpc, JSONRPC_INTERNAL_ERROR, NULL);
	}
	return -EINVAL;
}

static int attr_append(abus_t *abus, json_rpc_t *json_rpc, const char *service_name, const char *attr_name)
{
	abus_service_t *service;
	abus_attr_t *attr;
	int ret, attr_name_len;

	ret = attr_lookup(abus, service_name, attr_name, LookupOnly, &service, &attr);
	if (ret == 0)
		return attr_append_type(json_rpc, attr_name, attr->ref.type, attr->ref.u.data);

	attr_name_len = strlen(attr_name);
	if (attr_name_len > 0 && attr_name[attr_name_len-1] != '.') {
			json_rpc_set_error(json_rpc, JSONRPC_NO_METHOD, NULL);
			return JSONRPC_NO_METHOD;
	}

	/* iterate over prefix when exact match not found */
	if (hfirst(service->attr_htab)) do
	{
		const char *key = (const char *)hkey(service->attr_htab);

		if (!strncmp(key, attr_name, attr_name_len)) {
			attr = hstuff(service->attr_htab);
			ret = attr_append_type(json_rpc, key, attr->ref.type, attr->ref.u.data);
			if (ret)
				return ret;
		}
	}
	while (hnext(service->attr_htab));

	return 0;
}

/**
	Append to a RPC a new parameter and its value from a declared attribute

  \param abus	pointer to A-Bus handle
  \param json_rpc pointer to an opaque handle of a JSON RPC
  \param[in] service_name	name of service where the attribute belongs to
  \param[in] attr_name	name of attribute to append
  \return   0 if successful, non nul value otherwise
  \sa abus_decl_attr_*
 */
int abus_append_attr(abus_t *abus, json_rpc_t *json_rpc, const char *service_name, const char *attr_name)
{
	return attr_append(abus, json_rpc, service_name, attr_name);
}

static char json_type2char(int json_type)
{
	switch(json_type) {
	case JSON_INT:
		return 'i';
	case JSON_LLINT:
		return 'l';
	case JSON_FALSE:
	case JSON_TRUE:
		return 'b';
	case JSON_FLOAT:
		return 'f';
	case JSON_STRING:
		return 's';
	}
	return '?';
}

#define ABUS_ATTR_CHANGED_PREFIX "attr_changed%%"

static int attr_decl_type(abus_t *abus, const char *service_name, const char *attr_name, int json_type, void *val, int len, int flags, const char *descr)
{
	abus_attr_t *attr;
	int ret;
	char event_name[JSONRPC_METHNAME_SZ_MAX];
	char event_fmt[JSONRPC_METHNAME_SZ_MAX];

	ret = attr_lookup(abus, service_name, attr_name, CreateIfNotThere, NULL, &attr);
	if (ret)
		return ret;

	pthread_mutex_lock(&abus->mutex);

	if (attr->descr)
		free(attr->descr);
	if (attr->auto_alloc && attr->ref.u.data)
		free(attr->ref.u.data);

	attr->flags = flags;

	if (val) {
		attr->auto_alloc = false;
	} else {
		attr->auto_alloc = true;
		val = calloc(1, len);
		/* TODO: cleanup if alloc failed */
		if (!val)
			ret = -ENOMEM;
	}

	attr->ref.type = json_type;
	attr->ref.length = len;
	attr->ref.u.data = val;

	attr->descr = descr ? strdup(descr) : NULL;

	pthread_mutex_unlock(&abus->mutex);

	if (!(flags & ABUS_RPC_CONST)) {
		snprintf(event_name, sizeof(event_name), ABUS_ATTR_CHANGED_PREFIX "%s", attr_name);
		snprintf(event_fmt, sizeof(event_fmt), "%s:%c:%s",
					attr_name, json_type2char(json_type), descr);

		ret = abus_decl_event(abus, service_name, event_name, descr, event_fmt);
		if (ret)
			return ret;
	}

	return ret;
}

/**
  Declare an attribute of type integer in a service

  Redeclaration of an existing attribute is allowed.

  \param abus	pointer to A-Bus handle
  \param[in] service_name	name of service where the attribute belongs to
  \param[in] attr_name	name of attribute to declare
  \param[in,out] val	pointer to the variable holding the attribute value, NULL for auto allocation
  \param[in] flags		zero or ABUS_RPC_RDONLY flag if attribute is read-only, ABUS_RPC_CONST if attribute constant
  \param[in] descr	string describing the event to be declared, may be NULL
  \return   0 if successful, non nul value otherwise
  \sa abus_undecl_attr()
 */
int abus_decl_attr_int(abus_t *abus, const char *service_name, const char *attr_name, int *val, int flags, const char *descr)
{
	return attr_decl_type(abus, service_name, attr_name, JSON_INT, val, sizeof(int), flags, descr);
}

/**
  Declare an attribute of type long long integer in a service

  Redeclaration of an existing attribute is allowed.

  \param abus	pointer to A-Bus handle
  \param[in] service_name	name of service where the attribute belongs to
  \param[in] attr_name	name of attribute to declare
  \param[in,out] val	pointer to the variable holding the attribute value, NULL for auto allocation
  \param[in] flags		zero or ABUS_RPC_RDONLY flag if attribute is read-only, ABUS_RPC_CONST if attribute constant
  \param[in] descr	string describing the event to be declared, may be NULL
  \return   0 if successful, non nul value otherwise
  \sa abus_undecl_attr()
 */
int abus_decl_attr_llint(abus_t *abus, const char *service_name, const char *attr_name, long long *val, int flags, const char *descr)
{
	return attr_decl_type(abus, service_name, attr_name, JSON_LLINT, val, sizeof(long long), flags, descr);
}

/**
  Declare an attribute of type bool in a service

  Redeclaration of an existing attribute is allowed.

  \param abus	pointer to A-Bus handle
  \param[in] service_name	name of service where the attribute belongs to
  \param[in] attr_name	name of attribute to declare
  \param[in,out] val	pointer to the variable holding the attribute value, NULL for auto allocation
  \param[in] flags		zero or ABUS_RPC_RDONLY flag if attribute is read-only, ABUS_RPC_CONST if attribute constant
  \param[in] descr	string describing the event to be declared, may be NULL
  \return   0 if successful, non nul value otherwise
  \sa abus_undecl_attr()
 */
int abus_decl_attr_bool(abus_t *abus, const char *service_name, const char *attr_name, bool *val, int flags, const char *descr)
{
	return attr_decl_type(abus, service_name, attr_name, JSON_TRUE, val, sizeof(bool), flags, descr);
}

/**
  Declare an attribute of type double in a service

  Redeclaration of an existing attribute is allowed.

  \param abus	pointer to A-Bus handle
  \param[in] service_name	name of service where the attribute belongs to
  \param[in] attr_name	name of attribute to declare
  \param[in,out] val	pointer to the variable holding the attribute value, NULL for auto allocation
  \param[in] flags		zero or ABUS_RPC_RDONLY flag if attribute is read-only, ABUS_RPC_CONST if attribute constant
  \param[in] descr	string describing the event to be declared, may be NULL
  \return   0 if successful, non nul value otherwise
  \sa abus_undecl_attr()
 */
int abus_decl_attr_double(abus_t *abus, const char *service_name, const char *attr_name, double *val, int flags, const char *descr)
{
	return attr_decl_type(abus, service_name, attr_name, JSON_FLOAT, val, sizeof(double), flags, descr);
}

/**
  Declare an attribute of type string in a service

  Redeclaration of an existing attribute is allowed.

  \param abus	pointer to A-Bus handle
  \param[in] service_name	name of service where the attribute belongs to
  \param[in] attr_name	name of attribute to declare
  \param[in,out] val	pointer to the variable holding the attribute value, NULL for auto allocation
  \param[in] n	maximum memory size of the string, including nul end-of-string
  \param[in] flags		zero or ABUS_RPC_RDONLY flag if attribute is read-only, ABUS_RPC_CONST if attribute constant
  \param[in] descr	string describing the event to be declared, may be NULL
  \return   0 if successful, non nul value otherwise
  \sa abus_undecl_attr()
 */
int abus_decl_attr_str(abus_t *abus, const char *service_name, const char *attr_name, char *val, size_t n, int flags, const char *descr)
{
	return attr_decl_type(abus, service_name, attr_name, JSON_STRING, val, n, flags, descr);
}

/**
	Undeclare an A-Bus attribute

  \param abus	pointer to A-Bus handle
  \param[in] service_name	name of service where the attribute belongs to
  \param[in] attr_name	name of attribute to be unexposed
  \return 0 if successful, non nul value otherwise
  \sa abus_decl_attr_*()
 */
int abus_undecl_attr(abus_t *abus, const char *service_name, const char *attr_name)
{
	abus_attr_t *attr;
	abus_service_t *service;
	int ret, flags;
	char event_name[JSONRPC_METHNAME_SZ_MAX];

	ret = attr_lookup(abus, service_name, attr_name, LookupOnly, &service, &attr);
	if (ret)
		return ret;

	if (!service || !attr)
		return JSONRPC_NO_METHOD;

	pthread_mutex_lock(&abus->mutex);

	if (attr->descr)
		free(attr->descr);
	if (attr->auto_alloc && attr->ref.u.data)
		free(attr->ref.u.data);

	flags = attr->flags;

	/* FIXME: assumes the hashtab still pointing at element found */
	free(hkey(service->attr_htab));
	free(hstuff(service->attr_htab));
	hdel(service->attr_htab);

	service_may_cleanup(abus, service, service_name);

	pthread_mutex_unlock(&abus->mutex);

	/* unregister associated attr_changed events */
	if (!(flags & ABUS_RPC_CONST)) {
		snprintf(event_name, sizeof(event_name), ABUS_ATTR_CHANGED_PREFIX "%s", attr_name);
		abus_undecl_event(abus, service_name, event_name);
	}

	return 0;
}

/**
  Notify that the value of an attribute has changed

  abus_attr_changed() will publish notification to all subscribers
  that the value of the attribute has its value changed.

  \param abus	pointer to A-Bus handle
  \param[in] service_name	name of service where the attribute belongs to
  \param[in] attr_name	name of attribute which value has changed
  \return   0 if successful, non nul value otherwise
  \sa abus_decl_attr_*(), abus_attr_subscribe_onchange()
 */
int abus_attr_changed(abus_t *abus, const char *service_name, const char *attr_name)
{
	json_rpc_t *json_rpc;
	int ret;
	char event_name[JSONRPC_METHNAME_SZ_MAX];

	snprintf(event_name, sizeof(event_name), ABUS_ATTR_CHANGED_PREFIX "%s", attr_name);

	json_rpc = abus_request_event_init(abus, service_name, event_name);
	if (!json_rpc)
		return -ENOMEM;

	/* TODO: honor the without_val flag per subscribers */
	ret = attr_append(abus, json_rpc, service_name, attr_name);
	if (ret == 0)
		ret = abus_request_event_publish(abus, json_rpc, 0);

	abus_request_event_cleanup(abus, json_rpc);

	return ret;
}

static int attr_get_local(abus_t *abus, const abus_attr_t *attr, int json_type, void *val, size_t len)
{
	/* promoted int */
	if (json_type == JSON_LLINT && attr->ref.type == JSON_INT) {
		*(long long*)val = (long long) *(const int*)attr->ref.u.data;
		return 0;
	}

	/* demoted int */
	if (json_type == JSON_INT && attr->ref.type == JSON_LLINT) {
		long long ll = *(const long long*)attr->ref.u.data;
		if (ll > (long long)INT_MAX || ll < (long long)INT_MIN)
			return -ERANGE;
		*(int *)val = ll;
		return 0;
	}

	if (!json_rpc_type_eq(attr->ref.type, json_type))
		return JSONRPC_INVALID_METHOD;

	switch (json_type) {
	case JSON_INT:
		*(int*)val = *(const int*)attr->ref.u.data;
		break;
	case JSON_LLINT:
		/* FIXME: this may not be atomic */
		*(long long*)val = *(const long long*)attr->ref.u.data;
		break;
	case JSON_FALSE:
	case JSON_TRUE:
		*(bool*)val = *(const bool*)attr->ref.u.data;
		break;
	case JSON_FLOAT:
		/* FIXME: this may not be atomic */
		*(double*)val = *(const double*)attr->ref.u.data;
		break;
	case JSON_STRING:
		/* FIXME: this is not atomic */
		strncpy(val, attr->ref.u.data, len);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}


static int attr_get_type(abus_t *abus, const char *service_name, const char *attr_name, int json_type, void *val, size_t len, int timeout)
{
	json_rpc_t *json_rpc;
	abus_service_t *service;
	abus_attr_t *attr;
	int ret;

	/* no RPC where attr's service is local to process/abus context */
	if (attr_lookup(abus, service_name, attr_name, LookupOnly, &service, &attr) == 0) {
		pthread_mutex_lock(&service->attr_mutex);
		ret = attr_get_local(abus, attr, json_type, val, len);
		pthread_mutex_unlock(&service->attr_mutex);
		return ret;
	}

    json_rpc = abus_request_method_init(abus, service_name, ABUS_GET_METHOD);
    if (!json_rpc)
        return -ENOMEM;

	/* "service.get" "attr":[{"name":attr.a}] -> attr.a:xxx */

	/* begin the array */
	json_rpc_append_args(json_rpc,
					JSON_KEY, "attr", -1,
					JSON_ARRAY_BEGIN,
					JSON_OBJECT_BEGIN,
					-1);

    json_rpc_append_str(json_rpc, "name", attr_name);

	/* end the array */
	json_rpc_append_args(json_rpc,
					JSON_OBJECT_END,
					JSON_ARRAY_END,
					-1);

	ret = abus_request_method_invoke(abus, json_rpc, ABUS_RPC_FLAG_NONE, timeout);
	if (ret != 0) {
		abus_request_method_cleanup(abus, json_rpc);
		return ret;
	}

	switch (json_type) {
	case JSON_INT:
    	ret = json_rpc_get_int(json_rpc, attr_name, (int *)val);
		break;
	case JSON_LLINT:
    	ret = json_rpc_get_llint(json_rpc, attr_name, (long long *)val);
		break;
	case JSON_TRUE:
	case JSON_FALSE:
    	ret = json_rpc_get_bool(json_rpc, attr_name, (bool*)val);
		break;
	case JSON_FLOAT:
    	ret = json_rpc_get_double(json_rpc, attr_name, (double *)val);
		break;
	case JSON_STRING:
    	ret = json_rpc_get_str(json_rpc, attr_name, (char *)val, len);
		break;
	default:
		ret = JSONRPC_INTERNAL_ERROR;
	}

    abus_request_method_cleanup(abus, json_rpc);

	return ret;
}

static int attr_set_local(abus_t *abus, abus_attr_t *attr, const char *service_name, const char *attr_name, int json_type, const void *val, size_t len)
{
	bool attr_changed = false;
	long long ll_val;
	int i_val;

	if (json_type == JSON_INT && attr->ref.type == JSON_LLINT) {
		ll_val = (long long) *(const int*)val;
		val = &ll_val;
		/* promoted int */
		json_type = JSON_LLINT;
	} else if (json_type == JSON_LLINT && attr->ref.type == JSON_INT) {
		ll_val = *(const long long *)val;
		if (ll_val > (long long)INT_MAX || ll_val < (long long)INT_MIN)
			return -ERANGE;
		i_val = ll_val;
		val = &i_val;
		/* demoted int */
		json_type = JSON_INT;
	}

	if (!json_rpc_type_eq(attr->ref.type, json_type))
		return JSONRPC_INVALID_METHOD;

	if (attr->flags & ABUS_RPC_CONST)
		return JSONRPC_INVALID_METHOD;

	switch (json_type) {
	case JSON_INT:
		if (*(const int*)attr->ref.u.data != *(const int*)val) {
			*(int*)attr->ref.u.data = *(const int*)val;
			attr_changed = true;
		}
		break;
	case JSON_LLINT:
		if (*(const long long*)attr->ref.u.data != *(const long long*)val) {
			/* FIXME: this may not be atomic */
			*(long long*)attr->ref.u.data = *(const long long*)val;
			attr_changed = true;
		}
		break;
	case JSON_FALSE:
	case JSON_TRUE:
		if (*(const bool*)attr->ref.u.data != *(const bool*)val) {
			*(bool*)attr->ref.u.data = *(const bool*)val;
			attr_changed = true;
		}
		break;
	case JSON_FLOAT:
		if (*(const double*)attr->ref.u.data != *(const double*)val) {
			/* FIXME: this may not be atomic */
			*(double*)attr->ref.u.data = *(const double*)val;
			attr_changed = true;
		}
		break;
	case JSON_STRING:
		/* TODO check also len */
		if (strncmp(attr->ref.u.data, val, attr->ref.length) != 0) {
			/* FIXME: this is not atomic */
			strncpy(attr->ref.u.data, val, attr->ref.length);
			attr_changed = true;
		}
		break;
	default:
		return -EINVAL;
	}

	if (attr_changed)
		abus_attr_changed(abus, service_name, attr_name);

	return 0;
}

static int attr_set_type(abus_t *abus, const char *service_name, const char *attr_name, int json_type, const void *val, size_t len, int timeout)
{
	json_rpc_t *json_rpc;
	abus_service_t *service;
	abus_attr_t *attr;
	int ret;

	/* no RPC where attr's service is local to process/abus context */
	if (attr_lookup(abus, service_name, attr_name, LookupOnly, &service, &attr) == 0) {
		pthread_mutex_lock(&service->attr_mutex);
		ret = attr_set_local(abus, attr, service_name, attr_name, json_type, val, len);
		pthread_mutex_unlock(&service->attr_mutex);
		return ret;
	}

	json_rpc = abus_request_method_init(abus, service_name, ABUS_SET_METHOD);
	if (!json_rpc)
		return -ENOMEM;

	/* "service.set" "attr":[{"name":attr.a, "value":new_value}] */

	/* begin the array */
	json_rpc_append_args(json_rpc,
					JSON_KEY, "attr", -1,
					JSON_ARRAY_BEGIN,
					JSON_OBJECT_BEGIN,
					-1);

    json_rpc_append_str(json_rpc, "name", attr_name);

	switch (json_type) {
	case JSON_INT:
    	ret = json_rpc_append_int(json_rpc, "value", *(int *)val);
		break;
	case JSON_LLINT:
    	ret = json_rpc_append_llint(json_rpc, "value", *(long long *)val);
		break;
	case JSON_TRUE:
	case JSON_FALSE:
    	ret = json_rpc_append_bool(json_rpc, "value", *(bool*)val);
		break;
	case JSON_FLOAT:
    	ret = json_rpc_append_double(json_rpc, "value", *(double *)val);
		break;
	case JSON_STRING:
    	ret = json_rpc_append_str(json_rpc, "value", (char *)val);
		break;
	default:
		ret = JSONRPC_INTERNAL_ERROR;
	}

	/* end the array */
	json_rpc_append_args(json_rpc,
					JSON_OBJECT_END,
					JSON_ARRAY_END,
					-1);

    ret = abus_request_method_invoke(abus, json_rpc, ABUS_RPC_FLAG_NONE, timeout);

    abus_request_method_cleanup(abus, json_rpc);

	return ret;
}

/**
  Get the value of an integer attribute from a service

  \param abus	pointer to A-Bus handle
  \param[in] service_name	name of service where the attribute belongs to
  \param[in] attr_name	name of attribute to get
  \param[out] val	pointer to the variable where to store the attribute value
  \param[in] timeout	RPC waiting timeout in milliseconds
  \return   0 if successful, non nul value otherwise
 */
int abus_attr_get_int(abus_t *abus, const char *service_name, const char *attr_name, int *val, int timeout)
{
	return attr_get_type(abus, service_name, attr_name, JSON_INT, val, 0, timeout);
}

/**
  Get the value of a long long integer attribute from a service

  \param abus	pointer to A-Bus handle
  \param[in] service_name	name of service where the attribute belongs to
  \param[in] attr_name	name of attribute to get
  \param[out] val	pointer to the variable where to store the attribute value
  \param[in] timeout	RPC waiting timeout in milliseconds
  \return   0 if successful, non nul value otherwise
 */
int abus_attr_get_llint(abus_t *abus, const char *service_name, const char *attr_name, long long *val, int timeout)
{
	return attr_get_type(abus, service_name, attr_name, JSON_LLINT, val, 0, timeout);
}

/**
  Get the value of an boolean attribute from a service

  \param abus	pointer to A-Bus handle
  \param[in] service_name	name of service where the attribute belongs to
  \param[in] attr_name	name of attribute to get
  \param[out] val	pointer to the variable where to store the attribute value
  \param[in] timeout	RPC waiting timeout in milliseconds
  \return   0 if successful, non nul value otherwise
 */
int abus_attr_get_bool(abus_t *abus, const char *service_name, const char *attr_name, bool *val, int timeout)
{
	return attr_get_type(abus, service_name, attr_name, JSON_TRUE, val, 0, timeout);
}

/**
  Get the value of an double float attribute from a service

  \param abus	pointer to A-Bus handle
  \param[in] service_name	name of service where the attribute belongs to
  \param[in] attr_name	name of attribute to get
  \param[out] val	pointer to the variable where to store the attribute value
  \param[in] timeout	RPC waiting timeout in milliseconds
  \return   0 if successful, non nul value otherwise
 */
int abus_attr_get_double(abus_t *abus, const char *service_name, const char *attr_name, double *val, int timeout)
{
	return attr_get_type(abus, service_name, attr_name, JSON_FLOAT, val, 0, timeout);
}

/**
  Get the value of an string attribute from a service

  \param abus	pointer to A-Bus handle
  \param[in] service_name	name of service where the attribute belongs to
  \param[in] attr_name	name of attribute to get
  \param[out] val	pointer to the variable where to store the attribute value
  \param[in] len	maximum memory size of \a val
  \param[in] timeout	RPC waiting timeout in milliseconds
  \return   0 if successful, non nul value otherwise
 */
int abus_attr_get_str(abus_t *abus, const char *service_name, const char *attr_name, char *val, size_t len, int timeout)
{
	return attr_get_type(abus, service_name, attr_name, JSON_STRING, val, len, timeout);
}

/**
  Set the value of an integer attribute in a service

  \param abus	pointer to A-Bus handle
  \param[in] service_name	name of service where the attribute belongs to
  \param[in] attr_name	name of attribute to set
  \param[in] val	value of the attribute to set
  \param[in] timeout	RPC waiting timeout in milliseconds
  \return   0 if successful, non nul value otherwise
 */
int abus_attr_set_int(abus_t *abus, const char *service_name, const char *attr_name, int val, int timeout)
{
	return attr_set_type(abus, service_name, attr_name, JSON_INT, &val, 0, timeout);
}

/**
  Set the value of a long long integer attribute in a service

  \param abus	pointer to A-Bus handle
  \param[in] service_name	name of service where the attribute belongs to
  \param[in] attr_name	name of attribute to set
  \param[in] val	value of the attribute to set
  \param[in] timeout	RPC waiting timeout in milliseconds
  \return   0 if successful, non nul value otherwise
 */
int abus_attr_set_llint(abus_t *abus, const char *service_name, const char *attr_name, long long val, int timeout)
{
	return attr_set_type(abus, service_name, attr_name, JSON_LLINT, &val, 0, timeout);
}

/**
  Set the value of an boolean attribute in a service

  \param abus	pointer to A-Bus handle
  \param[in] service_name	name of service where the attribute belongs to
  \param[in] attr_name	name of attribute to set
  \param[in] val	value of the attribute to set
  \param[in] timeout	RPC waiting timeout in milliseconds
  \return   0 if successful, non nul value otherwise
 */
int abus_attr_set_bool(abus_t *abus, const char *service_name, const char *attr_name, bool val, int timeout)
{
	return attr_set_type(abus, service_name, attr_name, JSON_TRUE, &val, 0, timeout);
}

/**
  Set the value of an double float attribute in a service

  \param abus	pointer to A-Bus handle
  \param[in] service_name	name of service where the attribute belongs to
  \param[in] attr_name	name of attribute to set
  \param[in] val	value of the attribute to set
  \param[in] timeout	RPC waiting timeout in milliseconds
  \return   0 if successful, non nul value otherwise
 */
int abus_attr_set_double(abus_t *abus, const char *service_name, const char *attr_name, double val, int timeout)
{
	return attr_set_type(abus, service_name, attr_name, JSON_FLOAT, &val, 0, timeout);
}

/**
  Set the value of an string attribute in a service

  \param abus	pointer to A-Bus handle
  \param[in] service_name	name of service where the attribute belongs to
  \param[in] attr_name	name of attribute to set
  \param[in] val	value of the attribute to set
  \param[in] timeout	RPC waiting timeout in milliseconds
  \return   0 if successful, non nul value otherwise
 */
int abus_attr_set_str(abus_t *abus, const char *service_name, const char *attr_name, const char *val, int timeout)
{
	return attr_set_type(abus, service_name, attr_name, JSON_STRING, val, 0, timeout);
}

/**
  Subscribe to changes of the values of attributes in a service

  \param abus	pointer to A-Bus handle
  \param[in] service_name	name of service where the event belongs to
  \param[in] attr_name	name of attribute to subscribe to
  \param[in] callback	function to be called upon event publication or subscribe timeout.
  \param[in] flags		ABUS_RPC flags
  \param[in] arg		opaque pointer value to be passed to \a callback. may be NULL.
  \param[in] timeout	receive timeout of subscribe request in milliseconds
  \return   0 if successful, non nul value otherwise
 */
int abus_attr_subscribe_onchange(abus_t *abus, const char *service_name, const char *attr_name, abus_callback_t callback, int flags, void *arg, int timeout)
{
	char event_name[JSONRPC_METHNAME_SZ_MAX];

	snprintf(event_name, sizeof(event_name), ABUS_ATTR_CHANGED_PREFIX "%s", attr_name);

	return abus_event_subscribe(abus, service_name, event_name, callback, flags, arg, timeout);
}

/**
  Unsubscribe to changes of the values of attributes from a service

  \param abus	pointer to A-Bus handle
  \param[in] service_name	name of service where the event belongs to
  \param[in] attr_name	name of attribute to unsubscribe from
  \param[in] callback	function to be called upon event publication or subscribe timeout.
  \param[in] arg		opaque pointer value to be passed to \a callback. may be NULL.
  \param[in] timeout	receive timeout of unsubscribe request in milliseconds
  \return   0 if successful, non nul value otherwise
 */
int abus_attr_unsubscribe_onchange(abus_t *abus, const char *service_name, const char *attr_name, abus_callback_t callback, void *arg, int timeout)
{
	char event_name[JSONRPC_METHNAME_SZ_MAX];

	snprintf(event_name, sizeof(event_name), ABUS_ATTR_CHANGED_PREFIX "%s", attr_name);

	return abus_event_unsubscribe(abus, service_name, event_name, callback, arg, timeout);
}


/*
  callback for internal use, to offer get accessor of an attribute
 */
void abus_req_attr_get_cb(json_rpc_t *json_rpc, void *arg)
{
	abus_t *abus = (abus_t *)arg;
	abus_service_t *service;
	const char *attr_name;
	int ret, i, count;

    count = json_rpc_get_array_count(json_rpc, "attr");
    if (count < 0) {
		json_rpc_set_error(json_rpc, count, NULL);
		return;
    }

	ret = service_lookup(abus, json_rpc->service_name, LookupOnly, &service);
	if (ret < 0) {
		json_rpc_set_error(json_rpc, ret, NULL);
		return;
	}
	pthread_mutex_lock(&service->attr_mutex);


    for (i = 0; i<count; i++) {
		/* Aim at i-th element within array "attr" */
		json_rpc_get_point_at(json_rpc, "attr", i);

		/* nb: allow empty attr name to retrieve all attributes */
		ret = json_rpc_get_strp(json_rpc, "name", &attr_name, NULL);
		if (ret != 0) {
			json_rpc_set_error(json_rpc, ret, NULL);
			pthread_mutex_unlock(&service->attr_mutex);
			return;
		}

		ret = attr_append(abus, json_rpc, json_rpc->service_name, attr_name);
		if (ret != 0) {
			json_rpc_set_error(json_rpc, ret, NULL);
			pthread_mutex_unlock(&service->attr_mutex);
			return;
		}
	}

	pthread_mutex_unlock(&service->attr_mutex);

	/* Aim back out of array */
	json_rpc_get_point_at(json_rpc, NULL, 0);
}

/*
  callback for internal use, to offer set accessor of an attribute
 */
void abus_req_attr_set_cb(json_rpc_t *json_rpc, void *arg)
{
	abus_t *abus = (abus_t *)arg;
	abus_service_t *service;
	const char *attr_name;
	int i, count;
	size_t attr_len, len=0;
	abus_attr_t *attr;
	int ret;
	int a;
	bool b;
	double d;
	long long ll;
	const void *val;

    count = json_rpc_get_array_count(json_rpc, "attr");
    if (count < 0) {
		json_rpc_set_error(json_rpc, count, NULL);
		return;
    }

	ret = service_lookup(abus, json_rpc->service_name, LookupOnly, &service);
	if (ret < 0) {
		json_rpc_set_error(json_rpc, ret, NULL);
		return;
	}
	pthread_mutex_lock(&service->attr_mutex);

    for (i = 0; i<count; i++) {

		/* Aim at i-th element within array "attr" */
		json_rpc_get_point_at(json_rpc, "attr", i);

		ret = json_rpc_get_strp(json_rpc, "name", &attr_name, &attr_len);
		if (ret != 0 || attr_len == 0) {
			json_rpc_set_error(json_rpc, ret, NULL);
			pthread_mutex_unlock(&service->attr_mutex);
			return;
		}

		ret = attr_lookup(abus, json_rpc->service_name, attr_name, LookupOnly, NULL, &attr);
		if (ret) {
			json_rpc_set_error(json_rpc, ret, NULL);
			pthread_mutex_unlock(&service->attr_mutex);
			return;
		}

		if (attr->flags & (ABUS_RPC_RDONLY|ABUS_RPC_CONST)) {
			json_rpc_set_error(json_rpc, JSONRPC_INVALID_METHOD, "Cannot set read-only/constant attribute");
			pthread_mutex_unlock(&service->attr_mutex);
			return;
		}

		switch(attr->ref.type) {
		case JSON_INT:
			ret = json_rpc_get_int(json_rpc, "value", &a);
			val = &a;
			break;
		case JSON_LLINT:
			ret = json_rpc_get_llint(json_rpc, "value", &ll);
			val = &ll;
			break;
		case JSON_FALSE:
		case JSON_TRUE:
			ret = json_rpc_get_bool(json_rpc, "value", &b);
			val = &b;
			break;
		case JSON_FLOAT:
			ret = json_rpc_get_double(json_rpc, "value", &d);
			val = &d;
			break;
		case JSON_STRING:
			ret = json_rpc_get_strp(json_rpc, "value", (const char **)&val, &len);
			break;
		default:
			ret = JSONRPC_INTERNAL_ERROR;
		}

		if (ret == 0)
			ret = attr_set_local(abus, attr, json_rpc->service_name, attr_name, attr->ref.type, val, len);

		if (ret) {
			json_rpc_set_error(json_rpc, ret, NULL);
			pthread_mutex_unlock(&service->attr_mutex);
			return;
		}
	}

	pthread_mutex_unlock(&service->attr_mutex);

	/* Aim back out of array */
	json_rpc_get_point_at(json_rpc, NULL, 0);
}

/*! @} */
