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

#include <sys/event.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ucred.h>
#include <sys/un.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "reply_collector.h"
#include "socket_config.h"
#include "socket_handle.h"
#include "socket_handle_errors.h"

#define SH_EVT_CMD_SHUTDOWN 0
#define SH_MAXCONNECT 4
#define SH_CMDLEN 4
#define SH_ERRMSGLEN 512

struct socket_listener {
	/* subscriber function
	 * - void * pointer to context
	 * - const char * short command ident
	 * - const char * command data to parse further (terminated by zero char?)
	 * - size_t length of data buffer
	 */
	int (*on_data)(void *, uid_t uid, pid_t pid, const char *, const char *, size_t,
		       struct socket_reply_collector *);
	void *ctx;

	SLIST_ENTRY(socket_listener) entries;
};


/*
 * data being parsed from socket
 */
struct socket_cmdparsedata {
	char *zerocmd;
	char *zerodata;
	size_t cmdlen;
	size_t datalen;
	char errmsg[SH_ERRMSGLEN];
	uint64_t errcode;
};

/*
 * stores client connection
 */
struct socket_connection {
	int clientfd;
	uid_t uid;
	pid_t pid;

	/* number of bytes read */
	size_t bytes_read;
	/* number of \0 characters received */
	unsigned short zero_recv;
	char buffer[SHC_MAXTRANSPORTDATA];
	
	void *ctx;

	struct socket_reply_collector *src;

	LIST_ENTRY(socket_connection) entries;
};

/*
 * structure holding socket details and listeners
 */
struct socket_handle {
	int socketfd;
	int keventfd;

	char *sockpath;

	enum {
		INITIALIZING = 0,
		READY = 1,
		STARTED = 2,
		RUNNING = 3,
		STOPPING = 4,
		STOPPED = 5
	} state;
	
	pthread_t listener_thread;
	pthread_cond_t listener_ready;
	pthread_mutex_t mtx;
	
	SLIST_HEAD(, socket_listener) listeners;
	LIST_HEAD(, socket_connection) connections;
};

/*
 * release a previously allocated socket listener
 */
void
shl_free(struct socket_listener *shl)
{
	free(shl);
}

/*
 * instantiate a new socket listener
 */
struct socket_listener *
shl_new(void *ctx, int (*on_data)(void*, uid_t, pid_t, const char*, const char*, size_t,
				  struct socket_reply_collector *))
{
	struct socket_listener *shl = 0;

	shl = malloc(sizeof(struct socket_listener));
	if (!shl)
		return NULL;
	shl->ctx = ctx;
	shl->on_data = on_data;

	return shl;
}

/*
 * release a previously allocated socket connection
 */
void
shc_free(struct socket_connection *shc)
{
	if (!shc)
		return;

	src_free(shc->src);

	if (shc->clientfd)
		close(shc->clientfd);
	shc->clientfd = 0;

	free(shc);
}

/*
 * drop count number of bytes from buffer and move remaining
 * data left.
 *
 * returns 0 on success
 */
int
shc_dropbytes(struct socket_connection *shc, size_t count)
{
	if (!shc || !count)
		return -1;

	/* TODO implement this as ring buffer instead? */
	if ((count == SHC_MAXTRANSPORTDATA) || (count >= shc->bytes_read)) {
		bzero(shc->buffer, SHC_MAXTRANSPORTDATA);
		shc->bytes_read = 0;
		return 0;
	}

	char *temp = malloc(SHC_MAXTRANSPORTDATA - count + 1);
	if (!temp)
		return -1;

	memcpy(temp, shc->buffer + count, SHC_MAXTRANSPORTDATA - count);
	bzero(shc->buffer+count, SHC_MAXTRANSPORTDATA - count);
	memcpy(shc->buffer, temp, SHC_MAXTRANSPORTDATA - count);
	shc->bytes_read -= count;

	free(temp);

	return 0;
}

/*
 * drop the message in front of the buffer
 */
