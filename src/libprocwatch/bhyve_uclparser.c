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
#include <string.h>

#include "private/ucl/ucl.h"

#include "bhyve_config.h"
#include "bhyve_uclparser.h"
#include "bhyve_uclparser_funcs.h"

#include "../libcommand/nvlist_mapping.h"

/*
 * attempt to find a match for a variable name
 */
struct bhyve_uclparser_item *
bup_find_subparser(struct bhyve_uclparser_item *bup, size_t bup_count, const char *varname)
{
	if (!bup || !bup_count || !varname) {
		errno = EINVAL;
		return NULL;
	}

	while (bup_count && strcmp(bup->varname, varname)) {
		bup++;
		bup_count--;
	}

	if (bup_count)
		return bup;
	
	return NULL;
}

/*
 * look up matching mapping
 */
struct nvlistitem_mapping *
bup_findmapping(struct nvlistitem_mapping *mappings, size_t count, const char *varname)
{
	if (!mappings || !count || !varname) {
		errno = EINVAL;
		return NULL;
	}
	
	while (count && strcmp(mappings->varname, varname)) {
		mappings++;
		count--;
	}

	if (count)
		return mappings;

	return NULL;
}

/*
 * read list configuration from ucl object and delegate sub parsing to
 * a custom handler function
 */
int
bup_parselistfromucl(void *ctx, const ucl_object_t *confobj,
		     int(*subparser)(void *, const char *,
				     const ucl_object_t *confobj))
{
	ucl_object_iter_t it = NULL, it_obj = NULL;
	const ucl_object_t *cur, *tmp, *obj = 0;
	const char *keyname = NULL;

	while ((cur = ucl_iterate_object(confobj, &it, true))) {
		/* Iterate over the values of a key */
		keyname = ucl_object_key(cur);

		if (subparser(ctx, keyname, cur))
			return -1;
	}

	return 0;
}

/*
 * generalized parser from ucl object
 */
int
bup_generic_parsefromucl(void *ctx, const ucl_object_t *confobj,
		 struct nvlistitem_mapping *mappings, size_t mappings_count,
		 struct bhyve_uclparser_item *subparsers, size_t subparsers_count)
{
	ucl_object_iter_t it = NULL, it_obj = NULL;
	const ucl_object_t *cur, *tmp, *obj = 0;
	const char *keyname = NULL;
	struct nvlistitem_mapping *mapping = 0;
	struct bhyve_uclparser_item *bui = 0;
	char *strptr;
	char **pstrptr;
	const char *keyval = NULL;
	size_t size = 0;
	uint64_t *puint64 = 0;
	uint32_t *puint32 = 0;
	bool *pbool = 0;
	void *ctxptr = 0;
	while ((cur = ucl_iterate_object(confobj, &it, true))) {
		/* Iterate over the values of a key */
		keyname = ucl_object_key(cur);
		
		/* look up key name in config mapping */
		mapping = bup_findmapping(mappings, mappings_count, keyname);
		if (!mapping) {
			/* attempt lookup as function parser */
			if (subparsers)
				bui = bup_find_subparser(subparsers, subparsers_count, keyname);
			if (bui && bui->delegate_func) {
				ctxptr = ctx + bui->offset;
				if (bui->delegate_func(ctx, cur, ctxptr)) {
					/* TODO log error */
				}
			}

			/* otherwise ignore and continue */			
			continue;
		}
		
		switch (mapping->value_type) {
		case FIXEDSTRING:
				strptr = (ctx) + mapping->offset;
				keyval = ucl_object_tostring(cur);
				strncpy(strptr, keyval, mapping->size);
				break;
		case DYNAMICSTRING:
			pstrptr = ctx + mapping->offset;
			if (!ucl_object_tostring(cur)) {
				if (*pstrptr) {
					free(*pstrptr);
					*pstrptr = NULL;
				}
				continue;
			}
			
			size = strlen(ucl_object_tostring(cur)) + 1;
			/* free any previously allocated data */
			if (*pstrptr)
				free(*pstrptr);
			*pstrptr = malloc(size);
			if (!*pstrptr) {
				return -1;
			}
			keyval = ucl_object_tostring(cur);
			strncpy(*pstrptr, keyval, size);
			break;
		case UINT64:
			puint64 = ctx + mapping->offset;
			/* TODO implement with atoll instead? */
			*puint64 = ucl_object_toint(cur);
			break;
		case UINT32:
			puint32 = ctx + mapping->offset;
			*puint32 = ucl_object_toint(cur);
			break;
		case BOOLEAN:
			pbool = ctx + mapping->offset;
			*pbool = ucl_object_toboolean(cur);				
			break;
		default:
			errno = ENOSYS;
			return -1;
		}
	}

	return 0;
}

/*
 * read configuration from ucl object
 */
int
bup_parsefromucl(struct bhyve_configuration *bc, const ucl_object_t *confobj,
		 struct nvlistitem_mapping *mappings, size_t mappings_count,
		 struct bhyve_uclparser_item *subparsers, size_t subparsers_count)
{
	return bup_generic_parsefromucl(bc, confobj, mappings,
					mappings_count, subparsers,
					subparsers_count);
}
