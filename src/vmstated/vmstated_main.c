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

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <syslog.h>
#include <unistd.h>

#include "../liblogging/log_director.h"

#include "../libprocwatch/bhyve_config.h"
#include "../libprocwatch/bhyve_config_object.h"
#include "../libprocwatch/bhyve_director.h"

#include "../libsocket/socket_handle.h"

#include "config_generator.h"
#include "subscriber.h"
#include "vmstated_config.h"

pthread_mutex_t vmstated_sigmtx;
pthread_cond_t  vmstated_termcond;
sig_t           vmstated_defaultsig = 0;
sig_t           vmstated_defaultsig_int = 0;

/*
 * runtime options relevant for vmstated
 */
struct vmstated_opts {
	bool foreground;
	int verbose;
	char configdir_path[PATH_MAX];
	char pidfile_path[PATH_MAX];
	char socket_path[PATH_MAX];
	char log_path[PATH_MAX];
};

/*
 * listener method for receiving data from socket
 */
int
vmstated_recv_data(void *ctx, uid_t uid, pid_t pid, const char *cmd, const char *data, size_t datalen)
{
	return 0;
}

/*
 * launch vmstated
 */
int
vmstated_launch(struct vmstated_opts *opts,
		struct bhyve_configuration_store *bcs)
{
	struct log_director *ld = 0;
	struct bhyve_configuration_store_obj *bcso = 0;
	struct bhyve_director *bd = 0;
	struct socket_handle *sh = 0;
	struct vmstated_message_subscriber *vmsms = 0;

	if (!(ld = ld_new(opts->verbose, opts->log_path))) {
	}

	if (!(bcso = bcsobj_frombcs(bcs))) {
		ld_free(ld);
		bcs_free(bcs);
		err(errno, "Failed to construct configuration store object");
	}

	if (!(bd = bd_new(bcso, ld))) {
		ld_free(ld);
		bcs_free(bcs);
		err(errno, "Failed to construct bhyve director");
	}

	/* assign a new cgo */
	if (bd_set_cgo(bd, &vmstated_cgo)) {
		bd_free(bd);
		ld_free(ld);
		bcs_free(bcs);
		err(errno, "Failed to prepare configuration generator");
	}

	do {
		if (!(sh = sh_new(opts->socket_path, 0))) {
			ld_free(ld);
			sh_free(sh);
			bd_free(bd);
			bcs_free(bcs);
			err(errno, "Failed to set up socket \"%s\"", opts->socket_path);
		}

		if (!(vmsms = vmsms_new(sh))) {
			ld_free(ld);
			sh_free(sh);
			bd_free(bd);
			bcs_free(bcs);
			err(errno, "Failed to set up message helper");
		}

		if (bd_subscribe_commands(bd, vmsms_getmessagesub_obj(vmsms))) {
			sh_free(sh);
			ld_free(ld);

			/* failed to subscribe to data recv */
			err(errno, "Failed to subscribe bhyve director to message reception");
			
			break;
		}
		
		/* start listener */
		if (sh_start(sh)) {
			/* failed to start socket */
			ld_free(ld);
			sh_free(sh);
			break;
		}

		if (pthread_mutex_lock(&vmstated_sigmtx))
			err(errno, "Failed to lock signal mutex");

		if (pthread_cond_wait(&vmstated_termcond, &vmstated_sigmtx))
			err(errno, "Failed to wait for condition var");

		if (pthread_mutex_unlock(&vmstated_sigmtx))
			err(errno, "Failed to unlock signal mutex");

		/* stop listener */
		sh_stop(sh);

	} while(0);

	/* release helper */
	vmsms_free(vmsms);
	/* release socket */
	sh_free(sh);
	/* release resources */
	bd_free(bd);
	ld_free(ld);
	
	return 0;
}

/*
 * called when a signal was received
 */
void
vmstated_sigrecv(int sig)
{
	switch(sig) {
	case SIGTERM:
	case SIGINT:
		if (pthread_mutex_lock(&vmstated_sigmtx))
			err(errno, "Failed to lock signal mutex");
		if (pthread_cond_signal(&vmstated_termcond))
			err(errno, "Failed to signal condition var");
		if (pthread_mutex_unlock(&vmstated_sigmtx))
			err(errno, "Failed to unlock signal mutex");
		break;
	}
}

/*
 * sets vmstated up for signal handling
 */
void
setup_sighandler()
{
	if (pthread_mutex_init(&vmstated_sigmtx, NULL))
		err(errno, "Failed to initialize signal mutex");
	if (pthread_cond_init(&vmstated_termcond, NULL)) {
		pthread_mutex_destroy(&vmstated_sigmtx);
		err(errno, "Failed to initialize termination condition var");
	}
	/* redirect TERM signal handler */
	vmstated_defaultsig = signal(SIGTERM, vmstated_sigrecv);
	/* redirect INT signal handler */
	vmstated_defaultsig_int = signal(SIGINT, vmstated_sigrecv);
}

/*
 * cleans up signal handler
 */
void
teardown_sighandler()
{
	if (pthread_cond_destroy(&vmstated_termcond))
		err(errno, "Failed to clean up termination condition var");
	if (pthread_mutex_destroy(&vmstated_sigmtx))
		err(errno, "Failed to clean up signal mutex");
	signal(SIGTERM, vmstated_defaultsig);
	signal(SIGINT, vmstated_defaultsig_int);
}

