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

#include <sys/event.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "bhyve_config.h"
#include "bhyve_config_object.h"
#include "bhyve_command.h"
#include "bhyve_director.h"
#include "bhyve_director_errors.h"
#include "bhyve_messagesub_object.h"
#include "config_generator_object.h"
#include "process_def.h"
#include "process_state.h"
#include "process_state_errors.h"
#include "reboot_manager_object.h"

#include "../libcommand/bhyve_command.h"
#include "../libcommand/vm_info.h"
#include "../liblogging/log_director.h"

/* private API method */
int psv_onexit(struct process_state_vm *psv, unsigned short exitcode);
int bd_requestreboot(struct bhyve_director *bd, struct process_state_vm *psv);

struct reboot_manager_funcs bhyve_director_rmo_funcs = {
	.request_reboot = (void*) bd_requestreboot
};

/*
 * registers startup attempt timestamps
 */
struct bhyve_start_timestamp {
	time_t timestamp;

	STAILQ_ENTRY(bhyve_start_timestamp) entries;
};

/*
 * combines process state and vm configuration
 */
struct bhyve_watched_vm {
	struct process_state_vm *state;
	const struct bhyve_configuration *config;
	struct log_director_redirector *ldr;
	
	SLIST_ENTRY(bhyve_watched_vm) entries;
	STAILQ_ENTRY(bhyve_watched_vm) reboot_entries;
	STAILQ_HEAD(,bhyve_start_timestamp) startups;
};

/*
 * The bhyve_director provides a listening method that parses
 * incoming nvlist command data and converts it into actions.
 */
struct bhyve_director {
	/* counts the number of messages received */
	uint64_t msgcount;

	/* kqueue handle */
	int kqueuefd;
	
	pthread_mutex_t mtx;
	pthread_cond_t cond_ready;
	pthread_cond_t reboot_wakeup;
	pthread_t kqueue_thread;
	pthread_t reboot_thread;
	int reboot_state;

	struct bhyve_configuration_store_obj *store_obj;
	struct reboot_manager_object rmo;
	struct log_director *ld;
	struct config_generator_object *cgo;

	SLIST_HEAD(, bhyve_watched_vm) statelist;
	STAILQ_HEAD(, bhyve_watched_vm) rebootlist;
};

void bwv_free(struct bhyve_watched_vm *bwv);

/*
 * allocate a new watched structure
 */
struct bhyve_watched_vm *
bwv_new(const struct bhyve_configuration *config,
	struct reboot_manager_object *rmo,
	struct log_director *ld)
{
	struct bhyve_watched_vm *bwv = 0;
	struct process_state_vm *psv = 0;

	if (!config) {
		errno = EINVAL;
		return NULL;
	}
	
	bwv = malloc(sizeof(struct bhyve_watched_vm));
	if (!bwv)
		return NULL;

	bzero(bwv, sizeof(struct bhyve_watched_vm));
	STAILQ_INIT(&bwv->startups);

	bwv->config = config;

	if (ld) {
		bwv->ldr = ld_register_redirect(ld, bc_get_name(config));
	}
	
	/* construct a new process_def with call to bhyve with parameters */
	psv = psv_new(bwv->config);
	psv = psv_withrebootmgr(psv, rmo);
	bwv->state = psv_with_logredirector(psv, bwv->ldr);
	
	if (!bwv->state) {
		/* failed to build process definition */
		bwv_free(bwv);
		return NULL;
	}

	return bwv;
}

/*
 * get timestamp of last boot
 *
 * returns 0 if vm was never booted and errno is set.
 */
time_t
bwv_get_lastboot(struct bhyve_watched_vm *bwv)
{
	struct bhyve_start_timestamp *bst = 0;

	if (!bwv) {
		errno = EINVAL;
		return 0;
	}

	if (STAILQ_EMPTY(&bwv->startups)) {
		errno = ECHILD;
		return 0;
	}

	bst = STAILQ_LAST(&bwv->startups, bhyve_start_timestamp, entries);
	return bst->timestamp;
}

/*
 * set and add the current timestamp into the watched vm structure
 */