int
shc_dropmessage(struct socket_connection *shc, struct socket_cmdparsedata *parsedata)
{
	if (!shc || !parsedata) {
		errno = EINVAL;
		return -1;
	}

	size_t skip_bytes = parsedata->cmdlen + parsedata->datalen + 2;

	return shc_dropbytes(shc, skip_bytes);
}

/*
 * instantiate a new socket connection
 */
struct socket_connection *
shc_new(void *ctx, int clientfd, uid_t uid, pid_t pid)
{
	struct socket_connection *shc = malloc(sizeof(struct socket_connection));

	if (!shc)
		return NULL;

	shc->ctx = ctx;
	shc->clientfd = clientfd;
	shc->uid = uid;
	shc->pid = pid;

	shc->bytes_read = 0;
	shc->zero_recv = 0;
	bzero(&shc->buffer, sizeof(shc->buffer));

	shc->src = src_new();

	return shc;
}

/*
 * subscribe to receive data from socket
 *
 * - ctx: context pointer to provide to on_data when data is received
 * - on_data: subscriber method to call when data is received
 *
 * on_data needs to return 0 for all subsequent subscribers to receive data
 * on their on_data subscriber methods. This way, an exclusive subscriber
 * that is certain that no other subscriber is capable of parsing and
 * handling received data can stop the processing of data by other
 * subscribers.
 */
int
sh_subscribe_ondata(struct socket_handle *sh,
		    void *ctx, int (*on_data)(void*, uid_t, pid_t, const char*, const char*, size_t,
					      struct socket_reply_collector *))
{
	if (!sh || !on_data)
		return SH_ERR_INVALIDPARAMS;

	struct socket_listener *shl = shl_new(ctx, on_data);
	if (!shl)
		return SH_ERR_ITEMALLOCFAIL;

	if (pthread_mutex_lock(&sh->mtx))
		return SH_ERR_MUTEXLOCKFAIL;
	
	/* add subscriber */
	SLIST_INSERT_HEAD(&sh->listeners, shl, entries);

	pthread_mutex_unlock(&sh->mtx);

	return 0;
}

/*
 * look up a client connection
 *
 * returns NULL if no match was found.
 */
struct socket_connection *
sh_lookup_connection(struct socket_handle *sh, int clientfd)
{
	if (!sh || !clientfd)
		return NULL;

	struct socket_connection *shc = 0;

	LIST_FOREACH(shc, &sh->connections, entries) {
		if (shc->clientfd == clientfd)
			return shc;
	}

	return NULL;
}

/*
 * reply an error message to the client
 */
int
sh_reply_msg(struct socket_handle *sh, struct socket_connection *shc,
	       uint64_t errcode, const char *msg)
{
	ssize_t sent_bytes = 0;
	ssize_t sent_total = 0;
	size_t msglen = strlen(msg) + 1;

	if (!shc || !msg)
		return -1;

	while (msglen && ((sent_bytes = write(shc->clientfd, msg + sent_total, msglen)) > 0)) {
		msglen -= sent_bytes;
		sent_total += sent_bytes;
	}
	
	return - !(sent_total == (strlen(msg)+1));
}

/*
 * send data blob from reply collector to client
 */
int
sh_reply_data(struct socket_handle *sh, struct socket_connection *shc)
{
	ssize_t sent_bytes = 0;
	ssize_t sent_total = 0;
	const void *buffer;
	size_t bufferlen = 0;
	char datastr[128] = {0};
	size_t msglen = 0;	

	if (!shc || !sh)
		return -1;

	if (!(buffer = src_get_reply(shc->src, &bufferlen))) {
		return -1;
	}

	snprintf(datastr, 128, "DATA %ld", bufferlen);
	/* include zero byte at the end */
	msglen = strlen(datastr) + 1;

	/* send DATA string */
	while (msglen && ((sent_bytes = write(shc->clientfd, datastr + sent_total, msglen)) > 0)) {
		msglen -= sent_bytes;
		sent_total += sent_bytes;
	}

	assert(sent_total == (strlen(datastr)+1));

	/* now send binary blob */
	sent_total = 0;
	while (bufferlen && ((sent_bytes = write(shc->clientfd, buffer + sent_total, bufferlen)) > 0)) {
		bufferlen -= sent_bytes;
		sent_total += sent_bytes;
	}

	assert(0 == bufferlen);
	
	return 0;
}

