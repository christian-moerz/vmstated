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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bhyve_config_console.h"
#include "../libcommand/nvlist_mapping.h"

/*
 * represents a console configuration
 */
struct bhyve_configuration_console {
	char *name;
	bool enabled;

	LIST_ENTRY(bhyve_configuration_console) entries;
};

/*
 * represents a list of consoles
 */
struct bhyve_configuration_console_list {
	/* list of console entries */
	LIST_HEAD(,bhyve_configuration_console) consoles;
};

struct nvlistitem_mapping bc_console2config[] = {
	{
		.offset = offsetof(struct bhyve_configuration_console, name),
		.value_type = DYNAMICSTRING,
		.size = sizeof(char*),
		.varname = "name"
	},
	{
		.offset = offsetof(struct bhyve_configuration_console, enabled),
		.value_type = BOOLEAN,
		.size = sizeof(bool),
		.varname = "enabled"
	}
};

/*
 * get number of mappings
 */
size_t
bcc_get_mapping_count()
{
	return sizeof(bc_console2config)/sizeof(struct nvlistitem_mapping);
}

/*
 * get nvlist mapping
 */
struct nvlistitem_mapping *
bcc_get_mapping()
{
	return bc_console2config;
}

/*
 * construct a new, uninitialized console
 */
struct bhyve_configuration_console *
bcc_new_empty()
{
	struct bhyve_configuration_console *bcc = 0;

	if (!(bcc = malloc(sizeof(struct bhyve_configuration_console))))
		return NULL;

	bzero(bcc, sizeof(struct bhyve_configuration_console));

	return bcc;
}

/*
 * construct a new console
 */
struct bhyve_configuration_console *
bcc_new(const char *name, bool enabled)
{
	struct bhyve_configuration_console *bcc = 0;

	if (!name) {
		errno = EINVAL;
		return NULL;
	}
	
	if (!(bcc = malloc(sizeof(struct bhyve_configuration_console))))
		return NULL;

	bzero(bcc, sizeof(struct bhyve_configuration_console));
	
	if (!(bcc->name = strdup(name))) {
		free(bcc);
		return NULL;
	}

	bcc->enabled = enabled;
	return bcc;
}

/*
 * release previously allocated console structure
 */
void
bcc_free(struct bhyve_configuration_console *bcc)
{
	if (!bcc) {
		errno = EINVAL;
		return;
	}

	free(bcc->name);
	free(bcc);
}

/*
 * constructs a new bhyve console list
 */
struct bhyve_configuration_console_list *
bccl_new()
{
	struct bhyve_configuration_console_list *bccl = 0;

	if (!(bccl = malloc(sizeof(struct bhyve_configuration_console_list))))
		return NULL;
	
	LIST_INIT(&bccl->consoles);

	return bccl;
}

/*
 * add a console into the list
 */
int
bccl_add(struct bhyve_configuration_console_list *bccl,
	 struct bhyve_configuration_console *bcc)
{
	if (!bccl || !bcc)
		return -1;

	LIST_INSERT_HEAD(&bccl->consoles, bcc, entries);

	return 0;
}

/*
 * get number of consoles in the list
 */
size_t
bccl_count(struct bhyve_configuration_console_list *bccl)
{
	if (!bccl)
		return 0;

	size_t counter = 0;
	struct bhyve_configuration_console *bcc = 0;

	LIST_FOREACH(bcc, &bccl->consoles, entries) {
		counter++;
	}

	return counter;
}

/*
 * release resources from a console list
 */
void
bccl_free(struct bhyve_configuration_console_list *bccl)
{
	struct bhyve_configuration_console *bcc = 0;

	if (!bccl) {
		errno = EINVAL;
		return;
	}

	while (!LIST_EMPTY(&bccl->consoles)) {
		bcc = LIST_FIRST(&bccl->consoles);
		LIST_REMOVE(bcc, entries);
		bcc_free(bcc);
	}

	free(bccl);
}