int
bwv_timestamp(struct bhyve_watched_vm *bwv)
{
	if (!bwv)
		return -1;

	struct bhyve_start_timestamp *bst = malloc(sizeof(struct bhyve_start_timestamp));
	if (!bst)
		return -1;

	bst->timestamp = time(NULL);
	STAILQ_INSERT_TAIL(&bwv->startups, bst, entries);

	return 0;
}

/*
 * count the number of registered restarts since deadline
 *
 * returns 0 if an error occurred and errno is set.
 */
unsigned int
bwv_countrestarts_since(struct bhyve_watched_vm *bwv, time_t deadline)
{
	if (!bwv) {
		errno = EINVAL;
		return 0;
	}

	if (STAILQ_EMPTY(&bwv->startups))
		return 0;
	
	unsigned int counter = 0;
	struct bhyve_start_timestamp *bst = STAILQ_FIRST(&bwv->startups);

	while(bst->timestamp < deadline) {
		/* element before deadline, remove from list */
		STAILQ_REMOVE_HEAD(&bwv->startups, entries);
		if (STAILQ_EMPTY(&bwv->startups)) {
			bst = NULL;
			break;
		}
			
		bst = STAILQ_FIRST(&bwv->startups);
	}

	if (!bst) {
		/* nothing left on the list */
		return 0;
	}

	do {
		counter++;
	} while (NULL != (bst = STAILQ_NEXT(bst, entries)));

	return counter;		
}

/*
 * Generate a merged configuration, by using a generator if it is available.
 */
int
bwv_generate_config(struct bhyve_watched_vm *bwv,
		    struct config_generator_object *cgo)
{
	if (!bwv)
		return -1;

	if (!cgo) {
		return 0;
	}

	char generated_name[PATH_MAX] = {0};

	/* TODO use generated name from bhyve_config if set */

	snprintf(generated_name, PATH_MAX, "%s.generated",
		 bc_get_configfile(bwv->config));
	syslog(LOG_INFO, "Generating configuratino for vm \"%s\" in file " \
	       "\"%s\"", bc_get_name(bwv->config), generated_name);
	
	if (cgo->generate_config_file)
		if (!cgo->generate_config_file(bwv->config, generated_name)) {
			return psv_set_configfile(bwv->state, generated_name);
		}
	
	return -1;
}

/*
 * checks whether the vm is in failure state because of too many
 * consecutive quick restart attempts
 *
 * returns TRUE when in failure mode
 */
bool
bwv_is_countfail(struct bhyve_watched_vm *bwv)
{
	if (!bwv) {
		errno = EINVAL;
		return true;
	}

	errno = 0;
	
	time_t deadline = time(NULL) - bc_get_maxrestarttime(bwv->config);
	unsigned int count = bwv_countrestarts_since(bwv, deadline);

	/* if we failed to correctly count, we assume error */
	if (errno && !count)
		return true;

	return (count > bc_get_maxrestart(bwv->config));
}

/*
 * release a previously allocated watch structure
 */
void
bwv_free(struct bhyve_watched_vm *bwv)
{
	if (!bwv)
		return;

	struct bhyve_start_timestamp *bst = 0;
	
	while (!STAILQ_EMPTY(&bwv->startups)) {
		bst = STAILQ_FIRST(&bwv->startups);
		STAILQ_REMOVE_HEAD(&bwv->startups, entries);
		free(bst);
	}
	
	psv_free(bwv->state);
	free(bwv);
}

/*
 * request a reboot for a vm from its process_state_vm structure
 */
int
bd_requestreboot(struct bhyve_director *bd, struct process_state_vm *psv)
{
	if (!bd || !psv) {
		errno = EINVAL;
		return -1;
	}

	struct bhyve_watched_vm *bwv = 0;
	int result = 0;

	/* look up bwv object matching psv */
	if (pthread_mutex_lock(&bd->mtx)) {
		errno = EDEADLK;
		return -1;
	}

	SLIST_FOREACH(bwv, &bd->statelist, entries) {
		if (bwv->state == psv)
			break;
	}

	if (bwv->state != psv) {
		errno = ENOENT;
		pthread_mutex_unlock(&bd->mtx);
		return -1;
	}

	/* add to reboot list */
	STAILQ_INSERT_TAIL(&bd->rebootlist, bwv, reboot_entries);

	/* notify reboot thread */
	if (pthread_cond_signal(&bd->reboot_wakeup)) {
		/* TODO put another errno here */
		errno = EDEADLK;
		result = -1;
	}
	
	if (pthread_mutex_unlock(&bd->mtx)) {
		errno = EDEADLK;
		return -1;
	}
	
	
	return result;
}

