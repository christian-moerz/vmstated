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

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "reply_collector.h"

#include "../libutils/transmit_collect.h"
#include "../libutils/bhyve_utils.h"

/*
 * The socket_reply_collector collects reply message data from data listeners
 * and allows socket_handle to combine replies after it has finished calling
 * on_data event handlers.
 */
struct socket_reply_collector {
	/* buffer collector */
	struct socket_transmission_collector *stc;

	/* buffer for overwriting OK/NOK messages */
	char *short_reply;
	/* collected buffer to provide to callers */
	char *collected_buffer;
};

/*
 * instantiate a new socket reply collector
 *
 * returns NULL and sets errno on error.
 */
struct socket_reply_collector *
src_new()
{
	struct socket_reply_collector *src = 0;

	src = malloc(sizeof(struct socket_reply_collector));
	if (!src)
		return NULL;

	src->short_reply = NULL;
	src->collected_buffer = NULL;
	
	src->stc = stc_new();
	if (!src->stc) {
		free(src);
		return NULL;
	}
	
	return src;
}

/*
 * release previously allocated socket_reply_collector
 */
void
src_free(struct socket_reply_collector *src)
{
	if (!src)
		return;

	stc_free(src->stc);
	free(src->collected_buffer);
	free(src->short_reply);
	free(src);
}

/*
 * get const pointer to collected buffer; the buffer belongs to the
 * collector and must not be freed by the caller.
 *
 * returns NULL and errno set on error.
 */
const void *
src_get_reply(struct socket_reply_collector *src, size_t *bufferlen)
{
	if (!src || !bufferlen) {
		errno = EINVAL;
		return NULL;
	}

	if (src->collected_buffer) {
		free(src->collected_buffer);
		src->collected_buffer = NULL;
	}

	size_t *lengths;
	size_t lengths_len;

	lengths_len = sizeof(size_t) * stc_getbuffercount(src->stc);
	if (!(lengths = malloc(lengths_len))) {
		return NULL;
	}

	if (!(src->collected_buffer = malloc(stc_getbuffersize(src->stc)))) {
		free(lengths);
		return NULL;
	}

	if (stc_collect(src->stc, src->collected_buffer, stc_getbuffersize(src->stc),
			lengths, lengths_len)) {
		free(lengths);
		return NULL;
	}

	*bufferlen = stc_getbuffersize(src->stc);
	/* lengths array no longer required */
	free(lengths);
	
	return src->collected_buffer;
}

/*
 * sets a short reply, unless one is already set.
 *
 * returns 0 on success, -1 and errno if an error occurred,
 * 1 if a short reply was already set.
 */
int
src_short_reply(struct socket_reply_collector *src, const char *reply)
{
	if (!src) {
		errno = EINVAL;
		return -1;
	}

	if (src->short_reply)
		return 1;

	if (!(src->short_reply = strdup(reply))) {
		return -1;
	}

	return 0;
}

/*
 * check whether a long reply was set
 */
bool
src_has_reply(struct socket_reply_collector *src)
{
	if (!src)
		return false;
	return (0 != stc_getbuffersize(src->stc));
}

/*
 * check whether a short reply was set
 */
bool
src_has_short_reply(struct socket_reply_collector *src)
{
	if (!src)
		return false;
	return (0 != src->short_reply);
}

/*
 * add reply data to the collector */
int
src_reply(struct socket_reply_collector *src, const void *buffer, size_t bufferlen)
{
	if (!src || !buffer) {
		errno = EINVAL;
		return -1;
	}

	return stc_store_transmit(src->stc, buffer, bufferlen);
}

CREATE_GETTERFUNC_STR(socket_reply_collector, src, short_reply);
