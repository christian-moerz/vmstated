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

#include <atf-c.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "../../liblogging/log_director.h"
#include "../bhyve_director.h"
#include "../bhyve_messagesub_object.h"
#include "../process_def_object.h"
#include "../process_state.h"

int tc_bd_initfree_callcounter = 0;

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

struct bhyve_watched_vm {
	struct process_state_vm *state;
	const struct bhyve_configuration *config;
};

int
tc_bd_initfree_subscribe(void *, void *, int(*on_data)(void *, uid_t, pid_t, const char *,
						       const char *, size_t,
						       struct bhyve_messagesub_replymgr *));

int bwv_timestamp(struct bhyve_watched_vm *bwv);
unsigned int bwv_countrestarts_since(struct bhyve_watched_vm *bwv, time_t deadline);
struct bhyve_watched_vm *bd_getvmbyname(struct bhyve_director *bd, const char *name);
bool bwv_is_countfail(struct bhyve_watched_vm *bwv);

struct bhyve_messagesub_obj tc_bd_initfree_msgsub = {
	.obj = NULL,
	.subscribe_ondata = tc_bd_initfree_subscribe
};

int
tc_bd_initfree_subscribe(void *obj, void *ctx, int(*on_data)(void *, uid_t, pid_t, const char *,
							     const char *, size_t,
							     struct bhyve_messagesub_replymgr *))
{
	tc_bd_initfree_callcounter++;
	
	return on_data(ctx, 0, 0, "test", "data", 0, NULL);
}

ATF_TC_WITH_CLEANUP(tc_bd_initfree);
ATF_TC_HEAD(tc_bd_initfree, tc)
{
	tc_bd_initfree_callcounter = 0;
	unlink("/tmp/testfile");
}
ATF_TC_BODY(tc_bd_initfree, tc)
{
	int filefd = 0;
	const char *teststring = "something { configfile = test.conf;\nowner = root; group = wheel;}\n" \
		"another_one { configfile = test2.conf;\nowner = lclchristianm; }\n";
	struct bhyve_configuration_store *bcs = bcs_new("/tmp");
	struct bhyve_configuration_store_obj *bcso = 0;
	struct bhyve_director *bd = 0;

	errno = 0;
	filefd = open("/tmp/testfile2", O_RDWR | O_CREAT);
	fchmod(filefd, S_IRWXU | S_IRWXG | S_IROTH );
	ATF_REQUIRE_EQ(0, errno);
	ATF_REQUIRE(filefd >= 0);
	ATF_REQUIRE(write(filefd, teststring, strlen(teststring))>0);
	close(filefd);

	ATF_REQUIRE_EQ(0, bcs_parseucl(bcs, "/tmp/testfile2"));

	ATF_REQUIRE(0 != (bcso = bcsobj_frombcs(bcs)));

	ATF_REQUIRE(0 != (bd = bd_new(bcso, NULL)));

	ATF_REQUIRE_EQ(0, bd_subscribe_commands(bd, &tc_bd_initfree_msgsub));

	ATF_REQUIRE_EQ(1, tc_bd_initfree_callcounter);
	ATF_REQUIRE_EQ(1, bd_getmsgcount(bd));

	bd_free(bd);
	bcsobj_free(bcso);

	bcs_free(bcs);
}
ATF_TC_CLEANUP(tc_bd_initfree, tc)
{
	unlink("/tmp/testfile2");
}

ATF_TC_WITH_CLEANUP(tc_bd_timestamping);
ATF_TC_HEAD(tc_bd_timestamping, tc)
{
}
ATF_TC_BODY(tc_bd_timestamping, tc)
{
	int filefd = 0;
	const char *teststring = "something { configfile = test.conf;\nowner = root; group = wheel;}\n" \
		"another_one { configfile = test2.conf;\nowner = lclchristianm; maxrestart = 3; maxrestarttime = 10; }";
	struct bhyve_configuration_store *bcs = bcs_new("/tmp");
	struct bhyve_configuration_store_obj *bcso = 0;
	struct bhyve_watched_vm *bwv = 0;
	struct bhyve_director *bd = 0;
	struct bhyve_configuration *bc = 0;
	time_t tstamp = 0;

	errno = 0;
	filefd = open("/tmp/testfile", O_RDWR | O_CREAT);
	fchmod(filefd, S_IRWXU | S_IRWXG | S_IROTH );
	ATF_REQUIRE_EQ(0, errno);
	ATF_REQUIRE(filefd >= 0);
	ATF_REQUIRE(write(filefd, teststring, strlen(teststring))>0);
	lseek(filefd, 0, SEEK_SET);
	close(filefd);

	ATF_REQUIRE_EQ(0, bcs_parseucl(bcs, "/tmp/testfile"));

	ATF_REQUIRE(0 != (bcso = bcsobj_frombcs(bcs)));

	/* acquire another_one config */
	/* this does not need to be freed, because it's owned by the backing bcs */
	ATF_REQUIRE(0 != (bc = bcso->funcs->getconfig_byname(bcso->ctx, "another_one")));

	/* construct director */
	ATF_REQUIRE(0 != (bd = bd_new(bcso, NULL)));

	ATF_REQUIRE_EQ(0, bd_getvmbyname(bd, "nothing"));

	ATF_REQUIRE(0 != (bwv = bd_getvmbyname(bd, "another_one")));

	ATF_REQUIRE_EQ(0, bwv_countrestarts_since(bwv, 0));
	ATF_REQUIRE_EQ(0, bwv_timestamp(bwv));
	ATF_REQUIRE_EQ(1, bwv_countrestarts_since(bwv, 0));

	sleep(1);

	tstamp = time(NULL);

	ATF_REQUIRE_EQ(0, bwv_timestamp(bwv));
	ATF_REQUIRE_EQ(0, bwv_timestamp(bwv));
	ATF_REQUIRE_EQ(0, bwv_timestamp(bwv));

	ATF_REQUIRE_EQ(3, bwv_countrestarts_since(bwv, tstamp));
	ATF_REQUIRE_EQ(false, bwv_is_countfail(bwv));

	ATF_REQUIRE_EQ(0, bwv_timestamp(bwv));	
	ATF_REQUIRE_EQ(true, bwv_is_countfail(bwv));

	bd_free(bd);
	bcsobj_free(bcso);

	bcs_free(bcs);
}
ATF_TC_CLEANUP(tc_bd_timestamping, tc)
{
	unlink("/tmp/testfile");
}