/*
 * look up a vm by its name
 *
 * returns NULL if nothing matches.
 */
struct bhyve_watched_vm *
bd_getvmbyname(struct bhyve_director *bd, const char *name)
{
	if (!bd || !name) {
		errno = EINVAL;
		return NULL;
	}

	struct bhyve_watched_vm *bwv = 0;

	if (pthread_mutex_lock(&bd->mtx)) {
		errno = EDEADLK;
		return NULL;
	}

	SLIST_FOREACH(bwv, &bd->statelist, entries) {
		if (!strcmp(name, bc_get_name(bwv->config))) {
			if (pthread_mutex_unlock(&bd->mtx)) {
				err(EDEADLK, "failed to unlock mutex");
			}
			
			return bwv;
		}
	}

	pthread_mutex_unlock(&bd->mtx);
	
	return NULL;
}

/*
 * attempt stopping a vm
 */
int
bd_stopvm(struct bhyve_director *bd, const char *name)
{
	if (!bd || !name) {
		errno = EINVAL;
		return -1;
	}

	struct bhyve_watched_vm *bwv = bd_getvmbyname(bd, name);

	if (!bwv) {
		errno = ENOENT;
		return -1;
	}

	return psv_stopvm(bwv->state, NULL);
}

/*
 * attempt failure reset for a vm
 */
int
bd_resetfailvm(struct bhyve_director *bd, const char *name)
{
	if (!bd || !name) {
		errno = EINVAL;
		return -1;
	}
	
	struct bhyve_watched_vm *bwv = bd_getvmbyname(bd, name);

	if (!bwv) {
		errno = ENOENT;
		return -1;
	}
	
	if (!psv_is_failurestate(bwv->state))
		return BD_ERR_VMSTATENOFAIL;

	return psv_resetfailure(bwv->state);		
}

/*
 * attempt starting a vm
 */
int
bd_startvm(struct bhyve_director *bd, const char *name)
{
	if (!bd || !name) {
		errno = EINVAL;
		return -1;
	}

	struct bhyve_watched_vm *bwv = bd_getvmbyname(bd, name);
	struct kevent event = {0};
	pid_t pid = 0;
	int result = 0;
	int pidstat = 0;

	if (!bwv) {
		errno = ENOENT;
		return -1;
	}

	/* if we exceed restart count in max restart time */
	if (bwv_is_countfail(bwv)) {
		/* put vm in failed state */
		psv_failurestate(bwv->state);
		return BD_ERR_VMSTATEISFAIL;
	}

	/* if the state is already fail, bail */
	if (psv_is_failurestate(bwv->state))
		return BD_ERR_VMSTATEISFAIL;

	bwv_timestamp(bwv);

	/* attempt re-generating bhyve config, if we have a generator */
	if (bwv_generate_config(bwv, bd->cgo)) {
		/* generation failed */
		psv_failurestate(bwv->state);
		return BD_ERR_VMCONFGENFAIL;
	}

	/* launch vm */
	if (0 != (result = psv_startvm(bwv->state, &pid, bwv->ldr))) {
		if (result > 0) {
			if (PSV_ERR_MUTEXUNLOCKFAIL & result)
				return BD_ERR_UNLOCKFAILURE;

			if (PSV_ERR_TRANSITIONSFAIL & result)
				return BD_ERR_TRANSITCHFAIL;
		}
		
		/* failed */
		return BD_ERR_VMSTARTFAILED;
	}

	errno = 0;
	if (kill(pid, 0) < 0) {
		/* process already terminated, assume error because pid vanished */
		psv_onexit(bwv->state, 4);
	} else {
		syslog(LOG_INFO, "pid %d confirmed as valid", pid);
		/* check if child pid isn't already dead */
		if (waitpid(pid, &pidstat, WNOHANG | WEXITED | WSTOPPED) == pid) {
			syslog(LOG_INFO, "waitpid pidstat = %d", pidstat);
			/* tell it's already terminated */
			if (WIFEXITED(pidstat)) {
				syslog(LOG_ERR,
				       "psv_startvm called but immediate exit, exit = %d",
				       WEXITSTATUS(pidstat));
			} else if (WIFSIGNALED(pidstat)) {
				syslog(LOG_ERR,
				       "psv_startvm called but immedate sig, signal = %d",
				       WSTOPSIG(pidstat));
			}
			if (WIFEXITED(pidstat) || WIFSIGNALED(pidstat)) {
				if (psv_onexit(bwv->state, WEXITSTATUS(pidstat))) {
					/* TODO improve error logging */
					syslog(LOG_ERR, "psv_onexit failed");
					psv_failurestate(bwv->state);
				}
				syslog(LOG_ERR, "pid %d immediately died after start", pid);
				return BD_ERR_VMSTARTDIEDIM;
			}
		} else {
			syslog(LOG_INFO, "Registering for kqueue events for pid %d", pid);
			/* otherwise, we register the pid */
			EV_SET(&event, pid, EVFILT_PROC, EV_ADD | EV_ENABLE, NOTE_EXIT, 0, bwv);
			if (kevent(bd->kqueuefd, &event, 1, NULL, 0, NULL) < 0)
				return BD_ERR_KEVENTREGFAIL;
		}
	}

	syslog(LOG_INFO, "bd_startvm return 0");
	
	return 0;
}

