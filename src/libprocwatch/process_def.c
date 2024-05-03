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
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "../libutils/bhyve_utils.h"

#include "bhyve_config.h"
#include "process_def.h"

/* external global variable for environment variables */
extern char **environ;

/*
 * a definition of a process to start, watch and relaunch if necessary
 */
struct process_def {
	char name[PATH_MAX];
	char *description;
	char procpath[PATH_MAX];
	char **procargs; /* null terminated argument list */
	
	void *ctx; /* user context */
};

void pd_free(struct process_def *pd);

struct process_def *
pd_new(const char *name,
       const char *description,
       const char *procpath,
       const char **procargs,
       void *ctx)
{
	if (!name || !procpath) {
		errno = EINVAL;
		return NULL;
	}

	struct process_def *pd = 0;
	size_t counter = 0, arglen = 0;
	const char **ptrdata = procargs;

	pd = malloc(sizeof(struct process_def));
	if (!pd)
		return NULL;

	strncpy(pd->name, name, PATH_MAX);
	
	bzero(pd, sizeof(struct process_def));
	if (description) {
		size_t desclen = strlen(description) + 1;
		pd->description = malloc(desclen);
		if (!pd->description) {
			free(pd);
			return NULL;
		}
		strncpy(pd->description, description, desclen);
	}

	strncpy(pd->procpath, procpath, PATH_MAX);

	if (procargs) {
		while (*ptrdata) {
			ptrdata++;
			counter++;
		}
		pd->procargs = malloc((counter+1) * sizeof(char *));
		if (!pd->procargs) {
			free(pd->description);
			free(pd);
			return NULL;
		}
		bzero(pd->procargs, sizeof(char *) * (counter+1));
		
		ptrdata = procargs;
		counter = 0;

		while (*ptrdata) {
			arglen = strlen(*ptrdata);
			pd->procargs[counter] = malloc(arglen+1);
			if (!pd->procargs[counter]) {
				pd_free(pd);
				return NULL;
			}
			strncpy(pd->procargs[counter], *ptrdata, arglen);
			
			ptrdata++;
			counter++;
		}
	} else {
		/* TODO place empty NULL pointer instead */
	}

	pd->ctx = ctx;

	return pd;
}

/*
 * launch application with redirection
 */
int
pd_launch_redirected(struct process_def *pd, pid_t *pid,
		     struct log_director_redirector *ldr)
{
	if (!pd || !pid)
		return -1;

	/* TODO add wrapper to add environment variable with vm name and command
	 *      i.e. vmname = test, cmd = start
	 */
	
	pid_t procpid = 0;
	struct log_director_redirector_client *ldrd = 0;

	if (ldr)
		if (!(ldrd = ldr_newclient(ldr)))
			return -1;

	syslog(LOG_INFO, "forking for \"%s\"...", pd->procpath);
	if (0 == (procpid = fork())) {
		syslog(LOG_INFO, "Starting child executable");
		if (ldrd) {
			ldrd_redirect_stdout(ldrd);
			ldrd_redirect_stderr(ldrd);
		}
		
		if (execve(pd->procpath, pd->procargs, environ) < 0) {
			syslog(LOG_ERR, "pd_launch_redirected: execve failed: errno = %d", errno);
			syslog(LOG_ERR, "pd_launch_redirected: pd->procpath: \"%s\"",
			       pd->procpath);
			syslog(LOG_ERR, "pd_launch_redirected: pd->procargs[0] = \"%s\"",
			       pd->procargs[0]);
			exit(1);
		}
		
		exit(0);
	}
	if (ldrd)
		ldrd_accept_redirect(ldrd);

	if (procpid < 0) {
		syslog(LOG_ERR, "fork failed - procpid = %d, errno = %d", procpid, errno);
		return procpid;
	}

	if (pid)
		*pid = procpid;

	syslog(LOG_INFO, "pd_launch returning 0 for \"%s\"", pd->procpath);
	
	return 0;
}

/*
 * launch application
 *
 * - pd: pointer to process definition
 * - pid: pointer to receive the pid if successful launch
 *
 * returns 0 on success
 */
int
pd_launch(struct process_def *pd, pid_t *pid)
{
	return pd_launch_redirected(pd, pid, NULL);
}

/*
 * build a new process definition from a configuration
 */
struct process_def *
pd_fromconfig(const struct bhyve_configuration *bc)
{
	if (!bc)
		return NULL;

	struct process_def *pd = malloc(sizeof(struct process_def));
	size_t desclen = 0;
	
	if (!pd)
		return NULL;

	/* add name and description */
	strncpy(pd->name, bc_get_name(bc), PATH_MAX);
	if (bc_get_description(bc)) {
		desclen = strlen(bc_get_description(bc));
		pd->description = malloc(desclen+1);
		if (!pd->description) {
			free(pd);
			return NULL;
		}
		strncpy(pd->description, bc_get_description(bc), desclen+1);
	} else {
		pd->description = NULL;
	}

	/* transfer program arguments */
	strncpy(pd->procpath, BHYVEBIN, PATH_MAX);
	
	/* add -k option */
	pd->procargs = malloc(sizeof(char **)*4);
	if (!pd->procargs) {
		free(pd->description);
		free(pd);
		return NULL;
	}
	pd->procargs[0] = strdup(pd->procpath);
	pd->procargs[3] = NULL;
	pd->procargs[1] = malloc(3);
	if (!pd->procargs[1]) {
		free(pd->procargs[0]);
		free(pd->description);
		free(pd);
		return NULL;
	}
	strncpy(pd->procargs[1], "-k", 3);
	pd->procargs[2] = malloc(PATH_MAX);
	if (!pd->procargs[2]) {
		free(pd->procargs[0]);
		free(pd->procargs[1]);
		free(pd->description);
		free(pd);
		return NULL;
	}
	/*
	 * if we have a generated config, we use that instead of the original one
	 */
	if (bc_get_generated_config(bc)) 
		strncpy(pd->procargs[2], bc_get_generated_config(bc), PATH_MAX);
	else 
		strncpy(pd->procargs[2], bc_get_configfile(bc), PATH_MAX);

	return pd;
}

/*
 * Updates the config file path given to bhyve
 */
int
pd_set_configfile(struct process_def *pd, const char *configfile)
{
	if (!pd || !configfile) {
		errno = EINVAL;
		return -1;
	}

	bzero(pd->procargs[2], PATH_MAX);
	strncpy(pd->procargs[2], configfile, PATH_MAX);
	return 0;
}

/*
 * free a previously allocated process definition
 */
void
pd_free(struct process_def *pd)
{
	char **ptrdata = 0;

	if (!pd)
		return;

	if (pd->procargs) {
		ptrdata = pd->procargs;
		while (*ptrdata) {
			free(*ptrdata);
			ptrdata++;
		}
		free(pd->procargs);
	}
	free(pd->description);
	free(pd);	
}
