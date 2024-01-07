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
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "../socket_handle.h"
#include "../socket_handle_errors.h"

#define SH_CMDLEN 4
#define SH_ERRMSGLEN 512
#define SHC_MAXTRANSPORTDATA 1024

struct socket_cmdparsedata {
	char *zerocmd;
	char *zerodata;
	size_t cmdlen;
	size_t datalen;
	char errmsg[SH_ERRMSGLEN];
	uint64_t errcode;
};
struct socket_connection {
	int clientfd;
	uid_t uid;
	pid_t pid;

	/* number of bytes read */
	size_t bytes_read;
	/* number of \0 characters received */
	unsigned short zero_recv;
	char buffer[SHC_MAXTRANSPORTDATA];
	
	void *ctx;
};

int shc_dropmessage(struct socket_connection *shc, struct socket_cmdparsedata *parsedata);
int sh_try_cmdparsing(struct socket_connection *shc, struct socket_cmdparsedata *parsedata);

ATF_TC(tc_shc_invalid);
ATF_TC_HEAD(tc_shc_invalid, tc)
{
}
ATF_TC_BODY(tc_shc_invalid, tc)
{
	struct socket_connection shc = {0};
	struct socket_cmdparsedata parsedata = {0};
	size_t datalen = 0;

	ATF_REQUIRE(0 != sh_try_cmdparsing(NULL, &parsedata));
	ATF_REQUIRE(0 != sh_try_cmdparsing(&shc, NULL));
	ATF_REQUIRE_EQ(0, sh_try_cmdparsing(&shc, &parsedata));
	printf("parsedata.errcode: %lu\n", parsedata.errcode);
	/* warning on empty data */
	ATF_REQUIRE_EQ(SH_WRN_KEEPREADNMORE, parsedata.errcode);

	memset(shc.buffer, 1, SHC_MAXTRANSPORTDATA);
	/* state that the buffer is full */
	shc.bytes_read = SHC_MAXTRANSPORTDATA;
	ATF_REQUIRE_EQ(0, sh_try_cmdparsing(&shc, &parsedata));
	ATF_REQUIRE_EQ(SH_CMDERR_GARBAGECMD, parsedata.errcode);
	printf("err msg: %s\n", parsedata.errmsg);

	/* after receiving garbage, the buffer should be empty */
	ATF_REQUIRE_EQ(0, shc.bytes_read);

	bzero(shc.buffer, SHC_MAXTRANSPORTDATA);

	/* put only data without command */
	strncpy(shc.buffer, "Testing", SHC_MAXTRANSPORTDATA);
	printf("buffer: %s\n", shc.buffer);
	/* need to increase the number of bytes written to buffer */
	shc.bytes_read = SHC_MAXTRANSPORTDATA;
	ATF_REQUIRE_EQ(0, sh_try_cmdparsing(&shc, &parsedata));
	printf("cmdlen: %lu\n", parsedata.cmdlen);
	ATF_REQUIRE_EQ(strlen("Testing"), parsedata.cmdlen);
	ATF_REQUIRE_EQ(SH_CMDERR_INVALIDCMD, parsedata.errcode);
	printf("err msg: %s\n", parsedata.errmsg);

	/* after receiving invalid command, buffer should be empty again */
	printf("remaining bytes read: %lu\n", shc.bytes_read);
	printf("expecting %lu\n", SHC_MAXTRANSPORTDATA - strlen("Testing") - 2);
	ATF_REQUIRE_EQ(SHC_MAXTRANSPORTDATA - strlen("Testing") - 2, shc.bytes_read);

	/* change to valid command, but don't give any complete data */
	strncpy(shc.buffer, "Test", SHC_MAXTRANSPORTDATA);
	datalen = strlen("Test") + 1;
	ATF_REQUIRE_EQ(0, shc.buffer[datalen]);
	ATF_REQUIRE_EQ(0, shc.buffer[datalen-1]);
	memset(shc.buffer + datalen, 1, SHC_MAXTRANSPORTDATA - datalen);
	shc.bytes_read = SHC_MAXTRANSPORTDATA;
	ATF_REQUIRE_EQ(0, sh_try_cmdparsing(&shc, &parsedata));
	printf("errcode: %lu\n", parsedata.errcode);
	ATF_REQUIRE_EQ(SH_CMDERR_DATATOOLON, parsedata.errcode);
	printf("err msg: %s\n", parsedata.errmsg);

	/* buffer should be empty after too long data */
	ATF_REQUIRE_EQ(0, shc.bytes_read);
}
ATF_TC_CLEANUP(tc_shc_invalid, tc)
{
}

