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

#ifndef __BHYVE_UCLPARSER_H__
#define __BHYVE_UCLPARSER_H__

#include <private/ucl/ucl.h>

#include "bhyve_config.h"
#include "../libcommand/nvlist_mapping.h"

/*
 * represents a parser item
 */
struct bhyve_uclparser_item {
	char *varname;
	/* offset of void* to embedded struct variable */
	size_t offset;
	
	/* delgation method that will parse further data */
	int (*delegate_func)(struct bhyve_configuration *bc, const ucl_object_t *confobj, void **ctx);
};

struct bhyve_uclparser_item *bup_find_subparser(struct bhyve_uclparser_item *bup,
						size_t bup_count,
						const char *varname);

int
bup_generic_parsefromucl(void *ctx, const ucl_object_t *confobj,
			 struct nvlistitem_mapping *mappings,
			 size_t mappings_count,
			 struct bhyve_uclparser_item *subparsers,
			 size_t subparsers_count);

int
bup_parsefromucl(struct bhyve_configuration *bc, const ucl_object_t *confobj,
		 struct nvlistitem_mapping *mappings, size_t mappings_count,
		 struct bhyve_uclparser_item *subparsers, size_t subparsers_count);

int
bup_parselistfromucl(void *ctx, const ucl_object_t *confobj,
		     int(*subparser)(void *, const char *,
				     const ucl_object_t *confobj));
#endif /* __BHYVE_UCLPARSER_H__ */
