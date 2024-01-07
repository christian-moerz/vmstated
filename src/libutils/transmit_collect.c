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

#include <sys/queue.h>

#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "transmit_collect.h"

/*
 * a byte buffer to retain as payload for transmission
 */
struct socket_transmission_collector_item {
	void *buffer;
	size_t bufferlen;

	STAILQ_ENTRY(socket_transmission_collector_item) entries;
};

struct socket_transmission_collector {
	pthread_mutex_t mtx;

	STAILQ_HEAD(,socket_transmission_collector_item) items;
};

/*
 * construct a new transmission collector
 */
struct socket_transmission_collector *
stc_new()
{
	struct socket_transmission_collector *stc = 0;

	if (!(stc = malloc(sizeof(struct socket_transmission_collector))))
		return NULL;

	STAILQ_INIT(&stc->items);

	if (pthread_mutex_init(&stc->mtx, NULL)) {
		stc_free(stc);
		return NULL;
	}

	return stc;
}

/*
 * store buffer for transmission (i.e. for reply to client)
 *
 * the buffer is copied into the collector; so the original can be free
 * right after calling this method.
 */
int
stc_store_transmit(struct socket_transmission_collector *stc, const void *buffer, size_t bufferlen)
{
	if (!stc || !buffer || !bufferlen)
		return -1;

	struct socket_transmission_collector_item *stci = 0;

	if (!(stci = malloc(sizeof(struct socket_transmission_collector_item))))
		return -1;

	if (!(stci->buffer = malloc(bufferlen))) {
		free(stci);
		return -1;
	}
	bcopy(buffer, stci->buffer, bufferlen);
	stci->bufferlen = bufferlen;

	if (pthread_mutex_lock(&stc->mtx)) {
		free(stci->buffer);
		free(stci);
		return -1;
	}
	STAILQ_INSERT_TAIL(&stc->items, stci, entries);
	if (pthread_mutex_unlock(&stc->mtx))
		return -1;

	return 0;
}

/*
 * collect buffer data from collector
 *
 * copies all separate buffers that were collected into
 * buffer and provides separate buffer lengths.
 *
 * - stc: collector handle
 * - buffer: the buffer to copy into, must be allocated
 * - bufferlen: size of buffer
 * - buffer_lens: receives array of size_t of the different
 *                lengths of buffers that are copied into
 *                buffer
 * - buffer_lenslen: size of bufferlens in bytes, to ensure
 *                   we are not copying too many items into
 *                   bufferlens
 */
int
stc_collect(struct socket_transmission_collector *stc,
	    void *buffer, ssize_t bufferlen,
	    size_t *buffer_lens, size_t buffer_lenslen)
{
	if (!stc || !buffer || !bufferlen || !buffer_lens ||
	    !buffer_lenslen)
		return -1;

	/* unable to fit buffers? */
	if ((bufferlen < stc_getbuffersize(stc)) ||
	    (buffer_lenslen < (sizeof(size_t)*stc_getbuffercount(stc)))) {
		errno = ENOMEM;
		return -1;
	}

	ssize_t offset = 0;
	size_t counter = 0;
	int result = 0;
	struct socket_transmission_collector_item *stci = 0;

	if (pthread_mutex_lock(&stc->mtx))
		return -1;

	STAILQ_FOREACH(stci, &stc->items, entries) {
		bcopy(stci->buffer, buffer + offset, stci->bufferlen);
		buffer_lens[counter] = stci->bufferlen;

		counter++;
		offset += stci->bufferlen;
	}

	if (pthread_mutex_unlock(&stc->mtx))
		return -1;
	
	return result;
}

/*
 * get number of buffers in this collector
 *
 * if an error occurs, errno will be set and zero will be returned.
 */
size_t
stc_getbuffercount(struct socket_transmission_collector *stc)
{
	size_t total = 0;
	struct socket_transmission_collector_item *stci = 0;

	if (pthread_mutex_lock(&stc->mtx))
		return 0;

	STAILQ_FOREACH(stci, &stc->items, entries) {
		total++;
	}

	if (pthread_mutex_unlock(&stc->mtx))
		return 0;

	return total;
}

/*
 * get size of all buffers in total in this collector
 *
 * if an error occurs, errno will be set and zero will be returned.
 */
ssize_t
stc_getbuffersize(struct socket_transmission_collector *stc)
{
	ssize_t total = 0;
	struct socket_transmission_collector_item *stci = 0;
	
	if (pthread_mutex_lock(&stc->mtx))
		return 0;

	STAILQ_FOREACH(stci, &stc->items, entries) {
		total += stci->bufferlen;
	}

	if (pthread_mutex_unlock(&stc->mtx))
		return 0;

	return total;
}

/*
 * release previously allocated transmission collector
 */
void
stc_free(struct socket_transmission_collector *stc)
{
	if (!stc)
		return;

	struct socket_transmission_collector_item *stci = 0;

	pthread_mutex_lock(&stc->mtx);

	while(!STAILQ_EMPTY(&stc->items)) {
		stci = STAILQ_FIRST(&stc->items);
		STAILQ_REMOVE_HEAD(&stc->items, entries);

		free(stci->buffer);
		free(stci);
	}

	pthread_mutex_unlock(&stc->mtx);

	pthread_mutex_destroy(&stc->mtx);

	free(stc);
}