/*
 * reply a generic message
 */
int
sh_reply_genericmsg(struct socket_handle *sh, struct socket_connection *shc, uint64_t errcode)
{
	char buffer[SHC_MAXTRANSPORTDATA] = {0};

	snprintf(buffer, SHC_MAXTRANSPORTDATA, "%04ld: %sOK",
		 errcode,
		 0 == errcode ? "" : "N");
	return sh_reply_msg(sh, shc, errcode, buffer);
}

/*
 * reply a generic message
 */
int
sh_reply_shortmsg(struct socket_handle *sh, struct socket_connection *shc, uint64_t errcode,
	const char *msg)
{
	char buffer[SHC_MAXTRANSPORTDATA] = {0};

	snprintf(buffer, SHC_MAXTRANSPORTDATA, "%04ld: %s",
		 errcode,
		 msg);
	return sh_reply_msg(sh, shc, errcode, buffer);
}

/*
 * call listeners and send reply to client
 */
int
sh_call_listeners(struct socket_handle *sh, struct socket_connection *shc)
{
	if (!sh || !shc)
		return -1;

	if (pthread_mutex_lock(&sh->mtx))
		err(SH_ERR_MUTEXLOCKFAIL, "Failed mutex lock on call thread");

	struct socket_listener *shl = 0, *temp = 0;
	int retcode = 0;
	size_t cmdlen = strlen(shc->buffer);
	
	SLIST_FOREACH_SAFE(shl, &sh->listeners, entries, temp) {
		if (shl->on_data) {
			/* unlock while calling? */
			if (pthread_mutex_unlock(&sh->mtx)) {
				retcode = -1;
				break;
			}
			
			retcode = shl->on_data(shl->ctx,                      /* context */
					       shc->uid,                      /* user id */
					       shc->pid,                      /* process id */
					       shc->buffer,                   /* command */
					       shc->buffer + cmdlen + 1,      /* data buffer */
					       shc->bytes_read - cmdlen - 1,  /* data buffer len */
					       shc->src);                     /* reply mgr */

			if (pthread_mutex_lock(&sh->mtx)) {
				retcode = -1;
				break;
			}
		}
		/* we got an error code */
		if (retcode)
			break;
	}
	
	pthread_mutex_unlock(&sh->mtx);
	
	return retcode;
}

/*
 * attempts parsing a command from the beginning of the byte buffer
 *
 * returns 0 on success.
 */
