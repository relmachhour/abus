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

#include "abus_config.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "sock_un.h"

#define LogError(...)    do { fprintf(stderr, ##__VA_ARGS__); fprintf(stderr, "\n"); } while (0)
#define LogDebug(...)    do { fprintf(stderr, ##__VA_ARGS__); fprintf(stderr, "\n"); } while (0)


const char *abus_prefix = "/tmp/abus";
int abus_msg_verbose;

static void un_sock_print_message(int out, const struct sockaddr *sockaddr, const char *msg, int msglen)
{
	LogDebug("## %5d %s %s:%d %.*s",
					getpid(),
					out ? "->" : "<-", 
					sockaddr ? un_sock_name(sockaddr) : "",
					msglen, 
					msglen, msg);
}

int un_sock_create(void)
{
	struct sockaddr_un sockaddrun;
	int sock, ret;
	int reuse_addr = 1;

	sock = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (sock < 0) {
		ret = -errno;
		LogError("%s: failed to bind server socket: %s", __func__, strerror(errno));
		return ret;
	}

	memset(&sockaddrun, 0, sizeof(sockaddrun));
	sockaddrun.sun_family = AF_UNIX;

	/* TODO: prefix from env variable */
	snprintf(sockaddrun.sun_path, sizeof(sockaddrun.sun_path)-1,
					"%s/_%d", abus_prefix, getpid());

	if (bind(sock, (struct sockaddr *) &sockaddrun, SUN_LEN(&sockaddrun)) < 0)
	{
		ret = -errno;
		LogError("%s: failed to bind server socket: %s", __func__, strerror(errno));
		close(sock);
		return ret;
	}

	/* So that we can re-bind to it without TIME_WAIT problems */
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse_addr, sizeof(reuse_addr)) < 0)
	{
		ret = -errno;
		LogError("%s: failed to set SO_REUSEADDR option on server socket: %s", __func__, strerror(errno));
		close(sock);
		return ret;
	}

	return sock;
}

int un_sock_close(int sock)
{
	char pid_path[UNIX_PATH_MAX];

	if (sock == -1)
		return 0;

	close(sock);

	/* TODO: prefix from env variable */
	snprintf(pid_path, sizeof(pid_path)-1, "%s/_%d", abus_prefix, getpid());

	unlink(pid_path);

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
		ret = -errno;
		LogError("%s: select fails with error: %s", __func__, strerror(errno));
		return ret;
	}
	if (ret == 0)
	{
		return 0;
	}
	if (FD_ISSET(sock, &sete))
	{
		LogError("%s: error detected on sock by select", __func__);
		return -EIO;
	}
	return 1;
}



int un_sock_sendto_svc(int sock, const void *buf, size_t len, const char *service_name)
{
	struct sockaddr_un sockaddrun;
	ssize_t ret;

	sockaddrun.sun_family = AF_UNIX;

	/* TODO: prefix from env variable */
	snprintf(sockaddrun.sun_path, sizeof(sockaddrun.sun_path)-1,
					"%s/%s", abus_prefix, service_name);

	if (abus_msg_verbose)
		un_sock_print_message(true, (const struct sockaddr *)&sockaddrun, buf, len);

	ret = sendto(sock, buf, len, MSG_NOSIGNAL,
					(const struct sockaddr *)&sockaddrun, SUN_LEN(&sockaddrun));
	if (ret == -1) {
		ret = -errno;
		if (errno != ECONNREFUSED && errno != ENOENT)
			LogError("%s(): sendto failed: %s", __func__, strerror(errno));
		return ret;
	}

	return 0;
}

int un_sock_sendto_sock(int sock, const void *buf, size_t len, const struct sockaddr *dest_addr, int addrlen)
{
	int ret;

	if (abus_msg_verbose)
		un_sock_print_message(true, dest_addr, buf, len);

	ret = sendto(sock, buf, len, MSG_NOSIGNAL|MSG_DONTWAIT, dest_addr, addrlen);
	return ret == -1 ? -errno : ret;
}

int un_sock_transaction(const int sockarg, void *buf, size_t len, size_t bufsz, const char *service_name, int timeout)
{
	int sock, ret;
	int passcred;

	if (sockarg == -1) {
		sock = socket(AF_UNIX, SOCK_DGRAM, 0);
		if (sock < 0) {
			ret = -errno;
			LogError("%s: failed to create socket: %s", __func__, strerror(errno));
			return ret;
		}
	
		/* autobind */
		passcred = 1;
		ret = setsockopt(sock, SOL_SOCKET, SO_PASSCRED, &passcred, sizeof(passcred));
		if (ret != 0) {
			ret = -errno;
			LogError("%s: abus clnt setsockopt(SO_PASSCRED): %s",
					__func__, strerror(errno));
			close(sock);
			return ret;
		}
	} else {
		sock = sockarg;
	}

	ret = un_sock_sendto_svc(sock, buf, len, service_name);
	if (ret != 0) {
		if (sockarg == -1)
			close(sock);
		return ret;
	}

	ret = select_for_read(sock, timeout);
	if (ret == -1) {
		ret = -errno;
		if (sockarg == -1)
			close(sock);
		return ret;
	}
	if (ret == 0) {
		if (sockarg == -1)
			close(sock);
		return -ETIMEDOUT;
	}

	/* recycle req buf */

	len = recv(sock, buf, bufsz, 0);
	if (len == -1) {
		ret = -errno;
		LogError("%s(): abus clnt recv: %s", __func__, strerror(errno));
		if (sockarg == -1)
			close(sock);
		return ret;
	}
	ret = len;

	if (abus_msg_verbose)
		un_sock_print_message(false, NULL, buf, len);

	if (sockarg == -1)
		close(sock);

	return ret;
}

ssize_t un_sock_recvfrom(int sockfd, void *buf, size_t len,
                        struct sockaddr *src_addr, socklen_t *addrlen)
{
	ssize_t ret;

	ret = recvfrom(sockfd, buf, len, 0, src_addr, addrlen);
	if (ret == -1) {
		ret = -errno;
		return ret;
	}

	if (*addrlen < sizeof(struct sockaddr_un))
		((char *)src_addr)[*addrlen] = '\0';

	if (abus_msg_verbose)
		un_sock_print_message(false, src_addr, buf, ret);

	return ret;
}

