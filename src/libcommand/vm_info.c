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

#include <sys/types.h>
#include <sys/nv.h>

#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../libutils/bhyve_utils.h"
#include "../libutils/transmit_collect.h"

#include "bhyve_command.h"
#include "nvlist_mapping.h"
#include "vm_info.h"

/*
 * contains information about a bhyve virtual machine and its metadata
 */
struct bhyve_vm_info {
	char *vmname;
	char *os;
	char *osversion;
	uint32_t vmstate;
	pid_t pid;
	time_t lastboot;
	char *owner;
	char *group;
	char *description;
};

/*
 * contains information about bhyve management system
 */
struct bhyve_vm_manager_info {
	struct bhyve_vm_info **vm_infos;
	size_t vm_count;
	
	ssize_t msgcount;
};

struct nvlistitem_mapping vminfoitem2nvlist[] = {
	{
		.offset = offsetof(struct bhyve_vm_info, vmname),
		.value_type = DYNAMICSTRING,
		.size = sizeof(char*),
		.varname = "vmname"
	},
	{
		.offset = offsetof(struct bhyve_vm_info, os),
		.value_type = DYNAMICSTRING,
		.size = sizeof(char*),
		.varname = "os",
	},
	{
		.offset = offsetof(struct bhyve_vm_info, osversion),
		.value_type = DYNAMICSTRING,
		.size = sizeof(char*),
		.varname = "osversion",
	},
	{
		.offset = offsetof(struct bhyve_vm_info, vmstate),
		.value_type = UINT32,
		.size = sizeof(uint32_t),
		.varname = "vmstate",
	},
	{
		.offset = offsetof(struct bhyve_vm_info, vmstate),
		.value_type = UINT64,
		.size = sizeof(uint64_t),
		.varname = "pid",
	},
	{
		.offset = offsetof(struct bhyve_vm_info, lastboot),
		.value_type = UINT64,
		.size = sizeof(uint64_t),
		.varname = "lastboot",
	},
	{
		.offset = offsetof(struct bhyve_vm_info, owner),
		.value_type = DYNAMICSTRING,
		.size = sizeof(char*),
		.varname = "owner",
	},
	{
		.offset = offsetof(struct bhyve_vm_info, group),
		.value_type = DYNAMICSTRING,
		.size = sizeof(char*),
		.varname = "group",
	},
	{
		.offset = offsetof(struct bhyve_vm_info, description),
		.value_type = DYNAMICSTRING,
		.size = sizeof(char*),
		.varname = "description",
	},
};

struct nvlistitem_mapping vminfo2nvlist[] = {
	{
		.offset = offsetof(struct bhyve_vm_manager_info, vm_count),
		.value_type = UINT32,
		.size = sizeof(uint32_t),
		.varname = "vm_count"
	},
	{
		.offset = offsetof(struct bhyve_vm_manager_info, msgcount),
		.value_type = UINT64,
		.size = sizeof(uint64_t),
		.varname = "msgcount"
	}
};

/*
 * construct a new vm manager info structure
 */
struct bhyve_vm_manager_info *
bvmmi_new(struct bhyve_vm_info **vm_infos,
	  size_t vm_count,
	  ssize_t msgcount)
{
	struct bhyve_vm_manager_info *bvmmi = 0;
	size_t counter = 0;

	if (!(bvmmi = malloc(sizeof(struct bhyve_vm_manager_info))))
		return NULL;

	if (!(bvmmi->vm_infos = malloc(sizeof(struct bhyve_vm_info *) * vm_count))) {
		free(bvmmi);
		return NULL;
	}
	for (counter = 0; counter < vm_count; counter++) {
		bvmmi->vm_infos[counter] = vm_infos[counter];
	}
	
	bvmmi->vm_count = vm_count;
	bvmmi->msgcount = msgcount;

	return bvmmi;
}

/*
 * encode contents of bhyve_vm_info into pre-initialized
 * nvlist
 *
 * returns 0 on success; errno will be set on failure
 */
