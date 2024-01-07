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
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "../socket_handle.h"

ATF_TC(tc_sh_newfree);
ATF_TC_HEAD(tc_sh_newfree, tc)
{
}
ATF_TC_BODY(tc_sh_newfree, tc)
{
	struct socket_handle *sh = sh_new("./testsocket.sock", 0);
	ATF_REQUIRE(0 != sh);
	sh_free(sh);
}
ATF_TC_CLEANUP(tc_sh_newfree, tc)
{
}

ATF_TC(tc_sh_permfail);
ATF_TC_HEAD(tc_sh_permfail, tc)
{
}
ATF_TC_BODY(tc_sh_permfail, tc)
{
	errno = 0;
	struct socket_handle *sh = sh_new("/var/run/testsocket.sock", 0);
	ATF_REQUIRE_EQ(0, sh);
	printf("errno = %d\n", errno);
	ATF_REQUIRE_EQ(ENOENT, errno);
}
ATF_TC_CLEANUP(tc_sh_permfail, tc)
{
}

ATF_TC(tc_sh_newfail);
ATF_TC_HEAD(tc_sh_newfail, tc)
{
}
ATF_TC_BODY(tc_sh_newfail, tc)
{
	struct socket_handle *sh = sh_new("/tmp", 0);
	/* this must fail */
	ATF_REQUIRE_EQ(0, sh);
	printf("errno = %d\n", errno);
	/* ATF_REQUIRE_EQ(EADDRINUSE, errno); */
	ATF_REQUIRE_EQ(EPERM, errno);
}
ATF_TC_CLEANUP(tc_sh_newfail, tc)
{
}


/*
 * helper method */
int
tc_sh_on_data(void *ctx, uid_t uid, pid_t pid, const char *cmd, const char *data, size_t datalen,
	      struct socket_reply_collector *src)
{
	int *counter = ctx;
	(*counter)++;
	return 0;
}

/*
 * helper method */
int
tc_sh_on_data2(void *ctx, uid_t uid, pid_t pid, const char *cmd, const char *data, size_t datalen,
	       struct socket_reply_collector *src)
{
	int *counter = ctx;
	(*counter)++;
	return 1;
}

ATF_TC(tc_sh_stopfail);
ATF_TC_HEAD(tc_sh_stopfail, tc)
{
}
ATF_TC_BODY(tc_sh_stopfail, tc)
{
	struct socket_handle *sh = sh_new("./testsocket.sock", 0);
	ATF_REQUIRE(0 != sh);
	ATF_REQUIRE(0 != sh_stop(sh));
	sh_free(sh);
}
ATF_TC_CLEANUP(tc_sh_stopfail, tc)
{
}

ATF_TC(tc_sh_startstop);
ATF_TC_HEAD(tc_sh_startstop, tc)
{
}
ATF_TC_BODY(tc_sh_startstop, tc)
{
	struct socket_handle *sh = sh_new("./testsocket.sock", 0);
	ATF_REQUIRE(0 != sh);
	ATF_REQUIRE_EQ(0, sh_start(sh));
	sleep(2);
	ATF_REQUIRE(0 != sh_start(sh));
	ATF_REQUIRE_EQ(0, sh_stop(sh));
	ATF_REQUIRE(0 != sh_stop(sh));
	sh_free(sh);
}
ATF_TC_CLEANUP(tc_sh_startstop, tc)
{
}

ATF_TP_ADD_TCS(testplan)
{
	ATF_TP_ADD_TC(testplan, tc_sh_newfree);
	ATF_TP_ADD_TC(testplan, tc_sh_permfail);
	ATF_TP_ADD_TC(testplan, tc_sh_newfail);
	ATF_TP_ADD_TC(testplan, tc_sh_stopfail);
	ATF_TP_ADD_TC(testplan, tc_sh_startstop);

	return atf_no_error();
}