/*
 * restart vm thread
 */
void *
bd_restart_thread(struct bhyve_director *bd)
{
	struct bhyve_watched_vm *bwv;
	bool interrupt = false;

	if (!bd) {
		errno = EINVAL;
		return NULL;
	}

	if (pthread_mutex_lock(&bd->mtx))
		return NULL;

	interrupt = bd->reboot_state;

	if (pthread_mutex_unlock(&bd->mtx))
		return NULL;

	while (!interrupt) {
		if (pthread_mutex_lock(&bd->mtx))
			break;

		pthread_cond_wait(&bd->reboot_wakeup, &bd->mtx);

		while (!STAILQ_EMPTY(&bd->rebootlist)) {
			bwv = STAILQ_FIRST(&bd->rebootlist);
			STAILQ_REMOVE_HEAD(&bd->rebootlist, reboot_entries);

			interrupt = bd->reboot_state;

			/* unlock before launching */
			if (pthread_mutex_unlock(&bd->mtx))
				break;

			if (interrupt)
				break;
			
			if (bd_startvm(bd, bc_get_name(bwv->config))) {
				syslog(LOG_ERR, "Failed to restart vm \"%s\"",
				       bc_get_name(bwv->config));
			}

			/* lock again after launching */
			if (pthread_mutex_lock(&bd->mtx))
				break;
		}

		if (pthread_mutex_unlock(&bd->mtx))
			break;
	}

	return NULL;
}

/*
 * kernel queue listener thread
 */
void *
bd_kqueue_thread(struct bhyve_director *bd)
{
	struct kevent event = {0};
	int result = 0;
	struct bhyve_watched_vm *bwv = 0;
	unsigned short exitcode = 0;

	if (!bd) {
		errno = EINVAL;
		return NULL;
	}

	/* register for shutdown events */
	EV_SET(&event, 0, EVFILT_USER, 0, 0, 0, 0);
	if (kevent(bd->kqueuefd, &event, 1, NULL, 0, 0) < 0) {
		result = -1;
	}

	if (pthread_mutex_lock(&bd->mtx))
		return NULL;

	/* signal that we're ready with setup */
	if (pthread_cond_signal(&bd->cond_ready))
		result = -1;
	
	pthread_mutex_unlock(&bd->mtx);

	do {
		result = kevent(bd->kqueuefd, NULL, 0, &event, 1, 0);
		if (result < 0)
			break;

		switch(event.filter) {
		case EVFILT_USER:
			/* shutdown signal */
			result = -1;
			break;
		case EVFILT_PROC:
			bwv = (void *) event.udata;
			if (WIFEXITED(event.data)) {
				exitcode = WEXITSTATUS(event.data);
				psv_onexit(bwv->state, exitcode);
			} else {
				if (WIFSIGNALED(event.data)) {
					syslog(LOG_ERR, "process %d received signal %ld",
					       psv_getpid(bwv->state),
					       WTERMSIG(event.data));
				}
				syslog(LOG_ERR, "vm \"%s\" shut down unexpectedly",
				       bc_get_name(bwv->config));
				/* process core dumped or other exit state */
				/* TODO move to error state */
				psv_failurestate(bwv->state);
			}
			
			break;
		}
	} while (result >= 0);
	
	return NULL;
}

