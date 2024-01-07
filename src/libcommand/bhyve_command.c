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

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bhyve_command.h"
#include "command_sender.h"

struct nvlistitem_mapping bhyve_command_nv[] = {
	{
		.offset = offsetof(struct bhyve_usercommand, cmd),
		.value_type = DYNAMICSTRING,
		.size = sizeof(char*),
		.varname = "cmd"
	},
	{
		.offset = offsetof(struct bhyve_usercommand, vmname),
		.value_type = DYNAMICSTRING,
		.size = sizeof(char*),
		.varname = "vmname"
	},
	{
		.offset = offsetof(struct bhyve_usercommand, result),
		.value_type = UINT32,
		.size = sizeof(int),
		.varname = "result"
	},
	{
		.offset = offsetof(struct bhyve_usercommand, reply),
		.value_type = DYNAMICSTRING,
		.size = sizeof(char*),
		.varname = "reply"
	}
};

/*
 * encode contents of buffer into nvlist; nvlist must be initialized already.
 */
int
bcmd_encodenvlist(struct nvlistitem_mapping *mapping, size_t mapping_count,
		 void *buffer, nvlist_t *nvl)
{
	if (!mapping || !buffer || !nvl) {
		errno = EINVAL;
		return -1;
	}

	size_t counter = 0;
	const char *strptr;
	const char **pstrptr;
	uint64_t *puint64 = 0;
	uint32_t *puint32 = 0;

	for (counter = 0; counter < mapping_count; counter++) {
		switch (mapping[counter].value_type) {
		case FIXEDSTRING:
			strptr = buffer + mapping[counter].offset;
			if (strptr)
				nvlist_add_string(nvl, mapping[counter].varname, strptr);
			break;
		case DYNAMICSTRING:
			pstrptr = buffer + mapping[counter].offset;
			if (*pstrptr)
				nvlist_add_string(nvl, mapping[counter].varname, *pstrptr);
			else
				nvlist_add_null(nvl, mapping[counter].varname);
			break;
		case UINT64:
			puint64 = buffer + mapping[counter].offset;
			nvlist_add_number(nvl, mapping[counter].varname, *puint64);
			break;
		case UINT32:
			puint32 = buffer + mapping[counter].offset;
			nvlist_add_number(nvl, mapping[counter].varname, *puint32);
			break;
		default:
			errno = EOPNOTSUPP;
			return -1;
		}
	}

	return 0;

}

/*
 * use default implementation to encode buffer (struct bhyve_usercommand) into nvlist
 */
int
bcmd_encodenvlist_command(struct bhyve_usercommand *buffer, nvlist_t *nvl)
{
	return bcmd_encodenvlist(bhyve_command_nv,
				 sizeof(bhyve_command_nv)/sizeof(struct nvlistitem_mapping),
				 buffer, nvl);
}

/*
 * parse contents of nvlist into buffer (struct bhyve_usercommand)
 */
int
bcmd_decodenvlist(struct nvlistitem_mapping *mapping, size_t mapping_count,
	       void *buffer, nvlist_t *nvl)
{
	size_t counter = 0;

	char *strptr;
	char **pstrptr;
	size_t size = 0;
	uint64_t *puint64;
	uint32_t *puint32;

	for (counter = 0; counter < mapping_count; counter++) {
		switch (mapping[counter].value_type) {
		case FIXEDSTRING:
			if (!nvlist_exists_string(nvl, mapping[counter].varname))
				continue;

			strptr = (buffer + mapping[counter].offset);
			strncpy(strptr, nvlist_get_string(nvl, mapping[counter].varname),
				mapping[counter].size);
			break;
		case DYNAMICSTRING:
			pstrptr = (buffer + mapping[counter].offset);
			if (!nvlist_exists_string(nvl, mapping[counter].varname)) {
				if (*pstrptr) {
					free(*pstrptr);
					*pstrptr = NULL;
				}
				continue;
			}
			
			size = strlen(nvlist_get_string(nvl, mapping[counter].varname)) + 1;
			/* free any previously allocated data */
			if (*pstrptr)
				free(*pstrptr);
			*pstrptr = malloc(size);
			if (!*pstrptr) {
				return -1;
			}
			strncpy(*pstrptr, nvlist_get_string(nvl, mapping[counter].varname), size);
			break;
		case UINT64:
			if (!nvlist_exists_number(nvl, mapping[counter].varname))
				continue;
			
			puint64 = (buffer + mapping[counter].offset);
			/* TODO implement with atoll instead? */
			*puint64 = nvlist_get_number(nvl, mapping[counter].varname);
			break;
		case UINT32:
			if (!nvlist_exists_number(nvl, mapping[counter].varname))
				continue;

			puint32 = (buffer + mapping[counter].offset);
			/* TODO implement with atoll instead? */
			*puint32 = nvlist_get_number(nvl, mapping[counter].varname);
			break;
		default:
			errno = EOPNOTSUPP;
			return -1;
		}
	}
	
	return 0;
}

/*
 * parse nvlist contents from buffer (packed nvlist) into struct bhyve_usercommand
 */
int
bcmd_parse_nvlistcmd(const char *buffer, size_t bufferlen, struct bhyve_usercommand *bcmd)
{
	nvlist_t *nvl = 0;
	int result = 0;

	if (!(nvl = nvlist_unpack(buffer, bufferlen, 0))) {
		return -1;
	}

	result = bcmd_decodenvlist(bhyve_command_nv,
				sizeof(bhyve_command_nv)/sizeof(struct nvlistitem_mapping),
				bcmd, nvl);
	
	nvlist_destroy(nvl);

	return result;
}

/*
 * releases previously allocated resources in a statically allocated
 * struct bhyve_usercommand
 */
void
bcmd_freestatic(struct bhyve_usercommand *bcmd)
{
	if (!bcmd)
		return;

	free(bcmd->cmd);
	free(bcmd->vmname);
	free(bcmd->reply);
}

/*
 * release previously allocated resources
 */
void
bcmd_free(struct bhyve_usercommand *bcmd)
{
	if (!bcmd)
		return;

	bcmd_freestatic(bcmd);
	free(bcmd);
}
