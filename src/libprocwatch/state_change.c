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

#include <sys/stat.h>
#include <sys/wait.h>

#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>

#include "process_def.h"
#include "process_state.h"
#include "state_change.h"

#include "../libstate/state_node.h"

/*
 * called when a new state is being entered
 *
 * should return 0 when it completed successfully.
 */
int
sch_onenter(struct state_node *new_state, void *ctx, struct state_node *from, uint64_t from_state)
{
	struct process_state_vm *psv = ctx;
	struct stat file_info = {0};
	char exepath[PATH_MAX] = {0};

	/* run script with name of target state */
	const char *scriptpath = psv_get_scriptpath(psv);

	snprintf(exepath, PATH_MAX, "%s/%s", scriptpath, new_state->name);
	
	if (stat(exepath, &file_info) < 0) {
		/* ignore if file does not exist */

		/* TODO differentiate errno cases; file not found is ok */
		
		return 0;
	}

	syslog(LOG_INFO, "sch_onenter: sch_runscript(\"%s\", true)", exepath);
	return sch_runscript(exepath, true, psv_get_logredirector(psv));
}

/*
 * executes a user script and collects return code if waitfinish is true
 */
int
sch_runscript(const char *exepath, bool waitfinish, struct log_director_redirector *ldr)
{
	struct process_def *pd = 0;
	pid_t pid = 0;
	int result = 0;
	int status = 0;
	const char *argptr[2] = {0};

	argptr[0] = exepath;
	argptr[1] = NULL;

	pd = pd_new("user script", "run when state changes",
		    exepath,
		    argptr, /* no additional parameters */
		    NULL);
	if (!pd) {
		syslog(LOG_ERR, "sch_runscript: failed to instantiate new process_def");
		return -1;
	}

	/* just using redirected method, because if ldr is NULL, it will
	 * behave like there is no redirection */
	result = pd_launch_redirected(pd, &pid, ldr);

	pthread_yield();
	if ((!result) && waitfinish) {
		waitpid(pid, &status, 0);

		if (WIFEXITED(status)) {
			result = WEXITSTATUS(status);
			syslog(LOG_INFO, "pd_launch exit code = %d", result);
		} else {
			syslog(LOG_ERR, "WIFEXITED(status) not true");

			syslog(LOG_ERR, "WIFSIGNALED(status) = %d",
			       WIFSIGNALED(status));
			if (WIFSIGNALED(status)) {
				syslog(LOG_ERR, "signal: %d",
				       WTERMSIG(status));
				syslog(LOG_ERR, "coredump: %d",
				       WCOREDUMP(status));
			}
			syslog(LOG_ERR, "WIFSTOPPED(status) = %d",
			       WIFSTOPPED(status));
			
			result = -1;
		}
	} else {
		syslog(LOG_INFO, "pd_launch result = %d", result);
	}

	pd_free(pd);
	
	return result;
}