int
sh_try_cmdparsing(struct socket_connection *shc, struct socket_cmdparsedata *parsedata)
{
	if (!shc || !parsedata)
		return -1;

	if (!shc->bytes_read) {
		/* nothing to parse */
		parsedata->errcode = SH_WRN_KEEPREADNMORE;
		return 0;
	}		
	
	/* we have exhausted reading, attempt to parse out command and data */
	parsedata->zerocmd = memchr(shc->buffer, 0, shc->bytes_read);

	if (!parsedata->zerocmd) {
		/* no zero character found! */

		if (shc->bytes_read >= SHC_MAXTRANSPORTDATA) {
			/* we drop everything so far */
			shc_dropbytes(shc, SHC_MAXTRANSPORTDATA);

			parsedata->errcode = SH_CMDERR_GARBAGECMD;
			strncpy(parsedata->errmsg, "Invalid input", SH_ERRMSGLEN);
		}

		/* waiting for more data */
		return 0;
	}
	
	parsedata->cmdlen = parsedata->zerocmd - shc->buffer;
	if (parsedata->cmdlen > SH_CMDLEN) {
		/* this is an invalid command */
		snprintf(parsedata->errmsg, SH_ERRMSGLEN,
			 "%04d: Invalid command \"%s\" is too long (%lu bytes)",
			 SH_CMDERR_INVALIDCMD,
			 shc->buffer,
			 parsedata->cmdlen);
		parsedata->errcode = SH_CMDERR_INVALIDCMD;
		
		/* drop data until zero ptr and retry parsing */
		if (shc_dropmessage(shc, parsedata))
			err(SH_ERR_BUFFERCHGFAIL, "Failed to modify connection buffer");

		return 0;
	} else {
		/* parse data length */
		parsedata->zerodata = memchr(
			parsedata->zerocmd + 1, 0,
			SHC_MAXTRANSPORTDATA - parsedata->cmdlen - 1);
		if (!parsedata->zerodata) {
			/* no end of data yet? */
			if (shc->bytes_read == SHC_MAXTRANSPORTDATA) {
				/* data too big, drop everything after error message */
				snprintf(parsedata->errmsg,
					 512,
					 "%04d: Data for command \"%s\" is too long",
					 SH_CMDERR_DATATOOLON,
					 shc->buffer);
				parsedata->errcode = SH_CMDERR_DATATOOLON;

				/* drop remaining data */
				if (shc_dropbytes(shc, SHC_MAXTRANSPORTDATA))
					err(SH_ERR_BUFFERCHGFAIL, "Failed to modify connection buffer");

				return 0;
			}

			parsedata->errcode = SH_WRN_KEEPREADNMORE;

			/* we will wait for more data */
			return 0;
		}

		/* we found the end of the data block */
		parsedata->datalen = parsedata->zerodata - parsedata->zerocmd - 1;
	}

	return 0;

}

/*
 * handle reading from a connection
 *
 * returns 0 on success; errno is set on error.
 */
int
sh_read_data(struct socket_handle *sh,
	     struct socket_connection *shc,
	     size_t bytes_ready)
{
	size_t to_read = 0, bytes_remaining = 0;
	ssize_t done_read = 0;
	char *offsetptr = 0;

	if (!sh || !shc) {
		errno = EINVAL;
		return -1;
	}

	/* calculate maximum bytes to read */
	bytes_remaining = SHC_MAXTRANSPORTDATA - shc->bytes_read;
	to_read = bytes_ready > bytes_remaining ? bytes_remaining : bytes_ready;

	/* set buffer pointer */
	offsetptr = shc->buffer + shc->bytes_read;

	while(bytes_remaining && bytes_ready &&
	      (done_read = read(shc->clientfd, offsetptr, to_read))) {
		/* we have read data */
		shc->bytes_read += done_read;
		/* we are reducing number of bytes available */
		bytes_remaining -= done_read;
		to_read = bytes_ready > bytes_remaining ? bytes_remaining : bytes_ready;
		offsetptr += done_read;

		/* decrement number of bytes in buffer */
		bytes_ready -= done_read;
	}

	return 0;
}

/*
 * disconnect a client
 */
int
sh_disconnect_client(struct socket_handle *sh, struct socket_connection *shc)
{
	/* remove connection from listing */
	if (pthread_mutex_lock(&sh->mtx))
		return -1;

	LIST_REMOVE(shc, entries);
	pthread_mutex_unlock(&sh->mtx);

	syslog(LOG_INFO, "Disconnecting client");
	
	/* close handle, that also removes it from kqueue */
	close(shc->clientfd);
	shc->clientfd = 0;
	
	shc_free(shc);
	return 0;
}

/*
 * thread accepting incoming connections, reading data
 */
