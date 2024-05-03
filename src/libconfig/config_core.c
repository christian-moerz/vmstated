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

#include <sys/queue.h>

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config_block.h"
#include "config_controller.h"
#include "config_core.h"
#include "config_hostbridge.h"
#include "config_network.h"
#include "config_vnc.h"

#include "parser_offsets.h"

#include "../libutils/bhyve_utils.h"

#define BPC_SETTER_FUNC(varname, vartype)		\
	int \
	bpc_set_##varname(struct bhyve_parameters_core *bpc, vartype varname) \
	{ \
		if (!bpc) {	\
			errno = EINVAL;		\
			return -1;		\
		}				\
		bpc->varname = varname;		\
		return 0;			\
	}


struct bhyve_parameters_cdrom {
};

/*
 * represents a pci slot assignment
 */
struct bhyve_parameters_pcislot {
	bhyve_parameters_pcislot_t slot_type;
	/* bus location */
	uint8_t bus;
	/* pci slot location */
	uint8_t pcislot;
	/* function location */
	uint8_t function;

	union {
		struct bhyve_parameters_hostbridge hostbridge;
		struct bhyve_parameters_block block;
		struct bhyve_parameters_network network;
		struct bhyve_parameters_controller controller;
		struct bhyve_parameters_cdrom cdrom;
		struct bhyve_parameters_vnc vnc;
	} data;

	SLIST_ENTRY(bhyve_parameters_pcislot) entries;
};

/*
 * iterator over pci slots
 */
struct bhyve_parameters_pcislot_iter {
	struct bhyve_parameters_pcislot *current;
	struct bhyve_parameters_pcislot *first;
};

/*
 * core parameters specific to a virtual machine
 */
struct bhyve_parameters_core {
	bool generate_acpi_tables;
	uint32_t memory;
	uint16_t numcpus;
	uint16_t sockets;
	uint16_t cores;
	bool yield_on_hlt;
	bool wire_memory;
	bool rtc_keeps_utc;
	bool x2apic_mode;
	char vmname[BPC_NAME_MAX];

	struct bhyve_parameters_comport comport[4];
	struct bhyve_parameters_bootrom bootrom;
	
	SLIST_HEAD(, bhyve_parameters_pcislot) pcislots;
};

struct bhyve_parameters_parser_info mapping_vars2core[] = {
	{
		.mapping = {
			.offset = offsetof(struct bhyve_parameters_core, generate_acpi_tables),
			.value_type = BOOLEAN,
			.size = sizeof(bool),
			.varname = "acpi_tables"
		},
		.filter = po_filter_bool
	},
	{
		.mapping = {
			.offset = offsetof(struct bhyve_parameters_core, memory),
			.value_type = UINT64,
			.size = sizeof(uint64_t),
			.varname = "memory.size"
		},
		.filter = po_filter_memory
	},
	{
		.mapping = {
			.offset = offsetof(struct bhyve_parameters_core, yield_on_hlt),
			.value_type = BOOLEAN,
			.size = sizeof(bool),
			.varname = "x86.vmexit_on_hlt"
		},
		.filter = po_filter_bool
	}
};

/*
 * get pointer to parser mapping
 */
const struct bhyve_parameters_parser_info *
bpc_get_parsermapping()
{
	return mapping_vars2core;
}

/* TODO implement method to get next available slot id */

/*
 * get bootrom access
 */
const struct bhyve_parameters_bootrom *
bpc_get_bootrom(const struct bhyve_parameters_core *bpc)
{
	if (!bpc) {
		errno = EINVAL;
		return NULL;
	}

	return &bpc->bootrom;
}

/*
 * set bootrom settings
 */
int
bpc_set_bootrom(struct bhyve_parameters_core *bpc,
		const char *bootrom, bool with_vars, const char *varsfile)
{
	if (!bpc || !bootrom || (with_vars && !varsfile)) {
		errno = EINVAL;
		return -1;
	}

	strncpy(bpc->bootrom.bootrom, bootrom, PATH_MAX);
	bpc->bootrom.with_vars = with_vars;
	if (with_vars)
		strncpy(bpc->bootrom.varsfile, varsfile, PATH_MAX);

	return 0;
}

/*
 * get comport access
 */
const struct bhyve_parameters_comport *
bpc_get_comport(const struct bhyve_parameters_core *bpc, size_t comport)
{
	if (!bpc) {
		errno = EINVAL;
		return NULL;
	}
	
	if (comport > 3) {
		errno = EDOM;
		return NULL;
	}
	
	return &bpc->comport[comport];
}

/*
 * enable or disable a comport
 */
int
bpc_enable_comport(struct bhyve_parameters_core *bpc,
		   const char *portname, uint8_t comport, bool enabled)
{
	if (comport > 3) {
		errno = EDOM;
		return -1;
	}

	bpc->comport[comport].enabled = enabled;
	strncpy(bpc->comport[comport].portname, portname, BPC_NAME_MAX);

	return 0;
}

/*
 * set backend for comport
 */
