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

#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "../liblogging/log_director.h"
#include "../libstate/state_handler.h"
#include "../libutils/bhyve_utils.h"

#include "bhyve_config.h"
#include "process_def_object.h"
#include "process_state.h"
#include "process_state_errors.h"
#include "reboot_manager_object.h"
#include "state_change.h"

/*
 * list of process states and transition functions
 */
struct state_node process_state_list[] = {
	/* 0 */
	{
		.id = INIT,
		.name = "init",
		.on_enter = NULL,
		.on_exit = NULL
	},
	/* 1 */
	{
		.id = START_NETWORK,
		.name = "start_network",
		.on_enter = sch_onenter,
		.on_exit = NULL
	},
	/* 2 */
	{
		.id = PRESTART_AFTER_RESTART,
		.name = "prestart_after_restart",
		.on_enter = sch_onenter,
		.on_exit = NULL
	},
	/* 3 */
	{
		.id = START_STORAGE,
		.name = "start_storage",
		.on_enter = sch_onenter,
		.on_exit = NULL
	},
	/* 4 */
	{
		.id = START_AFTER_RESTART,
		.name = "start_after_restart",
		.on_enter = sch_onenter,
		.on_exit = NULL
	},
	/* 5 */
	{
		.id = RUNNING,
		.name = "running",
		.on_enter = sch_onenter,
		.on_exit = NULL
	},
	/* 6 */
	{
		.id = RESTARTED,
		.name = "restarted",
		.on_enter = sch_onenter,
		.on_exit = NULL
	},
	/* 7 */
	{
		.id = STOP_STORAGE,
		.name = "stop_storage",
		.on_enter = sch_onenter,
		.on_exit = NULL
	},
	/* 8 */
	{
		.id = PRESTOP_BEFORE_RESTART,
		.name = "prestop_before_restart",
		.on_enter = sch_onenter,
		.on_exit = NULL
	},
	/* 9 */
	{
		.id = STOP_NETWORK,
		.name = "stop_network",
		.on_enter = sch_onenter,
		.on_exit = NULL
	},
	/* 10 */
	{
		.id = RESTARTING,
		.name = "restarting",
		.on_enter = sch_onenter,
		.on_exit = NULL
	},
	/* 11 */
	{
		.id = STOPPED,
		.name = "stopped",
		.on_enter = sch_onenter,
		.on_exit = NULL
	},
	/* 12 */
	{
		.id = RESTART_STOPPED,
		.name = "restart_stopped",
		.on_enter = sch_onenter,
		.on_exit = NULL
	},
	/* 13 */
	{
		.id = FAILED,
		.name = "failed",
		.on_enter = sch_onenter,
		.on_exit = NULL
	},
	/* 14 */
	{
		.id = STOPPING,
		.name = "stopping",
		.on_enter = sch_onenter,
		.on_exit = NULL
	}
};

/*
 * a helper macro to get a specific state structure
 */
#define PS_STATE(state) \
	&process_state_list[sth_lookupstate(process_state_list, sizeof(process_state_list)/sizeof(struct state_node), state)]

/*
 * possible transition between states
 */
