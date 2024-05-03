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

#include "../ident_keeper.h"

ATF_TC(tc_ik_init);
ATF_TC_HEAD(tc_ik_init, tc)
{
}
ATF_TC_BODY(tc_ik_init, tc)
{
	struct ident_keeper *ik = 0;

	ik = ik_new(NULL, 1, 5, NULL);
	ATF_REQUIRE(0 != ik);

	ik_free(ik);
}

ATF_TC(tc_ik_testreserve);
ATF_TC_HEAD(tc_ik_testreserve, tc)
{
}
ATF_TC_BODY(tc_ik_testreserve, tc)
{
	struct ident_keeper *ik = 0;
	struct ident_keeper_reservation *ikr = 0;

	ik = ik_new(NULL, 1, 5, NULL);
	ATF_REQUIRE(0 != ik);

	ikr = ik_reserve(ik);
	ATF_REQUIRE(0 != ikr);

	ATF_REQUIRE_EQ(1, ikr_get_ident(ikr));
	ik_dispose(ik, ikr);
	
	ik_free(ik);
}

ATF_TC(tc_ik_testreserverepeat);
ATF_TC_HEAD(tc_ik_testreserverepeat, tc)
{
}
ATF_TC_BODY(tc_ik_testreserverepeat, tc)
{
	struct ident_keeper *ik = 0;
	struct ident_keeper_reservation *ikr = 0;

	ik = ik_new(NULL, 1, 5, NULL);
	ATF_REQUIRE(0 != ik);

	ikr = ik_reserve(ik);
	ATF_REQUIRE(0 != ikr);

	ATF_REQUIRE_EQ(1, ikr_get_ident(ikr));
	ik_dispose(ik, ikr);	

	ikr = ik_reserve(ik);
	ATF_REQUIRE(0 != ikr);

	ATF_REQUIRE_EQ(1, ikr_get_ident(ikr));
	ik_dispose(ik, ikr);
	
	ik_free(ik);
}


ATF_TC(tc_ik_testreservefull);
ATF_TC_HEAD(tc_ik_testreservefull, tc)
{
}
ATF_TC_BODY(tc_ik_testreservefull, tc)
{
	struct ident_keeper *ik = 0;
	struct ident_keeper_reservation *ikr[5] = { 0 };

	ik = ik_new(NULL, 1, 5, NULL);
	ATF_REQUIRE(0 != ik);

	ikr[0] = ik_reserve(ik);
	ATF_REQUIRE(0 != ikr[0]);
	ATF_REQUIRE_EQ(1, ikr_get_ident(ikr[0]));

	ikr[1] = ik_reserve(ik);
	ATF_REQUIRE(0 != ikr[1]);
	ATF_REQUIRE_EQ(2, ikr_get_ident(ikr[1]));

	ikr[2] = ik_reserve(ik);
	ATF_REQUIRE(0 != ikr[2]);
	ATF_REQUIRE_EQ(3, ikr_get_ident(ikr[2]));

	ikr[3] = ik_reserve(ik);
	ATF_REQUIRE(0 != ikr[3]);
	ATF_REQUIRE_EQ(4, ikr_get_ident(ikr[3]));

	ikr[4] = ik_reserve(ik);
	ATF_REQUIRE(0 != ikr[4]);
	ATF_REQUIRE_EQ(5, ikr_get_ident(ikr[4]));

	ATF_REQUIRE_EQ(0, ik_reserve(ik));
	ATF_REQUIRE_EQ(ENOENT, errno);
	
	ik_dispose(ik, ikr[0]);	
	ik_dispose(ik, ikr[1]);	
	ik_dispose(ik, ikr[2]);	
	ik_dispose(ik, ikr[3]);	
	ik_dispose(ik, ikr[4]);	

	ik_free(ik);
}


ATF_TP_ADD_TCS(testplan)
{
	ATF_TP_ADD_TC(testplan, tc_ik_init);
	ATF_TP_ADD_TC(testplan, tc_ik_testreserve);
	ATF_TP_ADD_TC(testplan, tc_ik_testreserverepeat);
	ATF_TP_ADD_TC(testplan, tc_ik_testreservefull);
	return atf_no_error();
}