int
bpc_set_comport_backend(struct bhyve_parameters_core *bpc,
			uint8_t comport, const char *backend)
{
	if (comport > 3) {
		errno = EDOM;
		return -1;
	}

	strncpy(bpc->comport[comport].backend, backend, PATH_MAX);

	return 0;
}

/*
 * construct a new bhyve parameter config
 */
struct bhyve_parameters_core *
bpc_new(const char *vmname)
{
	struct bhyve_parameters_core *bpc = 0;

	if (!vmname) {
		errno = EINVAL;
		return NULL;
	}

	if (!(bpc = malloc(sizeof(struct bhyve_parameters_core))))
		return NULL;

	bzero(bpc, sizeof(struct bhyve_parameters_core));

	strncpy(bpc->vmname, vmname, BPC_NAME_MAX);

	SLIST_INIT(&bpc->pcislots);

	return bpc;
}

/*
 * get an iterator over all pci slots
 */
struct bhyve_parameters_pcislot_iter *
bpc_iter_pcislots(const struct bhyve_parameters_core *bpc)
{
	if (!bpc) {
		errno = EINVAL;
		return NULL;
	}

	struct bhyve_parameters_pcislot_iter *bppi = 0;

	if (!(bppi = malloc(sizeof(struct bhyve_parameters_pcislot_iter))))
		return NULL;

	bppi->current = NULL;
	
	if (SLIST_EMPTY(&bpc->pcislots))
		bppi->first = NULL;
	else
		bppi->first = SLIST_FIRST(&bpc->pcislots);

	return bppi;
}

/*
 * move iterator forward
 */
bool
bppi_next(struct bhyve_parameters_pcislot_iter *bppi)
{
	if (!bppi)
		return false;

	if (bppi->first) {
		bppi->current = bppi->first;
		bppi->first = NULL;
		return true;
	}

	if (!bppi->first && !bppi->current)
		return false;

	bppi->current = SLIST_NEXT(bppi->current, entries);
	return (bppi->current != NULL);
}

/*
 * release previously allocated iterator
 */
void
bppi_free(struct bhyve_parameters_pcislot_iter *bppi)
{
	if (!bppi)
		return;

	free(bppi);
}

/*
 * get current item from iterator
 */
const struct bhyve_parameters_pcislot *
bppi_item(struct bhyve_parameters_pcislot_iter *bppi)
{
	if (!bppi) {
		errno = EINVAL;
		return NULL;
	}
	
	return bppi->current;
}

/*
 * add a pci slot at a specific position
 */
int
bpc_addpcislot_at(struct bhyve_parameters_core *bpc,
		  uint8_t bus,
		  uint8_t pcislot,
		  uint8_t function,
		  struct bhyve_parameters_pcislot *bpp)
{
	if (!bpc || !bpp) {
		errno = EINVAL;
		return -1;
	}

	bpp->bus = bus;
	bpp->pcislot = pcislot;
	bpp->function = function;

	SLIST_INSERT_HEAD(&bpc->pcislots, bpp, entries);

	return 0;
}

/*
 * get vm name
 */
const char *
bpc_get_vmname(const struct bhyve_parameters_core *bpc)
{
	if (!bpc) {
		errno = EINVAL;
		return NULL;
	}

	return bpc->vmname;
}

/*
 * release a previusly allocated pci slot
 */
void
bpp_free(struct bhyve_parameters_pcislot *bpp)
{
	if (!bpp)
		return;

	free(bpp);
}

/*
 * release previously allocated paramter config
 */
void
bpc_free(struct bhyve_parameters_core *bpc)
{
	if (!bpc)
		return;

	struct bhyve_parameters_pcislot *bpp = 0;

	while (!SLIST_EMPTY(&bpc->pcislots)) {
		bpp = SLIST_FIRST(&bpc->pcislots);
		SLIST_REMOVE_HEAD(&bpc->pcislots, entries);
		bpp_free(bpp);
	}
	
	free(bpc);
}

/*
 * internal method for instantiating a new pci slot
 */
struct bhyve_parameters_pcislot *
bpp_new_pcislot(bhyve_parameters_pcislot_t slot_type)
{
	struct bhyve_parameters_pcislot *bpp = 0;

	if (!(bpp = malloc(sizeof(struct bhyve_parameters_pcislot))))
		return NULL;

	bzero(bpp, sizeof(struct bhyve_parameters_pcislot));
	bpp->slot_type = slot_type;

	return bpp;	
}

/*
 * construct new LPC ISA bridge
 */
struct bhyve_parameters_pcislot *
bpp_new_isabridge()
{
	return bpp_new_pcislot(TYPE_ISABRIDGE);
}

/*
 * construct a new hostbridge
 */
struct bhyve_parameters_pcislot *
bpp_new_hostbridge(bhyve_parameters_hostbridge_t hostbridge_type)
{
	struct bhyve_parameters_pcislot *bpp = bpp_new_pcislot(TYPE_HOSTBRIDGE);

	if (!bpp)
		return NULL;

	bpp->data.hostbridge.hostbridge_type = hostbridge_type;

	return bpp;
}

