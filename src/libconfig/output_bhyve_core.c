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
 * fill in core parameters
 */
int
obc_set_core(struct output_bhyve_core *obc)
{
	char memory[32] = {0};
	char numvar[32] = {0};
	
	obc_add(obc, "name", bpc_get_vmname(obc->bpc));
	if (bpc_get_memory(obc->bpc) > 0) {
		snprintf(memory, 32, "%dM", bpc_get_memory(obc->bpc));
		obc_add(obc, "memory.size", memory);
	}

	if (bpc_get_numcpus(obc->bpc)) {
		snprintf(numvar, 32, "%d", bpc_get_numcpus(obc->bpc));
		obc_add(obc, "cpus", numvar);
	}

	if (bpc_get_sockets(obc->bpc)) {
		snprintf(numvar, 32, "%d", bpc_get_sockets(obc->bpc));
		obc_add(obc, "sockets", numvar);
	}

	if (bpc_get_cores(obc->bpc)) {
		snprintf(numvar, 32, "%d", bpc_get_cores(obc->bpc));
		obc_add(obc, "sockets", numvar);
	}

	obc_add_bool(obc, "x86.vmexit_on_hlt", bpc_get_yieldonhlt(obc->bpc));

	obc_add_bool(obc, "acpi_tables", bpc_get_generateacpi(obc->bpc));

	return 0;	
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
