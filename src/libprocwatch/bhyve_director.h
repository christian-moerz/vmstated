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

#ifndef __BHYVE_DIRECTOR_H__
#define __BHYVE_DIRECTOR_H__

#include "bhyve_config_object.h"
#include "bhyve_messagesub_object.h"
#include "config_generator_object.h"

#include "../liblogging/log_director.h"

struct bhyve_director;

int bd_subscribe_commands(struct bhyve_director *bd, struct bhyve_messagesub_obj *bmo);
struct bhyve_director *bd_new(struct bhyve_configuration_store_obj *bcso,
			      struct log_director *ld);
void bd_free(struct bhyve_director *bd);
uint64_t bd_getmsgcount(struct bhyve_director *bd);
int bd_startvm(struct bhyve_director *bd, const char *name);
int bd_resetfailvm(struct bhyve_director *bd, const char *name);
int bd_stopvm(struct bhyve_director *bd, const char *name);
struct bhyve_vm_manager_info *bd_getinfo(struct bhyve_director *bd);
int
bd_set_cgo(struct bhyve_director *bd,
	   struct config_generator_object *cgo);
int bd_runautostart(struct bhyve_director *bd);

#endif /* __BHYVE_DIRECTOR_H__ */
