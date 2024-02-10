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
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <syslog.h>
#include <unistd.h>

#include "../libconfig/config_core.h"
#include "../libconfig/output_bhyve_core.h"
#include "../libprocwatch/config_generator_object.h"
#include "../libtranslate/param_translate_core.h"

#include "config_generator.h"

int
vmstated_generate_config_file(const struct bhyve_configuration *bc,
			      const char *filename);

/*
 * config generator definition
 */
struct config_generator_object vmstated_cgo = {
	.generate_config_file = vmstated_generate_config_file
};

/*
 * Generates configuration merged from bhyve_configuration structure
 * and a preexisting bhyve_config file
 */
int
vmstated_generate_config_file(const struct bhyve_configuration *bc,
			      const char *filename)
{
	struct param_translate_core *ptc = 0;
	const struct bhyve_parameters_core *bpc = 0;
	struct output_bhyve_core *obc = 0;
	int result = 0;

	if (!(ptc = ptc_new(bc)))
		return -1;

	do {
		if (ptc_translate(ptc)) {
			result = -1;
			break;
		}

		/* get set of core parameters after translation */
		if (!(bpc = ptc_get_parameters(ptc))) {
			result = -1;
			break;
		}

		if (!(obc = obc_new(filename, bpc))) {
			result = -1;
			break;
		}

		result = obc_combine_with(obc, bc_get_configfile(bc));
			
		obc_free(obc);
	} while(0);

	ptc_free(ptc);

	return result;
}
