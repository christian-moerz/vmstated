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

#include <string.h>
#include "bhyve_command.h"
#include "bhyve_director.h"
#include "process_state.h"

/*
 * defines a bhyve command
 */
struct bhyve_command {
	const char *command;

	int(*func_numeric)(struct bhyve_director *, const char *);
};

struct bhyve_command bhyve_director_commands[] = {
	{
		.command = "startvm",
		.func_numeric = bd_startvm
	},
	{
		.command = "stopvm",
		.func_numeric = bd_stopvm
	}
};

/*
 * look up available command
 */
struct bhyve_command *
bc_lookupcmd(const char *command)
{
	if (!command)
		return NULL;

	size_t total = sizeof(bhyve_director_commands)/sizeof(struct bhyve_command);
	size_t counter = 0;

	for (counter = 0; counter < total; counter++) {
		if (!strcmp(bhyve_director_commands[counter].command, command))
			return &bhyve_director_commands[counter];
	}
	
	return NULL;
}

/*
 * execute a function call with numeric return value
 */
int
bc_call_numeric(struct bhyve_command *bc, struct bhyve_director *bd, const char *vmname)
{
	if (!bc || !bd || !vmname)
		return -1;

	if (!bc->func_numeric)
		return -1;

	return bc->func_numeric(bd, vmname);
}
