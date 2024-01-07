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

#include <sys/nv.h>

#include "../vm_info.h"

ATF_TC(tc_bvmmi_encodedecode);
ATF_TC_HEAD(tc_bvmmi_encodedecode, tc)
{
}
ATF_TC_BODY(tc_bvmmi_encodedecode, tc)
{
	struct bhyve_vm_info *bvmi = 0;
	struct bhyve_vm_info *bvmi_array[2];
	const struct bhyve_vm_info *cbvmi_array[2] = {0};
	struct bhyve_vm_manager_info *bvmmi = 0, *bvmmi_ret = 0;
	nvlist_t *nvl = 0;
	size_t counter = 0;

	bvmi = bvmi_new("test",
			"FreeBSD",
			"14.0",
			NULL,
			NULL,
			"Something",
			0,
			0,
			0);
	
	ATF_REQUIRE(0 != bvmi);
	bvmi_array[0] = bvmi;

	bvmi = bvmi_new("test.2",
			"Windows",
			"NT 4.0",
			"root",
			"wheel",
			NULL,
			0, 0, 0);
	ATF_REQUIRE(0 != bvmi);
	bvmi_array[1] = bvmi;

	bvmmi = bvmmi_new(bvmi_array, 2, 20);
	ATF_REQUIRE(0 != bvmmi);

	ATF_REQUIRE(0 != (nvl = nvlist_create(0)));

	ATF_REQUIRE_EQ(0, bvmmi_encodenvlist(bvmmi, nvl));

	ATF_REQUIRE(0 != (bvmmi_ret = bvmmi_new(NULL, 0, 0)));

	ATF_REQUIRE_EQ(0, bvmmi_decodenvlist(nvl, bvmmi_ret));
       
	nvlist_destroy(nvl);

	ATF_REQUIRE_EQ(2, bvmmi_getvmcount(bvmmi_ret));

	for (counter = 0; counter < bvmmi_getvmcount(bvmmi_ret); counter++) {
		cbvmi_array[0] = bvmmi_getvminfo_byidx(bvmmi, counter);
		cbvmi_array[1] = bvmmi_getvminfo_byidx(bvmmi_ret, counter);
		
		ATF_REQUIRE_EQ(0, strcmp(bvmi_get_vmname(cbvmi_array[0]),
					 bvmi_get_vmname(cbvmi_array[1])));

		ATF_REQUIRE_EQ(0, strcmp(bvmi_get_os(cbvmi_array[0]),
					 bvmi_get_os(cbvmi_array[1])));

		ATF_REQUIRE_EQ(0, strcmp(bvmi_get_osversion(cbvmi_array[0]),
					 bvmi_get_osversion(cbvmi_array[1])));

		if (!bvmi_get_owner(cbvmi_array[0]))
			ATF_REQUIRE_EQ(0, bvmi_get_owner(cbvmi_array[1]));
		else
			ATF_REQUIRE_EQ(0, strcmp(bvmi_get_owner(cbvmi_array[0]),
						 bvmi_get_owner(cbvmi_array[1])));

		if (!bvmi_get_group(cbvmi_array[0]))
			ATF_REQUIRE_EQ(0, bvmi_get_group(cbvmi_array[1]));
		else
			ATF_REQUIRE_EQ(0, strcmp(bvmi_get_group(cbvmi_array[0]),
						 bvmi_get_group(cbvmi_array[1])));

		if (!bvmi_get_description(cbvmi_array[0]))
			ATF_REQUIRE_EQ(0, bvmi_get_description(cbvmi_array[1]));
		else
			ATF_REQUIRE_EQ(0, strcmp(bvmi_get_description(cbvmi_array[0]),
						 bvmi_get_description(cbvmi_array[1])));
	}

	bvmmi_free(bvmmi);
	bvmmi_free(bvmmi_ret);
}


ATF_TP_ADD_TCS(testplan)
{
	ATF_TP_ADD_TC(testplan, tc_bvmmi_encodedecode);	

	return atf_no_error();
}
