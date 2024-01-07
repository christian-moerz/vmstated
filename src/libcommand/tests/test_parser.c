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

#include <sys/nv.h>

#include <atf-c.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../bhyve_command.h"

ATF_TC(tc_bc_parsenvcmd);
ATF_TC_HEAD(tc_bc_parsenvcmd, tc)
{
}
ATF_TC_BODY(tc_bc_parsenvcmd, tc)
{
	nvlist_t *nvl = 0;
	void *buffer = 0;
	size_t buflen = 0;
	struct bhyve_usercommand bcf = {0};

	ATF_REQUIRE(0 != (nvl = nvlist_create(0)));

	bcf.cmd = strdup("startvm");
	bcf.vmname = strdup("test");

	/* encode structure into nvlist */
	ATF_REQUIRE_EQ(0, bcmd_encodenvlist_command(&bcf, nvl));

	/* reset structure */
	free(bcf.cmd);
	free(bcf.vmname);
	bzero(&bcf, sizeof(struct bhyve_usercommand));

	ATF_REQUIRE(0 != (buffer = nvlist_pack(nvl, &buflen)));
	ATF_REQUIRE(buflen > 0);

	ATF_REQUIRE_EQ(0, bcmd_parse_nvlistcmd(buffer, buflen, &bcf));
	ATF_REQUIRE_EQ(0, strcmp("startvm", bcf.cmd));

	free(bcf.cmd);
	free(buffer);

	nvlist_destroy(nvl);
}

ATF_TP_ADD_TCS(testplan)
{
	ATF_TP_ADD_TC(testplan, tc_bc_parsenvcmd);

	return atf_no_error();
}
