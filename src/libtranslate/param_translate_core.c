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
#include <stddef.h>
#include <stdlib.h>
#include <strings.h>
#include <syslog.h>
#include <unistd.h>

#include "../libconfig/config_core.h"
#include "../libprocwatch/bhyve_config.h"
#include "../libprocwatch/bhyve_config_console.h"

#define bc_transfer_nn_variable(structname, varname)	\
	if (bc_get_##varname(structname->bc)) { \
		bpc_set_##varname(structname->bpc, bc_get_##varname(ptc->bc)); \
	}

/*
 * Translator / interpreter for bringing input parameters from a
 * bhyve_configuration into a config_core structure.
 */
struct param_translate_core {
	const struct bhyve_configuration *bc;
	struct bhyve_parameters_core *bpc;
};

/*
 * get translated parameters
 */
const struct bhyve_parameters_core *
ptc_get_parameters(const struct param_translate_core *ptc)
{
	if (!ptc) {
		errno = EINVAL;
		return NULL;
	}

	return ptc->bpc;
}


/*
 * Translates contents of bhyve_configuration into bhyve_parameters_core
 */
int
ptc_translate(struct param_translate_core *ptc)
{
	int i = 0;
	
	if (!ptc) {
		errno = EINVAL;
		return -1;
	}

	syslog(LOG_INFO, "Staring bhyve_config to core translation");

	ptc->bpc = bpc_new(bc_get_name(ptc->bc));

	/* add bootrom */
	if (bc_get_bootrom(ptc->bc)) {
		/* TODO add support for vars file */
		bpc_set_bootrom(ptc->bpc, bc_get_bootrom(ptc->bc), false, NULL);
	}

	/* add core configuration variables */
	bpc_set_yieldonhlt(ptc->bpc, bc_get_vmexithlt(ptc->bc));
	bpc_set_generateacpi(ptc->bpc, bc_get_generateacpi(ptc->bc));
	bpc_set_wired(ptc->bpc, bc_get_wired(ptc->bc));

	/* add memory and cpu data */
	bc_transfer_nn_variable(ptc, memory);
	bc_transfer_nn_variable(ptc, numcpus);
	bc_transfer_nn_variable(ptc, sockets);
	bc_transfer_nn_variable(ptc, cores);

	if (bc_get_hostbridge(ptc->bc)) {
		syslog(LOG_INFO, "Translating hostbridge configuration");
		/* add a hostbridge */
		if (bpc_addpcislot_at(ptc->bpc,
				  0, 0, 0,
				  bpp_new_hostbridge(
					  !strcasecmp(bc_get_hostbridge(ptc->bc), "amd") ? HOSTBRIDGE_AMD : HOSTBRIDGE))) {
			syslog(LOG_ERR, "Failed to add hostbridge to core config");
			return -1;
		}
	} else {
		syslog(LOG_INFO, "No hostbridge specified in config");
	}
	
	/* add consoles */
	if (bc_get_consolecount(ptc->bc)) {
		/* transfer console information */
		const struct bhyve_configuration_console_list *bccl = 0;
		const struct bhyve_configuration_console *bcc = 0;

		bccl = bc_get_consolelist(ptc->bc);

		for(i = 0; i < 4; i++) {
			bcc = bccl_get_consolebyidx(bccl, i);
			if (bcc) {
				/* translate specific console */
				if (bpc_enable_comport(ptc->bpc, bcc_get_name(bcc), i, true))
					return -1;

				if (bcc_get_backend(bcc) && *bcc_get_backend(bcc)) {
					bpc_set_comport_backend(ptc->bpc,
								i, bcc_get_backend(bcc));
				}
			}
		}

		/* we need an LPC ISA bridge now to connect consoles */
		if (bpc_addpcislot_at(ptc->bpc, 0, 1, 0, bpp_new_isabridge()))
			return -1;
	}

	syslog(LOG_INFO, "Translation completed");

	return 0;
}

/*
 * Construct a new translator
 */
struct param_translate_core *
ptc_new(const struct bhyve_configuration *bc)
{
	struct param_translate_core *ptc = 0;
	
	if (!bc) {
		errno = EINVAL;
		return NULL;
	}

	if (!(ptc = malloc(sizeof(struct param_translate_core))))
		return NULL;

	bzero(ptc, sizeof(struct param_translate_core));
	
	ptc->bc = bc;	

	return ptc;
}

/*
 * Release a previously allocated translator
 */
void
ptc_free(struct param_translate_core *ptc)
{
	bpc_free(ptc->bpc);
	free(ptc);
}
