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

#include "../process_def.h"

struct bhyve_configuration {
	char name[PATH_MAX];
	char configfile[PATH_MAX];
	char scriptpath[PATH_MAX];
	char *os;
	char *osversion;
	char *owner;
	char *group;
	char *description;

	uint32_t maxrestart;
	time_t maxrestarttime;

	/* console configuration options */
	struct bhyve_configuration_console_list *consoles;
};

struct bhyve_configuration *
bc_new(const char *name,
       const char *configfile,
       const char *os,
       const char *osversion, const char*, const char*);
void bc_free(struct bhyve_configuration *bc);

ATF_TC_WITH_CLEANUP(tc_bcs_initfree);
ATF_TC_HEAD(tc_bcs_initfree, tc)
{
}
ATF_TC_BODY(tc_bcs_initfree, tc)
{
	struct bhyve_configuration_store *bcs = bcs_new("./configs");

	ATF_REQUIRE(0 != bcs);
	bcs_free(bcs);
}
ATF_TC_CLEANUP(tc_bcs_initfree, tc)
{
}

ATF_TC_WITH_CLEANUP(tc_bc_convert2nvlist);
ATF_TC_HEAD(tc_bc_convert2nvlist, tc)
{
}
ATF_TC_BODY(tc_bc_convert2nvlist, tc)
{
	struct bhyve_configuration *bc = bc_new("vmname",
						"vm.conf",
						"FreeBSD",
						"14.0",
						NULL,
						"wheel");
	struct bhyve_configuration *bc2 = bc_new("xxx",
						 "xxx",
						 "xxx",
						 "xxx",
						 "xxx",
						 "xxx");
	nvlist_t *nvl = nvlist_create(0);
	ATF_REQUIRE(0 != bc);
	ATF_REQUIRE(0 != nvl);

	errno = 0;
	int result = bc_tonvlist(bc, nvl);
	printf("result = %d\n", result);
	printf("errno = %d\n", errno);
	ATF_REQUIRE_EQ(0, result);
	nvlist_dump(nvl, 0);

	ATF_REQUIRE_EQ(0, bc_fromnvlist(bc2, nvl));
	ATF_REQUIRE_EQ(0, strcmp(bc->name, bc2->name));
	ATF_REQUIRE_EQ(0, strcmp(bc->configfile, bc2->configfile));
	ATF_REQUIRE_EQ(0, strcmp(bc->os, bc2->os));
	ATF_REQUIRE_EQ(0, strcmp(bc->osversion, bc2->osversion));
	printf("bc2->owner = %s\n", bc2->owner);
	ATF_REQUIRE_EQ(0, bc2->owner);
	ATF_REQUIRE_EQ(0, strcmp(bc->group, bc2->group));

	bc_free(bc);
	nvlist_destroy(nvl);
}
ATF_TC_CLEANUP(tc_bc_convert2nvlist, tc)
{
}

ATF_TC_WITH_CLEANUP(tc_bc_usrgrp);
ATF_TC_HEAD(tc_bc_usrgrp, tc)
{
}
ATF_TC_BODY(tc_bc_usrgrp, tc)
{
	struct bhyve_configuration *bc = bc_new("vmname",
						"vm.conf",
						"FreeBSD",
						"14.0",
						NULL,
						"wheel");

	ATF_REQUIRE_EQ(-1, bc_getuid(bc));
	ATF_REQUIRE_EQ(0, bc_getgid(bc));

	bc_free(bc);

	bc = bc_new("vmname",
		    "vm.conf",
		    "FreeBSD",
		    "14.0",
		    "root",
		    "video");

	ATF_REQUIRE_EQ(0, bc_getuid(bc));
	ATF_REQUIRE_EQ(44, bc_getgid(bc));

	bc_free(bc);
}
ATF_TC_CLEANUP(tc_bc_usrgrp, tc)
{
}

