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

#include <stdlib.h>
#include <unistd.h>

#include "process_def.h"
#include "process_def_object.h"

struct process_def_funcs pd_funcs = {
	.launch = (void*) pd_launch,
	.launch_redirected = (int (*)(void *,
				      pid_t *,
				      struct log_director_redirector *)) pd_launch_redirected,
	.free = (void*) pd_free
};

/*
 * construct a process_def_object from a process_def
 *
 * The process_def_obj assumes ownership of the provided process_def
 * and is therefore released when the process_def_obj is released.
 */
struct process_def_obj *
pdobj_frompd(struct process_def *pd)
{
	struct process_def_obj *pdo = malloc(sizeof(struct process_def_obj));
	if (!pdo)
		return NULL;

	pdo->ctx = pd;
	pdo->funcs = &pd_funcs;

	return pdo;
}

/*
 * constructs an object directly out of a bhyve_configuration
 */
struct process_def_obj *
pdobj_fromconfig(const struct bhyve_configuration *bc)
{
	struct process_def *pd = pd_fromconfig(bc);

	if (!pd)
		return NULL;

	return pdobj_frompd(pd);
}

/*
 * release a process_def_obj and its context, if free is pointing to a
 * valid release function
 */
void
pdobj_free(struct process_def_obj *pdo)
{
	if (!pdo)
		return;
	if (pdo->funcs->free)
		pdo->funcs->free(pdo->ctx);
	
	free(pdo);
}
