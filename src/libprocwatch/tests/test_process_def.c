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

#include <atf-c.h>
#include <atf-c/macros.h>
#include <atf-c/tc.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/stat.h>
#include <sys/wait.h>

#include <fcntl.h>

#include "../bhyve_config.h"
#include "../process_def.h"
#include "../process_def_object.h"
#include "../../liblogging/log_director.h"

pthread_mutex_t tc_pd_complexscript_mtx;
pthread_cond_t tc_pd_complexscript_cond;
int tc_pd_complexscript_status;
struct process_def *tc_pd_complexscript_pd;

struct process_def {
	char name[PATH_MAX];
	char *description;
	char procpath[PATH_MAX];
	char **procargs; /* null terminated argument list */
	
	void *ctx; /* user context */
};

ATF_TC_WITH_CLEANUP(tc_pd_fromconfig);
ATF_TC_HEAD(tc_pd_fromconfig, tc)
{
}
ATF_TC_BODY(tc_pd_fromconfig, tc)
{
	int filefd = 0;
	const char *teststring = "something { configfile = test.conf;\nowner = root; group = wheel;}\n" \
		"another_one { configfile = test2.conf;\nowner = lclchristianm; maxrestart = 3; maxrestarttime = 10; }";
	struct bhyve_configuration_store *bcs = bcs_new("/tmp");
	const struct bhyve_configuration *bc = 0;
	struct bhyve_configuration_iterator *bci = 0;
	struct process_def *pd = 0;
	struct process_def_obj *pdo = 0;
	int counter = 0;

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
	
	ATF_REQUIRE(0 != (pd = pd_fromconfig(bc)));

	/* confirm that contents are set up correctly */
	ATF_REQUIRE_EQ(0, strcmp(pd->name, "another_one"));
	ATF_REQUIRE_EQ(0, strcmp(pd->procpath, "/usr/sbin/bhyve"));
	ATF_REQUIRE(0 != pd->procargs[0]);
	ATF_REQUIRE(0 != pd->procargs[1]);
	ATF_REQUIRE_EQ(0, strcmp("-k", pd->procargs[1]));
	ATF_REQUIRE_EQ(0, strcmp("test2.conf", pd->procargs[2]));
	ATF_REQUIRE_EQ(0, pd->procargs[3]);

	ATF_REQUIRE(0 != (pdo = pdobj_frompd(pd)));

	pdobj_free(pdo);

	bcs_free(bcs);
}
ATF_TC_CLEANUP(tc_pd_fromconfig, tc)
{
	unlink("/tmp/testfile");
}

ATF_TC_WITH_CLEANUP(tc_pd_simpleexec);
ATF_TC_HEAD(tc_pd_simpleexec, tc)
{
}
ATF_TC_BODY(tc_pd_simpleexec, tc)
{
	struct process_def *pd = pd_new("test", "description",
					"/bin/ls", NULL, NULL);

	struct process_def_obj *pdobj = 0;
	int status = 0;

	pid_t lspid = 0;
	ATF_REQUIRE(0 != pd);
	ATF_REQUIRE_EQ(0, pd_launch(pd, &lspid));
	ATF_REQUIRE(0 != lspid);

	ATF_REQUIRE_EQ(lspid, waitpid(lspid, &status, 0));
	
	lspid = 0;
	pdobj = pdobj_frompd(pd);
	ATF_REQUIRE(0 != pdobj);

	ATF_REQUIRE_EQ(0, pdobj->funcs->launch(pdobj, &lspid));
	ATF_REQUIRE(0 != lspid);
	
	pdobj_free(pdobj);
}
ATF_TC_CLEANUP(tc_pd_simpleexec, tc)
{
}

