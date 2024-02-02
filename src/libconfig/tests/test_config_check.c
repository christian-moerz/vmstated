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

#include "../check.h"
#include "../config_block.h"
#include "../config_core.h"

ATF_TC(tc_check_basic);
ATF_TC_HEAD(tc_check_basic, tc)
{
}
ATF_TC_BODY(tc_check_basic, tc)
{
	struct bhyve_parameters_core *bpc = 0;
	struct bhyve_parameters_pcislot *bpp = 0;
	const struct bhyve_parameters_check *bpcheck = 0;

	bpc = bpc_new("testvm");
	bpc_set_bootrom(bpc, "something", false, NULL);
	bpp = bpp_new_isabridge();
	bpc_addpcislot_at(bpc, 0, 1, 0, bpp);

	ATF_REQUIRE(0 != bpc);

	/* we need to a add a hostbridge or we fail */
	ATF_REQUIRE(0 != check_parameters_ok(bpc));
	
	ATF_REQUIRE(0 != (bpp = bpp_new_hostbridge(HOSTBRIDGE)));
	ATF_REQUIRE_EQ(0, bpc_addpcislot_at(bpc, 0, 0, 0, bpp));

	bpcheck = check_parameters_ok(bpc);
	if (bpcheck)
		printf("error: %s\n", check_get_errormsg(bpcheck));
	
	ATF_REQUIRE_EQ(0, bpcheck);

	bpc_free(bpc);

	ATF_REQUIRE(0 != check_parameters_ok(NULL));

	bpc = bpc_new("");
	ATF_REQUIRE(0 != bpc);
	ATF_REQUIRE(0 != check_parameters_ok(bpc));
	
	bpc_free(bpc);
}

ATF_TC(tc_check_multihost);
ATF_TC_HEAD(tc_check_multihost, tc)
{
}
ATF_TC_BODY(tc_check_multihost, tc)
{
	struct bhyve_parameters_core *bpc = 0;
	struct bhyve_parameters_pcislot *bpp = 0;
	const struct bhyve_parameters_check *bpcheck = 0;

	bpc = bpc_new("testvm");

	ATF_REQUIRE(0 != bpc);

	/* we need to a add a hostbridge or we fail */
	ATF_REQUIRE(0 != check_parameters_ok(bpc));
	
	ATF_REQUIRE(0 != (bpp = bpp_new_hostbridge(HOSTBRIDGE)));
	ATF_REQUIRE_EQ(0, bpc_addpcislot_at(bpc, 0, 0, 0, bpp));
	bpc_set_bootrom(bpc, "something", false, NULL);
	bpp = bpp_new_isabridge();
	ATF_REQUIRE_EQ(0, bpc_addpcislot_at(bpc, 0, 1, 0, bpp));

	bpcheck = check_parameters_ok(bpc);
	if (bpcheck)
		printf("error: %s\n", check_get_errormsg(bpcheck));
	ATF_REQUIRE_EQ(0, bpcheck);

	/* add a second host bridge and we should get a failure */

	ATF_REQUIRE(0 != (bpp = bpp_new_hostbridge(HOSTBRIDGE)));
	ATF_REQUIRE_EQ(0, bpc_addpcislot_at(bpc, 1, 0, 0, bpp));
	
	ATF_REQUIRE(0 != check_parameters_ok(bpc));

	bpc_free(bpc);
}

ATF_TC(tc_check_nonzerolpc);
ATF_TC_HEAD(tc_check_nonzerolpc, tc)
{
}
ATF_TC_BODY(tc_check_nonzerolpc, tc)
{
	struct bhyve_parameters_core *bpc = 0;
	struct bhyve_parameters_pcislot *bpp = 0;
	const struct bhyve_parameters_check *bpcheck = 0;

	bpc = bpc_new("testvm");
	ATF_REQUIRE(0 != bpc);

	bpc_set_bootrom(bpc, "something", false, NULL);
	bpp = bpp_new_isabridge();
	bpc_addpcislot_at(bpc, 1, 0, 0, bpp);
	ATF_REQUIRE(0 != (bpp = bpp_new_hostbridge(HOSTBRIDGE)));
	ATF_REQUIRE_EQ(0, bpc_addpcislot_at(bpc, 0, 0, 0, bpp));

	bpcheck = check_parameters_ok(bpc);
	if (bpcheck)
		printf("error: %s\n", check_get_errormsg(bpcheck));
	
	ATF_REQUIRE(0 != bpcheck);

	bpc_free(bpc);
}