struct state_transition process_transition_list[] = {
	{ /* INIT -> PRESTART */
		.from = &process_state_list[0],
		.to = &process_state_list[1]
	},
	{ /* START_NETWORK -> START_STORAGE */
		.from = &process_state_list[1],
		.to = &process_state_list[3]
	},
	{ /* START_NETWORK -> STOP_NETWORK */
		.from = &process_state_list[1],
		.to = &process_state_list[9]
	},
	{ /* START -> RUNNING */
		.from = &process_state_list[3],
		.to = &process_state_list[5]
	},
	{ /* RUNNNING -> STOPPING */
		.from = &process_state_list[5],
		.to = &process_state_list[14]
	},
	{ /* STOPPING -> STOP_STORAGE */
		.from = &process_state_list[14],
		.to = &process_state_list[7]
	},
	{ /* RUNNING -> PRESTOP_BEFORE_RESTART */
		.from = &process_state_list[5],
		.to = &process_state_list[8]
	},
	{ /* STOP_STORAGE -> STOP_NETWORK */
		.from = &process_state_list[7],
		.to = &process_state_list[9]
	}, 
	{ /* STOP_NETWORK -> STOPPED */
		.from = &process_state_list[9],
		.to = &process_state_list[11],
	},
	{ /* PRESTOP_BEFORE_RESTART -> RESTARTING */
		.from = &process_state_list[8],
		.to = &process_state_list[10]
	},
	{ /* RESTARTING -> RESTART_STOPPED */
		.from = &process_state_list[10],
		.to = &process_state_list[12]
	},
	{ /* RESTART_STOPPED -> PRESTART_AFTER_RESTART */
		.from = &process_state_list[12],
		.to = &process_state_list[2]
	},
	{ /* PRESTART_AFTER_RESTART -> START_AFTER_RESTART */
		.from = &process_state_list[2],
		.to = &process_state_list[4]
	},
	{ /* START_AFTER_RESTART -> RESTARTED */
		.from = &process_state_list[4],
		.to = &process_state_list[6]
	},
	{ /* RESTARTED -> RUNNING */
		.from = &process_state_list[6],
		.to = &process_state_list[5]
	},
	{ /* STOPPED -> FAILURE */
		.from = &process_state_list[11],
		.to = &process_state_list[13]
	},
	{ /* X -> STOP_STORAGE */
		.from = NULL,
		.to = &process_state_list[7]
	},
	{ /* START_NETWORK -> STOP_NETWORK */
		.from = &process_state_list[1],
		.to = &process_state_list[9]
	},
	{ /* STOPPED -> START_NETWORK */
		.from = &process_state_list[11],
		.to = &process_state_list[1]
	}
};

/*
 * combines bhyve configuration and actual process state
 */
struct process_state_vm {
	struct process_def_obj *pdo;
	struct state_handler *sth;

	/* structure lock */
	pthread_mutex_t mtx;

	/* pointer to a reboot manager that can service reboot requests */
	struct reboot_manager_object *rmo;

	/* pointer to a log redirector */
	struct log_director_redirector *ldr;

	/* the path where state scripts can be found */
	char *scriptpath;

	/* contains process id if it was started */
	pid_t processid;
};

/*
 * get current vm state
 */
bhyve_vmstate_t
psv_getstate(const struct process_state_vm *psv)
{
	return sth_getcurrentid(psv->sth);
}

/*
 * get process id
 */
pid_t
psv_getpid(const struct process_state_vm *psv)
{
	pid_t retpid = 0;

	if (pthread_mutex_lock((pthread_mutex_t *) &psv->mtx)) {
		errno = EDEADLK;
		return 0;
	}

	retpid = psv->processid;

	pthread_mutex_unlock((pthread_mutex_t *) &psv->mtx);
	
	return retpid;
}

/*
 * handler method, called whenever child process exits
 */
int
psv_onexit(struct process_state_vm *psv, unsigned short exitcode)
{
	uint64_t state = psv_getstate(psv);

	syslog(LOG_INFO, "psv_onexit started for process %d with exit code %d",
	       psv->processid, exitcode);

	switch (state) {
	case STOPPED:
		/* we are already stopped and don't do anything */
		break;
	case PRESTOP_BEFORE_RESTART:
		switch (exitcode) {
		case 0:
			/* reboot */
			if (sth_transitionto(psv->sth, RESTARTING))
				return -1;
			if (sth_transitionto(psv->sth, RESTART_STOPPED))
				return -1;
			if (psv->rmo)
				/* delegate reboot to reboot manager */
				if (psv->rmo->funcs->request_reboot(psv->rmo->ctx,
								    psv)) {
					psv_failurestate(psv);
					return -1;
				}
			break;
		case 1:
		case 2:
			/* should never happen */
			break;
		default:
			/* TODO log error */
			break;
		}
		break;
	case RUNNING:
		/* we have come here because the VM did something */
		switch (exitcode) {
		case 0:
			/* reboot */
			/* TODO queue for restart by bhyve_director via psv_startvm */
			if (sth_transitionto(psv->sth, PRESTOP_BEFORE_RESTART))
				return -1;
			if (sth_transitionto(psv->sth, RESTARTING))
				return -1;
			if (sth_transitionto(psv->sth, RESTART_STOPPED))
				return -1;
			if (psv->rmo)
				/* delegate reboot to reboot manager */
				if (psv->rmo->funcs->request_reboot(psv->rmo->ctx,
								    psv)) {
					psv_failurestate(psv);
					return -1;
				}
			break;
		case 1:
		case 2:
			/* shutdown and halt */
			if (sth_transitionto(psv->sth, STOP_STORAGE))
				return -1;
			if (sth_transitionto(psv->sth, STOP_NETWORK))
				return -1;
			if (sth_transitionto(psv->sth, STOPPED))
				return -1;
			
			break;
		}
		break;
	case STOPPING:
		/* we have come here after signaling TERM */
		if ((1 == exitcode) || (2 == exitcode)) {
		} else if (0 == exitcode) {
			/* unexpected reboot return code */
			/* TODO think about handling this differently */
			syslog(LOG_ERR, "Unexpected return code 0");
		}
		/* we shut down successfully */
		if (sth_transitionto(psv->sth, STOP_STORAGE))
			return -1;
		if (sth_transitionto(psv->sth, STOP_NETWORK))
			return -1;
		if (sth_transitionto(psv->sth, STOPPED))
			return -1;
		break;
	}
	
	if (exitcode >= 3) {
		/* TODO lock overall and build a unlocked-failurestate func instead */
		/* this would combine the locking */

		/* still progress through shut down */
		/* we have a failure state! */
		psv_failurestate(psv);
		
		if (pthread_mutex_lock(&psv->mtx)) {
			return PSV_ERR_MUTEXLOCKFAILED;
		}
		
		/* reset the pid */
		psv->processid = 0;
		
		if (pthread_mutex_unlock(&psv->mtx)) {
			return PSV_ERR_MUTEXUNLOCKFAIL;
		}
	}
	
	return 0;
}