void *
sh_accept_thread(void *data)
{
	struct socket_handle *sh = data;
	struct socket_connection *shc = 0;
	struct kevent event = {0};
	struct xucred cred = {0};
	struct socket_cmdparsedata parsedata = {0};
	socklen_t credlen = 0;
	int clientfd = 0;
	int result = 0;

	if (!sh)
		return NULL;

	if (pthread_mutex_lock(&sh->mtx))
		err(SH_ERR_MUTEXLOCKFAIL, "Failed mutex lock on accept thread");

	sh->state = RUNNING;

	/* signal ready */
	pthread_cond_signal(&sh->listener_ready);

	pthread_mutex_unlock(&sh->mtx);

	while (1) {
		if (kevent(sh->keventfd, NULL, 0, &event, 1, 0) < 0)
			err(SH_ERR_KQUQUERFAILED, "Failed kevent query on accept thread");

		if (event.ident == SH_EVT_CMD_SHUTDOWN)
			break;

		if (sh->socketfd == event.ident) {
			/* need to accept new connection */
			if ((clientfd = accept(sh->socketfd, NULL, 0)) < 0)
				err(SH_ERR_SOCKTACCTFAIL, "Failed to accept socket connection");

			/* get user credentials behind connection */
			bzero(&cred, sizeof(struct xucred));
			credlen = sizeof(struct xucred);
			if (getsockopt(clientfd, 0, LOCAL_PEERCRED, &cred, &credlen) < 0)
				err(SH_ERR_GETSOCKOPFAIL,
				    "Failed to get credentials from connection");
			
			if (!(shc = shc_new(sh, clientfd, cred.cr_uid, cred.cr_pid)))
				err(SH_ERR_ITEMALLOCFAIL, "Failed to allocate connection");

			assert(shc->clientfd == clientfd);

			/* add client to connections */
			if (pthread_mutex_lock(&sh->mtx))
				err(SH_ERR_MUTEXLOCKFAIL, "Failed mutex lock during accept");

			LIST_INSERT_HEAD(&sh->connections, shc, entries);

			pthread_mutex_unlock(&sh->mtx);

			/* register for read events on connection */
			bzero(&event, sizeof(struct kevent));
			EV_SET(&event, shc->clientfd, EVFILT_READ, EV_ADD, 0, 0, NULL);
			if (kevent(sh->keventfd, &event, 1, NULL, 0, NULL) < 0)
				err(SH_ERR_ADDKEVENTFAIL, "Failed to add kevent to queue");
		} else {
			if (event.flags & EV_EOF) {
				/* client is disconnecting */
				shc = sh_lookup_connection(sh, event.ident);
				if (!shc)
					continue;

				syslog(LOG_ERR, "Disconnecting client");

				if (sh_disconnect_client(sh, shc)) {
					/* TODO log error */
				}
			} else if (EVFILT_READ & event.filter) {
				syslog(LOG_ERR, "Received READ kevent");
				
				/* we want to read from client */
				/* find matching connection and its buffer */
				shc = sh_lookup_connection(sh, event.ident);
				if (!shc) {
					/* log error */
					syslog(LOG_ERR, "Failed to lookup connection");
					close(event.ident);
					continue;
				}
				
				/* read data from client */
				if (sh_read_data(sh, shc, event.data) < 0) {
					/* close client connection on failure */
					if (sh_disconnect_client(sh, shc)) {
						syslog(LOG_ERR, "Failed to disconnect client");
						/* TODO log error */
					}
					syslog(LOG_ERR, "Failed to read from client");
					continue;
				}

				bzero(&parsedata, sizeof(parsedata));				
				if (sh_try_cmdparsing(shc, &parsedata)) {
					syslog(LOG_ERR, "Failed to parse message data");
					err(SH_ERR_MSGPARSERFAIL, "Failed to parse message data");
				}

				/* if we're supposed to read more data, do just that */
				if (SH_WRN_KEEPREADNMORE == parsedata.errcode) {
					syslog(LOG_ERR, "Awaiting more input data");
					continue;
				}

				if (parsedata.errcode) {
					if (sh_reply_msg(sh, shc,
							 parsedata.errcode,
							 parsedata.errmsg)) {
						syslog(LOG_ERR, "Failed to reply error message");
						err(SH_ERR_REPLYMESGFAIL,
						    "Failed to reply error message");
					}
				}

				/* call listeners and send reply data to listeners */
				result = sh_call_listeners(sh, shc);
				/* shc contains a reply collector that helps us decide
				 * whether to send a generic response or actual blob data
				 */
				if (!src_has_reply(shc->src)) {
					if (src_has_short_reply(shc->src)) {
						/* transmit short reply instead */
						if (sh_reply_shortmsg(sh, shc, result,
								      src_get_short_reply(shc->src))) {
							/* TODO log error instead */
							err(SH_ERR_REPLYMESGFAIL,
							    "Failed to reply call message");
						}
					} else {
						if (sh_reply_genericmsg(sh, shc, result)) {
							/* TODO log error instead */
							err(SH_ERR_REPLYMESGFAIL,
							    "Failed to reply call message");
						}
					}
				} else {
					/* TODO handle long replies */
					if (sh_reply_data(sh, shc)) {
						/* TODO log error */
					}
				}

				/* Then drop the message from the buffer */
				if (shc_dropmessage(shc, &parsedata))
					err(SH_ERR_BUFFERCHGFAIL, "Failed to drop processed messge");
			}
		}
	}

	if (pthread_mutex_lock(&sh->mtx))
		err(SH_ERR_MUTEXLOCKFAIL, "Failed mutex lock on accept thread");

	sh->state = STOPPED;
	pthread_mutex_unlock(&sh->mtx);

	return NULL;
}

