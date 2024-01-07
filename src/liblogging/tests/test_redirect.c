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

#include <sys/wait.h>

#include <atf-c.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../log_director.h"
#include "../../libprocwatch/state_change.h"

ATF_TC(tc_ld_initfree);
ATF_TC_HEAD(tc_ld_initfree, tc)
{
}
ATF_TC_BODY(tc_ld_initfree, tc)
{
	struct log_director *ld = ld_new(1, "/tmp");
	struct log_director_redirector *ldr = 0;

	ATF_REQUIRE(0 != ld);
	ATF_REQUIRE(0 != (ldr = ld_register_redirect(ld, "testfile")));

	ld_free(ld);

	atf_utils_file_exists("/tmp/testfile.log");
	unlink("/tmp/testfile.log");
}

extern char **environ;

ATF_TC(tc_ld_forktest);
ATF_TC_HEAD(tc_ld_forktest, tc)
{
}
ATF_TC_BODY(tc_ld_forktest, tc)
{
	struct log_director *ld = ld_new(1, "/tmp");
	struct log_director_redirector *ldr = 0;
	struct log_director_redirector_client *ldrd = 0;
	size_t counter = 0;
	pid_t pid = 0;
	int status = 0;
	char *argv[4] = {0};
	char scriptname[PATH_MAX] = {0};

	argv[0] = "/bin/ls";
	argv[1] = "-l";
	argv[2] = "/tmp";

	unlink("/tmp/testfile2.log");
	
	ATF_REQUIRE(0 != ld);

	for (counter = 0; counter < 10; counter++) {
		snprintf(scriptname, PATH_MAX, "testfile2_%zu", counter);
		ATF_REQUIRE(0 != (ldr = ld_register_redirect(ld, scriptname)));

		ATF_REQUIRE(0 != (ldrd = ldr_newclient(ldr)));
		
		if (0 == (pid = fork())) {
			ldrd_redirect_stdout(ldrd);
			
			exit(execve("/bin/ls", argv, environ));
		}
		
		ldrd_accept_redirect(ldrd);
		
		waitpid(pid, &status, 0);
		
		ATF_REQUIRE_EQ(true, WIFEXITED(status));
		printf("exit code = %d\n", WEXITSTATUS(status));
		ATF_REQUIRE_EQ(0, WEXITSTATUS(status));

		/* create a new client */
		ATF_REQUIRE(0 != (ldrd = ldr_newclient(ldr)));

		if (0 == (pid = fork())) {
			ldrd_redirect_stdout(ldrd);
			
			exit(execve("/bin/ls", argv, environ));
		}
		
		ldrd_accept_redirect(ldrd);
		
		waitpid(pid, &status, 0);
		
		ATF_REQUIRE_EQ(true, WIFEXITED(status));
		printf("exit code x2 = %d\n", WEXITSTATUS(status));
		ATF_REQUIRE_EQ(0, WEXITSTATUS(status));
		
	}

	ld_free(ld);

	atf_utils_file_exists("/tmp/testfile2_0.log");
	atf_utils_cat_file("/tmp/testfile2_0.log", "testfile2_0.log");
	
	unlink("/tmp/testfile2.log");
	for (counter = 0; counter < 10; counter++) {
		snprintf(scriptname, PATH_MAX, "/tmp/testfile2_%zu.log", counter);
		unlink(scriptname);
	}
}

ATF_TC(tc_ld_procwatchrun);
ATF_TC_HEAD(tc_ld_procwatchrun, tc)
{
}
ATF_TC_BODY(tc_ld_procwatchrun, tc)
{
	struct log_director *ld = ld_new(1, "/tmp");
	struct log_director_redirector *ldr = 0;
	size_t counter = 0;
	pid_t pid = 0;
	int status = 0;
	char *argv[4] = {0};
	char scriptname[PATH_MAX] = {0};

	argv[0] = "/bin/ls";
	argv[1] = "-l";
	argv[2] = "/tmp";

	unlink("/tmp/testfile2.log");
	
	ATF_REQUIRE(0 != ld);

	for (counter = 0; counter < 10; counter++) {
		snprintf(scriptname, PATH_MAX, "testfile2_%zu", counter);
		ATF_REQUIRE(0 != (ldr = ld_register_redirect(ld, scriptname)));

		ATF_REQUIRE_EQ(0, sch_runscript("/bin/ls", false, ldr));
		ATF_REQUIRE_EQ(0, sch_runscript("/bin/ls", true, ldr));
	}

	sleep(2);
	ld_free(ld);

	atf_utils_file_exists("/tmp/testfile2_0.log");
	atf_utils_cat_file("/tmp/testfile2_0.log", "testfile2_0.log");
	
	unlink("/tmp/testfile2.log");
	for (counter = 0; counter < 10; counter++) {
		snprintf(scriptname, PATH_MAX, "/tmp/testfile2_%zu.log", counter);
		unlink(scriptname);
	}
	
}


ATF_TP_ADD_TCS(testplan)
{
	ATF_TP_ADD_TC(testplan, tc_ld_initfree);
	ATF_TP_ADD_TC(testplan, tc_ld_forktest);
	ATF_TP_ADD_TC(testplan, tc_ld_procwatchrun);

	return atf_no_error();
}