ATF_TC_WITH_CLEANUP(tc_bd_vmstartstop);
ATF_TC_HEAD(tc_bd_vmstartstop, tc)
{
	int filefd = 0;
	const char *teststring = "something { configfile = test.conf;\nowner = root; group = wheel;}\n" \
		"another_one { configfile = test2.conf;\nowner = lclchristianm; maxrestart = 3; maxrestarttime = 10; }";
	const char *testscript = "#!/bin/sh\necho 1 >> /tmp/output\n" \
		"if [ \"$1\" != \"-k\" ]; then\n" \
		"  exit 1\n" \
		"fi\n" \
		"if [ \"$2\" != \"test2.conf\" ]; then\n" \
		"  exit 1\n" \
		"fi\n";

	filefd = open("/tmp/testfile", O_RDWR | O_CREAT);
	fchmod(filefd, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH );
	write(filefd, teststring, strlen(teststring));
	close(filefd);

	filefd = open("/tmp/testscript", O_RDWR | O_CREAT);
	fchmod(filefd, S_IRWXU | S_IRWXG | S_IROTH );
	write(filefd, testscript, strlen(testscript));
	close(filefd);

	unlink("/tmp/output");	
}
ATF_TC_BODY(tc_bd_vmstartstop, tc)
{	
	struct bhyve_configuration_store *bcs = bcs_new("/tmp");
	struct bhyve_configuration_store_obj *bcso = 0;
	struct bhyve_watched_vm *bwv = 0;
	struct bhyve_director *bd = 0;
	struct bhyve_configuration *bc = 0;
	struct process_def *pd = 0;
	int filefd = 0;
	char buffer[512] = {0};

	ATF_REQUIRE_EQ(0, bcs_parseucl(bcs, "/tmp/testfile"));

	ATF_REQUIRE(0 != (bcso = bcsobj_frombcs(bcs)));

	/* acquire another_one config */
	/* this does not need to be freed, because it's owned by the backing bcs */
	ATF_REQUIRE(0 != (bc = bcso->funcs->getconfig_byname(bcso->ctx, "another_one")));

	/* construct director */
	ATF_REQUIRE(0 != (bd = bd_new(bcso, NULL)));
	ATF_REQUIRE(0 != (bwv = bd_getvmbyname(bd, "another_one")));

	ATF_REQUIRE_EQ(0, strcmp(bwv->state->scriptpath, "/tmp"));
	ATF_REQUIRE_EQ(0, bwv->state->processid);
	ATF_REQUIRE_EQ(INIT, psv_getstate(bwv->state));

	pd = bwv->state->pdo->ctx;
	ATF_REQUIRE(0 != pd);
	
	/* modify bhyve to testscript */
	ATF_REQUIRE_EQ(0, strcmp(pd->procpath, "/usr/sbin/bhyve"));
	strcpy(pd->procpath, "/tmp/testscript");

	/* disable reboot manager on this, otherwise it'll loop */
	bwv->state->rmo = NULL;
	
	/* start and confirm it ends up stopped right after */
	ATF_REQUIRE_EQ(0, bd_startvm(bd, "another_one"));

	sleep(1);
	ATF_REQUIRE(0 != bwv->state->processid);

	ATF_REQUIRE_EQ(-1, bd_stopvm(bd, "another_one"));
	
	/* confirm 1 was written to output file */
	/* start and restart until failure */
	ATF_REQUIRE((filefd = open("/tmp/output", O_RDONLY)));
	ATF_REQUIRE(read(filefd, buffer, 512) > 0);
	printf("buffer = \"%s\"\n", buffer);
	ATF_REQUIRE_EQ(0, strcmp("1\n", buffer));
	ATF_REQUIRE_EQ(0, close(filefd));

	/* lets retry starting 4 times; should lead to failure */
	ATF_REQUIRE_EQ(0, bd_startvm(bd, "another_one"));
	sleep(1);
	ATF_REQUIRE_EQ(0, bd_startvm(bd, "another_one"));
	sleep(1);
	ATF_REQUIRE_EQ(0, bd_startvm(bd, "another_one"));
	sleep(1);
	ATF_REQUIRE(0 != bd_startvm(bd, "another_one"));
	/* we should now be in failure mode because we exceeded
	   max reboot */
	ATF_REQUIRE_EQ(1, psv_is_failurestate(bwv->state));
	

	bd_free(bd);
	bcsobj_free(bcso);
	bcs_free(bcs);
}
ATF_TC_CLEANUP(tc_bd_vmstartstop, tc)
{
	unlink("/tmp/testfile");
	unlink("/tmp/testscript");
	unlink("/tmp/output");
}