/*
 * put into failure state, i.e. if too many attempted but
 * failed restarts within maxrestarttime
 */
int
psv_failurestate(struct process_state_vm *psv)
{
	if (!psv)
		return -1;

	if (sth_transitionto(psv->sth, STOP_STORAGE))
		return -1;
	if (sth_transitionto(psv->sth, STOP_NETWORK))
		return -1;
	if (sth_transitionto(psv->sth, STOPPED))
		return -1;
	if (sth_transitionto(psv->sth, FAILED))
		return -1;

	return 0;
}

/*
 * check if the vm is in failure state
 */
bool
psv_is_failurestate(struct process_state_vm *psv)
{
	if (!psv)
		return true;
	
	return (FAILED == psv_getstate(psv));
}

/*
 * start the vm
 *
 * returns 0 on success; puts resulting pid into pid
 */
int
psv_startvm(struct process_state_vm *psv, pid_t *pid,
	    struct log_director_redirector *ldr)
{
	if (!psv) {
		errno = EINVAL;
		return -1;
	}

	bhyve_vmstate_t current_state = psv_getstate(psv);
	bool restarting = (RESTART_STOPPED == current_state);
	uint64_t target_state = 0;
	bool failure = false;
	int result = 0;

	syslog(LOG_INFO, "current_state = %d", current_state);
	
	/* if we're not in stopped or init state, we fail */
	if ((INIT != current_state) && (STOPPED != current_state) &&
	    (RESTART_STOPPED != current_state)) {
		errno = EALREADY;
		syslog(LOG_ERR, "vm already running");
		return -1;
	}

	/* we're either INIT | STOPPED | RESTART_STOPPED */

	/* START_NETWORK | PRESTART_AFTER_RESTART */
	target_state = restarting ? PRESTART_AFTER_RESTART : START_NETWORK;
	if (sth_transitionto(psv->sth, target_state)) {
		syslog(LOG_ERR, "Failed to transition to PRESTART/START_NETWORK");
		if (INIT == current_state) {
			syslog(LOG_ERR, "Failed out of INIT - staying in INIT");
			/* just stay in INIT */
			return -1;
		}
		
		/* goto failure */
		if (restarting) {
			/* we go to STOP_STORAGE only if we are restarting; because if
			 * we are not restarting, we have not yet started storage
			 * therefore we don't want to stop it either
			 */
			if (sth_transitionto(psv->sth, STOP_STORAGE)) {
				syslog(LOG_ERR, "Failed to transition to STOP_STORAGE");
				return -1;
			}
		}
		if (sth_transitionto(psv->sth, STOP_NETWORK)) {
			syslog(LOG_ERR, "Failed to transition to STOP_NETWORK");
			return -1;
		}
		if (sth_transitionto(psv->sth, STOPPED)) {
			syslog(LOG_ERR, "Failed to transition to STOPPED");
			return -1;
		}
		if (sth_transitionto(psv->sth, FAILED)) {
			syslog(LOG_ERR, "Failed to transition to FAILED");
			return -1;
		}
	}

