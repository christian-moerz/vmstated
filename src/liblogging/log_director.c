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
#include <sys/stat.h>
#include <sys/queue.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "log_director.h"

int
ld_register_pipe(struct log_director *ld, struct log_director_redirector *ldr,
		 struct log_director_redirector_client *ldrd);

/*
 * a client connecting to a redirector
 */
struct log_director_redirector_client {
	int pipefd[2];

	bool other_end_closed;
	bool this_accepted;
	bool closed;

	struct log_director_redirector *ldr;

	LIST_ENTRY(log_director_redirector_client) entries;
};

/*
 * a user requiring pipe redirection to log
 */
struct log_director_redirector {
	char *logname;

	int logfilefd;
	
	struct log_director *ld;

	pthread_mutex_t mtx;

	LIST_ENTRY(log_director_redirector) entries;
	LIST_HEAD(, log_director_redirector_client) clients;
};

/*
 * maintains logging facilities
 */
struct log_director {
	int log_level;
	char *log_directory;

	int kqueuefd;

	pthread_mutex_t mtx;	
	pthread_t event_thread;
	pthread_cond_t event_ready;
	bool thread_started;

	LIST_HEAD(,log_director_redirector) redirects;
};

/*
 * get sending end of redirector
 */
int
ldrd_get_senderpipe(struct log_director_redirector_client *ldrd)
{
	if (ldrd) {
		errno = EINVAL;
		return -1;
	}

	return ldrd->pipefd[1];
}

/*
 * called when data is ready to be raed from pipe and posted to log
 */
int
ldr_recv_ondata(struct log_director_redirector_client *ldrd, int64_t bytes_ready)
{
	/* TODO implement */
	char *buffer = 0;
	long done_bytes = 0, total = bytes_ready, offset = 0;

	if (pthread_mutex_lock(&ldrd->ldr->mtx))
		return -1;
	
	syslog(LOG_INFO, "ldr_recv_ondata bytes_ready = %ld", bytes_ready);
	
	if (!(buffer = malloc(bytes_ready))) {
		/* TODO log error */
		pthread_mutex_unlock(&ldrd->ldr->mtx);
		return -1;
	}

	/* read into buffer */
	while (total && ((done_bytes = read(ldrd->pipefd[0], buffer + offset, total)) >= 0)) {
		total -= done_bytes;
		offset += done_bytes;
	}

	total = bytes_ready;
	offset = 0;
	while (total && ((done_bytes = write(ldrd->ldr->logfilefd, buffer + offset, total)) >= 0)) {
		total -= done_bytes;
		offset += done_bytes;
	}

	pthread_mutex_unlock(&ldrd->ldr->mtx);
	
	free(buffer);

	return 0;
}

/*
 * kqueue thread handling kevent events
 */
void *
ld_kqueue_thread(void *ctx)
{
	struct log_director *ld = ctx;
	struct log_director_redirector_client *ldrd = 0;
	bool shutdown = false;
	struct kevent event = {0};
	
	if (!ld)
		return NULL;

	if (pthread_mutex_lock(&ld->mtx)) {
		return NULL;
	}

	ld->thread_started = true;
	pthread_cond_signal(&ld->event_ready);

	pthread_mutex_unlock(&ld->mtx);

	syslog(LOG_INFO, "ld_kqueue_thread started");
	
	while (!shutdown) {
		if (kevent(ld->kqueuefd, NULL, 0, &event, 1, NULL) < 0)
			break;

		syslog(LOG_INFO, "ld_kqueue thread woke up");

		switch(event.filter) {
		case EVFILT_READ:
			/* handle data to read from a pipe */
			ldrd = event.udata;
			syslog(LOG_INFO, "ld_kqueue thread woke up with EVFILT_READ");
			
			if (EV_EOF == event.flags) {
				/* pipe was closed on the other end */
				syslog(LOG_INFO, "Process closed pipe end");
				ldrd_freeclient(ldrd);
			} else {
				/* data to receive */
				if (ldr_recv_ondata(ldrd, event.data)) {
					/* TODO log error */
					syslog(LOG_ERR, "Failed to process inbound pipe data");
				}
			}
			break;
		case EVFILT_USER:
			syslog(LOG_INFO, "shutdown event");
			shutdown = true;
			break;
		default:
			syslog(LOG_INFO, "kevent no match");
			break;
		}

	}	
	
	return NULL;
}

/*
 * redirect stdout to pipe end
 *
 * returns 0 on success
 */