ATF_TC(tc_check_pciidconflict);
ATF_TC_HEAD(tc_check_pciidconflict, tc)
{
}
ATF_TC_BODY(tc_check_pciidconflict, tc)
{
	struct bhyve_parameters_core *bpc = 0;
	struct bhyve_parameters_pcislot *bpp = 0;
	const struct bhyve_parameters_check *bpcheck = 0;

	bpc = bpc_new("testvm");

	ATF_REQUIRE(0 != bpc);

	/* create and add new hostbridge */
	ATF_REQUIRE(0 != (bpp = bpp_new_hostbridge(HOSTBRIDGE)));
	ATF_REQUIRE_EQ(0, bpc_addpcislot_at(bpc, 0, 0, 0, bpp));
	ATF_REQUIRE_EQ(0, bpc_set_bootrom(bpc, "something", false, NULL));
	ATF_REQUIRE(0 != (bpp = bpp_new_isabridge()));
	ATF_REQUIRE_EQ(0, bpc_addpcislot_at(bpc, 0, 0, 0, bpp));

	bpcheck = check_parameters_ok(bpc);
	if (bpcheck)
		printf("error: %s\n", check_get_errormsg(bpcheck));

	ATF_REQUIRE(0 != bpcheck);

	bpc_free(bpc);	
}

ATF_TC(tc_check_addblock);
ATF_TC_HEAD(tc_check_addblock, tc)
{
}
ATF_TC_BODY(tc_check_addblock, tc)
{
	struct bhyve_parameters_core *bpc = 0;
	struct bhyve_parameters_pcislot *bpp = 0;
	const struct bhyve_parameters_check *bpcheck = 0;

	bpc = bpc_new("testvm");

	ATF_REQUIRE(0 != bpc);

	/* create and add new hostbridge */
	ATF_REQUIRE(0 != (bpp = bpp_new_hostbridge(HOSTBRIDGE)));
	ATF_REQUIRE_EQ(0, bpc_addpcislot_at(bpc, 0, 0, 0, bpp));
	ATF_REQUIRE_EQ(0, bpc_set_bootrom(bpc, "something", false, NULL));
	ATF_REQUIRE(0 != (bpp = bpp_new_isabridge()));
	ATF_REQUIRE_EQ(0, bpc_addpcislot_at(bpc, 0, 1, 0, bpp));
	ATF_REQUIRE(0 != (bpp = bpp_new_block()));
	ATF_REQUIRE_EQ(0, bpp_new_block_virtioblk(bpp_get_block(bpp), "test"));
	ATF_REQUIRE_EQ(0, bpc_addpcislot_at(bpc, 0, 2, 0, bpp));

	bpcheck = check_parameters_ok(bpc);
	if (bpcheck)
		printf("error: %s\n", check_get_errormsg(bpcheck));

	ATF_REQUIRE_EQ(0, bpcheck);

	bpc_free(bpc);	
	
}

ATF_TP_ADD_TCS(testplan)
{
	ATF_TP_ADD_TC(testplan, tc_check_basic);
	ATF_TP_ADD_TC(testplan, tc_check_multihost);
	ATF_TP_ADD_TC(testplan, tc_check_nonzerolpc);
	ATF_TP_ADD_TC(testplan, tc_check_pciidconflict);
	ATF_TP_ADD_TC(testplan, tc_check_addblock);

	return atf_no_error();
}