ATF_TC_WITH_CLEANUP(tc_bc_libucl);
ATF_TC_HEAD(tc_bc_libucl, tc)
{
	unlink("/tmp/testfile");
}
ATF_TC_BODY(tc_bc_libucl, tc)
{
	int filefd = 0;
	const char *teststring = "something { key = value; }\n";
	struct ucl_parser *uclp = 0;
	ucl_object_t *uclo = 0;

	filefd = open("/tmp/testfile", O_RDWR | O_CREAT);
	fchmod(filefd, S_IRWXU | S_IRWXG | S_IROTH );
	printf("errno = %d\n", errno);
	ATF_REQUIRE(filefd >= 0);
	ATF_REQUIRE(write(filefd, teststring, strlen(teststring))>0);
	lseek(filefd, 0, SEEK_SET);
	close(filefd);

	uclp = ucl_parser_new(UCL_PARSER_KEY_LOWERCASE | UCL_PARSER_ZEROCOPY);
	ATF_REQUIRE(0 != uclp);

	ATF_REQUIRE_EQ(1, ucl_parser_add_file(uclp, "/tmp/testfile"));

	printf("errno = %d\n", errno);

	if (ucl_parser_get_error(uclp))
		printf("ucl_error: %s\n",
		       ucl_parser_get_error(uclp));

	uclo = ucl_parser_get_object(uclp);
	ATF_REQUIRE(0 != uclo);

	const ucl_object_t *obj = NULL;
	ucl_object_iter_t it = NULL, it_obj = NULL;
	const ucl_object_t *cur, *tmp;

	/* Iterate over the object */
	while ((obj = ucl_iterate_object(uclo, &it, true))) {
		printf("key: \"%s\"\n", ucl_object_key(obj));
		/* Iterate over the values of a key */
		while((cur = ucl_iterate_object(obj, &it_obj, false))) {
			printf("value: \"%s\"\n", ucl_object_tostring_forced(cur));
		}
	}

	ucl_parser_free(uclp);
}
ATF_TC_CLEANUP(tc_bc_libucl, tc)
{
	unlink("/tmp/testfile");
}

ATF_TC_WITH_CLEANUP(tc_bc_uclparsing);
ATF_TC_HEAD(tc_bc_uclparsing, tc)
{
}
ATF_TC_BODY(tc_bc_uclparsing, tc)
{
	int filefd = 0;
	const char *teststring = "something { configfile = test.conf;\nowner = root; group = wheel;}\n";
	struct bhyve_configuration_store *bcs = bcs_new("/tmp");
	struct bhyve_configuration *bc = 0;

	filefd = open("/tmp/testfile", O_RDWR | O_CREAT);
	fchmod(filefd, S_IRWXU | S_IRWXG | S_IROTH );
	printf("errno = %d\n", errno);
	ATF_REQUIRE(filefd >= 0);
	ATF_REQUIRE(write(filefd, teststring, strlen(teststring))>0);
	lseek(filefd, 0, SEEK_SET);
	close(filefd);

	ATF_REQUIRE_EQ(0, bcs_parseucl(bcs, "/tmp/testfile"));

	ATF_REQUIRE(0 != (bc = bcs_getconfig_byname(bcs, "something")));

	printf("bc->configfile = %s\n", bc->configfile);
	printf("bc->owner = %s\n", bc->owner);
	ATF_REQUIRE_EQ(0, strcmp(bc->owner, "root"));
	ATF_REQUIRE_EQ(0, strcmp(bc->configfile, "test.conf"));

	bcs_free(bcs);

}
ATF_TC_CLEANUP(tc_bc_uclparsing, tc)
{
	unlink("/tmp/testfile");
}

ATF_TC_WITH_CLEANUP(tc_bc_uclmultiparsing);
ATF_TC_HEAD(tc_bc_uclmultiparsing, tc)
{
}
ATF_TC_BODY(tc_bc_uclmultiparsing, tc)
{
	int filefd = 0;
	const char *teststring = "something { configfile = test.conf;\nowner = root; group = wheel;}\n" \
		"another_one { configfile = test2.conf;\nowner = lclchristianm; maxrestart = 3; maxrestarttime = 10; }";
	struct bhyve_configuration_store *bcs = bcs_new("/tmp");
	struct bhyve_configuration *bc = 0;

	errno = 0;
	filefd = open("/tmp/testfile", O_RDWR | O_CREAT);
	fchmod(filefd, S_IRWXU | S_IRWXG | S_IROTH );
	ATF_REQUIRE_EQ(0, errno);
	ATF_REQUIRE(filefd >= 0);
	ATF_REQUIRE(write(filefd, teststring, strlen(teststring))>0);
	close(filefd);

	ATF_REQUIRE_EQ(0, bcs_parseucl(bcs, "/tmp/testfile"));

	ATF_REQUIRE(0 != (bc = bcs_getconfig_byname(bcs, "something")));

	ATF_REQUIRE_EQ(0, strcmp(bc->owner, "root"));
	ATF_REQUIRE_EQ(0, strcmp(bc->configfile, "test.conf"));

	ATF_REQUIRE(0 != (bc = bcs_getconfig_byname(bcs, "another_one")));
	printf("bc->owner = %s\n", bc->owner);
	ATF_REQUIRE_EQ(0, strcmp(bc->owner, "lclchristianm"));
	ATF_REQUIRE_EQ(3, bc->maxrestart);
	ATF_REQUIRE_EQ(10, bc->maxrestarttime);

	bcs_free(bcs);
}
ATF_TC_CLEANUP(tc_bc_uclmultiparsing, tc)
{
	unlink("/tmp/testfile");
}