/*
 * get pointer to host bridge data
 */
const struct bhyve_parameters_hostbridge *
bpp_get_hostbridge(const struct bhyve_parameters_pcislot *bpp)
{
	if (!bpp) {
		errno = EINVAL;
		return NULL;
	}

	if (TYPE_HOSTBRIDGE != bpp_get_slottype(bpp))
		return NULL;

	return &bpp->data.hostbridge;
}

struct bhyve_parameters_pcislot *
bpp_new_xhci(bool tablet)
{
	struct bhyve_parameters_pcislot *bpp = bpp_new_pcislot(TYPE_CONTROL);

	if (!bpp)
		return NULL;

	if (bpp_controller_new_xhci(&bpp->data.controller, tablet)) {
		bpp_free(bpp);
		return NULL;
	}

	return bpp;		
}

/*
 * get block device object
 */
struct bhyve_parameters_block *
bpp_get_block(struct bhyve_parameters_pcislot *bpp)
{
	if (TYPE_BLOCK == bpp->slot_type)
		return &bpp->data.block;

	return NULL;
}

/*
 * create a new block device PCI slot
 */
struct bhyve_parameters_pcislot *
bpp_new_block()
{
	return bpp_new_pcislot(TYPE_BLOCK);
}

/*
 * get slot type
 */
bhyve_parameters_pcislot_t
bpp_get_slottype(const struct bhyve_parameters_pcislot *bpp)
{
	if (!bpp) {
		errno = EINVAL;
		return TYPE_INVALID;
	}

	return bpp->slot_type;
}

/*
 * get pci slot placement
 */
int
bpp_get_pciid(const struct bhyve_parameters_pcislot *bpp,
	      uint8_t *bus,
	      uint8_t *pcislot,
	      uint8_t *function)
{
	if (!bpp || !bus || !pcislot || !function) {
		errno = EINVAL;
		return -1;
	}

	*bus = bpp->bus;
	*pcislot = bpp->pcislot;
	*function = bpp->function;

	return 0;
}

/*
 * set cpu config
 */
int
bpc_set_cpulayout(struct bhyve_parameters_core *bpc,
		  uint16_t numcpus, uint16_t sockets, uint16_t cores)
{
	if (!bpc) {
		errno = EINVAL;
		return -1;
	}
	
	bpc->numcpus = numcpus;
	bpc->sockets = sockets;
	bpc->cores = cores;
	return 0;
}


/*
 * set memory
 */
int
bpc_set_memory(struct bhyve_parameters_core *bpc, uint32_t memory)
{
	if (!bpc) {
		errno = EINVAL;
		return -1;
	}
	
	bpc->memory = memory;
	return 0;
}

/*
 * get memory amount in megabytes
 */
uint32_t
bpc_get_memory(const struct bhyve_parameters_core *bpc)
{
	if (!bpc) {
		errno = EINVAL;
		return -1;
	}
	return bpc->memory;
}

/*
 * set yield on halt state
 */
int
bpc_set_yieldonhlt(struct bhyve_parameters_core *bpc, bool yield)
{
	if (!bpc) {
		errno = EINVAL;
		return -1;
	}

	bpc->yield_on_hlt = yield;

	return 0;
}

/*
 * get yield on halt value
 */
bool
bpc_get_yieldonhlt(const struct bhyve_parameters_core *bpc)
{
	if (!bpc) {
		errno = EINVAL;
		return -1;
	}
	return bpc->yield_on_hlt;
}

/*
 * get wired flag
 */
bool
bpc_get_wired(const struct bhyve_parameters_core *bpc)
{
	if (!bpc) {
		errno = EINVAL;
		return false;
	}

	return bpc->wire_memory;
}

/*
 * set wired flag
 */
int
bpc_set_wired(struct bhyve_parameters_core *bpc, bool wired)
{
	if (!bpc) {
		errno = EINVAL;
		return -1;
	}

	bpc->wire_memory = wired;
	return 0;
}

/*
 * set acpi table generation
 */
int
bpc_set_generateacpi(struct bhyve_parameters_core *bpc, bool acpi_tables)
{
	if (!bpc) {
		errno = EINVAL;
		return -1;
	}
	bpc->generate_acpi_tables = acpi_tables;
	return 0;
}

bool
bpc_get_generateacpi(const struct bhyve_parameters_core *bpc)
{
	if (!bpc) {
		errno = EINVAL;
		return -1;
	}
	return bpc->generate_acpi_tables;
}

BPC_SETTER_FUNC(numcpus, uint16_t);
BPC_SETTER_FUNC(sockets, uint16_t);
BPC_SETTER_FUNC(cores, uint16_t);

CREATE_GETTERFUNC_UINT16(bhyve_parameters_core, bpc, numcpus);
CREATE_GETTERFUNC_UINT16(bhyve_parameters_core, bpc, sockets);
CREATE_GETTERFUNC_UINT16(bhyve_parameters_core, bpc, cores);