int
bvmi_encodenvlist(struct bhyve_vm_info *bvmi, nvlist_t *nvl)
{
	if (!bvmi || !nvl) {
		errno = EINVAL;
		return -1;
	}

	return bcmd_encodenvlist(vminfoitem2nvlist,
				 sizeof(vminfoitem2nvlist)/sizeof(struct nvlistitem_mapping),
				 bvmi, nvl);
}

/*
 * decodes contents from nvlist into an already allocated
 * struct bhyve_vm_info
 *
 * returns 0 on success
 */
int
bvmi_decodenvlist(nvlist_t *nvl, struct bhyve_vm_info *bvmi)
{
	if (!nvl || !bvmi) {
		errno = EINVAL;
		return -1;
	}

	return bcmd_decodenvlist(vminfoitem2nvlist,
				 sizeof(vminfoitem2nvlist)/sizeof(struct nvlistitem_mapping),
				 bvmi, nvl);
}

/*
 * decode contents of a nvlist into a bhyve_vm_manager_info; bvmmi must
 * already be allocated when calling this function.
 *
 * returns 0 on success; errno will be set on failure
 */
int
bvmmi_decodenvlist(nvlist_t *nvl, struct bhyve_vm_manager_info *bvmmi)
{
	const uint64_t *nvl_lengths = 0;
	size_t nvl_lengths_count = 0;
	int result = 0;
	ssize_t offset = 0;
	size_t counter = 0;
	struct bhyve_vm_info *bvi = 0;
	nvlist_t *itemnvl = 0;
	const void *buffer = 0;
	size_t bufferlen = 0;
	ssize_t total = 0;
	
	if (!bvmmi || !nvl) {
		errno = EINVAL;
		return -1;
	}

	/* zero memory first */
	bzero(bvmmi, sizeof(struct bhyve_vm_manager_info));
	
	/* decode bvmmi first */
	if (bcmd_decodenvlist(vminfo2nvlist,
			      sizeof(vminfo2nvlist)/sizeof(struct nvlistitem_mapping),
			      bvmmi, nvl))
		return -1;

	do {
		if (!nvlist_exists_number_array(nvl, "vm_infos.lengths")) {
			result = -1;
			break;
		}

		if (!(nvl_lengths = nvlist_get_number_array(nvl, "vm_infos.lengths", &nvl_lengths_count))) {
			result = -1;
			break;
		}
		assert(nvl_lengths_count == bvmmi->vm_count);

		/* allocate memory for vm_info pointers */
		if (!(bvmmi->vm_infos = malloc(sizeof(struct bhyve_vm_info*) * bvmmi->vm_count))) {
			result = -1;
			break;
		}

		/* get buffer length and buffer */
		buffer = nvlist_get_binary(nvl, "vm_infos", &bufferlen);

		/* cross check buffer lengths */
		for (counter = 0; counter < bvmmi->vm_count; counter++) {
			total += nvl_lengths[counter];
		}
		assert(total == bufferlen);

		for (counter = 0; counter < bvmmi->vm_count; counter++) {
			if (!(bvi = malloc(sizeof(struct bhyve_vm_info)))) {
				result = -1;
				break;
			}
			bzero(bvi, sizeof(struct bhyve_vm_info));

			/* unpack nvlist from buffer */
			if (!(itemnvl = nvlist_unpack(buffer+offset,
						      nvl_lengths[counter],
						      0))) {
				result = -1;
				break;
			}

			/* decode nvlist into bvi */
			if (bvmi_decodenvlist(itemnvl, bvi)) {
				result = -1;
				break;
			}

			/* destroy the nvlist */
			nvlist_destroy(itemnvl);
			
			bvmmi->vm_infos[counter] = bvi;
			
			offset += nvl_lengths[counter];
		}
	} while(0);

	/* TODO clean up anything half allocated? */

	return result;		
}

/*
 * get number of vms contained in manager
 */
size_t
bvmmi_getvmcount(struct bhyve_vm_manager_info *bvmmi)
{
	if (!bvmmi) {
		errno = EINVAL;
		return 0;
	}

	return bvmmi->vm_count;
}

/*
 * get specific vm info from manager
 */
