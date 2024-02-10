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

#ifndef __PROCESS_DEF_OBJECT_H__
#define __PROCESS_DEF_OBJECT_H__

#include "bhyve_config.h"
#include "process_def.h"
#include "../liblogging/log_director.h"

/*
 * function table definition for process_def_obj
 */
struct process_def_funcs {
	/* launch the process definition */
	int (*launch)(void *ctx, pid_t *pid);
	/* launch process definition with redirection */
	int (*launch_redirected)(void *ctx, pid_t *pid, struct log_director_redirector *ldr);
	/* update configuration file to use when executing bhyve */
	int (*set_configfile)(void *ctx, const char *configfile);
	/* release context */
	void (*free)(void *ctx);
};

/*
 * object wrapper for process_def
 */
struct process_def_obj {
	void *ctx;

	struct process_def_funcs *funcs;
};

struct process_def_obj * pdobj_frompd(struct process_def *pd);
void pdobj_free(struct process_def_obj *pdo);
struct process_def_obj * pdobj_fromconfig(const struct bhyve_configuration *bc);

#endif /* __PROCESS_DEF_OBJECT_H__ */
