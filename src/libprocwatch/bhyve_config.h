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

#ifndef __BHYVE_CONFIG_H__
#define __BHYVE_CONFIG_H__

#include <sys/nv.h>
#include <time.h>

#define BHYVEBIN "/usr/sbin/bhyve"

struct bhyve_configuration;
struct bhyve_configuration_store;
struct bhyve_configuration_iterator;

int bc_tonvlist(struct bhyve_configuration *bc, nvlist_t *nvl);
int bc_fromnvlist(struct bhyve_configuration *bc, nvlist_t *nvl);
int bc_getuid(struct bhyve_configuration *bc);
int bc_getgid(struct bhyve_configuration *bc);

const char *bc_get_name(const struct bhyve_configuration *);
const char *bc_get_configfile(const struct bhyve_configuration *);
const char *bc_get_scriptpath(const struct bhyve_configuration *);
const char *bc_get_os(const struct bhyve_configuration *);
const char *bc_get_osversion(const struct bhyve_configuration *);
const char *bc_get_owner(const struct bhyve_configuration *);
const char *bc_get_group(const struct bhyve_configuration *);
const char *bc_get_description(const struct bhyve_configuration *);
const char *bc_get_backingfile(const struct bhyve_configuration *);
const char *bc_get_bootrom(const struct bhyve_configuration *);
int         bc_set_generated_config(struct bhyve_configuration *bc,
				    const char *generated_config);
const char *bc_get_generated_config(const struct bhyve_configuration *);
uint32_t    bc_get_maxrestart(const struct bhyve_configuration *bc);
time_t      bc_get_maxrestarttime(const struct bhyve_configuration *bc);
size_t      bc_get_consolecount(const struct bhyve_configuration *bc);
uint32_t    bc_get_memory(const struct bhyve_configuration *bc);
int         bc_set_numcpus(const struct bhyve_configuration *bc, uint16_t numcpus);
uint16_t    bc_get_numcpus(const struct bhyve_configuration *bc);
int         bc_set_sockets(const struct bhyve_configuration *bc, uint16_t sockets);
uint16_t    bc_get_sockets(const struct bhyve_configuration *bc);
int         bc_set_cores(const struct bhyve_configuration *bc, uint16_t cores);
uint16_t    bc_get_cores(const struct bhyve_configuration *bc);
const struct bhyve_configuration_console_list *
            bc_get_consolelist(const struct bhyve_configuration *bc);
bool
bc_get_vmexithlt(const struct bhyve_configuration *bc);
bool
bc_get_wired(const struct bhyve_configuration *bc);
bool
bc_get_generateacpi(const struct bhyve_configuration *bc);

struct bhyve_configuration_store *bcs_new(const char *searchpath);
void bcs_free(struct bhyve_configuration_store *bcs);
int bcs_parseucl(struct bhyve_configuration_store *bcs, const char *configfile);
struct bhyve_configuration *bcs_getconfig_byname(struct bhyve_configuration_store *bcs,
						 const char *name);
int bcs_walkdir(struct bhyve_configuration_store *bcs);

struct bhyve_configuration_iterator *bcs_iterate_configs(struct bhyve_configuration_store *bcs);
const struct bhyve_configuration *bci_getconfig(struct bhyve_configuration_iterator *bci);
bool bci_next(struct bhyve_configuration_iterator *bci);
void bci_free(struct bhyve_configuration_iterator *bci);

#endif /* __BHYVE_CONFIG_H__ */