const struct bhyve_vm_info *
bvmmi_getvminfo_byidx(struct bhyve_vm_manager_info *bvmmi, size_t idx)
{
	if (!bvmmi) {
		errno = EINVAL;
		return NULL;
	}

	if (idx >= bvmmi->vm_count) {
		errno = EDOM;
		return NULL;
	}

	return bvmmi->vm_infos[idx];
}

/*
 * encode vm_manager_info structure into binary blob
 *
 * encodes data and puts it into a newly allocated memory block that
 * is put into buffer; this memory needs to be freed by the caller.
 *
 * returns 0 on success
 */
int
bvmmi_encodebinary(struct bhyve_vm_manager_info *bvmmi, void **buffer, size_t *bufferlen)
{
	nvlist_t *nvl = nvlist_create(0);
	int result = 0;

	if (!nvl)
		return -1;

	if (!(result = bvmmi_encodenvlist(bvmmi, nvl))) {
		*buffer = nvlist_pack(nvl, bufferlen);

		/* result stays zero if buffer was allocated */
		result = (NULL == buffer);
	}

	nvlist_destroy(nvl);

	return result;
}

/*
 * decodes binary data tnto a bhyve_vm_manager_info structure; the
 * structure is allocated by the function and needs to be later free'd
 * by the caller.
 */
int
bvmmi_decodebinary(void *buffer, size_t bufferlen, struct bhyve_vm_manager_info **bvmmi)
{
	int result = 0;
	nvlist_t *nvl = nvlist_unpack(buffer, bufferlen, 0);

	if (!nvl)
		return -1;

	if (!(*bvmmi = malloc(sizeof(struct bhyve_vm_manager_info)))) {
		nvlist_destroy(nvl);
		return -1;
	}

	result = bvmmi_decodenvlist(nvl, *bvmmi);

	nvlist_destroy(nvl);

	return result;
}

/*
 * encode contents of bhyve_vm_manager_info into pre-initialized
 * nvlist
 *
 * returns 0 on success; errno will be set on failure
 */
int
bvmmi_encodenvlist(struct bhyve_vm_manager_info *bvmmi, nvlist_t *nvl)
{
	if (!bvmmi || !nvl) {
		errno = EINVAL;
		return -1;
	}

	size_t counter = 0;
	nvlist_t *itemnvl = 0;
	int result = 0;
	struct socket_transmission_collector *stc = 0;
	void *buffer = 0;
	size_t bufferlen = 0;
	size_t *lenarray = 0;
	uint64_t *lengths = 0;
	ssize_t outsize = 0;

	/* encode bvmmi first */
	if (bcmd_encodenvlist(vminfo2nvlist,
			      sizeof(vminfo2nvlist)/sizeof(struct nvlistitem_mapping),
			      bvmmi, nvl))
		return -1;

	/* build length array */
	if (!(lenarray = malloc(sizeof(size_t) * bvmmi->vm_count))) {
		return -1;
	}
	if (!(lengths = malloc(sizeof(uint64_t) * bvmmi->vm_count))) {
		free(lenarray);
		return -1;
	}
	
	if (!(stc = stc_new())) {
		free(lenarray);
		return -1;
	}

	for (counter = 0; (counter < bvmmi->vm_count) && (!result); counter++) {
		/* construct a new nvlist */
		if (!(itemnvl = nvlist_create(0))) {
			result = -1;
			break;
		}

		/* convert struct to nvlist */
		if (bvmi_encodenvlist(bvmmi->vm_infos[counter], itemnvl)) {
			result = -1;
		}

		/* pack nvlist into void buffer */
		if (!(buffer = nvlist_pack(itemnvl, &bufferlen))) {
			result = -1;
		} else {
			if (stc_store_transmit(stc, buffer, bufferlen))
				result = -1;

			free(buffer);
			buffer = NULL;
		}

		/* destroy nvlist again */
		nvlist_destroy(itemnvl);
	}

	/* build out buffer */
	do {
		outsize = stc_getbuffersize(stc);
		if (!(buffer = malloc(outsize))) {
			result = -1;
			break;
		}
		
		/* fill buffer and offset array */
		if (stc_collect(stc, buffer, outsize,
				lenarray, sizeof(size_t)*bvmmi->vm_count)) {
			result = -1;
			break;
		}

		/* add binary blob to nv list */
		nvlist_add_binary(nvl, "vm_infos", buffer, outsize);

		/* add lengths into array */
		/* write size_t array into uint64_t array */
		for (counter = 0; counter < bvmmi->vm_count; counter++) {
			lengths[counter] = lenarray[counter];
		}

		nvlist_add_number_array(nvl, "vm_infos.lengths", lengths, bvmmi->vm_count);
	} while(0);

	free(buffer);
	free(lengths);
	free(lenarray);
	stc_free(stc);

	return result;
}