void *
tc_pd_complexscript_thread(void *data)
{
	sleep(1);

	pid_t pid = 0;
	struct process_def *pd = tc_pd_complexscript_pd;
	int status = 0;
	struct log_director_redirector *ldr = data;

	pd_launch_redirected(pd, &pid, ldr);

	waitpid(pid, &status, 0);
	if (WIFEXITED(status)) {
		printf("exit code = %d\n", WEXITSTATUS(status));
	}
	if (WIFSIGNALED(status)) {
		printf("signal = %d\n", WTERMSIG(status));
	}
	tc_pd_complexscript_status = status;

	pthread_mutex_lock(&tc_pd_complexscript_mtx);
	pthread_cond_signal(&tc_pd_complexscript_cond);
	pthread_mutex_unlock(&tc_pd_complexscript_mtx);
	
	return NULL;
}

ATF_TC_WITH_CLEANUP(tc_pd_complexscript);
ATF_TC_HEAD(tc_pd_complexscript, tc)
{
	unlink("/tmp/pd_complex");
}
ATF_TC_BODY(tc_pd_complexscript, tc)
{
	/* write a script that does various things */
	const char *complex_script = "#!/bin/sh\n" \
		"echo \"Print to stderr\" 1>&2\n" \
		"echo \"Print to stdout\"\n" \
		"sleep 10\n" \
		"echo \"Print again to stderr\" 1>&2\n" \
		"echo \"Print again to stdout\"\n" \
		"exit 0\n";
	int scriptfd = 0;
	struct process_def *pd = 0;
	pthread_t thread;
	const char *procargs[2] = {0};
	struct log_director *ld = 0;
	struct log_director_redirector *ldr = 0;

	procargs[0] = "/tmp/pd_complex";
	procargs[1] = NULL;

	ATF_REQUIRE(0 != (ld = ld_new(1, "/tmp")));
	ATF_REQUIRE(0 != (ldr = ld_register_redirect(ld, "complexscript")));
	
	ATF_REQUIRE((scriptfd = open("/tmp/pd_complex", O_RDWR | O_CREAT | O_TRUNC)) >= 0);
	ATF_REQUIRE_EQ(0, fchmod(scriptfd, S_IRWXU));

	ATF_REQUIRE(write(scriptfd, complex_script, strlen(complex_script)) > 0);
	
	ATF_REQUIRE_EQ(0, close(scriptfd));

	ATF_REQUIRE_EQ(0, pthread_mutex_init(&tc_pd_complexscript_mtx, NULL));
	ATF_REQUIRE_EQ(0, pthread_cond_init(&tc_pd_complexscript_cond, NULL));

	pd = pd_new("test", "description", "/tmp/pd_complex", procargs, NULL);
	
	ATF_REQUIRE(0 != pd);

	tc_pd_complexscript_pd = pd;

	ATF_REQUIRE_EQ(0, pthread_create(&thread, NULL, tc_pd_complexscript_thread, ldr));

	ATF_REQUIRE_EQ(0, pthread_mutex_lock(&tc_pd_complexscript_mtx));
	ATF_REQUIRE_EQ(0, pthread_cond_wait(&tc_pd_complexscript_cond,
					    &tc_pd_complexscript_mtx));
	ATF_REQUIRE_EQ(0, pthread_mutex_unlock(&tc_pd_complexscript_mtx));

	/* wait for thread completion */
	ATF_REQUIRE_EQ(0, pthread_join(thread, NULL));

	ATF_REQUIRE_EQ(0, pthread_cond_destroy(&tc_pd_complexscript_cond));
	ATF_REQUIRE_EQ(0, pthread_mutex_destroy(&tc_pd_complexscript_mtx));

	pd_free(pd);
	ld_free(ld);
	unlink("/tmp/pd_complex");

	ATF_REQUIRE_EQ(1, WIFEXITED(tc_pd_complexscript_status));
	ATF_REQUIRE_EQ(0, WIFSIGNALED(tc_pd_complexscript_status));
	ATF_REQUIRE_EQ(0, WEXITSTATUS(tc_pd_complexscript_status));

}
ATF_TC_CLEANUP(tc_pd_complexscript, tc)
{
}

ATF_TP_ADD_TCS(testplan)
{
	ATF_TP_ADD_TC(testplan, tc_pd_fromconfig);
	ATF_TP_ADD_TC(testplan, tc_pd_simpleexec);
	ATF_TP_ADD_TC(testplan, tc_pd_complexscript);

	return atf_no_error();
}
