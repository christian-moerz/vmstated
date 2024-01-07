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

#ifndef __LOG_DIRECTOR_H__
#define __LOG_DIRECTOR_H__

struct log_director_redirector_client;
struct log_director_redirector;
struct log_director;

int ldr_get_senderpipe(struct log_director_redirector *ldr);

struct log_director_redirector *ld_register_redirect(struct log_director *ld, const char *logname);
struct log_director *ld_new(int verbosity, char *log_directory);
void ld_free(struct log_director *ld);
int ldrd_redirect_stdout(struct log_director_redirector_client *ldr);
int ldrd_redirect_stderr(struct log_director_redirector_client *ldr);
int ldrd_accept_redirect(struct log_director_redirector_client *ldr);
struct log_director_redirector_client *ldr_newclient(struct log_director_redirector *ldr);
void ldrd_freeclient(struct log_director_redirector_client *ldrd);

#endif /* __LOG_DIRECTOR_H__ */