/*
 * stop listener thread
 */
int
sh_stop(struct socket_handle *sh)
{
	if (!sh)
		return SH_ERR_INVALIDPARAMS;

	struct kevent event = {0};

	if (pthread_mutex_lock(&sh->mtx))
		return SH_ERR_MUTEXLOCKFAIL;

	if (sh->state != RUNNING) {
		pthread_mutex_unlock(&sh->mtx);
		return SH_ERR_TISNOTRUNNING;
	}
	sh->state = STOPPING;
	pthread_mutex_unlock(&sh->mtx);

	EV_SET(&event, SH_EVT_CMD_SHUTDOWN, EVFILT_USER, 0, NOTE_TRIGGER, 0, sh);
	if (kevent(sh->keventfd, &event, 1, NULL, 0, 0) < 0)
		err(SH_ERR_STOPKEVFAILED, "Failed to issue stop command");

	/* wait for thread completion */
	if (pthread_join(sh->listener_thread, NULL))
		err(SH_ERR_THREADSTOFAIL, "Failed to join listener thread");

	return 0;
}

/*
 * start listener thread
 */
int
sh_start(struct socket_handle *sh)
{
	int result = 0;
	
	if (!sh || (sh->state < READY))
		return SH_ERR_INVALIDPARAMS;

	if (pthread_mutex_lock(&sh->mtx))
		return SH_ERR_MUTEXLOCKFAIL;

	if (sh->state >= STARTED) {
		pthread_mutex_unlock(&sh->mtx);
		return SH_ERR_ALREADYRUNNIN;
	}

	sh->state = STARTED;

	if (pthread_create(&sh->listener_thread, NULL,
			  sh_accept_thread, sh)) {
		result = SH_ERR_THREADSTAFAIL;
		sh->state = READY;
	}

	/* wait for thread ready */
	pthread_cond_wait(&sh->listener_ready, &sh->mtx);
	
	pthread_mutex_unlock(&sh->mtx);

	pthread_yield();
	
	return result;
}

/*
 * construct a new socket on a given path
 *
 * - sockpath: the socket file path
 * - mode: the mode to set the socket to (if not zero)
 *
 * returns NULL on error
 */