ATF_TC(tc_bc_parsedir);
ATF_TC_HEAD(tc_bc_parsedir, tc)
{
}
ATF_TC_BODY(tc_bc_parsedir, tc)
{
	struct bhyve_configuration_store *bcs = bcs_new("/tmp/testdir");
	struct bhyve_configuration *bc = 0;

	int filefd = 0;
	const char *teststring = "something { configfile = test.conf;\nowner = root; group = wheel;}\n" \
		"another_one { configfile = test2.conf;\nowner = lclchristianm; }";
	const char *teststring2 = "another_here { configfile = abc.conf;\n" \
		"owner = root;\n}";

	mkdir("/tmp/testdir", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	mkdir("/tmp/testdir/something", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	mkdir("/tmp/testdir/another", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

	errno = 0;
	filefd = open("/tmp/testdir/something/config", O_RDWR | O_CREAT);
	fchmod(filefd, S_IRWXU | S_IRWXG | S_IROTH );
	ATF_REQUIRE_EQ(0, errno);
	ATF_REQUIRE(filefd >= 0);
	ATF_REQUIRE(write(filefd, teststring, strlen(teststring))>0);
	close(filefd);

	filefd = open("/tmp/testdir/another/config", O_RDWR | O_CREAT);
	fchmod(filefd, S_IRWXU | S_IRWXG | S_IROTH );
	ATF_REQUIRE_EQ(0, errno);
	ATF_REQUIRE(filefd >= 0);
	ATF_REQUIRE(write(filefd, teststring2, strlen(teststring2))>0);
	close(filefd);
	
	ATF_REQUIRE_EQ(0, bcs_walkdir(bcs));
	printf("errno = %d\n", errno);

	ATF_REQUIRE(0 != (bc = bcs_getconfig_byname(bcs, "something")));

	ATF_REQUIRE_EQ(0, strcmp(bc->owner, "root"));
	ATF_REQUIRE_EQ(0, strcmp(bc->configfile, "test.conf"));

	ATF_REQUIRE(0 != (bc = bcs_getconfig_byname(bcs, "another_one")));
	printf("bc->owner = %s\n", bc->owner);
	ATF_REQUIRE_EQ(0, strcmp(bc->owner, "lclchristianm"));

	ATF_REQUIRE(0 != (bc = bcs_getconfig_byname(bcs, "another_here")));
	printf("bc->owner = %s\n", bc->owner);
	ATF_REQUIRE_EQ(0, strcmp(bc->owner, "root"));

	ATF_REQUIRE_EQ(0, strcmp(bc_get_name(bc), "another_here"));
	
	bcs_free(bcs);
	ATF_REQUIRE_EQ(0, unlink("/tmp/testdir/something/config"));
	unlink("/tmp/testdir/another/config");
	rmdir("/tmp/testdir/something");
	rmdir("/tmp/testdir/another");
	rmdir("/tmp/testdir");

}

ATF_TC_WITH_CLEANUP(tc_bc_iterator);
ATF_TC_HEAD(tc_bc_iterator, tc)
{
}
ATF_TC_BODY(tc_bc_iterator, tc)
{
	int filefd = 0;
	const char *teststring = "something { configfile = test.conf;\nowner = root; group = wheel;}\n" \
		"another_one { configfile = test2.conf;\nowner = lclchristianm; maxrestart = 3; maxrestarttime = 10; }";
	struct bhyve_configuration_store *bcs = bcs_new("/tmp");
	const struct bhyve_configuration *bc = 0;
	struct bhyve_configuration_iterator *bci = 0;
	int counter = 0;

	errno = 0;
	filefd = open("/tmp/testfile", O_RDWR | O_CREAT);
	fchmod(filefd, S_IRWXU | S_IRWXG | S_IROTH );
	ATF_REQUIRE_EQ(0, errno);
	ATF_REQUIRE(filefd >= 0);
	ATF_REQUIRE(write(filefd, teststring, strlen(teststring))>0);
	lseek(filefd, 0, SEEK_SET);
	close(filefd);

	ATF_REQUIRE_EQ(0, bcs_parseucl(bcs, "/tmp/testfile"));

	ATF_REQUIRE(0 != (bci = bcs_iterate_configs(bcs)));
	while (bci_next(bci)) {
		printf("counter = %d\n", counter);
		ATF_REQUIRE(0 != (bc = bci_getconfig(bci)));
		counter++;
	};
	ATF_REQUIRE_EQ(0, bci_getconfig(bci));
	ATF_REQUIRE_EQ(2, counter);
	bci_free(bci);

	bcs_free(bcs);
}
ATF_TC_CLEANUP(tc_bc_iterator, tc)
{
	unlink("/tmp/testfile");
}

ATF_TC_WITH_CLEANUP(tc_bc_networkconfig);
ATF_TC_HEAD(tc_bc_networkconfig, tc)
{
}
ATF_TC_BODY(tc_bc_networkconfig, tc)
{
	int filefd = 0;
	const char *teststring = "another_one {\n"
		"configfile = test2.conf;\n" \
		"owner = lclchristianm;\n" \
		"maxrestart = 3;\n" \
		"maxrestarttime = 10;\n" \
		"\tconsoles {\n" \
		"\t\tconsole0 {\n" \
		"\t\t\tname = vmconsole0;\n" \
		"\t\t}\n" \
		"\t} \n" \
		"}";
	struct bhyve_configuration_store *bcs = bcs_new("/tmp");
	const struct bhyve_configuration *bc = 0;
	struct bhyve_configuration_iterator *bci = 0;
	int counter = 0;

	errno = 0;
	filefd = open("/tmp/config_with_network", O_RDWR | O_CREAT);
	fchmod(filefd, S_IRWXU | S_IRWXG | S_IROTH );
	ATF_REQUIRE_EQ(0, errno);
	ATF_REQUIRE(filefd >= 0);
	ATF_REQUIRE(write(filefd, teststring, strlen(teststring))>0);
	lseek(filefd, 0, SEEK_SET);
	close(filefd);

	ATF_REQUIRE_EQ(0, bcs_parseucl(bcs, "/tmp/config_with_network"));

	ATF_REQUIRE(0 != (bci = bcs_iterate_configs(bcs)));
	while (bci_next(bci)) {
		printf("counter = %d\n", counter);
		ATF_REQUIRE(0 != (bc = bci_getconfig(bci)));
		ATF_REQUIRE_EQ(0, strcmp(bc_get_name(bc), "another_one"));
		ATF_REQUIRE_EQ(0, strcmp(bc_get_owner(bc), "lclchristianm"));
		counter++;
	};
	ATF_REQUIRE_EQ(0, bci_getconfig(bci));
	ATF_REQUIRE_EQ(1, counter);

	ATF_REQUIRE_EQ(1, bc_get_consolecount(bc));
	bci_free(bci);

	bcs_free(bcs);
}
ATF_TC_CLEANUP(tc_bc_networkconfig, tc)
{
	unlink("/tmp/config_with_network");	
}


ATF_TP_ADD_TCS(testplan)
{
	ATF_TP_ADD_TC(testplan, tc_bcs_initfree);
	ATF_TP_ADD_TC(testplan, tc_bc_convert2nvlist);
	ATF_TP_ADD_TC(testplan, tc_bc_usrgrp);
	ATF_TP_ADD_TC(testplan, tc_bc_libucl);
	ATF_TP_ADD_TC(testplan, tc_bc_uclparsing);
	ATF_TP_ADD_TC(testplan, tc_bc_uclmultiparsing);
	ATF_TP_ADD_TC(testplan, tc_bc_parsedir);
	ATF_TP_ADD_TC(testplan, tc_bc_iterator);
	ATF_TP_ADD_TC(testplan, tc_bc_networkconfig);

	unlink("/tmp/testdir/something/config");
	unlink("/tmp/testdir/another/config");
	rmdir("/tmp/testdir/something");
	rmdir("/tmp/testdir/another");
	rmdir("/tmp/testdir");
	
	return atf_no_error();
}
