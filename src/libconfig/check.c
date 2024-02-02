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

#include <errno.h>

#include "config_core.h"
#include "check.h"
#include "check_core.h"
#include "check_isabridge.h"

/*
 * defines a check to apply to a parameter configuration
 */
struct bhyve_parameters_check {
	const char *rule_name;
	int(*check)(struct bhyve_parameters_core *bpc);
	const char *error_message;
};

struct bhyve_parameters_check bpchecks[] = {
	{
		.rule_name = "valid parameter memory",
		.check = check_bpc_nonnull,
		.error_message = "NULL pointer as parameter is invalid"
	},
	{
		.rule_name = "non-empty vm name",
		.check = check_bpc_name,
		.error_message = "No name set for virtual machine"
	},
	{
		.rule_name = "hostbridge existence",
		.check = check_bpc_hostbridge,
		.error_message = "No hostbridge assigned"
	},
	{
		.rule_name = "single hostbridge",
		.check = check_bpc_singlehostbridge,
		.error_message = "Only one hostbridge supported"
	},
	{
		.rule_name = "hostbridge placement",
		.check = check_bpc_hostbridgeslot,
		.error_message = "Hostbridge must be at 0:0:0"
	},
	{
		.rule_name = "single isabridge",
		.check = check_bpc_singleisabridge,
		.error_message = "Only one isa bridge supported"
	},
	{
		.rule_name = "com port with isa bridge",
		.check = check_bpc_comwithisa,
		.error_message = "Having a COM port requires an LPC ISA bridge"
	},
	{
		.rule_name = "bootrom requires isa bridge",
		.check = check_bpc_bootromwithisa,
		.error_message = "Having a boot rom set requires an LPC ISA bridge"
	},
	{
		.rule_name = "bootrom specified",
		.check = check_bpc_bootrom,
		.error_message = "No bootrom specified"
	},
	{
		.rule_name = "isabridge bus zero",
		.check = check_bpc_isabridgebus,
		.error_message = "LPC ISA bridge not on bus 0"
	},
	{
		.rule_name = "pci id conflict",
		.check = check_bpc_pciidconflict,
		.error_message = "PCI ID conflict"
	}
};

/*
 * get error message for a failed rule
 */
const char *
check_get_errormsg(const struct bhyve_parameters_check *check)
{
	if (!check) {
		errno = EINVAL;
		return NULL;
	}

	return check->error_message;		
}

/*
 * apply rule set to parameters and check if any rules are broken.
 *
 * if no rules were broken, function returns NULL. Otherwise the first
 * broken rule is returned
 */
const struct bhyve_parameters_check *
check_parameters_ok(struct bhyve_parameters_core *bpc)
{
	size_t counter = 0;
	size_t total = sizeof(bpchecks) / sizeof(struct bhyve_parameters_check);

	for (counter = 0; counter < total; counter++) {
		if (bpchecks[counter].check(bpc))
			return &bpchecks[counter];
	}

	return NULL;
}
