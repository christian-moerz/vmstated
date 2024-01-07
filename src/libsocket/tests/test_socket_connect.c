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
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "../socket_handle.h"
#include "../socket_connect.h"

ATF_TC(tc_sc_connectandleave);
ATF_TC_HEAD(tc_sc_connectandleave, tc)
{
	unlink("/tmp/testsocket.sock");
}
ATF_TC_BODY(tc_sc_connectandleave, tc)
{
	struct socket_handle *sh = sh_new("/tmp/testsocket.sock", 0);
	struct socket_connection *sc = sc_new("/tmp/testsocket.sock");
	ATF_REQUIRE(0 != sh);
	ATF_REQUIRE(0 != sc);
	ATF_REQUIRE_EQ(0, errno);

	printf("sh_start callin\n");

	ATF_REQUIRE_EQ(0, sh_start(sh));

	printf("sc_connect calling\n");

	int result = sc_connect(sc);

	ATF_REQUIRE_EQ(0, result);

	/* disconnect again */
	sc_free(sc);

	result = sh_stop(sh);
	printf("sh_stop returned %d\n", result);
	ATF_REQUIRE_EQ(0, result);
	sh_free(sh);
}
ATF_TC_CLEANUP(tc_sc_connectandleave, tc)
{
}

ATF_TC(tc_sc_connectandsend);
ATF_TC_HEAD(tc_sc_connectandsend, tc)
{
}
ATF_TC_BODY(tc_sc_connectandsend, tc)
{
	struct socket_connection *sc = sc_new("/tmp/testsocket.sock");
	struct socket_handle *sh = sh_new("/tmp/testsocket.sock", 0);
	char buffer[512] = {0};
	
	ATF_REQUIRE(0 != sh);
	ATF_REQUIRE(0 != sc);
	ATF_REQUIRE_EQ(0, errno);

	printf("starting server...\n");
	fflush(NULL);
	ATF_REQUIRE_EQ(0, sh_start(sh));
	printf("connecting...\n");
	fflush(NULL);
	ATF_REQUIRE_EQ(0, sc_connect(sc));
	printf("sending data...\n");
	fflush(NULL);
	ATF_REQUIRE_EQ(0, sc_sendrecv(sc, "TOOLONG", "Something", buffer, 512));

	printf("reply buffer: %s\n", buffer);

	ATF_REQUIRE_EQ(0, sc_sendrecv(sc, "MSG", "Something", buffer, 512));

	printf("reply buffer: %s\n", buffer);
	ATF_REQUIRE_EQ(0, strcmp("0000: OK", buffer));

	/* also test without data */
	ATF_REQUIRE_EQ(0, sc_sendrecv(sc, "MSG2", NULL, buffer, 512));
	printf("reply buffer: %s\n", buffer);
	ATF_REQUIRE_EQ(0, strcmp("0000: OK", buffer));

	printf("disconnecting/freeing client...\n");
	/* disconnect again */
	sc_free(sc);

	printf("stopping server...\n");
	//ATF_REQUIRE_EQ(0, sh_stop(sh));
	sh_free(sh);
	
}
ATF_TC_CLEANUP(tc_sc_connectandsend, tc)
{
}

ATF_TC(tc_sc_sendandclear);
ATF_TC_HEAD(tc_sc_sendandclear, tc)
{
}
ATF_TC_BODY(tc_sc_sendandclear, tc)
{
	struct socket_connection *sc = sc_new("/tmp/testsocket.sock");
	struct socket_handle *sh = sh_new("/tmp/testsocket.sock", 0);
	char buffer[512] = {0};
	
	ATF_REQUIRE(0 != sh);
	ATF_REQUIRE(0 != sc);
	ATF_REQUIRE_EQ(0, errno);

	ATF_REQUIRE_EQ(0, sh_start(sh));
	ATF_REQUIRE_EQ(0, sc_connect(sc));

	ATF_REQUIRE_EQ(0, sc_sendrecv(sc, "MSG", "Something", buffer, 512));

	printf("reply buffer: %s\n", buffer);
	ATF_REQUIRE_EQ(0, strcmp("0000: OK", buffer));

	/* also test without data */
	ATF_REQUIRE_EQ(0, sc_sendrecv(sc, "MSG2", NULL, buffer, 512));
	printf("reply buffer: %s\n", buffer);
	ATF_REQUIRE_EQ(0, strcmp("0000: OK", buffer));

	printf("disconnecting/freeing client...\n");
	/* disconnect again */
	sc_free(sc);

	printf("stopping server...\n");
	ATF_REQUIRE_EQ(0, sh_stop(sh));
	sh_free(sh);
}
ATF_TC_CLEANUP(tc_sc_sendandclear, tc)
{
}