/*
 * construct a new bhyve_vm_info structure
 */
struct bhyve_vm_info *
bvmi_new(const char *vmname,
	 const char *os,
	 const char *osversion,
	 const char *owner,
	 const char *group,
	 const char *description,
	 uint32_t vmstate,
	 pid_t pid,
	 time_t lastboot)
{
	struct bhyve_vm_info *bvmi = 0;

	if (!vmname) {
		errno = EINVAL;
		return NULL;
	}

	if (!(bvmi = malloc(sizeof(struct bhyve_vm_info))))
		return NULL;

	bzero(bvmi, sizeof(struct bhyve_vm_info));

	if (!(bvmi->vmname = strdup(vmname))) {
		free(bvmi);
		return NULL;
	}

	if (os)
		if (!(bvmi->os = strdup(os))) {
			bvmi_free(bvmi);
			return NULL;
		}

	if (osversion)
		if (!(bvmi->osversion = strdup(osversion))) {
			bvmi_free(bvmi);
			return NULL;
		}

	if (owner)
		if (!(bvmi->owner = strdup(owner))) {
			bvmi_free(bvmi);
			return NULL;
		}

	if (group)
		if (!(bvmi->group = strdup(group))) {
			bvmi_free(bvmi);
			return NULL;
		}

	if (description)
		if (!(bvmi->description = strdup(description))) {
			bvmi_free(bvmi);
			return NULL;
		}

	bvmi->vmstate = vmstate;
	bvmi->pid = pid;
	bvmi->lastboot = lastboot;

	return bvmi;
}

/*
 * release previously allocated vm info structure
 */
void
bvmi_free(struct bhyve_vm_info *bvmi)
{
	if (!bvmi)
		return;

	free(bvmi->vmname);
	free(bvmi->os);
	free(bvmi->osversion);
	free(bvmi->owner);
	free(bvmi->group);
	free(bvmi->description);
	free(bvmi);
}

/*
 * release a previously allocated bhyve_vm_manager_info structure
 */
void
bvmmi_free(struct bhyve_vm_manager_info *bvmmi)
{
	size_t counter = 0;

	for (counter = 0; counter < bvmmi->vm_count; counter++) {
		bvmi_free(bvmmi->vm_infos[counter]);
	}
	/* free backing pointer array */
	free(bvmmi->vm_infos);

	free(bvmmi);
}

CREATE_GETTERFUNC_STR(bhyve_vm_info, bvmi, vmname);
CREATE_GETTERFUNC_STR(bhyve_vm_info, bvmi, os);
CREATE_GETTERFUNC_STR(bhyve_vm_info, bvmi, osversion);
CREATE_GETTERFUNC_STR(bhyve_vm_info, bvmi, owner);
CREATE_GETTERFUNC_STR(bhyve_vm_info, bvmi, group);
CREATE_GETTERFUNC_STR(bhyve_vm_info, bvmi, description);

uint32_t
bvmi_get_state(const struct bhyve_vm_info *bvmi)
{
	return bvmi->vmstate;
}

pid_t
bvmi_get_pid(const struct bhyve_vm_info *bvmi)
{
	return bvmi->pid;
}

time_t
bvmi_get_lastboot(const struct bhyve_vm_info *bvmi)
{
	return bvmi->lastboot;
}
