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
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "config_core.h"
#include "file_memory.h"
#include "output_bhyve_core.h"

/*
 * A configuration line generated from a config_core object
 */
struct output_bhyve_core_configline {
	char *configline;

	SLIST_ENTRY(output_bhyve_core_configline) entries;
};

/*
 * Central object for translating a config_core and its substructures
 * into bhyve configuration file.
 */
struct output_bhyve_core {
	/* the name of file to write to */
	char *configfile;

	/* the input data holding configuration */
	const struct bhyve_parameters_core *bpc;

	/* list of generated config records */
	SLIST_HEAD(, output_bhyve_core_configline) lines;
};

/*
 * free config line
 */
void
obcc_free(struct output_bhyve_core_configline *obcc)
{
	if (!obcc) {
		errno = EINVAL;
		return;
	}

	free(obcc->configline);
	free(obcc);
}

/*
 * add a config line
 */
int
obc_add(struct output_bhyve_core *obc, const char *key, const char *value)
{
	struct output_bhyve_core_configline *obcc = 0;
	size_t len_key = 0, len_val = 0, len_new = 0;

	if (!key || !value) {
		errno = EINVAL;
		return -1;
	}

	if (!(obcc = malloc(sizeof(struct output_bhyve_core_configline))))
		return -1;

	obcc->configline = 0;
	
	len_key = strlen(key);
	len_val = strlen(value);
	len_new = len_key + len_val + 2;
	
	if (!(obcc->configline = malloc(len_new))) {
		obcc_free(obcc);
		return -1;
	}

	snprintf(obcc->configline, len_new, "%s=%s", key, value);

	SLIST_INSERT_HEAD(&obc->lines, obcc,entries);
	
	return 0;
}

/*
 * set a boolean value
 */
int
obc_add_bool(struct output_bhyve_core *obc, const char *key, bool value)
{
	return obc_add(obc, key, value ? "true" : "false");
}

/*
 * fill in pci slots
 */
int
obc_set_pcislots(struct output_bhyve_core *obc)
{
	struct bhyve_parameters_pcislot_iter *bppi = 0;
	const struct bhyve_parameters_pcislot *bpp = 0;
	uint8_t bus = 0, pcislot = 0, function = 0;
	char pci_path[PATH_MAX+128] = {0};
	char pci_device[256] = {0};
	char pci_backend[256] = {0};

	if (!(bppi = bpc_iter_pcislots(obc->bpc))) {
		syslog(LOG_ERR, "Failed to get pci slot iterator");
		return -1;
	}

	syslog(LOG_INFO, "Constructing pci data");

	while (bppi_next(bppi)) {
		bpp = bppi_item(bppi);
		if (!bpp) {
			syslog(LOG_ERR, "Got null from pci slot iterator");
			break;
		}

		if (bpp_get_pciid(bpp, &bus, &pcislot, &function)) {
			syslog(LOG_ERR, "Failed to get pci slot id");
			return -1;
		}

		snprintf(pci_path, sizeof(pci_path),
			 "pci.%d.%d.%d.path", bus, pcislot, function);
		snprintf(pci_device, sizeof(pci_device),
			 "pci.%d.%d.%d.device", bus, pcislot, function);
		snprintf(pci_backend, sizeof(pci_backend),
			 "pci.%d.%d.%d.backend", bus, pcislot, function);

		syslog(LOG_INFO, "Looking at pci slot type %d",
		       bpp_get_slottype(bpp));

		switch (bpp_get_slottype(bpp)) {
		case TYPE_ISABRIDGE:
			if (obc_add(obc, pci_device, "lpc")) {
				syslog(LOG_ERR, "Failed to add lpc output");
				return -1;
			}
			break;
		case TYPE_HOSTBRIDGE:
			/* add either default or amd host bridge */
			if (obc_add(obc, pci_device,
				    bpp_get_hostbridge(bpp)->hostbridge_type == HOSTBRIDGE_AMD ? "amd_hostbridge" : "hostbridge" )) {
				syslog(LOG_ERR, "Failed to add hostbridge output");
				return -1;
			}
			break;
		case TYPE_BLOCK:
			return -1;
		case TYPE_NET:
			return -1;
		case TYPE_CONTROL:
			return -1;
		case TYPE_CDROM:
			return -1;
		case TYPE_VNC:
			return -1;
		case TYPE_INVALID:
			return -1;
		}
	}

	bppi_free(bppi);

	syslog(LOG_INFO, "Completed pci data construction");

	return 0;
}

/*
 * fill in console definitions
 */
int
obc_set_consoles(struct output_bhyve_core *obc)
{
	const struct bhyve_parameters_comport *bpc_com = 0;
	char comdef[256] = {0};
	int result = 0;
	
	for (int i = 0; i < 4; i++) {
		/* check whether comport is active */
		bpc_com = bpc_get_comport(obc->bpc, i);

		if (!bpc_com)
			return -1;

		if (!bpc_com->enabled)
			continue;

		/* i+1 because it starts at com1, not com0 */
		snprintf(comdef, sizeof(comdef), "lpc.com%i.path", i + 1);
		if ((result = obc_add(obc, comdef, bpc_com->backend)))
			break;
	}

	return result;
}