	/* START_STORAGE | START_AFTER_RESTART */
	target_state = restarting ? START_AFTER_RESTART : START_STORAGE;
	if (sth_transitionto(psv->sth, target_state)) {
		syslog(LOG_ERR, "Failed to transition to START_AFTER/START_STORAGE");
		/* goto failure */
		if (psv_failurestate(psv)) {
			syslog(LOG_ERR, "Failed to transition to FAILURE");
			return -1;
		}
	}

	/* execute bhyve command, retrieve pid, sign up for kqueue monitor */

	if (pthread_mutex_lock(&psv->mtx)) {
		syslog(LOG_ERR, "Failed to lock mutex");
		
		if (psv_failurestate(psv)) {
			syslog(LOG_ERR, "Failed to transition to FAILURE");
			return -1;
		}
		
		errno = EDEADLK;
		return -1;
	}

	/* we let the object launch the program and return the
	 * process id directly into our structure variable
	 *
	 * we are always calling redirected method because if ldr is NULL
	 * it will behave as if ldr was not set and we run without
	 * redirection
	 */
	if ((result = psv->pdo->funcs->launch_redirected(psv->pdo->ctx, &psv->processid,
							 ldr ? ldr : psv->ldr))) {
		/* failure */
		failure = true;
		syslog(LOG_ERR, "Call to launch failed, result = %d", result);
	}

	/* transfer pid to caller */
	if (!failure)
		*pid = psv->processid;

	/* TODO rework the following steps...
	 * if we fail any of the following steps, the process could
	 * actually be running but the VM could land in a failure state
	 * regardless
	 *
	 * we need a config variable whether we want to stop the
	 * vm when a user script fails past the start step
	 */

	do {
		if (pthread_mutex_unlock(&psv->mtx) || failure) {
			syslog(LOG_ERR, "Mutex unlock fail while failure = %d", failure);
			
			result |= PSV_ERR_MUTEXUNLOCKFAIL;
			
			/* doesn't matter if transition fails, we return -1 anyways */
			if (psv_failurestate(psv)) {
				syslog(LOG_ERR, "Failed to transition to FAILURE");
				result |= PSV_ERR_TRANSITIONSFAIL;
			}
			
			break;
		}

		/* RUNNING | RESTARTED */
		target_state = restarting ? RESTARTED : RUNNING;
		if (sth_transitionto(psv->sth, target_state)) {
			result |= PSV_ERR_TRANSITIONSFAIL;

			/* goto failure */
			/* failure doesn't matter because we already
			   failed a transition */
			psv_failurestate(psv);
			break;
		}
		
		/* RUNNING */
		if (restarting) {
			target_state = RUNNING;
			if (sth_transitionto(psv->sth, target_state)) {
				result |= PSV_ERR_TRANSITIONSFAIL;
				
				/* goto failure */
				/* failure doesn't matter because we already
				   failed a transition */
				psv_failurestate(psv);
				break;
			}
		}
	} while(0);

	syslog(LOG_INFO, "psv_startvm returning result = %d", result);
	return result;
}

/*
 * helper method for restarting or shutting down a vm
 */
int
psv_stop_or_reboot(struct process_state_vm *psv, bool reboot, int *exitcode)
{
	if (!psv) {
		errno = EINVAL;
		return -1;
	}

	bhyve_vmstate_t current_state = psv_getstate(psv);
	pid_t pid = 0;
	uint64_t target_state = 0;

	/* if we're not in the RUNNING state, we fail */
	if (RUNNING != current_state) {
		errno = EALREADY;
		return -1;
	}

	target_state = reboot ? PRESTOP_BEFORE_RESTART : STOPPING;
	/* STOPPING or PRESTOP_BEFORE_RESTART */
	if (sth_transitionto(psv->sth, target_state)) {
		if (psv_failurestate(psv))
			return -1;
	}

	pid = psv_getpid(psv);

	if (pid) {
		if (kill(pid, SIGTERM) < 0) {
			/* failed to send signal */
			if (psv_failurestate(psv))
				return -1;
		}

		/* regular waitpid happens via kqueue, this is put here
		   to safeguard against blocks in test cases */

		if (exitcode) {
			/* wait for process completion */
			if (waitpid(pid, exitcode, 0) < 0) {
				psv_failurestate(psv);
				
				return -1;
			} else {
				unsigned short xexitcode = 0;
				if (WIFEXITED(*exitcode)) {
					xexitcode = WEXITSTATUS(*exitcode);
					if (xexitcode >= 3) {
						psv_failurestate(psv);
					}
				}
			}
		}
		pthread_yield();
	}
	
	/* TODO missing property: timeout? */

	return 0;		
}

