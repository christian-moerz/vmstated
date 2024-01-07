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

#ifndef __SOCKET_CONNECT_H__
#define __SOCKET_CONNECT_H__

struct socket_connection;

struct socket_connection *sc_new(const char *sockpath);
int		sc_connect(struct socket_connection *sc);
void		sc_free(struct socket_connection *sc);
int
sc_sendrecv(struct socket_connection *sc,
	    const char *command,
	    const char *data,
	    char *retbuffer,
	    size_t retbuffer_len);

int
sc_sendrecv_len(struct socket_connection *sc,
		const char *command,
		const void *data,
		size_t datalen,
		char *retbuffer,
		size_t retbuffer_len);

int
sc_sendrecv_dynamic(struct socket_connection *sc,
		    const char *command,
		    const void *data,
		    size_t datalen,
		    char *retbuffer,
		    size_t retbuffer_len,
		    void **blob_reply,
		    size_t *blob_reply_len);	

#endif /* __SOCKET_CONNECT_H__ */