/*
 * fill in core parameters
 */
int
obc_set_core(struct output_bhyve_core *obc)
{
	char memory[32] = {0};
	char numvar[32] = {0};
	char bootrom[PATH_MAX*2+64] = {0};
	const struct bhyve_parameters_bootrom *bp_boot = 0;
	int result = 0;
	
	obc_add(obc, "name", bpc_get_vmname(obc->bpc));
	if (bpc_get_memory(obc->bpc) > 0) {
		snprintf(memory, 32, "%dM", bpc_get_memory(obc->bpc));
		if ((result = obc_add(obc, "memory.size", memory)))
			return result;
	}

	if (bpc_get_numcpus(obc->bpc)) {
		snprintf(numvar, 32, "%d", bpc_get_numcpus(obc->bpc));
		if ((result = obc_add(obc, "cpus", numvar)))
			return result;
	}

	if (bpc_get_sockets(obc->bpc)) {
		snprintf(numvar, 32, "%d", bpc_get_sockets(obc->bpc));
		if ((result = obc_add(obc, "sockets", numvar)))
			return result;
	}

	if (bpc_get_cores(obc->bpc)) {
		snprintf(numvar, 32, "%d", bpc_get_cores(obc->bpc));
		if ((result = obc_add(obc, "sockets", numvar)))
			return result;
	}

	if ((result = obc_add_bool(obc,
				   "x86.vmexit_on_hlt",
				   bpc_get_yieldonhlt(obc->bpc))))
		return result;

	if ((result = obc_add_bool(obc,
				   "acpi_tables",
				   bpc_get_generateacpi(obc->bpc))))
		return result;

	if ((bp_boot = bpc_get_bootrom(obc->bpc))) {
		/* add bootrom if set */
		snprintf(bootrom, sizeof(bootrom), "%s%s%s",
			 bp_boot->bootrom,
			 bp_boot->with_vars ? "," : "",
			 bp_boot->varsfile);
		if (obc_add(obc, "lpc.bootrom", bootrom))
			return -1;
	}

	if ((result = obc_set_consoles(obc)))
		return result;

	result = obc_set_pcislots(obc);
	
	return result;
}

/*
 * Construct a new output helper
 */
struct output_bhyve_core *
obc_new(const char *configfile, const struct bhyve_parameters_core *bpc)
{
	if (!configfile || !bpc) {
		errno = EINVAL;
		return NULL;
	}

	struct output_bhyve_core *obc = 0;

	if (!(obc = malloc(sizeof(struct output_bhyve_core))))
		return NULL;

	bzero(obc, sizeof(struct output_bhyve_core));
	if (!(obc->configfile = strdup(configfile))) {
		free(obc);
		return NULL;
	}

	obc->bpc = bpc;

	SLIST_INIT(&obc->lines);

	syslog(LOG_INFO, "Building bhyve_config for file %s",
	       configfile);

	if (obc_set_core(obc)) {
		obc_free(obc);
		return NULL;
	}

	return obc;
}

/*
 * Generate output from input file and data in this config object
 */
int
obc_combine_with(struct output_bhyve_core *obc, const char *file_in)
{
	struct file_memory *fm = 0;
	int filefd = 0;
	size_t file_len = 0, line_len = 0;
	int result = 0;
	struct output_bhyve_core_configline *obcc = 0;

	if (!obc || !file_in) {
		errno = EINVAL;
		return -1;
	}

	if (!(fm = fm_new(file_in)))
		return -1;

	file_len = strlen(fm_get_memory(fm));

	do {
	
		if ((filefd = open(obc->configfile, O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC)) < 0) {
			result = -1;
			break;
		}

		if (fchmod(filefd, S_IRUSR | S_IWUSR | S_IRGRP ) < 0) {
			result = -1;
			break;
		}

		do {
			if (write(filefd, fm_get_memory(fm), file_len) < file_len) {
				result = -1;
				break;
			}

			if (write(filefd, "\n", 1) < 0) {
				result = -1;
				break;
			}

			SLIST_FOREACH(obcc, &obc->lines, entries) {
				line_len = strlen(obcc->configline);
				if (write(filefd, obcc->configline, line_len) < line_len) {
					result = -1;
					break;
				}
				if (write(filefd, "\n", 1) < 0) {
					result = -1;
					break;
				}
			}
		} while(0);

		close(filefd);
	} while(0);

	fm_free(fm);

	return result;
}

/*
 * Release perviously allocated output helper
 */
void
obc_free(struct output_bhyve_core *obc)
{
	if (!obc) {
		errno = EINVAL;
		return;
	}

	struct output_bhyve_core_configline *obcc = 0;

	while (!SLIST_EMPTY(&obc->lines)) {
		obcc = SLIST_FIRST(&obc->lines);
		SLIST_REMOVE_HEAD(&obc->lines, entries);
		obcc_free(obcc);
	}

	free(obc->configfile);
	free(obc);
}