struct socket_handle *
sh_new(const char *sockpath, mode_t mode)
{
	if (!sockpath) {
		errno = EINVAL;
		return NULL;
	}

	struct socket_handle *sh = NULL;
	struct sockaddr_un sa = {0};
	struct kevent event = {0};

	errno = 0;

	sh = malloc(sizeof(struct socket_handle));
	if (!sh)
		return NULL;

	bzero(sh, sizeof(struct socket_handle));
	
	SLIST_INIT(&sh->listeners);
	LIST_INIT(&sh->connections);
	
	sh->keventfd = kqueue();
	
	if ((sh->socketfd = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
		sh_free(sh);
		return NULL;
	}

	/* bind to socket to given path */
	sa.sun_family = AF_UNIX;
	strncpy(sa.sun_path, sockpath, sizeof(sa.sun_path));
	sh->sockpath = strdup(sockpath);

	if (bind(sh->socketfd, (void*) &sa, sizeof(struct sockaddr_un)) < 0) {
		sh_free(sh);
		return NULL;
	}

	/* update permissions if required */
	if (mode) {
		if (chmod(sockpath, mode)) {
			/* failed to set permissions */
			sh_free(sh);
			return NULL;
		}
	}

	if (listen(sh->socketfd, SH_MAXCONNECT) < 0) {
		sh_free(NULL);
		return NULL;
	}
	
	/* construct mutex */
	if (pthread_mutex_init(&sh->mtx, NULL)) {
		/* failed to allocate mutex */
		sh_free(sh);
		return NULL;
	}

	if (pthread_cond_init(&sh->listener_ready, NULL)) {
		/* failed to allocate condition variable */
		sh_free(sh);
		return NULL;
	}

	if (pthread_mutex_lock(&sh->mtx))
		err(SH_ERR_MUTEXLOCKFAIL, "Failed to lock mutex on new");

	bzero(&event, sizeof(struct kevent));
	/* prepare event for accept */
	EV_SET(&event, sh->socketfd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, &sh);
	if (kevent(sh->keventfd, &event, 1, NULL, 0, NULL) < 0) {
		sh_free(sh);
		return NULL;
	}

	bzero(&event, sizeof(struct kevent));
	
	/* prepare kevent for shut down */
	EV_SET(&event, SH_EVT_CMD_SHUTDOWN, EVFILT_USER, EV_ADD | EV_ENABLE,
	       0, 0, &sh);
	if (kevent(sh->keventfd, &event, 1, NULL, 0, NULL) < 0) {
		sh_free(sh);
		return NULL;
	}

	sh->state = READY;

	pthread_mutex_unlock(&sh->mtx);

	return sh;
}

/*
 * release a previously allocated socket handle
 */
void
sh_free(struct socket_handle *sh)
{
	if (!sh)
		return;

	struct socket_listener *shl = 0;
	struct socket_connection *shc = 0;

	if (pthread_mutex_lock(&sh->mtx))
		err(SH_ERR_MUTEXLOCKFAIL, "Failed mutex lock on free");

	/* close connections */
	while (!LIST_EMPTY(&sh->connections)) {
		shc = LIST_FIRST(&sh->connections);
		LIST_REMOVE(shc, entries);
		shc_free(shc);
	}

	close(sh->socketfd);
	sh->socketfd = 0;

	/* TODO make this optional */
	unlink(sh->sockpath);
	free(sh->sockpath);
	
	while (!SLIST_EMPTY(&sh->listeners)) {
		shl = SLIST_FIRST(&sh->listeners);
		SLIST_REMOVE_HEAD(&sh->listeners, entries);
		shl_free(shl);
	}

	if (sh->keventfd)
		close(sh->keventfd);
	sh->keventfd = 0;

	pthread_mutex_unlock(&sh->mtx);

	if (pthread_cond_destroy(&sh->listener_ready))
		err(SH_ERR_CONDDESTRFAIL, "Failed condition variable destruction");

	if (pthread_mutex_destroy(&sh->mtx))
		err(SH_ERR_MUTEXDESTFAIL, "Failed mutex destroy on free");

	free(sh);
}
