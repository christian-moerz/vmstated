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

#ifndef __PROCESS_STATE_H__
#define __PROCESS_STATE_H__

#include "../liblogging/log_director.h"
#include "../libstate/state_handler.h"
#include "../libstate/state_node.h"

#include "bhyve_config.h"
#include "reboot_manager_object.h"

/*
 * available states
 *
 * usual start sequence:
 * INIT -> PRESTART -> START -> RUNNING
 *
 * shutdown
 * RUNNING -> PRESTOP -> STOPPING -> STOPPED
 *
 * restart
 * RUNNING -> PRESTOP_BEFORE_RESTART -> RESTARTING -> RESTART_STOPPED ->
 *            PRESTART_AFTER_RETART -> START_AFTER_RESTART ->
 *            RESTARTED -> RUNNING
 *
 * FAILED can be reached from any point in between
 */
typedef enum {
	INIT = 0,
	START_NETWORK = 10,
	PRESTART_AFTER_RESTART = 11,
	START_STORAGE = 20,
	START_AFTER_RESTART = 21,
	RUNNING = 100,
	RESTARTED = 101,
	STOPPING = 102,
	STOP_STORAGE = 150,
	PRESTOP_BEFORE_RESTART = 160,
	STOP_NETWORK = 200,
	RESTARTING = 210,
	STOPPED = 300,
	RESTART_STOPPED = 310,
	FAILED = 400
} bhyve_vmstate_t;

struct process_state_vm;

struct process_state_vm *psv_new(const struct bhyve_configuration *bc);
void psv_free(struct process_state_vm *psv);
bhyve_vmstate_t psv_getstate(const struct process_state_vm *psv);
pid_t psv_getpid(const struct process_state_vm *psv);

const char *psv_get_scriptpath(const struct process_state_vm *);

int psv_startvm(struct process_state_vm *psv, pid_t *pid, struct log_director_redirector *ldr);
int psv_stopvm(struct process_state_vm *psv, int *exitcode);
int psv_rebootvm(struct process_state_vm *psv, int *exitcode);
int psv_failurestate(struct process_state_vm *psv);
bool psv_is_failurestate(struct process_state_vm *psv);
struct process_state_vm *
psv_withrebootmgr(struct process_state_vm *psv, struct reboot_manager_object *rmo);
struct process_state_vm *
psv_with_logredirector(struct process_state_vm *psv,
		       struct log_director_redirector *ldr);
struct log_director_redirector *psv_get_logredirector(struct process_state_vm *psv);

#endif /* __PROCESS_STATE_H__ */