int tc_sc_ondata_ok(void *ctx, uid_t uid, pid_t pid, const char *msg, const char *data, size_t datalen, struct socket_reply_collector *src)
{
	printf("[uid: %d, pid: %d] %s: %s\n", uid, pid, msg, data);
	printf("returning 0\n");

	if (ctx) {
		int *ictx = ctx;
		*ictx = 1024;
	}
	
	return 0;
}

int tc_sc_ondata_nok(void *ctx, uid_t uid, pid_t pid, const char *msg, const char *data, size_t datalen, struct socket_reply_collector *src)
{
	printf("[uid: %d, pid: %d] %s: %s\n", uid, pid, msg, data);
	printf("returning -1\n");

	if (ctx) {
		int *ictx = ctx;
		*ictx = 512;
	}
	
	return 99;
}

ATF_TC(tc_sc_sendsubscribe);
ATF_TC_HEAD(tc_sc_sendsubscribe, tc)
{
}
ATF_TC_BODY(tc_sc_sendsubscribe, tc)
{
	struct socket_connection *sc = sc_new("/tmp/testsocket.sock");
	struct socket_handle *sh = sh_new("/tmp/testsocket.sock", 0);
	char buffer[512] = {0};
	int ctx = 512;
	
	ATF_REQUIRE(0 != sh);
	ATF_REQUIRE(0 != sc);
	ATF_REQUIRE_EQ(0, errno);

	ATF_REQUIRE_EQ(0, sh_start(sh));

	ATF_REQUIRE_EQ(0, sh_subscribe_ondata(sh, &ctx, tc_sc_ondata_ok));
	
	ATF_REQUIRE_EQ(0, sc_connect(sc));

	ATF_REQUIRE_EQ(0, sc_sendrecv(sc, "MSG", "Something", buffer, 512));

	printf("reply buffer: %s\n", buffer);
	ATF_REQUIRE_EQ(0, strcmp("0000: OK", buffer));

	printf("ctx: %d\n", ctx);
	ATF_REQUIRE_EQ(1024, ctx);

	ATF_REQUIRE_EQ(0, sh_subscribe_ondata(sh, &ctx, tc_sc_ondata_nok));

	ATF_REQUIRE_EQ(0, sc_sendrecv(sc, "MSG", "Something", buffer, 512));

	printf("reply buffer: %s\n", buffer);
	ATF_REQUIRE_EQ(0, strcmp("0099: NOK", buffer));

	printf("ctx: %d\n", ctx);
	ATF_REQUIRE_EQ(512, ctx);

	printf("disconnecting/freeing client...\n");
	/* disconnect again */
	sc_free(sc);

	printf("stopping server...\n");
	ATF_REQUIRE_EQ(0, sh_stop(sh));
	sh_free(sh);
}
ATF_TC_CLEANUP(tc_sc_sendsubscribe, tc)
{
}

ATF_TP_ADD_TCS(testplan)
{
	ATF_TP_ADD_TC(testplan, tc_sc_connectandleave);
	ATF_TP_ADD_TC(testplan, tc_sc_connectandsend);
	ATF_TP_ADD_TC(testplan, tc_sc_sendandclear);
	ATF_TP_ADD_TC(testplan, tc_sc_sendsubscribe);

	return atf_no_error();
}
