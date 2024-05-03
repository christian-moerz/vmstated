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

#ifndef __CONFIG_CORE_H__
#define __CONFIG_CORE_H__

#include <limits.h>
#include <stdbool.h>
#include <unistd.h>

#include "config_hostbridge.h"

#define BPC_NAME_MAX 255
#define BPC_PASS_MAX 255
#define BPC_PARM_MAX 255

typedef enum {
	TYPE_HOSTBRIDGE = 0,
	TYPE_BLOCK = 1,
	TYPE_NET = 2,
	TYPE_CONTROL = 3,
	TYPE_CDROM = 4,
	TYPE_VNC = 5,
	TYPE_ISABRIDGE = 6,
	TYPE_INVALID = 999
} bhyve_parameters_pcislot_t;

struct bhyve_parameters_comport {
	char portname[BPC_NAME_MAX];
	bool enabled;
	/* device name to attach to, i.e. /dev/nmdb0 */
	char backend[PATH_MAX];
};

struct bhyve_parameters_bootrom {
	char bootrom[PATH_MAX];
	bool with_vars;
	char varsfile[PATH_MAX];
};

/* the core configuration structure
   use libconfig's ptc_translate to convert data from a
   bhyve_configuration structure into this */
struct bhyve_parameters_core;
struct bhyve_parameters_pcislot;
struct bhyve_parameters_pcislot_iter;

struct bhyve_parameters_core *bpc_new(const char *vmname);
void		bpp_free(struct bhyve_parameters_pcislot *bpp);
void		bpc_free(struct bhyve_parameters_core *bpc);
const struct bhyve_parameters_comport *
bpc_get_comport(const struct bhyve_parameters_core *bpc, size_t comport);
const struct bhyve_parameters_bootrom *
bpc_get_bootrom(const struct bhyve_parameters_core *bpc);
int
bpc_set_bootrom(struct bhyve_parameters_core *bpc,
		const char *bootrom, bool with_vars, const char *varsfile);
int
bpc_enable_comport(struct bhyve_parameters_core *bpc,
		   const char *portname, uint8_t comport, bool enabled);
int
bpc_set_comport_backend(struct bhyve_parameters_core *bpc,
			uint8_t comport, const char *backend);

struct bhyve_parameters_pcislot *bpp_new_hostbridge(bhyve_parameters_hostbridge_t hostbridge_type);
struct bhyve_parameters_pcislot *bpp_new_xhci(bool tablet);
struct bhyve_parameters_pcislot *bpp_new_block();
struct bhyve_parameters_pcislot *bpp_new_isabridge();
struct bhyve_parameters_block *bpp_get_block(struct bhyve_parameters_pcislot *bpp);

int
bpc_addpcislot_at(struct bhyve_parameters_core *bpc,
		  uint8_t bus,
		  uint8_t pcislot,
		  uint8_t function,
		  struct bhyve_parameters_pcislot *bpp);
const char     *bpc_get_vmname(const struct bhyve_parameters_core *bpc);

struct bhyve_parameters_pcislot_iter *bpc_iter_pcislots(const struct bhyve_parameters_core *bpc);
bool		bppi_next(struct bhyve_parameters_pcislot_iter *bppi);
const struct bhyve_parameters_pcislot *bppi_item(struct bhyve_parameters_pcislot_iter *bppi);
void		bppi_free(struct bhyve_parameters_pcislot_iter *bppi);

bhyve_parameters_pcislot_t
bpp_get_slottype(const struct bhyve_parameters_pcislot *bpp);

const struct bhyve_parameters_hostbridge *
bpp_get_hostbridge(const struct bhyve_parameters_pcislot *bpp);

int
bpp_get_pciid(const struct bhyve_parameters_pcislot *bpp,
	      uint8_t *bus,
	      uint8_t *pcislot,
	      uint8_t *function);

const struct bhyve_parameters_parser_info *bpc_get_parsermapping();

int
bpc_set_cpulayout(struct bhyve_parameters_core *bpc,
		  uint16_t numcpus, uint16_t sockets, uint16_t cores);
int bpc_set_numcpus(struct bhyve_parameters_core *, uint16_t);
int bpc_set_sockets(struct bhyve_parameters_core *, uint16_t);
int bpc_set_cores(struct bhyve_parameters_core *, uint16_t);

uint16_t bpc_get_numcpus (const struct bhyve_parameters_core *);
uint16_t bpc_get_sockets (const struct bhyve_parameters_core *);
uint16_t bpc_get_cores (const struct bhyve_parameters_core *);
int
bpc_set_memory(struct bhyve_parameters_core *bpc, uint32_t memory);
uint32_t
bpc_get_memory(const struct bhyve_parameters_core *bpc);
int
bpc_set_yieldonhlt(struct bhyve_parameters_core *bpc, bool yield);
bool
bpc_get_yieldonhlt(const struct bhyve_parameters_core *bpc);
int
bpc_set_generateacpi(struct bhyve_parameters_core *bpc, bool acpi_tables);
bool
bpc_get_generateacpi(const struct bhyve_parameters_core *bpc);
bool
bpc_get_wired(const struct bhyve_parameters_core *bpc);
int
bpc_set_wired(struct bhyve_parameters_core *bpc, bool wired);

#endif				/* __CONFIG_CORE_H__ */
