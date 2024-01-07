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

#ifndef __SOCKET_HANDLE_ERRORS_H__
#define __SOCKET_HANDLE_ERRORS_H__

#define SH_CMDERR_INVALIDCMD 001 /* invalid command - too long */
#define SH_CMDERR_GARBAGECMD 002 /* garbage input */
#define SH_CMDERR_DATATOOLON 003 /* data too long */

#define SH_ERR_INVALIDPARAMS 200 /* invalid call parameters */
#define SH_ERR_ITEMALLOCFAIL 197 /* failed to allocate memory */
#define SH_ERR_MUTEXLOCKFAIL 192
#define SH_ERR_MUTEXDESTFAIL 191 /* mutex destruction failed */
#define SH_ERR_THREADSTOFAIL 190 /* failed to stop management thread */
#define SH_ERR_ALREADYRUNNIN 189 /* management thread already active */
#define SH_ERR_TISNOTRUNNING 188 /* management thread not running */
#define SH_ERR_STOPKEVFAILED 187 /* failed to issue top kevent */
#define SH_ERR_KQUQUERFAILED 186 /* kqueue query failed */
#define SH_ERR_SOCBINDFAILED 185 /* socket bind failed */
#define SH_ERR_SLISTENFAILED 184 /* listen on socket failed */
#define SH_ERR_THREADSTAFAIL 183 /* thread start failed */

/* 170-180 reserved for socket handle errors */
#define SH_ERR_SOCKTACCTFAIL 180 /* failed to accept incoming connection */
#define SH_ERR_GETSOCKOPFAIL 179 /* failed to get socket details */
#define SH_ERR_ADDKEVENTFAIL 178 /* failed to add kqueue event to queue */
#define SH_ERR_READSOCKTFAIL 177 /* failed to read from socket */
#define SH_ERR_BUFFERCHGFAIL 176 /* failed to change buffer */
#define SH_ERR_REPLYMESGFAIL 175 /* failed to reply message */
#define SH_ERR_MSGPARSERFAIL 174 /* failed to parse message */
#define SH_ERR_CONDDESTRFAIL 173 /* failed destruction of condition variable */
#define SH_WRN_KEEPREADNMORE 172 /* keep reading more data, not yet done */

#endif /* __SOCKET_HANDLE_ERRORS_H__ */