/*
 * stop the vm
 *
 * if exitcode is not NULL, the function will wait for the
 * vm to shut down via waitpid.
 */
int
psv_stopvm(struct process_state_vm *psv, int *exitcode)
{
	return psv_stop_or_reboot(psv, false, exitcode);
}

/*
 * reboot the vm
 *
 * if exitcode is not NULL, the function will wait for the
 * vm to shut down via waitpid.
 */
int
psv_rebootvm(struct process_state_vm *psv, int *exitcode)
{
	return psv_stop_or_reboot(psv, true, exitcode);
}

/*
 * sets the reboot manager on the process state manager
 */
struct process_state_vm *
psv_withrebootmgr(struct process_state_vm *psv, struct reboot_manager_object *rmo)
{
	if (!psv)
		return NULL;

	psv->rmo = rmo;
	
	return psv;
}

/*
 * initialize a new process state structure
 * with a given process definition object.
 * This can be used for testing purposes.
 *
 * returns NULL if unsuccessful.
 */
struct process_state_vm *
psv_withconfig(struct process_def_obj *pdo, const char *scriptpath)
{
	struct process_state_vm *psv = 0;

	psv = malloc(sizeof(struct process_state_vm));
	if (!psv)
		return NULL;

	/* no reboot manager by default */
	psv->rmo = NULL;

	/* no log redirector by default */
	psv->ldr = NULL;

	psv->processid = 0;

	if (pthread_mutex_init(&psv->mtx, NULL)) {
		free(psv);
		return NULL;
	}
	
	psv->sth = sth_new(process_transition_list, sizeof(process_transition_list)/sizeof(struct state_transition),
			   PS_STATE(INIT), psv);
	if (!psv->sth) {
		free(psv);
		return NULL;
	}
	psv->pdo = pdo;

	if (scriptpath) {
		size_t scriptlen = strlen(scriptpath);
		psv->scriptpath = malloc(scriptlen+1);
		strncpy(psv->scriptpath, scriptpath, scriptlen+1);
	} else
		psv->scriptpath = NULL;
	
	return psv;
}

/*
 * add a log redirector into the process state vm
 */
struct process_state_vm *
psv_with_logredirector(struct process_state_vm *psv,
		       struct log_director_redirector *ldr)
{
	if (!psv) {
		errno = EINVAL;
		return NULL;
	}
	psv->ldr = ldr;

	return psv;
}

/*
 * initialize a new process state structure
 */
struct process_state_vm *
psv_new(const struct bhyve_configuration *bc)
{
	struct process_def_obj *pdo;
	const char *configfile = 0;
	char *configpath = 0, *slash = 0;
	size_t configlen = 0;
	const char *scriptpath = 0;
	struct process_state_vm *result = 0;

	pdo = pdobj_fromconfig(bc);
	if (!pdo) {
		return NULL;
	}

	scriptpath = bc_get_scriptpath(bc);
	if (!*scriptpath) {
		configfile = bc_get_backingfile(bc);
		if (configfile) {
			configlen = strlen(configfile)+1;
			configpath = malloc(configlen);
			strncpy(configpath, configfile, configlen);
			slash = strrchr(configpath, '/');
			if (slash)
				*slash = 0;
			scriptpath = configpath;
		}		
	}
	
	result = psv_withconfig(pdo, scriptpath);

	free(configpath);

	return result;
}

/*
 * free a previously allocated vm process state
 */
void
psv_free(struct process_state_vm *psv)
{
	if (!psv)
		return;

	if (pthread_mutex_lock(&psv->mtx)) {
		errno = EDEADLK;
		return;
	}

	free(psv->scriptpath);
	pthread_mutex_unlock(&psv->mtx);
	pthread_mutex_destroy(&psv->mtx);

	sth_free(psv->sth);
	pdobj_free(psv->pdo);

	free(psv);
}

/*
 * get redirector for process state vm
 */
struct log_director_redirector *
psv_get_logredirector(struct process_state_vm *psv)
{
	if (!psv) {
		errno = EINVAL;
		return NULL;
	}
	
	return psv->ldr;
}

CREATE_GETTERFUNC_STR(process_state_vm, psv, scriptpath);
