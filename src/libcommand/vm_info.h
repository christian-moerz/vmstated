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

#ifndef __VM_INFO_H__
#define __VM_INFO_H__

#include <sys/nv.h>
#include <stddef.h>
#include <time.h>

struct bhyve_vm_info;
struct bhyve_vm_manager_info;

struct bhyve_vm_info *bvmi_new(const char *vmname,
			       const char *os,
			       const char *osversion,
			       const char *owner,
			       const char *group,
			       const char *description,
			       uint32_t vmstate,
			       pid_t pid,
			       time_t lastboot);
void bvmi_free(struct bhyve_vm_info *bvmi);
struct bhyve_vm_manager_info *bvmmi_new(struct bhyve_vm_info **vm_infos,
					size_t vm_count,
					ssize_t msgcount);
void bvmmi_free(struct bhyve_vm_manager_info *bvmmi);

int bvmmi_decodenvlist(nvlist_t *nvl, struct bhyve_vm_manager_info *bvmmi);
int bvmmi_encodenvlist(struct bhyve_vm_manager_info *bvmmi, nvlist_t *nvl);
int bvmmi_encodebinary(struct bhyve_vm_manager_info *bvmmi, void **buffer, size_t *bufferlen);
int bvmmi_decodebinary(void *buffer, size_t bufferlen, struct bhyve_vm_manager_info **bvmmi);

size_t bvmmi_getvmcount(struct bhyve_vm_manager_info *bvmmi);
const struct bhyve_vm_info *bvmmi_getvminfo_byidx(struct bhyve_vm_manager_info *bvmmi, size_t idx);

const char * bvmi_get_vmname(const struct bhyve_vm_info *bvmi);
const char * bvmi_get_os(const struct bhyve_vm_info *bvmi);
const char * bvmi_get_osversion(const struct bhyve_vm_info *bvmi);
const char * bvmi_get_owner(const struct bhyve_vm_info *bvmi);
const char * bvmi_get_group(const struct bhyve_vm_info *bvmi);
const char * bvmi_get_description(const struct bhyve_vm_info *bvmi);
uint32_t bvmi_get_state(const struct bhyve_vm_info *bvmi);
const char *bvmi_get_statestring(const struct bhyve_vm_info *bvmi);
pid_t bvmi_get_pid(const struct bhyve_vm_info *bvmi);
time_t bvmi_get_lastboot(const struct bhyve_vm_info *bvmi);

#endif /* __VM_INFO_H__ */
