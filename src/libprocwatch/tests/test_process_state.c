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
#include <sys/types.h>

#include <atf-c.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "../../liblogging/log_director.h"
#include "../bhyve_director.h"
#include "../process_def_object.h"
#include "../process_state.h"

struct process_state_vm *
psv_withconfig(struct process_def_obj *pdo, const char *scriptpath);
struct bhyve_watched_vm *bd_getvmbyname(struct bhyve_director *bd, const char *name);
struct bhyve_watched_vm {
	struct process_state_vm *state;
	const struct bhyve_configuration *config;
};


struct process_def {
	char name[PATH_MAX];
	char *description;
	char procpath[PATH_MAX];
	char **procargs; /* null terminated argument list */
	
	void *ctx; /* user context */
};

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

int
tc_psv_launch_redirected(void *ctx, pid_t *pid, struct log_director_redirector *ldr)
{
	*pid = 0;
	return 0;
}

/*
 * helper method emulating launches
 */
int
tc_psv_launch(void *ctx, pid_t *pid)
{
	return tc_psv_launch_redirected(ctx, pid, NULL);
}

/*
 * helper method emulating free
 */
void
tc_psv_free(void *ctx)
{
}

struct process_def_funcs tc_psv_nopfuncs = {
	.launch = tc_psv_launch,
	.launch_redirected = tc_psv_launch_redirected,
	.free = tc_psv_free
};

ATF_TC(tc_psv_initfree);
ATF_TC_HEAD(tc_psv_initfree, tc)
{
}
ATF_TC_BODY(tc_psv_initfree, tc)
{
	struct process_def_obj *pdobj = malloc(sizeof(struct process_def_obj));

	ATF_REQUIRE(0 != pdobj);
	bzero(pdobj, sizeof(struct process_def_obj));
	pdobj->funcs = &tc_psv_nopfuncs;
	
	struct process_state_vm *psv = psv_withconfig(pdobj, ".");

	ATF_REQUIRE(0 != psv);

	psv_free(psv);
}
ATF_TC_CLEANUP(tc_psv_initfree, tc)
{
}

ATF_TC(tc_psv_statechanges);
ATF_TC_HEAD(tc_psv_statechanges, tc)
{
}
ATF_TC_BODY(tc_psv_statechanges, tc)
{
	struct process_def_obj *pdobj = malloc(sizeof(struct process_def_obj));
	pid_t pid = 0;
	int result = 0;

	ATF_REQUIRE(0 != pdobj);
	bzero(pdobj, sizeof(struct process_def_obj));
	pdobj->funcs = &tc_psv_nopfuncs;
	
	struct process_state_vm *psv = psv_withconfig(pdobj, ".");

	ATF_REQUIRE(0 != psv);

	ATF_REQUIRE_EQ(INIT, psv_getstate(psv));

	pid = 1;
	
	/* run launch */
	ATF_REQUIRE_EQ(0, psv_startvm(psv, &pid, NULL));

	ATF_REQUIRE_EQ(0, pid);

	ATF_REQUIRE_EQ(RUNNING, psv_getstate(psv));

	/* run stop */
	ATF_REQUIRE_EQ(0, psv_stopvm(psv, &result));

	/* we are only getting to STOPPING */
	/* STOPPED is reached via kqueue feedback */
	/* which does not happen in this test object */
	ATF_REQUIRE_EQ(STOPPING, psv_getstate(psv));

	ATF_REQUIRE_EQ(0, psv_failurestate(psv));
	pid = 0;
	ATF_REQUIRE_EQ(FAILED, psv_getstate(psv));
	ATF_REQUIRE_EQ(0, pid);
	ATF_REQUIRE_EQ(-1, psv_startvm(psv, &pid, NULL));

	psv_free(psv);
}
ATF_TC_CLEANUP(tc_psv_statechanges, tc)
{
}

ATF_TC(tc_psv_startstopexe);
ATF_TC_HEAD(tc_psv_startstopexe, tc)
{
}
ATF_TC_BODY(tc_psv_startstopexe, tc)
{
	int filefd = 0;
	const char *teststring = "something { configfile = test.conf;\nowner = root; group = wheel;}\n" \
		"another_one { configfile = test2.conf;\nowner = lclchristianm; maxrestart = 3; maxrestarttime = 10; }";
	struct bhyve_configuration_store *bcs = bcs_new("/tmp");
	const struct bhyve_configuration *bc = 0;
	struct bhyve_configuration_iterator *bci = 0;
	struct process_state_vm *psv = 0;
	struct process_def *pd = 0;
	struct process_def_obj *pdo = 0;
	int counter = 0;
	pid_t pid = 0;
	int result = 0;

	errno = 0;
	filefd = open("/tmp/testfile", O_RDWR | O_CREAT);
	fchmod(filefd, S_IRWXU | S_IRWXG | S_IROTH );
	ATF_REQUIRE_EQ(0, errno);
	ATF_REQUIRE(filefd >= 0);
	ATF_REQUIRE(write(filefd, teststring, strlen(teststring))>0);
	lseek(filefd, 0, SEEK_SET);
	close(filefd);

	ATF_REQUIRE_EQ(0, bcs_parseucl(bcs, "/tmp/testfile"));

	ATF_REQUIRE(0 != (bc = bcs_getconfig_byname(bcs, "another_one")));
	ATF_REQUIRE(0 != (psv = psv_new(bc)));
	ATF_REQUIRE(0 != (pdo = psv->pdo));

	pd = pdo->ctx;
	/* fix procpath to /bin/ls */
	strncpy(pd->procpath, "/bin/ls", PATH_MAX);

	ATF_REQUIRE_EQ(0, psv_startvm(psv, &pid, NULL));
	ATF_REQUIRE(0 != pid);
	result = psv_stopvm(psv, &result);
	printf("result = %d\n", result);
	ATF_REQUIRE_EQ(0, result);
	
	psv_free(psv);

	bcs_free(bcs);

	ATF_REQUIRE_EQ(0, unlink("/tmp/testfile"));
}
ATF_TC_CLEANUP(tc_psv_startstopexe, tc)
{
	unlink("/tmp/testfile");
}