ATF_TC(tc_shc_basic);
ATF_TC_HEAD(tc_shc_basic, tc)
{
}
ATF_TC_BODY(tc_shc_basic, tc)
{
	struct socket_connection shc = {0};
	struct socket_cmdparsedata parsedata = {0};

	strncpy(shc.buffer, "TESTXSome data", SHC_MAXTRANSPORTDATA);
	shc.buffer[strlen("TEST")] = 0;
	shc.bytes_read = strlen("TESTXSome data") + 1;
	ATF_REQUIRE_EQ(0, sh_try_cmdparsing(&shc, &parsedata));
	ATF_REQUIRE_EQ(0, parsedata.errcode);
	ATF_REQUIRE_EQ(strlen("TEST"), parsedata.cmdlen);
	printf("data len: %ld\n", parsedata.datalen);
	ATF_REQUIRE_EQ(strlen("Some data"), parsedata.datalen);
}
ATF_TC_CLEANUP(tc_shc_basic, tc)
{
}

ATF_TC(tc_shc_bordercase);
ATF_TC_HEAD(tc_shc_bordercase, tc)
{
}
ATF_TC_BODY(tc_shc_bordercase, tc)
{
	struct socket_connection shc = {0};
	struct socket_cmdparsedata parsedata = {0};
	size_t xlen = strlen("TEST") + 1;

	strncpy(shc.buffer, "TEST", SHC_MAXTRANSPORTDATA);
	memset(shc.buffer + xlen, 32, SHC_MAXTRANSPORTDATA - xlen);
	ATF_REQUIRE_EQ(0, shc.buffer[4]);
	ATF_REQUIRE_EQ(32, shc.buffer[5]);
	printf("shc.buffer: %s\n", shc.buffer);
	printf("xlen: %lu\n", xlen);
	shc.buffer[SHC_MAXTRANSPORTDATA-1] = 0;
	shc.bytes_read = SHC_MAXTRANSPORTDATA;
	ATF_REQUIRE_EQ(0, sh_try_cmdparsing(&shc, &parsedata));
	ATF_REQUIRE_EQ(0, parsedata.errcode);
	printf("cmd len: %ld\n", parsedata.cmdlen);
	ATF_REQUIRE_EQ(xlen - 1, parsedata.cmdlen);
	printf("data len: %ld\n", parsedata.datalen);
	printf("zerocmd / zerodata: %p / %p\n", parsedata.zerocmd, parsedata.zerodata);
	printf("%d - %ld ==> %ld - 1 \\0 character\n", SHC_MAXTRANSPORTDATA, xlen,
		SHC_MAXTRANSPORTDATA - xlen);
	ATF_REQUIRE_EQ(SHC_MAXTRANSPORTDATA - xlen - 1, parsedata.datalen);
}
ATF_TC_CLEANUP(tc_shc_bordercase, tc)
{
}

ATF_TC(tc_shc_readdrop);
ATF_TC_HEAD(tc_shc_readdrop, tc)
{
}
ATF_TC_BODY(tc_shc_readdrop, tc)
{
	struct socket_connection shc = {0};
	struct socket_cmdparsedata parsedata = {0};

	strncpy(shc.buffer, "TESTXSome dataXNEXTXMore dataXEMPTX", SHC_MAXTRANSPORTDATA);
	shc.buffer[strlen("TEST")] = 0;
	shc.buffer[strlen("TESTXSome data")] = 0;
	shc.buffer[strlen("TESTXSome dataXNEXT")] = 0;
	shc.buffer[strlen("TESTXSome dataXNEXTXMore dataXEMPT")] = 0;
	shc.bytes_read = strlen("TESTXSome dataXNEXTXMore dataXEMPTX") + 1;
	ATF_REQUIRE_EQ(0, sh_try_cmdparsing(&shc, &parsedata));
	ATF_REQUIRE_EQ(0, parsedata.errcode);
	ATF_REQUIRE_EQ(strlen("TEST"), parsedata.cmdlen);
	ATF_REQUIRE_EQ(strlen("Some data"), parsedata.datalen);
	ATF_REQUIRE_EQ(0, strcmp("TEST", shc.buffer));
	ATF_REQUIRE_EQ(0, shc_dropmessage(&shc, &parsedata));
	printf("shc.buffer = %s\n", shc.buffer);
	ATF_REQUIRE_EQ(0, strcmp("NEXT", shc.buffer));

	ATF_REQUIRE_EQ(0, shc_dropmessage(&shc, &parsedata));
	printf("shc.buffer = %s\n", shc.buffer);
	ATF_REQUIRE_EQ(0, strcmp("EMPT", shc.buffer));
	bzero(&parsedata, sizeof(struct socket_cmdparsedata));
	ATF_REQUIRE_EQ(0, sh_try_cmdparsing(&shc, &parsedata));
	ATF_REQUIRE_EQ(0, parsedata.datalen);

	ATF_REQUIRE_EQ(0, shc_dropmessage(&shc, &parsedata));
	ATF_REQUIRE_EQ(0, *shc.buffer);
}
ATF_TC_CLEANUP(tc_shc_readdrop, tc)
{
}

ATF_TP_ADD_TCS(testplan)
{
	ATF_TP_ADD_TC(testplan, tc_shc_invalid);
	ATF_TP_ADD_TC(testplan, tc_shc_basic);
	ATF_TP_ADD_TC(testplan, tc_shc_bordercase);
	ATF_TP_ADD_TC(testplan, tc_shc_readdrop);

	return atf_no_error();
}
