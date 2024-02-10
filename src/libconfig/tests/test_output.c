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

#include "../config_block.h"
#include "../config_core.h"
#include "../output_bhyve_core.h"

ATF_TC(tc_obc_output);
ATF_TC_HEAD(tc_obc_output, tc)
{
	int file_out = 0;
	const char *contents = "memory.size=8G\n\
x86.strictmsr=true\n\
x86.vmexit_on_hlt=true\n\
acpi_tables=true\n\
cpus=4\n\
destroy_on_poweroff=true\n\
pci.0.0.0.device=hostbridge\n\
pci.0.1.0.device=lpc\n\
pci.0.2.0.device=virtio-net\n\
pci.0.2.0.backend=tap99\n\
pci.0.3.0.device=virtio-blk\n\
pci.0.3.0.path=/dev/zvol/zroot/vols/testvm\n\
pci.0.3.0.direct=true\n\
pci.0.30.0.device=xhci\n\
pci.0.30.0.slot.1.device=tablet\n\
pci.0.31.0.device=fbuf\n\
pci.0.31.0.tcp=127.0.0.1:5900\n\
pci.0.31.0.w=1024\n\
pci.0.31.0.h=800\n\
pci.0.31.0.tablet=true\n\
lpc.com1.path=/dev/nmdm0A\n\
lpc.bootrom=/usr/local/share/uefi-firmware/BHYVE_UEFI.fd\n\
name=testvm\n\
";
	const char *crosscheck = "memory.size=8G\n\
x86.strictmsr=true\n\
x86.vmexit_on_hlt=true\n\
acpi_tables=true\n\
cpus=4\n\
destroy_on_poweroff=true\n\
pci.0.0.0.device=hostbridge\n\
pci.0.1.0.device=lpc\n\
pci.0.2.0.device=virtio-net\n\
pci.0.2.0.backend=tap99\n\
pci.0.3.0.device=virtio-blk\n\
pci.0.3.0.path=/dev/zvol/zroot/vols/testvm\n\
pci.0.3.0.direct=true\n\
pci.0.30.0.device=xhci\n\
pci.0.30.0.slot.1.device=tablet\n\
pci.0.31.0.device=fbuf\n\
pci.0.31.0.tcp=127.0.0.1:5900\n\
pci.0.31.0.w=1024\n\
pci.0.31.0.h=800\n\
pci.0.31.0.tablet=true\n\
lpc.com1.path=/dev/nmdm0A\n\
lpc.bootrom=/usr/local/share/uefi-firmware/BHYVE_UEFI.fd\n\
name=testvm\n\
\n\
acpi_tables=true\n\
x86.vmexit_on_hlt=true\n\
cpus=4\n\
memory.size=4096M\n\
name=testvm";
	size_t len = strlen(contents);

	file_out = open("combine_file", O_CREAT | O_RDWR);
	write(file_out, contents, len);
	fchmod(file_out, S_IRUSR | S_IWUSR);
	close(file_out);

	file_out = open("compare_file", O_CREAT | O_RDWR);
	len = strlen(crosscheck);
	write(file_out, crosscheck, len);
	close(file_out);
}
ATF_TC_BODY(tc_obc_output, tc)
{
	struct bhyve_parameters_core *bpc = 0;
	struct bhyve_parameters_pcislot *bpp = 0;
	struct output_bhyve_core *obc = 0;

	bpc = bpc_new("testvm");

	ATF_REQUIRE(0 != bpc);

	ATF_REQUIRE_EQ(0, bpc_set_bootrom(bpc, "bootrom", false, NULL));
	ATF_REQUIRE_EQ(0, bpc_enable_comport(bpc, "console0", 0, true));
	ATF_REQUIRE_EQ(0, bpc_set_comport_backend(bpc, 0, "/dev/nmbd0"));
	ATF_REQUIRE_EQ(0, bpc_set_cpulayout(bpc, 4, 0, 0));
	ATF_REQUIRE_EQ(0, bpc_set_memory(bpc, 1024 * 4));
	ATF_REQUIRE_EQ(0, bpc_set_generateacpi(bpc, true));
	ATF_REQUIRE_EQ(0, bpc_set_yieldonhlt(bpc, true));

	/* create and add new hostbridge */
	ATF_REQUIRE(0 != (bpp = bpp_new_hostbridge(HOSTBRIDGE)));
	ATF_REQUIRE_EQ(0, bpc_addpcislot_at(bpc, 0, 0, 0, bpp));
	ATF_REQUIRE_EQ(0, bpc_set_bootrom(bpc, "something", false, NULL));
	ATF_REQUIRE(0 != (bpp = bpp_new_isabridge()));
	ATF_REQUIRE_EQ(0, bpc_addpcislot_at(bpc, 0, 1, 0, bpp));
	ATF_REQUIRE(0 != (bpp = bpp_new_block()));
	ATF_REQUIRE_EQ(0, bpp_new_block_virtioblk(bpp_get_block(bpp), "test"));
	ATF_REQUIRE_EQ(0, bpc_addpcislot_at(bpc, 0, 2, 0, bpp));

	/* TODO write to output file, dump and compare to target */
	ATF_REQUIRE(0 != (obc = obc_new("test_output_file", bpc)));

	ATF_REQUIRE_EQ(0, obc_combine_with(obc, "combine_file"));
	atf_utils_cat_file("test_output_file", "output:");
	atf_utils_file_exists("test_output_file");
	atf_utils_compare_file("test_output_file", "compare_file");
	
	obc_free(obc);
	bpc_free(bpc);
}

ATF_TP_ADD_TCS(testplan)
{
	ATF_TP_ADD_TC(testplan, tc_obc_output);

	return atf_no_error();
}

