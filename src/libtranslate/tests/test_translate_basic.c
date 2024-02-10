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

#include <atf-c/macros.h>
#include <sys/stat.h>

#include <atf-c.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../param_translate_core.h"
#include "../../libconfig/config_core.h"

ATF_TC_WITH_CLEANUP(tc_ptc_init);
ATF_TC_HEAD(tc_ptc_init, tc)
{
}
ATF_TC_BODY(tc_ptc_init, tc)
{
	int filefd = 0;
	const char *teststring = "another_one {\n"
		"configfile = test2.conf;\n" \
		"owner = lclchristianm;\n" \
		"maxrestart = 3;\n" \
		"maxrestarttime = 10;\n" \
		"bootrom = some_file;\n" \
		"\tconsoles {\n" \
		"\t\tconsole0 {\n" \
		"\t\t\tname = vmconsole0;\n" \
		"\t\t}\n" \
		"\t} \n" \
		"}";
	struct bhyve_configuration_store *bcs = bcs_new("/tmp");
	struct param_translate_core *ptc = 0;
	const struct bhyve_configuration *bc = 0;
	int counter = 0;

	errno = 0;
	filefd = open("/tmp/config_for_ptcinit", O_RDWR | O_CREAT);
	fchmod(filefd, S_IRWXU | S_IRWXG | S_IROTH );
	ATF_REQUIRE_EQ(0, errno);
	ATF_REQUIRE(filefd >= 0);
	ATF_REQUIRE(write(filefd, teststring, strlen(teststring))>0);
	lseek(filefd, 0, SEEK_SET);
	close(filefd);

	ATF_REQUIRE_EQ(0, bcs_parseucl(bcs, "/tmp/config_for_ptcinit"));

	ATF_REQUIRE(0 != (bc = bcs_getconfig_byname(bcs, "another_one")));

	ATF_REQUIRE(0 != (ptc = ptc_new(bc)));

	/* free the store - that also frees the configs */
	bcs_free(bcs);
	ptc_free(ptc);
}
ATF_TC_CLEANUP(tc_ptc_init, tc)
{
	unlink("/tmp/config_for_ptcinit");
}

ATF_TC_WITH_CLEANUP(tc_ptc_translate);
ATF_TC_HEAD(tc_ptc_translate, tc)
{
}
ATF_TC_BODY(tc_ptc_translate, tc)
{
	int filefd = 0;
	const char *teststring = "another_one {\n"
		"configfile = test2.conf;\n" \
		"owner = lclchristianm;\n" \
		"maxrestart = 3;\n" \
		"maxrestarttime = 10;\n" \
		"bootrom = some_file;\n" \
		"\tconsoles {\n" \
		"\t\tconsole0 {\n" \
		"\t\t\tname = vmconsole0;\n" \
		"\t\t\tbackend = /dev/nmbd0;\n" \
		"\t\t}\n" \
		"\t} \n" \
		"}";
	struct bhyve_configuration_store *bcs = bcs_new("/tmp");
	struct param_translate_core *ptc = 0;
	const struct bhyve_configuration *bc = 0;
	const struct bhyve_parameters_core *bpc = 0;
	const struct bhyve_parameters_comport *bpcom = 0;
	int counter = 0;

	errno = 0;
	filefd = open("/tmp/config_for_ptcparse", O_RDWR | O_CREAT);
	fchmod(filefd, S_IRWXU | S_IRWXG | S_IROTH );
	ATF_REQUIRE_EQ(0, errno);
	ATF_REQUIRE(filefd >= 0);
	ATF_REQUIRE(write(filefd, teststring, strlen(teststring))>0);
	lseek(filefd, 0, SEEK_SET);
	close(filefd);

	ATF_REQUIRE_EQ(0, bcs_parseucl(bcs, "/tmp/config_for_ptcparse"));

	ATF_REQUIRE(0 != (bc = bcs_getconfig_byname(bcs, "another_one")));

	ATF_REQUIRE(0 != (ptc = ptc_new(bc)));

	ATF_REQUIRE_EQ(0, ptc_translate(ptc));
	ATF_REQUIRE(0 != (bpc = ptc_get_parameters(ptc)));

	ATF_REQUIRE_EQ(0, strcmp("another_one",
				 bpc_get_vmname(bpc)));
	ATF_REQUIRE_EQ(0, strcmp("some_file",
				 bpc_get_bootrom(bpc)->bootrom));
	printf("port name: %s\n", bpc_get_comport(bpc, 0)->portname);
	ATF_REQUIRE_EQ(0, strcmp("vmconsole0",
				 bpc_get_comport(bpc, 0)->portname));
	ATF_REQUIRE_EQ(1, bpc_get_comport(bpc, 0)->enabled);
	ATF_REQUIRE_EQ(0, strcmp("/dev/nmbd0",
				 bpc_get_comport(bpc, 0)->backend));

	/* free the store - that also frees the configs */
	bcs_free(bcs);
	ptc_free(ptc);
}
ATF_TC_CLEANUP(tc_ptc_translate, tc)
{
	unlink("/tmp/config_for_ptcparse");
}


ATF_TP_ADD_TCS(testplan)
{

	ATF_TP_ADD_TC(testplan, tc_ptc_init);
	ATF_TP_ADD_TC(testplan, tc_ptc_translate);

	return atf_no_error();
}