ATF_TC_WITH_CLEANUP(tc_psv_reboot);
ATF_TC_HEAD(tc_psv_reboot, tc)
{
	int filefd = 0;
	const char *teststring = "something { configfile = test.conf;\nowner = root; group = wheel;}\n" \
		"another_one { configfile = test2.conf;\nowner = lclchristianm; maxrestart = 3; maxrestarttime = 10; }";
	const char *longrunner = "#!/bin/sh\n" \
		"EXITNOW=0\n" \
		"\n" \
		"sigfunc() {\n" \
		"    echo Now exiting.\n" \
		"    EXITNOW=1\n" \
		"}\n\n" \
		"trap sigfunc TERM\n\n" \
		"while [ \"$EXITNOW\" == \"0\" ]; do\n" \
		"    sleep 1\n" \
		"done\n";

	filefd = open("/tmp/testfile_reboot", O_RDWR | O_CREAT);
	fchmod(filefd, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH );
	write(filefd, teststring, strlen(teststring));
	close(filefd);
	
	filefd = open("/tmp/testscript_reboot", O_RDWR | O_CREAT);
	fchmod(filefd, S_IRWXU | S_IRWXG | S_IROTH );
	write(filefd, longrunner, strlen(longrunner));
	close(filefd);
}
ATF_TC_BODY(tc_psv_reboot, tc)
{
	struct bhyve_configuration_store *bcs = bcs_new("/tmp");
	struct bhyve_configuration_store_obj *bcso = 0;
	struct bhyve_watched_vm *bwv = 0;
	struct bhyve_director *bd = 0;
	struct bhyve_configuration *bc = 0;
	struct process_def *pd = 0;
	int filefd = 0;
	char buffer[512] = {0};

	ATF_REQUIRE_EQ(0, bcs_parseucl(bcs, "/tmp/testfile_reboot"));

	ATF_REQUIRE(0 != (bcso = bcsobj_frombcs(bcs)));

	/* acquire another_one config */
	/* this does not need to be freed, because it's owned by the backing bcs */
	ATF_REQUIRE(0 != (bc = bcso->funcs->getconfig_byname(bcso->ctx, "another_one")));

	/* construct director */
	ATF_REQUIRE(0 != (bd = bd_new(bcso, NULL)));
	ATF_REQUIRE(0 != (bwv = bd_getvmbyname(bd, "another_one")));

	pd = bwv->state->pdo->ctx;
	ATF_REQUIRE(0 != pd);
	
	/* modify bhyve to testscript */
	strcpy(pd->procpath, "/tmp/testscript_reboot");
	
	/* start and confirm it still runs after a few seconds */
	/* starting it via director ensures that termination
	   and reboot will be handled correctly */
	ATF_REQUIRE_EQ(0, bd_startvm(bd, "another_one"));

	sleep(1);
	ATF_REQUIRE(0 != bwv->state->processid);

	/* first, we end the process without bhyve_director's knowledge */
	ATF_REQUIRE_EQ(0, psv_rebootvm(bwv->state, NULL));
	sleep(1);

	printf("bwv->state state = %d\n", psv_getstate(bwv->state));
	ATF_REQUIRE_EQ(RUNNING, psv_getstate(bwv->state));
	sleep(1);

	/* now shut down */
	ATF_REQUIRE_EQ(0, psv_stopvm(bwv->state, NULL));
	sleep(1);

	printf("bwv->state state = %d\n", psv_getstate(bwv->state));
	/* confirm that it realized it stopped */
	ATF_REQUIRE_EQ(STOPPED, psv_getstate(bwv->state));

	/* restart manually */
	ATF_REQUIRE_EQ(0, bd_startvm(bd, "another_one"));
	sleep(1);

	ATF_REQUIRE(0 != bwv->state->processid);
	/* kill process */
	kill(bwv->state->processid, SIGTERM);
	sleep(1);

	/* then we should have rebooted */
	ATF_REQUIRE_EQ(RUNNING, psv_getstate(bwv->state));

	ATF_REQUIRE_EQ(0, psv_stopvm(bwv->state, NULL));
	sleep(1);

	ATF_REQUIRE_EQ(STOPPED, psv_getstate(bwv->state));
	
	/* we cannot stop twice */
	ATF_REQUIRE_EQ(-1, bd_stopvm(bd, "another_one"));
	ATF_REQUIRE_EQ(0, psv_is_failurestate(bwv->state));

	bd_free(bd);
	bcsobj_free(bcso);
	bcs_free(bcs);
}
ATF_TC_CLEANUP(tc_psv_reboot, tc)
{
	unlink("/tmp/testfile_reboot");
	unlink("/tmp/testscript_reboot");
}

ATF_TP_ADD_TCS(testplan)
{
	ATF_TP_ADD_TC(testplan, tc_psv_initfree);
	ATF_TP_ADD_TC(testplan, tc_psv_statechanges);
	ATF_TP_ADD_TC(testplan, tc_psv_startstopexe);
	ATF_TP_ADD_TC(testplan, tc_psv_reboot);

	return atf_no_error();
}