/*
 * start the kqueue thread for the bhyve director
 *
 * returns 0 on success; errno will be set upon error.
 */
int
bd_thread_start(struct bhyve_director *bd)
{
	if (!bd) {
		errno = EINVAL;
		return -1;
	}

	int result = pthread_create(&bd->kqueue_thread, NULL, (void*) bd_kqueue_thread, bd);
	if (!result) {
		pthread_setname_np(bd->kqueue_thread, "bd kqueue");
	}

	return result;
}

/*
 * stop the kqueue thread for the bhyve director
 */
int
bd_thread_stop(struct bhyve_director *bd)
{
	struct kevent event = {0};

	EV_SET(&event, 0, EVFILT_USER, 0, NOTE_TRIGGER, 0, 0);
	if (kevent(bd->kqueuefd, &event, 1, NULL, 0, 0) < 0)
		return -1;

	/* wait for completion */
	if (pthread_join(bd->kqueue_thread, NULL))
		return -1;
	
	return 0;
}

/*
 * construct a new director
 */
struct bhyve_director *
bd_new(struct bhyve_configuration_store_obj *bcso,
	struct log_director *ld)
{
	struct bhyve_director *bd = 0;
	const struct bhyve_configuration *bc = 0;
	struct bhyve_configuration_iterator *bci = 0;
	struct bhyve_watched_vm *bwv = 0;

	if (!bcso) {
		errno = EINVAL;
		return NULL;
	}

	bd = malloc(sizeof(struct bhyve_director));
	if (!bcso)
		return NULL;

	bzero(bd, sizeof(struct bhyve_director));
	bd->rmo.ctx = bd;
	bd->rmo.funcs = &bhyve_director_rmo_funcs;
		
	if ((bd->kqueuefd = kqueue()) < 0) {
		free(bd);
		return NULL;
	}		

	if (pthread_mutex_init(&bd->mtx, NULL)) {
		free(bd);
		return NULL;
	}

	if (pthread_cond_init(&bd->cond_ready, NULL)) {
		pthread_mutex_destroy(&bd->mtx);
		free(bd);
		return NULL;
	}

	if (pthread_cond_init(&bd->reboot_wakeup, NULL)) {
		pthread_cond_destroy(&bd->cond_ready);
		pthread_mutex_destroy(&bd->mtx);
		free(bd);
		return NULL;
	}

	if (bd_thread_start(bd)) {
		pthread_cond_destroy(&bd->reboot_wakeup);
		pthread_cond_destroy(&bd->cond_ready);
		pthread_mutex_destroy(&bd->mtx);
		free(bd);
		return NULL;
	}

	if (pthread_create(&bd->reboot_thread, NULL, (void*) bd_restart_thread, bd)) {
		/* failed to start thread */
		bd_thread_stop(bd);
		pthread_cond_destroy(&bd->reboot_wakeup);
		pthread_cond_destroy(&bd->cond_ready);
		pthread_mutex_destroy(&bd->mtx);
		free(bd);
		return NULL;
	} else {
		pthread_setname_np(bd->reboot_thread, "bd reboot");
	}

	bd->msgcount = 0;
	bd->store_obj = bcso;
	bd->ld = ld;

	SLIST_INIT(&bd->statelist);
	STAILQ_INIT(&bd->rebootlist);

	/* construct state list from store configurations */
	bci = bcso->funcs->getiterator(bcso->ctx);
	if (!bci) {
		bd_free(bd);
		return NULL;
	}

	while (bci_next(bci)) {
		bc = bci_getconfig(bci);
		bwv = bwv_new(bc, &bd->rmo, ld);

		if (!bwv) {
			bd_free(bd);
			return NULL;
		}

		/* insert into listing */
		SLIST_INSERT_HEAD(&bd->statelist, bwv, entries);
	}

