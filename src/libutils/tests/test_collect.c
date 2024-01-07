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
#include <string.h>

#include "../transmit_collect.h"

ATF_TC(tc_stc_collect);
ATF_TC_HEAD(tc_stc_collect, tc)
{
}
ATF_TC_BODY(tc_stc_collect, tc)
{
	struct socket_transmission_collector *stc = 0;
	char buffer[512] = {0};
	size_t bufferlens[2] = {0};
	char defstring[4] = {0};

	strcpy(defstring, "def");

	ATF_REQUIRE(0 != (stc = stc_new()));

	ATF_REQUIRE_EQ(0, stc_store_transmit(stc, "abc", 3));
	ATF_REQUIRE_EQ(0, stc_store_transmit(stc, defstring, 4));
	ATF_REQUIRE_EQ(2, stc_getbuffercount(stc));
	ATF_REQUIRE_EQ(7, stc_getbuffersize(stc));
	errno = 0;
	ATF_REQUIRE_EQ(0, stc_collect(stc, buffer, 512, bufferlens, 2*sizeof(size_t)));
	ATF_REQUIRE_EQ(0, errno);
	ATF_REQUIRE_EQ(0, strcmp("abcdef", buffer));

	stc_free(stc);
}
ATF_TC_CLEANUP(tc_stc_collect, tc)
{
}

ATF_TP_ADD_TCS(testplan)
{
	ATF_TP_ADD_TC(testplan, tc_stc_collect);

	return atf_no_error();
}
