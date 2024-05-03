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

#include "../bhyve_config.h"

#include <sys/nv.h>
#include <sys/stat.h>

#include <private/ucl/ucl.h>

#include <atf-c.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "../daemon_config.h"

ATF_TC_WITH_CLEANUP(tc_dconf_parsing);
ATF_TC_HEAD(tc_dconf_parsing, tc)
{
}
ATF_TC_BODY(tc_dconf_parsing, tc)
{
	int filefd = 0;
	const char *teststring = "vmstated { tap_min = 1;\ntap_max = 1000; group = wheel;}\n";
	struct daemon_config *dc = dconf_new();

	ATF_REQUIRE(0 != dc);

	filefd = open("/tmp/dconf_test", O_RDWR | O_CREAT);
	fchmod(filefd, S_IRWXU | S_IRWXG | S_IROTH );
	printf("errno = %d\n", errno);
	ATF_REQUIRE(filefd >= 0);
	ATF_REQUIRE(write(filefd, teststring, strlen(teststring))>0);
	close(filefd);

	ATF_REQUIRE_EQ(0, dconf_parseucl(dc, "/tmp/dconf_test"));

	ATF_REQUIRE_EQ(1, dconf_get_tapid_min(dc));
	ATF_REQUIRE_EQ(1000, dconf_get_tapid_max(dc));
	ATF_REQUIRE_EQ(0, dconf_get_nmdmid_min(dc));

	dconf_free(dc);
	
}
ATF_TC_CLEANUP(tc_dconf_parsing, tc)
{
	unlink("/tmp/dconf_test");
}

ATF_TP_ADD_TCS(testplan)
{
	ATF_TP_ADD_TC(testplan, tc_dconf_parsing);
	return atf_no_error();
}
