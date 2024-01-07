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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <private/ucl/ucl.h>

#include "bhyve_config.h"
#include "bhyve_config_console.h"
#include "bhyve_uclparser.h"

#include "../libcommand/nvlist_mapping.h"

/*
 * parse specific console
 */
int
buf_parse_console(void *ctx, const char *consolename,
		  const ucl_object_t *confobj)
{
	struct bhyve_configuration_console_list *bccl = ctx;
	struct bhyve_configuration_console *bcc = 0;
	struct nvlistitem_mapping *mappings = bcc_get_mapping();

	/* set default name and enable by default */
	if (!(bcc = bcc_new(consolename, true)))
		return -1;

	if (bup_generic_parsefromucl(bcc, confobj,
					 mappings, bcc_get_mapping_count(),
					 NULL, 0)) {
		bccl_free(bccl);
		return -1;
	}

	if (bccl_add(bccl, bcc)) {
		bcc_free(bcc);
		return -1;
	}
	
	return 0;
}

/*
 * parses console sub elements
 */
int
buf_parse_console_list(struct bhyve_configuration *bc,
		       const ucl_object_t *confobj,
		       void **ctx)
{
	/* TODO implement */
	struct bhyve_configuration_console_list *bccl = 0;

	/* allocate a new console list */
	if (!(bccl = bccl_new()))
		return -1;

	if (bup_parselistfromucl(bccl, confobj, buf_parse_console)) {
		bccl_free(bccl);
		return -1;
	}

	if (ctx)
		*ctx = bccl;
	
	return 0;
}
