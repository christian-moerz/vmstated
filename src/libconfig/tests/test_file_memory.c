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
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../file_memory.h"

ATF_TC(tc_fm_readcompare);
ATF_TC_HEAD(tc_fm_readcompare, tc)
{
	unlink("testfile_text");
}
ATF_TC_BODY(tc_fm_readcompare, tc)
{
	int filefd = 0;
	const char *teststring = "This is a test";
	struct file_memory *fm = 0;

	ATF_REQUIRE((filefd = open("testfile_text", O_RDWR | O_CREAT )) > 0);

	ATF_REQUIRE_EQ(0, fchmod(filefd, S_IRUSR | S_IWUSR));

	ATF_REQUIRE(strlen(teststring) ==
		    write(filefd, teststring, strlen(teststring)));

	ATF_REQUIRE_EQ(0, close(filefd));

	fm = fm_new("testfile_text");
	printf("errno: %d\n", errno);
	ATF_REQUIRE(0 != fm);

	printf("memory content: \"%s\"", fm_get_memory(fm));

	ATF_REQUIRE_EQ(0, strcmp(fm_get_memory(fm), teststring));

	fm_free(fm);
	
	unlink("testfile_text");
}

ATF_TP_ADD_TCS(testplan)
{
	ATF_TP_ADD_TC(testplan, tc_fm_readcompare);

	return atf_no_error();
}
