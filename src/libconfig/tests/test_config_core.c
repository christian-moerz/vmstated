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

#include "../config_core.h"

ATF_TC(tc_bpp_initfree);
ATF_TC_HEAD(tc_bpp_initfree, tc)
{
}
ATF_TC_BODY(tc_bpp_initfree, tc)
{
	struct bhyve_parameters_core *bpc = 0;

	bpc = bpc_new("testvm");

	ATF_REQUIRE(0 != bpc);

	bpc_free(bpc);	
}

ATF_TC(tc_bpp_iterator);
ATF_TC_HEAD(tc_bpp_iterator, tc)
{
}
ATF_TC_BODY(tc_bpp_iterator, tc)
{
	struct bhyve_parameters_core *bpc = 0;
	struct bhyve_parameters_pcislot *bpp = 0;
	const struct bhyve_parameters_pcislot *cmp_bpp = 0;
	struct bhyve_parameters_pcislot_iter *bppi = 0;
	size_t counter = 0;

	bpc = bpc_new("testvm");

	ATF_REQUIRE(0 != bpc);

	/* create and add new hostbridge */
	ATF_REQUIRE(0 != (bpp = bpp_new_hostbridge(HOSTBRIDGE)));

	ATF_REQUIRE_EQ(0, bpc_addpcislot_at(bpc, 0, 0, 0, bpp));

	/* get iterator */
	ATF_REQUIRE(0 != (bppi = bpc_iter_pcislots(bpc)));

	while (bppi_next(bppi)) {
		ATF_REQUIRE(0 != (cmp_bpp = bppi_item(bppi)));
		ATF_REQUIRE_EQ(cmp_bpp, bpp);
		counter++;
	}

	ATF_REQUIRE_EQ(1, counter);

	bppi_free(bppi);

	bpc_free(bpc);	
}


ATF_TP_ADD_TCS(testplan)
{
	ATF_TP_ADD_TC(testplan, tc_bpp_initfree);
	ATF_TP_ADD_TC(testplan, tc_bpp_iterator);
	
	return atf_no_error();
}