int
ldrd_redirect_stdout(struct log_director_redirector_client *ldr)
{
	if (!ldr) {
		errno = EINVAL;
		return -1;
	}
	
	if (close(1) < 0)
		return -1;

	if (!ldr->other_end_closed) {
		if (close(ldr->pipefd[0]) < 0)
			return -1;
		ldr->other_end_closed = true;
	}
	return dup2(ldr->pipefd[1], 1);
}

/*
 * redirect stderr to pipe end
 */
int
ldrd_redirect_stderr(struct log_director_redirector_client *ldr)
{
	if (!ldr) {
		errno = EINVAL;
		return -1;
	}
	
	if (close(2) < 0)
		return -1;

	if (!ldr->other_end_closed) {
		if (close(ldr->pipefd[0]) < 0)
			return -1;
		ldr->other_end_closed = true;
	}
	
	return dup2(ldr->pipefd[1], 2);
}

/*
 * readies the receiving end for pipe input
 */
int
ldrd_accept_redirect(struct log_director_redirector_client *ldrd)
{
	if (!ldrd) {
		errno = EINVAL;
		return -1;
	}
	if (!ldrd->this_accepted) {
		ldrd->this_accepted = true;
		return close(ldrd->pipefd[1]);
	}

	return 0;
}

/*
 * release resources of a client
 */
void
ldrd_freeclient(struct log_director_redirector_client *ldrd)
{
	if (!ldrd) {
		errno = EINVAL;
		return;
	}

	if (!ldrd->this_accepted)
		close(ldrd->pipefd[1]);
	if (!ldrd->closed)
		close(ldrd->pipefd[0]);

	LIST_REMOVE(ldrd, entries);
	free(ldrd);
}

/*
 * prepare for a new client
 */
struct log_director_redirector_client *
ldr_newclient(struct log_director_redirector *ldr)
{
	struct log_director_redirector_client *ldrd = 0;

	if (!(ldrd = malloc(sizeof(struct log_director_redirector_client))))
		return NULL;

	ldrd->other_end_closed = false;
	ldrd->this_accepted = false;
	ldrd->closed = false;
	if (pipe(ldrd->pipefd) < 0) {
		free(ldrd);
		return NULL;
	}

	/* register for kqueue events */
	if (ld_register_pipe(ldr->ld, ldr, ldrd)) {
		close(ldrd->pipefd[1]);
		close(ldrd->pipefd[0]);
		free(ldrd);
		return NULL;
	}

	ldrd->ldr = ldr;

	if (pthread_mutex_lock(&ldr->mtx)) {
		ldrd_freeclient(ldrd);
		return NULL;
	}
		
	LIST_INSERT_HEAD(&ldr->clients, ldrd, entries);

	pthread_mutex_unlock(&ldr->mtx);

	return ldrd;
}

/*
 * construct a new redirector structure
 */
struct log_director_redirector *
ldr_new(struct log_director *ld, const char *log_directory, const char *logname)
{
	if (!logname) {
		errno = EINVAL;
		return NULL;
	}

	struct log_director_redirector *ldr = 0;
	char logfile_name[PATH_MAX] = {0};

	if (!(ldr = malloc(sizeof(struct log_director_redirector))))
		return NULL;

	if (pthread_mutex_init(&ldr->mtx, NULL))
		return NULL;

	if (!(ldr->logname = strdup(logname))) {
		free(ldr);
		return NULL;
	}

	ldr->ld = ld;

	LIST_INIT(&ldr->clients);

	snprintf(logfile_name, PATH_MAX, "%s/%s.log", log_directory, logname);
	if ((ldr->logfilefd = open(logfile_name, O_RDWR | O_CREAT | O_TRUNC)) < 0) {
		free(ldr->logname);
		free(ldr);
		return NULL;
	}

	/* fix permissions */
	if (fchmod(ldr->logfilefd, S_IRUSR | S_IWUSR) < 0) {
		close(ldr->logfilefd);
		free(ldr->logname);
		free(ldr);
		return NULL;
	}

	return ldr;	
}

/*
 * free resources of a log redirector
 */
void
ldr_free(struct log_director_redirector *ldr)
{
	if (!ldr)
		return;

	struct log_director_redirector_client *ldrd = 0;

	pthread_mutex_lock(&ldr->mtx);
	
	while (!LIST_EMPTY(&ldr->clients)) {
		ldrd = LIST_FIRST(&ldr->clients);
		LIST_REMOVE(ldrd, entries);
		ldrd_freeclient(ldrd);
	}
	pthread_mutex_unlock(&ldr->mtx);
	
	free(ldr->logname);
	pthread_mutex_destroy(&ldr->mtx);
	
	free(ldr);
}