ATF_TC_WITH_CLEANUP(tc_bd_vmstartstoplong);
ATF_TC_HEAD(tc_bd_vmstartstoplong, tc)
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

	filefd = open("/tmp/testfile_long", O_RDWR | O_CREAT);
	fchmod(filefd, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH );
	write(filefd, teststring, strlen(teststring));
	close(filefd);
	
	filefd = open("/tmp/testscript_long", O_RDWR | O_CREAT);
	fchmod(filefd, S_IRWXU | S_IRWXG | S_IROTH );
	write(filefd, longrunner, strlen(longrunner));
	close(filefd);
}
ATF_TC_BODY(tc_bd_vmstartstoplong, tc)
{
	struct bhyve_configuration_store *bcs = bcs_new("/tmp");
	struct bhyve_configuration_store_obj *bcso = 0;
	struct bhyve_watched_vm *bwv = 0;
	struct bhyve_director *bd = 0;
	struct bhyve_configuration *bc = 0;
	struct process_def *pd = 0;
	int filefd = 0;
	char buffer[512] = {0};

	ATF_REQUIRE_EQ(0, bcs_parseucl(bcs, "/tmp/testfile"));

	ATF_REQUIRE(0 != (bcso = bcsobj_frombcs(bcs)));

	/* acquire another_one config */
	/* this does not need to be freed, because it's owned by the backing bcs */
	ATF_REQUIRE(0 != (bc = bcso->funcs->getconfig_byname(bcso->ctx, "another_one")));

	/* construct director */
	ATF_REQUIRE(0 != (bd = bd_new(bcso, NULL)));
	ATF_REQUIRE(0 != (bwv = bd_getvmbyname(bd, "another_one")));

	ATF_REQUIRE_EQ(0, strcmp(bwv->state->scriptpath, "/tmp"));
	ATF_REQUIRE_EQ(0, bwv->state->processid);
	ATF_REQUIRE_EQ(INIT, psv_getstate(bwv->state));

	pd = bwv->state->pdo->ctx;
	ATF_REQUIRE(0 != pd);
	
	/* modify bhyve to testscript */
	ATF_REQUIRE_EQ(0, strcmp(pd->procpath, "/usr/sbin/bhyve"));
	strcpy(pd->procpath, "/tmp/testscript_long");
	
	/* start and confirm it still runs after a few seconds */
	ATF_REQUIRE_EQ(0, bd_startvm(bd, "another_one"));

	sleep(1);
	ATF_REQUIRE(0 != bwv->state->processid);

	/* first, we end the process without bhyve_director's knowledge */
	ATF_REQUIRE_EQ(0, psv_stopvm(bwv->state, NULL));
	sleep(1);

	/* confirm that it realized it stopped */
	ATF_REQUIRE_EQ(STOPPED, psv_getstate(bwv->state));
	/* we cannot stop twice */
	ATF_REQUIRE_EQ(-1, bd_stopvm(bd, "another_one"));
	ATF_REQUIRE_EQ(0, psv_is_failurestate(bwv->state));

	bd_free(bd);
	bcsobj_free(bcso);
	bcs_free(bcs);
	
}
ATF_TC_CLEANUP(tc_bd_vmstartstoplong, tc)
{
	unlink("/tmp/testfile_long");
	unlink("/tmp/testscript_long");
}

ATF_TP_ADD_TCS(testplan)
{
	ATF_TP_ADD_TC(testplan, tc_bd_initfree);
	ATF_TP_ADD_TC(testplan, tc_bd_timestamping);
	ATF_TP_ADD_TC(testplan, tc_bd_vmstartstop);
	ATF_TP_ADD_TC(testplan, tc_bd_vmstartstoplong);

	return atf_no_error();
}