	return bd;
}

/*
 * get message count
 *
 * returns zero and errno set on error.
 */
uint64_t
bd_getmsgcount(struct bhyve_director *bd)
{
	if (!bd) {
		errno = EINVAL;
		return 0;
	}

	uint64_t count = 0;

	if (pthread_mutex_lock(&bd->mtx)) {
		return 0;
	}

	count = bd->msgcount;
	
	if (pthread_mutex_unlock(&bd->mtx))
		return 0;
	
	return count;
}

/*
 * send bhyve director info back to client
 */
int
bd_reply_info(struct bhyve_director *bd, struct bhyve_messagesub_replymgr *bmr)
{
	struct bhyve_vm_manager_info *bvmmi = bd_getinfo(bd);
	void *buffer = 0;
	size_t bufferlen = 0;
	int result = 0;
	
	if (!bvmmi)
		return -1;

	do {
		if (bvmmi_encodebinary(bvmmi, &buffer, &bufferlen)) {
			result = -1;
			break;
		}

		/* send binary blob */
		result = bmr->reply(bmr->ctx, buffer, bufferlen);

		free(buffer);
	} while(0);

	bvmmi_free(bvmmi);

	return result;
}

/*
 * called when a command and data was received
 */
int
bd_recv_ondata(void *ctx, uid_t uid, pid_t pid, const char *cmd,
	       const char *data, size_t datalen, struct bhyve_messagesub_replymgr *bmr)
{
	struct bhyve_director *bd = ctx;
	struct bhyve_usercommand bcmd = {0};
	int result = 0;
	
	if (!bd)
		return -1;

	syslog(LOG_INFO, "bd_recv_ondata start");
	
	if (pthread_mutex_lock(&bd->mtx)) {
		syslog(LOG_ERR, "Failed to lock mutex");
		return -1;
	}
	bd->msgcount++;
	if (pthread_mutex_unlock(&bd->mtx)) {
		syslog(LOG_ERR, "Failed to unlock mutex");
		return -1;
	}

	/* TODO implement auth checks before running functions */

	if (strcmp("BHYV", cmd)) {
		/* not a bhyve command - skip */
		syslog(LOG_ERR, "not a BHYV command");
		return 0;
	}

	/* zero out memory so we can run free on all vars later */
	bzero(&bcmd, sizeof(struct bhyve_usercommand));

	if (bcmd_parse_nvlistcmd(data, datalen, &bcmd)) {
		syslog(LOG_ERR, "bcmd_parse_nvlistcmd failed");
		result = -1;
	}

	/* TODO implement with separate command handler */
	if (!result) {
		if (!strcmp(bcmd.cmd, "startvm")) {
			syslog(LOG_INFO, "calling bd_startvm");
			result = bd_startvm(bd, bcmd.vmname);
			syslog(LOG_INFO, "bd_startm result = %d", result);
		}
		if (!strcmp(bcmd.cmd, "stopvm")) {
			syslog(LOG_INFO, "calling bd_stopvm");
			result = bd_stopvm(bd, bcmd.vmname);
			syslog(LOG_INFO, "bd_stopvm result = %d", result);
		}
		if (!strcmp(bcmd.cmd, "status") && bmr) {
			/* bmr reply manager is given to info func for
			 * more in-depth reply than just error code
			 */
			result = bd_reply_info(bd, bmr);
		}
		if (!strcmp(bcmd.cmd, "resetfail")) {
			syslog(LOG_INFO, "calling bd_resetfailvm");
			result = bd_resetfailvm(bd, bcmd.vmname);
			syslog(LOG_INFO, "bd_resetfailvm result = %d", result);
		}
	}
	
	/* free memory again */
	bcmd_freestatic(&bcmd);

	syslog(LOG_INFO, "bd_recv_ondata stop");
	
	return result;
}

/*
 * subscribe director at message provider
 */
int
bd_subscribe_commands(struct bhyve_director *bd, struct bhyve_messagesub_obj *bmo)
{
	if (!bd || !bmo) {
		errno = EINVAL;
		return -1;
	}

	return bmo->subscribe_ondata(bmo->obj,
				     bd,
				     bd_recv_ondata);
}