/*
 * write pid file
 */
int
write_pidfile(const char *pidfile)
{
	int pidfd = 0;
	char pidstring[128] = {0};
	size_t pidlen = 0;
	int result = 0;

	if ((pidfd = open(pidfile, O_RDWR | O_CREAT | O_TRUNC)) < 0) {
		syslog(LOG_ERR, "Failed to open or create pid file \"%s\"", pidfile);
		return -1;
	}

	if (fchmod(pidfd, S_IRUSR | S_IWUSR | S_IRGRP) < 0)
		result = -1;

	snprintf(pidstring, 128, "%d\n", getpid());
	pidlen = strlen(pidstring);

	if (write(pidfd, pidstring, pidlen) < 0)
		result = -1;

	close(pidfd);

	return result;
}

/*
 * print usage and exit
 */
int
print_usage()
{
	printf("Usage: vmstated [-h] [-c configdir] [-f] [-p pidfile] [-v]\n\n");
	printf("\t-h\t\tPrint this help screen\n");
	printf("\t-c configdir\tLoad different configuration directory\n");
	printf("\t-f\t\tStay in foreground, do not daemonize\n");
	printf("\t-p\t\tWrite pidfile to different path\n");
	printf("\t-v\t\tBe more verbose\n");
	exit(0);
}

/*
 * handle program options
 */
int
handle_opts(int argc, char **argv, struct vmstated_opts *default_opts)
{
	int c;

	while ((c = getopt(argc, argv, "fhc:p:s:v")) != -1) {
		switch (c) {
		case 'f':
			default_opts->foreground = true;
			break;
		case 'p':
			strncpy(default_opts->pidfile_path, optarg, PATH_MAX);
			break;
		case 's':
			strncpy(default_opts->socket_path, optarg, PATH_MAX);
			break;
		case 'v':
			default_opts->verbose++;
			break;
		case 'h':
			print_usage();
			break;
		}
	}
	
	return 0;
}

/*
 * check whether program is already running and terminate
 * if it does
 */
int
check_already_running(struct vmstated_opts *default_opts)
{
	int pidfd = 0;
	char pid_number[10] = {0};
	pid_t old_pid = 0;

	if ((pidfd = open(default_opts->pidfile_path, O_RDONLY)) < 0) {
		/* no pid file */
		return 0;
	}

	if ((read(pidfd, pid_number, 10)) >= 0) {
		old_pid = atoll(pid_number);

		if (!errno) {
			/* we got a pid */
			if (!kill(old_pid, 0)) {
				fprintf(stderr,
					"vmstated already running (pid=%d).\n",
					old_pid);
				exit(1);
			}
		}
	}

	close(pidfd);
	return 0;
}

/*
 * run actual program
 */
int
run_program(struct vmstated_opts *default_opts,
	    struct bhyve_configuration_store *bcs)
{
	int result = 0;

	syslog(LOG_INFO, "vmstated starting");

	setup_sighandler();

	syslog(LOG_INFO, "Creating pid file \"%s\"",
	       default_opts->pidfile_path);
	if (write_pidfile(default_opts->pidfile_path)) {
		syslog(LOG_ERR, "Could not write pid file \"%s\"",
		       default_opts->pidfile_path);
		err(errno, "Failed to write pid file \"%s\"",
		    default_opts->pidfile_path);
	}
	
	result = vmstated_launch(default_opts, bcs);
	
	syslog(LOG_INFO, "vmstated shutting down");

	/* release store */
	bcs_free(bcs);
	
	teardown_sighandler();
	/* clear pid file */
	unlink(default_opts->pidfile_path);

	closelog();

	return result;
}

/*
 * program entry point
 */
int
main(int argc, char **argv)
{
	/* set sane defaults for the beginning */
	struct vmstated_opts default_opts = {0};
	struct bhyve_configuration_store *bcs = 0;
	int result = 0;
	
	openlog("vmstated", LOG_PID, LOG_DAEMON);

	strncpy(default_opts.configdir_path, "/usr/local/etc/vmstated", PATH_MAX);
	strncpy(default_opts.pidfile_path, DEFAULTPATH_PIDFILE, PATH_MAX);
	strncpy(default_opts.socket_path, DEFAULTPATH_SOCKET, PATH_MAX);
	strncpy(default_opts.log_path, DEFAULTPATH_LOGDIR, PATH_MAX);

	if (handle_opts(argc, argv, &default_opts))
		err(errno, "Failed to handle program arguments");

	if (check_already_running(&default_opts)) {
		err(errno, "Failed to check whether program is already running");
	}

	if (!(bcs = bcs_new(default_opts.configdir_path))) {
		err(ENOMEM, "Failed to instantiate configuration store");
	}

	syslog(LOG_INFO, "Loading config data");

	/* walk configuration directory */
	if (bcs_walkdir(bcs)) {
		bcs_free(bcs);
		err(errno, "Failed to walk configuration directory");
	}
	
	if (default_opts.foreground)
		return run_program(&default_opts, bcs);
	else {
		if (0 == fork()) {
			if (0 == fork()) {
				return run_program(&default_opts, bcs);
			}
			return 0;
			closelog();
		}
		closelog();
	}
	return 0;
}