/*
 * start kevent thread
 */
int
ld_thread_start(struct log_director *ld)
{
	int result = 0;

	if (!ld) {
		errno = EINVAL;
		return -1;
	}

	if (!(result = pthread_create(&ld->event_thread, NULL, ld_kqueue_thread, ld))) {
		pthread_setname_np(ld->event_thread, "ld thread");
	}

	return result;
}

/*
 * stop kevent thread
 */
int
ld_thread_stop(struct log_director *ld)
{
	struct kevent event = {0};

	EV_SET(&event, 0, EVFILT_USER, 0, NOTE_TRIGGER, 0, &ld);

	if (kevent(ld->kqueuefd, &event, 1, NULL, 0, NULL) < 0) {
		return -1;
	}

	/* wait for thread to complete */
	pthread_join(ld->event_thread, NULL);

	return 0;
}

/*
 * register a pipe for kevent redirection
 */
int
ld_register_pipe(struct log_director *ld, struct log_director_redirector *ldr,
		 struct log_director_redirector_client *ldrd)
{
	struct kevent event = {0};

	/* register pipefd[0] with kevent queue */
	syslog(LOG_INFO, "Registering kevent for pipefd = %d", ldrd->pipefd[0]);
	EV_SET(&event, ldrd->pipefd[0], EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, ldrd);
	if (kevent(ld->kqueuefd, &event, 1, NULL, 0, NULL) < 0) {
		return -1;
	}

	return 0;	
}

/*
 * register a new redirector
 */
struct log_director_redirector *
ld_register_redirect(struct log_director *ld, const char *logname)
{
	if (!ld || !logname) {
		errno = EINVAL;
		return NULL;
	}

	/* TODO first check if name isn't already taken */
	
	struct log_director_redirector *ldr = 0;

	/* construct new redirector */
	if (!(ldr = ldr_new(ld, ld->log_directory, logname))) {
		return NULL;
	}

	if (pthread_mutex_lock(&ld->mtx)) {
		ldr_free(ldr);
		return NULL;
	}

	/* add into list */
	LIST_INSERT_HEAD(&ld->redirects, ldr, entries);

	pthread_mutex_unlock(&ld->mtx);

	return ldr;
}

/*
 * creates a new log director
 */
struct log_director *
ld_new(int verbosity, char *log_directory)
{
	if (!log_directory) {
		errno = EINVAL;
		return NULL;
	}

	struct log_director *ld = malloc(sizeof(struct log_director));

	if (!ld)
		return NULL;

	ld->log_level = LOG_ERR;
	if (verbosity >= 1)
		ld->log_level = LOG_WARNING;
	if (verbosity >= 2)
		ld->log_level = LOG_NOTICE;
	if (verbosity >= 3)
		ld->log_level = LOG_INFO;
	if (verbosity >= 4)
		ld->log_level = LOG_DEBUG;

	ld->thread_started = false;

	ld->log_directory = strdup(log_directory);
	if (!ld->log_directory) {
		free(ld);
		return NULL;
	}

	if ((ld->kqueuefd = kqueue()) < 0) {
		free(ld->log_directory);
		free(ld);
		return NULL;
	}

	if (pthread_mutex_init(&ld->mtx, NULL)) {
		close(ld->kqueuefd);
		free(ld->log_directory);
		free(ld);
		return NULL;
	}

	if (pthread_cond_init(&ld->event_ready, NULL)) {
		pthread_mutex_destroy(&ld->mtx);
		close(ld->kqueuefd);
		free(ld->log_directory);
		free(ld);
		return NULL;
	}

	LIST_INIT(&ld->redirects);

	if (ld_thread_start(ld)) {
		ld_free(ld);
		return NULL;
	}		
	
	return ld;
}

/*
 * free the resource previously allocated for the log director
 */
void
ld_free(struct log_director *ld)
{
	if (!ld)
		return;

	struct log_director_redirector *ldr = 0;

	ld_thread_stop(ld);
	
	pthread_mutex_lock(&ld->mtx);

	while (!LIST_EMPTY(&ld->redirects)) {
		ldr = LIST_FIRST(&ld->redirects);
		LIST_REMOVE(ldr, entries);
		ldr_free(ldr);
	}
	
	pthread_mutex_unlock(&ld->mtx);

	pthread_cond_destroy(&ld->event_ready);
	pthread_mutex_destroy(&ld->mtx);
	
	close(ld->kqueuefd);
	free(ld->log_directory);
	free(ld);
}
