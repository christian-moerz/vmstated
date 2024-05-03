/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2023 Christian Moerz. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "socket_config.h"
#include "socket_connect.h"

/*
 * encapsulates a socket connection
 */
struct socket_connection {
	int clientfd;
	char sockpath[PATH_MAX];
};

/*
 * send arbitrary data to remote and receive reply, supporting
 * binary blob data replies in addition to string messages.
 */
int
sc_sendrecv_dynamic(struct socket_connection *sc,
		    const char *command,
		    const void *data,
		    size_t datalen,
		    char *retbuffer,
		    size_t retbuffer_len,
		    void **blob_reply,
		    size_t *blob_reply_len)
{
	if (!sc || !command) {
		errno = EINVAL;
		return -1;
	}

	char sendbuf[SHC_MAXTRANSPORTDATA] = {0};
	size_t sendlen = strlen(command) + (data ? datalen : 0) + 1;
	size_t bytes_sent = 0, bytes_total = 0;
	size_t bytes_read = 0;
	fd_set sendfds = {0};
	const char *numstring = 0;
	struct timeval timeout = {0};
	char recv_check[5] = {0};
	uint64_t bloblen = 0;

	if (sendlen > SHC_MAXTRANSPORTDATA) {
		errno = EMSGSIZE;
		return -1;
	}		

	timeout.tv_usec = 100;
	
	/* check if we can really send */
	FD_SET(sc->clientfd, &sendfds);
	if (select(1, NULL, &sendfds, NULL, &timeout) < 0) {
		/* failure to select */
		return -1;
	}

	/* copy command, zero byte and data into send buffer */
	snprintf(sendbuf, SHC_MAXTRANSPORTDATA, "%s%c",
		 command, 0);
	memcpy(sendbuf + strlen(command) + 1, data, datalen);

	/* send data to remote end */
	while(((sendlen - bytes_total) > 0) &&
	      (bytes_sent = write(sc->clientfd, sendbuf + bytes_total, sendlen - bytes_sent)) > 0) {
		bytes_total += bytes_sent;
	}

	/* check that we have something to read */
	if (select(1, &sendfds, NULL, NULL, &timeout) < 0) {
		/* failure to select for read */
		return -1;
	}

	/* first, we read only 4 bytes, to check whether we got "DATA"
	 * indicating a binary blob as reply */
	bytes_total = 0;
	while ((bytes_read = read(sc->clientfd, recv_check + bytes_total, 4 - bytes_total)) > 0) {
		bytes_total += bytes_read;
	}
	if (strcmp("DATA", recv_check)) {
		/* not data reply, put first four bytes into retbuffer */
		memcpy(retbuffer, recv_check, 4);
		
		/* after sending, we receive reply */
		/* TODO this needs fixing */
		while((bytes_sent = read(sc->clientfd, retbuffer+4, retbuffer_len-4)) > 0) {
			if (memchr(retbuffer, 0, retbuffer_len))
				break;
		}
	} else {
		/* read bytes until we receive zero byte */
		bytes_total = 4;
		while((bytes_read = read(sc->clientfd, retbuffer + bytes_total, 1)) > 0) {
			/* we have reached zero byte */
			if (0 == *(retbuffer + bytes_total))
				break;
			
			bytes_total += bytes_read;
			if (bytes_total >= retbuffer_len) {
				/* failure to receive zero byte - bail */
				/* TODO implement error handling */
				return -1;
			}
		}
		/* check bytes_total is at least some 6 chars:
		 * DATA 0 */
		numstring = retbuffer + 5;
		errno = 0;
		bloblen = atoll(numstring);
		if (errno) {
			/* TODO handle number error case */
			return -1;
		}

		/* attempt to allocate blob memory */
		if (!(*blob_reply = malloc(bloblen))) {
			/* TODO handle allocation error; should close the socket now, I guess */
			return -1;
		}

		*blob_reply_len = bloblen;
		bytes_total = 0;
		while(bloblen &&
		      (bytes_read = read(sc->clientfd,
					 (*blob_reply) + bytes_total,
					 bloblen)) > 0) {
			bytes_total += bytes_read;
			bloblen -= bytes_read;
		}
		assert(bytes_total == *blob_reply_len);

	}

	return 0;

}

/*
 * send arbitrary data to remote and and receive reply
 */
int
sc_sendrecv_len(struct socket_connection *sc,
		const char *command,
		const void *data,
		size_t datalen,
		char *retbuffer,
		size_t retbuffer_len)
{
	if (!sc || !command) {
		errno = EINVAL;
		return -1;
	}

	char sendbuf[SHC_MAXTRANSPORTDATA] = {0};
	size_t sendlen = strlen(command) + (data ? datalen : 0) + 1;
	size_t bytes_sent = 0, bytes_total = 0;
	fd_set sendfds = {0};
	struct timeval timeout = {0};

	if (sendlen > SHC_MAXTRANSPORTDATA) {
		errno = EMSGSIZE;
		return -1;
	}		

	timeout.tv_usec = 100;
	
	/* check if we can really send */
	FD_SET(sc->clientfd, &sendfds);
	if (select(1, NULL, &sendfds, NULL, &timeout) < 0) {
		/* failure to select */
		return -1;
	}

	snprintf(sendbuf, SHC_MAXTRANSPORTDATA, "%s%c",
		 command, 0);
	memcpy(sendbuf + strlen(command) + 1, data, datalen);

	while(((sendlen - bytes_total) > 0) &&
	      (bytes_sent = write(sc->clientfd, sendbuf + bytes_total, sendlen - bytes_sent)) > 0) {
		bytes_total += bytes_sent;
	}

	/* check that we have something to read */
	if (select(1, &sendfds, NULL, NULL, &timeout) < 0) {
		/* failure to select for read */
		return -1;
	}

	/* after sending, we receive reply */
	/* TODO this needs fixing */
	while((bytes_sent = read(sc->clientfd, retbuffer, retbuffer_len)) > 0) {
		if (memchr(retbuffer, 0, retbuffer_len))
			break;
	}

	return 0;
}

/*
 * send data to remote end and receive reply
 */
int
sc_sendrecv(struct socket_connection *sc,
	    const char *command,
	    const char *data,
	    char *retbuffer,
	    size_t retbuffer_len)
{
	if (!sc || !command) {
		errno = EINVAL;
		return -1;
	}

	return sc_sendrecv_len(sc, command, data,
			       data ? (strlen(data) + 1): 0,
			       retbuffer, retbuffer_len);
}

/*
 * connect to server socket
 *
 * returns 0 on success.
 */
int
sc_connect(struct socket_connection *sc)
{
	if (!sc) {
		errno = EINVAL;
		return -1;
	}

	struct sockaddr_un sa = {0};
	sa.sun_family = AF_UNIX;
	strncpy(sa.sun_path, sc->sockpath, sizeof(sa.sun_path));

	return connect(sc->clientfd, (void*) &sa, sizeof(struct sockaddr_un));
}

/*
 * initialize a new socket connection
 */
struct socket_connection *
sc_new(const char *sockpath)
{
	if (!sockpath) {
		errno = EINVAL;
		return NULL;
	}

	struct socket_connection *sc = malloc(sizeof(struct socket_connection));

	if ((sc->clientfd = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
		free(sc);
		return NULL;
	}

	bzero(sc->sockpath, PATH_MAX);
	strncpy(sc->sockpath, sockpath, PATH_MAX);

	return sc;
}

/*
 * closes and releases a client socket connection
 */
void
sc_free(struct socket_connection *sc)
{
	if (!sc)
		return;

	close(sc->clientfd);
	sc->clientfd = 0;

	free(sc);
}