/*
 * count the number of vms maintained by the director
 *
 * returns zero and errno set on error.
 */
uint64_t
bd_countvms(struct bhyve_director *bd)
{
	uint64_t count = 0;
	struct bhyve_watched_vm *bwv = 0;

	if (!bd) {
		errno = EINVAL;
		return 0;
	}	

	if (pthread_mutex_lock(&bd->mtx))
		return 0;

	SLIST_FOREACH(bwv, &bd->statelist, entries) {
		count++;
	}
	
	if (pthread_mutex_unlock(&bd->mtx))
		return 0;

	return count;
}

/*
 * get some system information from bhyve_director
 *
 * returns a newly allocated bhyve_vm_manager_info structure
 * the structure needs to be released with bvmmi_free
 *
 * returns NULL on failure with errno set.
 */
struct bhyve_vm_manager_info *
bd_getinfo(struct bhyve_director *bd)
{
	if (!bd) {
		errno = EINVAL;
		return NULL;
	}

	struct bhyve_vm_manager_info *bvmmi = 0;
	struct bhyve_watched_vm *bvw = 0;
	ssize_t vm_count = bd_countvms(bd);
	ssize_t counter = 0;
	struct bhyve_vm_info **ptrarray = 0;

	/* count number of vms first, if that fails, we bail */
	if (!vm_count && errno)
		return NULL;

	/* allocate pointer array for state infos */
	if (!(ptrarray = malloc(sizeof(struct bhyve_vm_info *) * vm_count)))
		return NULL;

	if (pthread_mutex_lock(&bd->mtx))
		return NULL;

	SLIST_FOREACH(bvw, &bd->statelist, entries) {
		/* construct a new bvmi for each configuration */
		ptrarray[counter] = bvmi_new(bc_get_name(bvw->config),
					     bc_get_os(bvw->config),
					     bc_get_osversion(bvw->config),
					     bc_get_owner(bvw->config),
					     bc_get_group(bvw->config),
					     bc_get_description(bvw->config),
					     psv_getstate(bvw->state),
					     psv_getpid(bvw->state),
					     bwv_get_lastboot(bvw));
		counter++;
	}

	if (pthread_mutex_unlock(&bd->mtx))
		return NULL;

	bvmmi = bvmmi_new(ptrarray, counter, bd_getmsgcount(bd));
	
	return bvmmi;
}

/*
 * set config generator object
 */
int
bd_set_cgo(struct bhyve_director *bd,
	   struct config_generator_object *cgo)
{
	if (!bd) {
		errno = EINVAL;
		return -1;
	}

	bd->cgo = cgo;

	return 0;
}

/*
 * release a previously allocated director
 */
void
bd_free(struct bhyve_director *bd)
{
	if (!bd)
		return;

	struct bhyve_watched_vm *vm = 0;

	if (bd_thread_stop(bd)) {
		/* TODO should probably use err instead */
		return;
	}

	if (pthread_mutex_lock(&bd->mtx)) {
		errno = EDEADLK;
		return;

	}

	/* shut down reboot thread */
	bd->reboot_state = 1;
	if (pthread_cond_signal(&bd->reboot_wakeup)) {
		/* kill thread instead */
	} else {
		pthread_mutex_unlock(&bd->mtx);

		pthread_join(bd->reboot_thread, NULL);

		pthread_mutex_lock(&bd->mtx);
	}

	while (!STAILQ_EMPTY(&bd->rebootlist)) {
		STAILQ_REMOVE_HEAD(&bd->rebootlist, reboot_entries);
	}
	
	while (!SLIST_EMPTY(&bd->statelist)) {
		vm = SLIST_FIRST(&bd->statelist);
		SLIST_REMOVE_HEAD(&bd->statelist, entries);
		bwv_free(vm);
	}

	pthread_mutex_unlock(&bd->mtx);
	pthread_cond_destroy(&bd->cond_ready);
	pthread_cond_destroy(&bd->reboot_wakeup);
	pthread_mutex_destroy(&bd->mtx);

	close(bd->kqueuefd);
	bd->kqueuefd = 0;

	free(bd);
}
