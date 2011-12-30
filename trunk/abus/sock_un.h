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

#ifndef _SOCK_UN_H
#define _SOCK_UN_H

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "sock_un.h"

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif

const char *abus_prefix;

int un_sock_create(void);
int un_sock_close(int sock);
int un_sock_sendto_svc(int sock, const void *buf, size_t len, const char *service_name);
int un_sock_sendto_sock(int sock, const void *buf, size_t len, const struct sockaddr *dest_addr, int addrlen);
int un_sock_transaction(const int sockarg, void *buf, size_t len, size_t bufsz, const char *service_name, int timeout);

static inline int un_sock_socklen(const struct sockaddr *sockaddr)
{
	return SUN_LEN(((const struct sockaddr_un *)sockaddr));
}

/* for debug purpose */
static inline const char *un_sock_name(const struct sockaddr *sockaddr)
{
	return ((const struct sockaddr_un *)sockaddr)->sun_path+1;
}

#endif /* _SOCK_UN_H */
